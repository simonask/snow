#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"
#include "snow/continuation.h"
#include "snow/task-intern.h"

#include <pthread.h>
#include <stdlib.h>
#include <malloc/malloc.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

// only included for access to data structure layout in GC phases
#include "snow/codegen.h"
#include "snow/pointer.h"
#include "snow/ast.h"

#define DEBUG_MALLOC 0 // set to 1 to override snow_malloc

volatile bool _snow_gc_is_collecting = false;

HIDDEN SnArray** _snow_store_ptr(); // necessary for accessing global stuff

struct SnGCObjectHead;
struct SnGCObjectTail;
struct SnGCAllocInfo;
struct SnGCMetaInfo;
struct SnGCHeap;
struct SnGCHeapList;

// internal functions
typedef void(*SnGCAction)(VALUE* root_pointer, bool on_stack);
static void gc_mark_root(VALUE* root, bool on_stack);
static void gc_update_root(VALUE* root, bool on_stack);

typedef void(*SnGCHeapAction)(struct SnGCHeap* heap, byte* object, struct SnGCAllocInfo* alloc_info, struct SnGCMetaInfo* meta_info, void* userdata);

static void gc_minor();
static void gc_major();
static byte* gc_find_object_start(const struct SnGCHeap* heap, const byte* ptr, struct SnGCAllocInfo**, struct SnGCMetaInfo**);
static struct SnGCHeap* gc_find_heap(const void* root);
static bool gc_heap_contains(const struct SnGCHeap* heap, const void* root);
static void gc_mark_stack(byte* bottom, byte* top);
static void gc_compute_checksum(struct SnGCAllocInfo* alloc_info);
static bool gc_check_checksum(const struct SnGCAllocInfo* alloc_info);
static bool gc_looks_like_allocation(const byte* ptr);
static void gc_transplant(byte* object, struct SnGCAllocInfo* alloc_info, struct SnGCMetaInfo* meta_info, struct SnGCHeapList* transplant_to);
static void gc_finalize_object(void* object, struct SnGCAllocInfo* alloc_info, struct SnGCMetaInfo* meta_info);
static void gc_clear_flags();
static void gc_with_each_object_in_heap_do(struct SnGCHeap* heap, SnGCHeapAction action, void* userdata);

typedef enum SnGCFlag {
	GC_NO_FLAGS      = 0,
	GC_MARK          = 1,         // the object is referenced by reachable roots
	GC_TRANSPLANTED  = 1 << 1,    // the object is no longer here
	GC_UPDATED       = 1 << 2,    // the object has had its roots updated
	GC_INDEFINITE    = 1 << 3,    // the object is referenced from the stack or another indefinite chunk of memory
	GC_FLAGS_MAX
} SnGCFlag;

typedef enum SnGCAllocType {
	GC_INVALID = 0x0,
	GC_OBJECT  = 0x1,
	GC_BLOB    = 0x2,
	GC_ATOMIC  = 0x3
	// must not exceed 2 bits.
} SnGCAllocType;

typedef struct SnGCAllocInfo {
	// Header info must never change throughout the lifetime of an object.
	// Only flag contents may change.
	// sizeof(SnGCAllocInfo) must be == 8 bytes.
	unsigned size         : 32;
	unsigned object_index : 24;
	unsigned alloc_type   : 2;
	unsigned checksum     : 6; // sum of bits set in all the other fields
} PACKED SnGCAllocInfo;

typedef struct SnGCMetaInfo {
	// sizeof(SnGCMetaInfo) must be == 8 bytes.
	SnGCFreeFunc free_func;
	#ifdef ARCH_IS_32_BIT
	byte padding[4];
	#endif
} PACKED SnGCMetaInfo;

#define MAGIC_BEAD_HEAD 0xbe4dbeedbaadb3ad
#define MAGIC_BEAD_TAIL 0xb3adbaadbeedbe4d
typedef uint64_t SnGCMagicBead;

typedef struct SnGCObjectHead {
	SnGCAllocInfo alloc_info;
	SnGCMagicBead bead;
} SnGCObjectHead;

typedef struct SnGCObjectTail {
	SnGCMetaInfo meta_info;
	SnGCMagicBead bead;
} SnGCObjectTail;

#define DEFAULT_HEAP_SIZE (1<<20)
typedef byte SnGCFlags;
static void* gc_alloc_chunk(size_t);
static void gc_free_chunk(void* chunk, size_t);
#include "snow/gcheap.h"

#define GC_NURSERY_SIZE 0x100000 // 1 MiB nurseries
#define GC_ADULT_SIZE 0x800000 // 8 MiB adult heaps
#define GC_BIG_ALLOCATION_SIZE_LIMIT 0x1000 // everything above 4K will go in the "biggies" allocation list

typedef struct SnGCNursery {
	SnGCHeap heap;
	struct SnGCNursery* next;
	struct SnGCNursery* previous;
} SnGCNursery;

struct {
	pthread_mutex_t nursery_lock;
	pthread_key_t nursery_key;
	SnGCNursery* nursery_head;
	
	SnGCHeapList adults;
	SnGCHeapList biggies;
	SnGCHeapList unkillables; // nurseries that contained indefinite roots, so cannot be deleted yet :(
	SnGCHeapList graveyard; // temporary list of dead heaps for use during collection
	
	uint16_t num_minor_collections_since_last_major_collection;
	byte* lower_bound;
	byte* upper_bound;
	
	struct {
		uint32_t survived;
		uint32_t freed;
		uint32_t moved;
		uint32_t indefinites;
		uint32_t total;
	} stats;
	
	struct {
		uintx allocated_size;
		uintx freed_size;
		uintx total_mem_usage;
	} info;
} GC;

static SnGCNursery* add_nursery() {
	SnGCNursery* nursery = (SnGCNursery*)snow_malloc(sizeof(SnGCNursery));
	gc_heap_init(&nursery->heap);
	pthread_setspecific(GC.nursery_key, nursery);
	nursery->previous = NULL;
	
	pthread_mutex_lock(&GC.nursery_lock);
	nursery->next = GC.nursery_head;
	if (nursery->next) {
		ASSERT(nursery->next->previous == NULL);
		nursery->next->previous = nursery;
	}
	GC.nursery_head = nursery;
	pthread_mutex_unlock(&GC.nursery_lock);
	return nursery;
}

static inline SnGCHeap* gc_my_nursery() {
	SnGCNursery* nursery = (SnGCNursery*)pthread_getspecific(GC.nursery_key);
	if (!nursery) {
		nursery = add_nursery();
	}
	return &nursery->heap;
}

static void gc_finalize_nursery(void* _nursery) {
	SnGCNursery* nursery = (SnGCNursery*)_nursery;
	pthread_mutex_lock(&GC.nursery_lock);
	gc_heap_list_push_heap(&GC.adults, &nursery->heap);
	if (nursery->previous) nursery->previous->next = nursery->next;
	if (nursery->next) nursery->next->previous = nursery->previous;
	if (GC.nursery_head == nursery) GC.nursery_head = nursery->next;
	pthread_mutex_unlock(&GC.nursery_lock);
	snow_free(nursery);
}

void snow_init_gc() {
	memset(&GC, 0, sizeof(GC));
	pthread_key_create(&GC.nursery_key, gc_finalize_nursery);
	int n = pthread_mutex_init(&GC.nursery_lock, NULL);
	ASSERT(n == 0);
	gc_heap_list_init(&GC.adults);
	gc_heap_list_init(&GC.biggies);
	gc_heap_list_init(&GC.unkillables);
}

static inline void gc_clear_statistics() {
	memset(&GC.stats, 0, sizeof(GC.stats));
}

static inline void gc_print_statistics(const char* phase) {
	debug("GC: %s collection statistics: %u survived, %u freed, %u moved, %u indefinites, of %u objects.\n", phase, GC.stats.survived, GC.stats.freed, GC.stats.moved, GC.stats.indefinites, GC.stats.total);
}

static inline void* gc_alloc_chunk(size_t size) {
	// allocate while updating bounds
	byte* ptr = (byte*)snow_malloc(size);
	if (ptr < GC.lower_bound || !GC.lower_bound)
		GC.lower_bound = ptr;
	if ((ptr + size) > GC.upper_bound)
		GC.upper_bound = ptr + size;
	GC.info.allocated_size += size;
	GC.info.total_mem_usage += size;
	return ptr;
}

static inline void gc_free_chunk(void* chunk, size_t size) {
	byte* ptr = (byte*)chunk;
	if (GC.lower_bound == ptr)
		GC.lower_bound = ptr + size; // guess a new lower bound
	if ((ptr + size) == GC.upper_bound)
		GC.upper_bound = ptr;
	GC.info.freed_size += size;
	GC.info.total_mem_usage -= size;
	snow_free(chunk);
}

static inline size_t gc_calculate_total_size(size_t size) {
	size_t extra_stuff = sizeof(SnGCObjectHead) + sizeof(SnGCObjectTail);
	return size + extra_stuff;
}

static inline byte* gc_init_allocation(byte* allocated_memory, size_t rounded_size, SnGCAllocType alloc_type, uint32_t object_index, SnGCFreeFunc free_func)
{
	byte* ptr = allocated_memory;
	
	SnGCObjectHead* head = (SnGCObjectHead*)ptr;
	ptr += sizeof(SnGCObjectHead);
	byte* data = ptr;
	ptr += rounded_size;
	SnGCObjectTail* tail = (SnGCObjectTail*)ptr;
	ptr += sizeof(SnGCObjectTail);
	
	head->bead = MAGIC_BEAD_HEAD;
	tail->bead = MAGIC_BEAD_TAIL;
	
	SnGCAllocInfo* alloc_info = &head->alloc_info;
	SnGCMetaInfo* meta_info = &tail->meta_info;
	
	// init alloc_info, and compute checksum
	alloc_info->size = rounded_size;
	alloc_info->object_index = object_index;
	alloc_info->alloc_type = alloc_type;
	gc_compute_checksum(alloc_info);
	
	// init meta_info
	meta_info->free_func = free_func;
	
	return data;
}

static inline void* gc_alloc(size_t size, SnGCAllocType alloc_type) {
	ASSERT(size); // 0-allocations not allowed.
	snow_gc_barrier();
	
	byte* ptr = NULL;
	uint32_t object_index = (uint32_t)-1;
	
	size_t rounded_size = snow_gc_round(size);
	size_t total_size = gc_calculate_total_size(rounded_size);
	
	if (total_size > GC_BIG_ALLOCATION_SIZE_LIMIT) {
		ptr = gc_heap_list_alloc(&GC.biggies, total_size, &object_index, total_size);
		ASSERT(ptr); // big allocation failed!
	}
	else
	{
		ptr = gc_heap_alloc(gc_my_nursery(), total_size, &object_index, GC_NURSERY_SIZE);
		if (!ptr) {
			snow_gc();
			ptr = gc_heap_alloc(gc_my_nursery(), total_size, &object_index, GC_NURSERY_SIZE);
			ASSERT(ptr); // garbage collection didn't free up enough space!
		}
	}
	
	byte* data = gc_init_allocation(ptr, rounded_size, alloc_type, object_index, NULL);

	++GC.stats.total;
	
	return data;
}

SnObjectBase* snow_gc_alloc_object(uintx size) {
	void* ptr = gc_alloc(size, GC_OBJECT);
	return (SnObjectBase*)ptr;
}

VALUE* snow_gc_alloc_blob(uintx size) {
	void* ptr = gc_alloc(size, GC_BLOB);
	return (VALUE*)ptr;
}

void* snow_gc_alloc_atomic(uintx size) {
	void* ptr = gc_alloc(size, GC_ATOMIC);
	return ptr;
}

void* snow_gc_realloc(void* ptr, uintx size) {
	SnGCHeap* heap = gc_find_heap(ptr);
	ASSERT(heap); // not GC-allocated memory!
	SnGCAllocInfo* alloc_info;
	SnGCMetaInfo* meta_info;
	void* ptr_start = gc_find_object_start(heap, (const byte*)ptr, &alloc_info, &meta_info);
	ASSERT(ptr_start == ptr); // ptr is not at the beginning of the allocation!
	if (size > alloc_info->size) {
		void* new_ptr = gc_alloc(size, alloc_info->alloc_type);
		if (meta_info->free_func)
			snow_gc_set_free_func(new_ptr, meta_info->free_func);
		ptr = new_ptr;
	}
	return ptr;
}

void snow_gc_set_free_func(const void* data, SnGCFreeFunc free_func) {
	SnGCHeap* heap = gc_find_heap(data);
	ASSERT(heap); // not a GC-allocated pointer
	SnGCAllocInfo* alloc_info;
	SnGCMetaInfo* meta_info;
	gc_find_object_start(heap, (const byte*)data, &alloc_info, &meta_info);
	meta_info->free_func = free_func;
}

uintx snow_gc_allocated_size(const void* data) {
	SnGCHeap* heap = gc_find_heap(data);
	ASSERT(heap); // not a GC-allocated pointer
	SnGCAllocInfo* alloc_info;
	SnGCMetaInfo* meta_info;
	gc_find_object_start(heap, (const byte*)data, &alloc_info, &meta_info);
	return alloc_info->size;
}

static inline void gc_finalize_object(void* object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta) {
	if (meta->free_func) {
		meta->free_func(object);
		meta->free_func = NULL;
	}
	memset(object, 0xef, alloc_info->size);
	alloc_info->alloc_type = GC_INVALID;
	gc_compute_checksum(alloc_info);
	++GC.stats.freed;
}

static inline bool gc_maybe_contains(const void* root) {
	return (const byte*)root >= GC.lower_bound && (const byte*)root <= GC.upper_bound;
}

SnGCHeap* gc_find_heap(const void* root) {
	if (!gc_maybe_contains(root)) return NULL;
	
	SnGCHeap* heap = NULL;
	pthread_mutex_lock(&GC.nursery_lock);
	for (SnGCNursery* nursery = GC.nursery_head; nursery != NULL; nursery = nursery->next) {
		if (gc_heap_contains(&nursery->heap, root)) {
			heap = &nursery->heap;
			break;
		}
	}
	pthread_mutex_unlock(&GC.nursery_lock);
	if (heap) return heap;
	
	heap = gc_heap_list_find_heap(&GC.adults, root);
	if (heap) return heap;
	heap = gc_heap_list_find_heap(&GC.biggies, root);
	if (heap) return heap;
	heap = gc_heap_list_find_heap(&GC.unkillables, root);
	if (heap) return heap;
	heap = gc_heap_list_find_heap(&GC.graveyard, root);
	return heap;
}

void snow_gc() {
	ASSERT(!_snow_gc_is_collecting);
	_snow_gc_is_collecting = true;
	snow_gc_barrier_enter();
	
	uintx mem_before = GC.info.total_mem_usage;
	
	// wait for other threads to reach barriers
	snow_set_gc_barriers();
	
	if (GC.num_minor_collections_since_last_major_collection > 10) {
		// TODO: Better heuristics for when to perform major collections
		gc_major();
		GC.num_minor_collections_since_last_major_collection = 0;
		gc_print_statistics("major");
	} else {
		gc_minor();
		++GC.num_minor_collections_since_last_major_collection;
		gc_print_statistics("minor");
	}
	
	gc_clear_flags();
	
	uintx mem_after = GC.info.total_mem_usage;
	double mem_diff_mb = ((double)mem_before - (double)mem_after) / (1024.0*1024.0);
	
	debug("GC: collection freed up %.2lf MiB (total GC memory usage: %.2lf MiB)\n", mem_diff_mb, ((double)GC.info.total_mem_usage)/(1024.0*1024.0));
	
	gc_clear_statistics();
	
	_snow_gc_is_collecting = false;
	snow_unset_gc_barriers();
	snow_gc_barrier_leave();
}

void gc_with_stack_do(byte* bottom, byte* top, SnGCAction action) {
	ASSERT((uintx)bottom % SNOW_GC_ALIGNMENT == 0);
	ASSERT((uintx)top % SNOW_GC_ALIGNMENT == 0);
	
	VALUE* p = (VALUE*)bottom;
	VALUE* end = (VALUE*)top;
	while (p < end) {
		if (gc_maybe_contains(*p))
			action(p, true);
		++p;
	}
}

void gc_with_definite_roots_do(SnGCAction action) {
	VALUE* store_ptr = (VALUE*)_snow_store_ptr();
	action(store_ptr, false); // _snow_store_ptr() returns an SnArray**
	
	VALUE* types = (VALUE*)snow_get_basic_types();
	for (size_t i = 0; i < SN_TYPE_MAX; ++i) {
		action(&types[i], false);
	}
}

static void do_task_stack(SnTask* task, void* userdata) {
	SnGCAction action;
	CAST_DATA_TO_FUNCTION(action, userdata);
	ASSERT(task->stack_top && task->stack_bottom); // a task is alive during GC?!?!
	gc_with_stack_do((byte*)task->stack_bottom, (byte*)task->stack_top, action);
}

void gc_with_everything_do(SnGCAction action) {
	snow_with_each_task_do(do_task_stack, action);
	
	gc_with_definite_roots_do(action);
}


#define MEMBER(TYPE, NAME) action((VALUE*)(data + offsetof(TYPE, NAME)), false)
#define MEMBER_ARRAY(TYPE, NAME) action((VALUE*)(data + offsetof(TYPE, NAME) + offsetof(struct array_t, data)), false)

void gc_with_object_do(VALUE object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta, SnGCAction action) {
	ASSERT(is_object(object));
	SnObjectBase* base = (SnObjectBase*)object;
	byte* data = (byte*)object;
	if (snow_is_normal_object(object)) {
		/*
			Mark up the regular objects, derived from SnObject.
		*/
		
		MEMBER(SnObject, prototype);
		MEMBER(SnObject, members);
		MEMBER_ARRAY(SnObject, property_names);
		MEMBER_ARRAY(SnObject, property_data);
		MEMBER_ARRAY(SnObject, included_modules);
		
		switch (base->type) {
			case SN_OBJECT_TYPE: break;
			case SN_CLASS_TYPE:
				MEMBER(SnClass, instance_prototype);
				break;
			case SN_FUNCTION_TYPE:
			{
				MEMBER(SnFunction, desc);
				MEMBER(SnFunction, declaration_context);
				SnFunction* func = (SnFunction*)base;
				for (size_t i = 0; i < func->num_variable_references; ++i) {
					action((VALUE*)&func->variable_references[i].context, false);
				}
				break;
			}
			case SN_EXCEPTION_TYPE:
				MEMBER(SnException, description);
				MEMBER(SnException, thrown_by);
				break;
			default:
				ASSERT(false); // wtf?
		}
	}
	else
	{
		/*
			Mark up the "thin" objects, derived only from SnObjectBase.
		*/
		
		switch (base->type) {
			case SN_CONTINUATION_TYPE:
				MEMBER(SnContinuation, return_to);
				MEMBER(SnContinuation, please_clean);
				MEMBER(SnContinuation, context);
				break;
			case SN_CONTEXT_TYPE:
				MEMBER(SnContext, static_parent);
				MEMBER(SnContext, function);
				MEMBER(SnContext, self);
				MEMBER(SnContext, local_names);
				MEMBER(SnContext, locals);
				MEMBER(SnContext, args);
				break;
			case SN_ARGUMENTS_TYPE:
				MEMBER_ARRAY(SnArguments, names);
				MEMBER_ARRAY(SnArguments, data);
				break;
			case SN_FUNCTION_DESCRIPTION_TYPE:
				MEMBER(SnFunctionDescription, defined_locals);
				MEMBER(SnFunctionDescription, argument_names);
				break;
			case SN_STRING_TYPE:
				MEMBER(SnString, data);
				break;
			case SN_ARRAY_TYPE:
				MEMBER_ARRAY(SnArray, a);
				break;
			case SN_MAP_TYPE:
				MEMBER(SnMap, data);
				break;
			case SN_CODEGEN_TYPE:
				MEMBER(SnCodegen, parent);
				MEMBER(SnCodegen, result);
				MEMBER_ARRAY(SnCodegen, variable_reference_names);
				MEMBER(SnCodegen, root);
				break;
			case SN_POINTER_TYPE:
				MEMBER(SnPointer, ptr); // TODO: Consider this.
				break;
			case SN_AST_TYPE:
			{
				SnAstNode* ast = (SnAstNode*)data;
				for (size_t i = 0; i < ast->size; ++i) {
					action(ast->children + i, false);
				}
				break;
			}
			default:
				ASSERT(false); // WTF?!
		}
	}
}

static inline void gc_clear_flags() {
	// only called during collection, no need to acquire locks
	for (SnGCNursery* nursery = GC.nursery_head; nursery != NULL; nursery = nursery->next) {
		gc_heap_clear_flags(&nursery->heap);
	}
	gc_heap_list_clear_flags(&GC.adults);
	gc_heap_list_clear_flags(&GC.biggies);
	gc_heap_list_clear_flags(&GC.unkillables);
}

static inline void gc_with_each_object_in_heap_do(SnGCHeap* heap, SnGCHeapAction action, void* userdata) {
	byte* p = heap->start;
	while (p < heap->current) {
		ASSERT(gc_looks_like_allocation(p));
		SnGCObjectHead* head = (SnGCObjectHead*)p;
		byte* object = p + sizeof(SnGCObjectHead);
		SnGCObjectTail* tail = (SnGCObjectTail*)(object + head->alloc_info.size);
		
		SnGCAllocInfo* alloc_info = &head->alloc_info;
		SnGCMetaInfo* meta_info = &tail->meta_info;
		
		action(heap, object, alloc_info, meta_info, userdata);
		
		p = ((byte*)tail) + sizeof(SnGCObjectTail);
	}
}

static inline void gc_invalidate_transplanted(SnGCHeap* heap, byte* object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta_info, void* userdata) {
	SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);
	if (flags & GC_TRANSPLANTED) {
		alloc_info->alloc_type = GC_INVALID;
		gc_compute_checksum(alloc_info);
	}
}

static inline void gc_transplant_or_finalize_object(SnGCHeap* heap, byte* object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta_info, void* userdata) {
	SnGCHeapList* transplant_to = (SnGCHeapList*)userdata;
	SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);
	if (flags & GC_MARK) {
		if (flags & GC_INDEFINITE) {
			// indefinitely referenced, can't transplant the object :(
		} else {
			gc_transplant(object, alloc_info, meta_info, transplant_to);
			gc_heap_set_flags(heap, alloc_info->object_index, GC_TRANSPLANTED);
			--heap->num_reachable;
		}
	} else {
		gc_finalize_object(object, alloc_info, meta_info);
	}
}

static inline void gc_sweep_heap(SnGCHeap* heap, SnGCHeapList* transplant_to) {
	gc_with_each_object_in_heap_do(heap, gc_transplant_or_finalize_object, transplant_to);
}

static inline void gc_sweep_nurseries() {
	// only called during collection, no need to acquire locks
	for (SnGCNursery* nursery = GC.nursery_head; nursery != NULL; nursery = nursery->next) {
		SnGCHeap* heap = &nursery->heap;
		if (heap->start == NULL) continue;

		gc_sweep_heap(heap, &GC.adults);
	}
}

static inline void gc_reset_or_save_nurseries() {
	// only called during collection, no need to acquire locks
	for (SnGCNursery* nursery = GC.nursery_head; nursery != NULL; nursery = nursery->next) {
		SnGCHeap* heap = &nursery->heap;
		
		if (heap->num_indefinite > 0) {
			gc_heap_list_push_heap(&GC.unkillables, heap);
			memset(heap, 0, sizeof(SnGCHeap));
		} else {
			gc_heap_finalize(heap);
		}
	}
}

void gc_minor() {
	gc_with_everything_do(gc_mark_root);

	gc_sweep_nurseries();

	gc_with_everything_do(gc_update_root);
	
	gc_reset_or_save_nurseries();
	
	// At this point, the "unkillable" heaps may still contain transplanted objects, so mark them as invalid pointers
	for (SnGCHeapListNode* node = GC.unkillables.head; node != NULL; node = node->next) {
		gc_with_each_object_in_heap_do(&node->heap, gc_invalidate_transplanted, NULL);
	}
}

void gc_major() {
	// TODO: This can be more efficient -- is it really necessary to run a full minor collection?
	
	gc_minor();
	gc_clear_flags();
	
	gc_with_everything_do(gc_mark_root);
	
	// now all objects are in the adult, unkillable, or big heaps, so deal with them with different strategies
	
	// compact the adult heaps by reallocating all reachable adult objects
	SnGCHeapList new_adults;
	SnGCHeapList new_unkillables;
	gc_heap_list_init(&new_adults);
	gc_heap_list_init(&new_unkillables);
	for (SnGCHeapListNode* node = GC.adults.head; node != NULL;) {
		SnGCHeap* heap = &node->heap;
		gc_sweep_heap(heap, &new_adults);
		if (heap->num_indefinite > 0) {
			// something was not put in new_adults
			gc_heap_list_push_heap(&new_unkillables, heap);
		} else {
			gc_heap_list_push_heap(&GC.graveyard, heap);
		}
		memset(heap, 0, sizeof(SnGCHeap));
		node = gc_heap_list_erase(&GC.adults, node);
	}
	ASSERT(GC.adults.head == NULL);
	gc_heap_list_clear(&GC.adults);
	memcpy(&GC.adults, &new_adults, sizeof(SnGCHeapList));
	
	// check unkillables for reachable objects -- don't touch them if there are reachable objects
	for (SnGCHeapListNode* node = GC.unkillables.head; node != NULL;) {
		SnGCHeap* heap = &node->heap;
		gc_sweep_heap(heap, &GC.adults);
		if (heap->num_indefinite > 0) {
			node = node->next;
		} else {
			// no indefinites, so no objects at all
			gc_heap_list_push_heap(&GC.graveyard, heap);
			memset(heap, 0, sizeof(SnGCHeap));
			node = gc_heap_list_erase(&GC.unkillables, node);
		}
	}
	// append the new unkillables from the adult sweep to the list of unkillables
	for (SnGCHeapListNode* node = new_unkillables.head; node != NULL;) {
		SnGCHeap* heap = &node->heap;
		gc_heap_list_push_heap(&GC.unkillables, heap);
		memset(heap, 0, sizeof(SnGCHeap));
		node = gc_heap_list_erase(&new_unkillables, node);
	}
	ASSERT(new_unkillables.head == NULL);
	gc_heap_list_clear(&new_unkillables);
	
	// TODO: Big objects
	
	gc_with_everything_do(gc_update_root);
	
	// pointers updates, let's scrap the graveyard
	for (SnGCHeapListNode* node = GC.graveyard.head; node != NULL;) {
		gc_heap_finalize(&node->heap);
		node = gc_heap_list_erase(&GC.graveyard, node);
	}
	ASSERT(GC.graveyard.head == NULL);
	gc_heap_list_clear(&GC.graveyard);
	
	// At this point, the "unkillable" heaps may still contain transplanted objects, so mark them as invalid pointers
	for (SnGCHeapListNode* node = GC.unkillables.head; node != NULL; node = node->next) {
		gc_with_each_object_in_heap_do(&node->heap, gc_invalidate_transplanted, NULL);
	}
}

static inline void gc_transplant(byte* object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta, SnGCHeapList* transplant_to) {
	size_t size = alloc_info->size;
	ASSERT(size % SNOW_GC_ALIGNMENT == 0);
	size_t total_size = gc_calculate_total_size(size);
	
	uint32_t object_index;
	byte* new_ptr = gc_heap_list_alloc(transplant_to, total_size, &object_index, GC_ADULT_SIZE);
	byte* new_object = gc_init_allocation(new_ptr, size, alloc_info->alloc_type, object_index, meta->free_func);
	memcpy(new_object, object, size);
	memset(object, 0xef, size);
	// place new pointer in the beginning of the old memory
	*(byte**)object = new_object;
	++GC.stats.moved;
}

void gc_mark_root(VALUE* root_p, bool on_stack) {
	SnGCHeap* heap = gc_find_heap(*root_p);
	if (heap) {
		SnGCAllocInfo* alloc_info;
		SnGCMetaInfo* meta;
		byte* object = gc_find_object_start(heap, (const byte*)*root_p, &alloc_info, &meta);
		
		if (alloc_info->alloc_type == GC_INVALID) return; // stale pointer
		
		SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);

		SnGCFlags new_flags = flags | GC_MARK;
		if (on_stack) {
			new_flags |= GC_INDEFINITE;
		}
		
		if (flags != new_flags) {
			gc_heap_set_flags(heap, alloc_info->object_index, new_flags);
			
			if (new_flags & GC_INDEFINITE) {
				++heap->num_indefinite;
				++GC.stats.indefinites;
			}
			if (new_flags & GC_MARK) {
				++heap->num_reachable;
				++GC.stats.survived;
			}
		}
		
		if (!(flags & GC_MARK)) {
			// was not already marked
			switch (alloc_info->alloc_type) {
				case GC_OBJECT:
				{
					gc_with_object_do(object, alloc_info, meta, gc_mark_root);
					break;
				}
				case GC_BLOB:
				{
					VALUE* blob = (VALUE*)object;
					for (size_t i = 0; i < alloc_info->size / sizeof(VALUE); ++i) {
						gc_mark_root(blob + i, false);
					}
					break;
				}
				case GC_ATOMIC:
				break; // do nothing; it's atomic.
			}
		}
	}
}

void gc_update_root(VALUE* root_p, bool on_stack) {
	SnGCHeap* heap = gc_find_heap(*root_p);
	if (heap) {
		SnGCAllocInfo* alloc_info;
		SnGCMetaInfo* meta;
		byte* object = gc_find_object_start(heap, (const byte*)*root_p, &alloc_info, &meta);
		
		ASSERT(on_stack || (alloc_info->alloc_type != GC_INVALID)); // stale pointer in non-stack memory?
		
		SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);
		
		if (flags & GC_TRANSPLANTED) {
			ASSERT(!on_stack); // an indefinite pointer was transplanted!
			ASSERT(!(flags & GC_INDEFINITE)); // an indefinite pointer was transplanted!
			size_t diff = ((byte*)*root_p) - object;
			object = *((VALUE*)object);
			*root_p = (VALUE)(object + diff);
			
			heap = gc_find_heap(*root_p);
			gc_find_object_start(heap, object, &alloc_info, &meta);
		}
		
		if (flags & GC_UPDATED) return;
		gc_heap_set_flags(heap, alloc_info->object_index, GC_UPDATED);
		
		switch (alloc_info->alloc_type) {
			case GC_OBJECT:
			{
				gc_with_object_do(object, alloc_info, meta, gc_update_root);
				break;
			}
			case GC_BLOB:
			{
				VALUE* blob = (VALUE*)object;
				for (size_t i = 0; i < alloc_info->size / sizeof(VALUE); ++i) {
					gc_update_root(blob + i, false);
				}
				break;
			}
			case GC_ATOMIC:
			break; // do nothing; it's atomic
		}
	}
}

bool gc_looks_like_allocation(const byte* ptr) {
	const SnGCObjectHead* head = (SnGCObjectHead*)ptr;
	const byte* data = ptr + sizeof(SnGCObjectHead);
	const byte* data_end = data + head->alloc_info.size;
	const SnGCObjectTail* tail = (SnGCObjectTail*)data_end;
	
	return head->bead == MAGIC_BEAD_HEAD
	    && gc_check_checksum(&head->alloc_info)
	    && gc_maybe_contains((const void*)data_end) // mostly necessary to avoid crashes on false positives, when checking for bead 2.
	    && tail->bead == MAGIC_BEAD_TAIL;
}

static inline byte* gc_find_object_start(const SnGCHeap* heap, const byte* data, SnGCAllocInfo** alloc_info_p, SnGCMetaInfo** meta_p) {
	const byte* ptr = data;
	ptr -= (uintx)ptr % SNOW_GC_ALIGNMENT;
	while (gc_heap_contains(heap, ptr) && !gc_looks_like_allocation(ptr))
	{
		ptr -= SNOW_GC_ALIGNMENT;
		ASSERT(gc_maybe_contains(ptr));
	}
	const byte* object_start = ptr + sizeof(SnGCObjectHead);
	*alloc_info_p = &((SnGCObjectHead*)ptr)->alloc_info;
	*meta_p = &((SnGCObjectTail*)(object_start + (*alloc_info_p)->size))->meta_info;
	return (byte*)object_start;
}


static inline void gc_compute_checksum(SnGCAllocInfo* alloc_info) {
	alloc_info->checksum = 0;
	alloc_info->checksum = snow_popcount64(*((uint64_t*)alloc_info));
}

static inline bool gc_check_checksum(const SnGCAllocInfo* alloc_info) {
	SnGCAllocInfo input = *alloc_info;
	input.checksum = 0;
	uint8_t checksum = snow_popcount64(*((uint64_t*)&input));
	return checksum == alloc_info->checksum;
}

#if DEBUG_MALLOC
#define HEAP_SIZE (1 << 27) // 128 mb
static byte heap[HEAP_SIZE];
static uintx heap_offset = 0;

struct alloc_header {
	uint16_t magic_bead1;
	void* ptr;
	uint32_t size;
	uint32_t freed;
	void(*alloc_rip)();
	void(*free_rip)();
	uint16_t magic_bead2;
};

#define MAGIC_BEAD 0xbead

static void verify_alloc_info(void* ptr, struct alloc_header* alloc_info)
{
	ASSERT((byte*)alloc_info == ((byte*)ptr - sizeof(struct alloc_header)));
	ASSERT(alloc_info->ptr == ptr);
	ASSERT(alloc_info->magic_bead1 == MAGIC_BEAD); // overrun of previous
	ASSERT(alloc_info->magic_bead2 == MAGIC_BEAD); // underrun
	ASSERT((alloc_info->freed && alloc_info->free_rip) || (!alloc_info->freed && !alloc_info->free_rip));
}

static void verify_heap()
{
	byte* data = (byte*)heap;
	uintx offset = 0;
	struct alloc_header* previous_alloc_info = NULL;
	while (offset < heap_offset)
	{
		struct alloc_header* alloc_info = (struct alloc_header*)(data + offset);
		offset += sizeof(struct alloc_header);
		void* ptr = data + offset;
		offset += alloc_info->size;
		verify_alloc_info(ptr, alloc_info);
		previous_alloc_info = alloc_info;
	}
	ASSERT(offset == heap_offset);
}
#endif // DEBUG_MALLOC

static inline uintx allocated_size_of(void* ptr) {
	#if DEBUG_MALLOC
	byte* data = (byte*)ptr;
	struct alloc_header* alloc_info = (struct alloc_header*)(data - sizeof(struct alloc_header));
	return alloc_info->size;
	#else
		#ifdef __APPLE__
	return malloc_size(ptr);
		#else
	return malloc_usable_size(ptr);
		#endif
	#endif
}

void* snow_malloc(uintx size)
{
	#if DEBUG_MALLOC
	if (size % 0x10)
		size += 0x10 - (size % 0x10); // align
	
	verify_heap();
	ASSERT(heap_offset < HEAP_SIZE); // oom
	struct alloc_header* alloc_info = (struct alloc_header*)((byte*)heap + heap_offset);
	heap_offset += sizeof(struct alloc_header);
	void* ptr = (byte*)heap + heap_offset;
	heap_offset += size;
	
	alloc_info->magic_bead1 = MAGIC_BEAD;
	alloc_info->ptr = ptr;
	alloc_info->size = size;
	alloc_info->freed = false;
	void(*rip)();
	GET_RETURN_PTR(rip);
	alloc_info->alloc_rip = rip;
	alloc_info->free_rip = NULL;
	alloc_info->magic_bead2 = MAGIC_BEAD;
	verify_heap();
	#else
	void* ptr = malloc(size);
	#endif
	
	#ifdef DEBUG
	memset(ptr, 0xcd, size);
	#endif
	
	return ptr;
}

void* snow_malloc_aligned(uintx size, uintx alignment)
{
	void* ptr = NULL;
	
	#if defined(_POSIX_ADVISORY_INFO) && _POSIX_ADVISORY_INFO > 0
	int r = posix_memalign(&ptr, alignment, size);
	ASSERT(r != EINVAL); // alignment not a power of 2, or not a multiple of sizeof(void*)
	ASSERT(r == 0); // other error (out of memory?)
	#else
	ASSERT(alignment <= 4096); // alignment must be less than 4K on systems without posix_memalign
	ASSERT((alignment & (alignment - 1)) == 0); // alignment is not a power of 2
	ptr = valloc(size);
	ASSERT(ptr); // memory allocation failed
	#endif
	
	return ptr;
}

void* snow_calloc(uintx count, uintx size)
{
	return snow_malloc(count*size);
}

void* snow_realloc(void* ptr, uintx new_size)
{
	uintx old_size = ptr ? allocated_size_of(ptr) : 0;
	if (old_size < new_size)
	{
		void* new_ptr = snow_malloc(new_size);
		memcpy(new_ptr, ptr, old_size);
		snow_free(ptr);
		ptr = new_ptr;
	}
	return ptr;
}

void snow_free(void* ptr)
{
	if (ptr)
	{
		#if DEBUG_MALLOC
		ASSERT((byte*)ptr > (byte*)heap && (byte*)ptr < ((byte*)heap + heap_offset));
		verify_heap();
		
		struct alloc_header* alloc_info = (struct alloc_header*)((byte*)ptr - sizeof(struct alloc_header));
		verify_alloc_info(ptr, alloc_info);
		alloc_info->freed = true;
		void(*rip)();
		GET_RETURN_PTR(rip);
		alloc_info->free_rip = rip;
		#endif
		
		uintx size = allocated_size_of(ptr);
		memset(ptr, 0xef, size);
		
		#if DEBUG_MALLOC
		verify_heap();
		#else
		free(ptr);
		#endif
	}
}
