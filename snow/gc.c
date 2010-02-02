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

// internal functions
typedef void(*SnGCAction)(VALUE* root_pointer, bool on_stack);
static void gc_mark_root(VALUE* root, bool on_stack);
static void gc_update_root(VALUE* root, bool on_stack);

struct SnGCObjectHead;
struct SnGCObjectTail;
struct SnGCAllocInfo;
struct SnGCMetaInfo;
struct SnGCHeap;

static void gc_minor();
static void gc_major();
static byte* gc_find_object_start(const struct SnGCHeap* heap, const byte* ptr, struct SnGCAllocInfo**, struct SnGCMetaInfo**);
static struct SnGCHeap* gc_find_heap(const void* root);
static bool gc_heap_contains(const struct SnGCHeap* heap, const void* root);
static void gc_mark_stack(byte* bottom, byte* top);
static void gc_compute_checksum(struct SnGCAllocInfo* alloc_info);
static bool gc_check_checksum(const struct SnGCAllocInfo* alloc_info);
static bool gc_looks_like_allocation(const byte* ptr);
static void gc_transplant_adult(byte* object, struct SnGCAllocInfo* alloc_info, struct SnGCMetaInfo* meta_info);
static void gc_finalize_object(void* object, struct SnGCAllocInfo* alloc_info, struct SnGCMetaInfo* meta_info);

typedef enum SnGCFlag {
	GC_NO_FLAGS      = 0,
	GC_MARK          = 1,         // the object is referenced by reachable roots
	GC_TRANSPLANTED  = 1 << 1,    // the object is no longer here
	GC_UPDATED       = 1 << 2,    // the object has had its roots updated
	GC_INDEFINITE    = 1 << 3,    // the object is referenced from the stack or another indefinite chunk of memory
	GC_FLAGS_MAX
} SnGCFlag;

typedef enum SnGCAllocType {
	GC_OBJECT = 0x1,
	GC_BLOB   = 0x2,
	GC_ATOMIC = 0x3,
	GC_ALLOC_TYPE_MAX
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

#define GC_NURSERY_SIZE 0x20000 // 128K nurseries
#define GC_ADULT_SIZE 0x100000 // 1M adult heaps
#define GC_BIG_ALLOCATION_SIZE_LIMIT 0x1000 // everything above 4K will go in the "biggies" allocation list

struct {
	SnGCHeap nurseries[SNOW_MAX_CONCURRENT_TASKS];
	
	SnGCHeapList adults;
	SnGCHeapList biggies;
	SnGCHeapList unkillables; // nurseries that contained indefinite roots, so cannot be deleted yet :(
	
	uint16_t num_minor_collections_since_last_major_collection;
	byte* lower_bound;
	byte* upper_bound;
} GC;

#define MY_NURSERY (&GC.nurseries[snow_get_current_thread_index()])

void snow_init_gc() {
	memset(&GC, 0, sizeof(GC));
	gc_heap_list_init(&GC.adults);
	gc_heap_list_init(&GC.biggies);
	gc_heap_list_init(&GC.unkillables);
}

static inline void* gc_alloc_chunk(size_t size) {
	// allocate while updating bounds
	byte* ptr = (byte*)snow_malloc(size);
	if (ptr < GC.lower_bound || !GC.lower_bound)
		GC.lower_bound = ptr;
	if ((ptr + size) > GC.upper_bound)
		GC.upper_bound = ptr + size;
	return ptr;
}

static inline void gc_free_chunk(void* chunk, size_t size) {
	byte* ptr = (byte*)chunk;
	if (GC.lower_bound == ptr)
		GC.lower_bound = ptr + size; // guess a new lower bound
	if ((ptr + size) == GC.upper_bound)
		GC.upper_bound = ptr;
	snow_free(chunk);
}

static inline size_t gc_calculate_total_size(size_t size) {
	size_t extra_stuff = sizeof(SnGCObjectHead) + sizeof(SnGCObjectTail);
	return size + extra_stuff;
}

static inline byte* gc_init_allocation(byte* allocated_memory, size_t rounded_size, SnGCAllocType alloc_type, uint32_t object_index)
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
	memset(meta_info, 0, sizeof(SnGCMetaInfo));
	
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
		ptr = gc_heap_alloc(MY_NURSERY, total_size, &object_index, GC_NURSERY_SIZE);
		if (!ptr) {
			snow_gc();
			ptr = gc_heap_alloc(MY_NURSERY, total_size, &object_index, GC_NURSERY_SIZE);
			ASSERT(ptr); // garbage collection didn't free up enough space!
		}
	}
	
	byte* data = gc_init_allocation(ptr, rounded_size, alloc_type, object_index);
	
	return data;
}

SnObjectBase* snow_gc_alloc_object(uintx size) {
	void* ptr = gc_alloc(size, GC_OBJECT);
	return (SnObjectBase*)ptr;
}

VALUE* snow_gc_alloc_blob(uintx size) {
	ASSERT(size % sizeof(VALUE) == 0); // blob allocation doesn't match size of roots
	void* ptr = gc_alloc(size, GC_BLOB);
	return (VALUE*)ptr;
}

void* snow_gc_alloc_atomic(uintx size) {
	void* ptr = gc_alloc(size, GC_ATOMIC);
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


static inline void gc_transplant_adult(byte* object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta) {
	size_t size = alloc_info->size;
	ASSERT(size % SNOW_GC_ALIGNMENT == 0);
	size_t total_size = gc_calculate_total_size(size);
	
	uint32_t object_index;
	byte* new_ptr = gc_heap_list_alloc(&GC.adults, total_size, &object_index, GC_ADULT_SIZE);
	byte* new_object = gc_init_allocation(new_ptr, size, alloc_info->alloc_type, object_index);
	memcpy(new_object, object, size);
	snow_gc_set_free_func(new_object, meta->free_func);
	// place new pointer in the beginning of the old memory
	*(byte**)object = new_object;
}

static inline void gc_finalize_object(void* object, SnGCAllocInfo* alloc_info, SnGCMetaInfo* meta) {
	if (meta->free_func)
		meta->free_func(object);
}

static inline bool gc_maybe_contains(const void* root) {
	return (const byte*)root >= GC.lower_bound && (const byte*)root <= GC.upper_bound;
}

SnGCHeap* gc_find_heap(const void* root) {
	if (!gc_maybe_contains(root)) return NULL;
	
	for (uint32_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		if (gc_heap_contains(GC.nurseries + i, root)) return GC.nurseries + i;
	}
	
	SnGCHeap* heap = NULL;
	heap = gc_heap_list_find_heap(&GC.adults, root);
	if (heap) return heap;
	heap = gc_heap_list_find_heap(&GC.biggies, root);
	if (heap) return heap;
	heap = gc_heap_list_find_heap(&GC.unkillables, root);
	return heap;
}

bool _snow_gc_task_at_barrier() {
	return snow_thread_is_at_gc_barrier(snow_get_current_thread_index());
}

void _snow_gc_task_set_barrier() {
	snow_thread_set_gc_barrier(snow_get_current_thread_index());
}

void _snow_gc_task_unset_barrier() {
	snow_thread_unset_gc_barrier(snow_get_current_thread_index());
}

void snow_gc() {
	_snow_gc_is_collecting = true;
	snow_gc_barrier_enter();
	void* rsp;
	GET_STACK_PTR(rsp);
	snow_thread_departing_from_system_stack(rsp); // register extents for ourselves
	
	// wait for other threads to reach barriers
	while (!snow_all_threads_at_gc_barrier()) { snow_task_yield(); }
	
	if (GC.num_minor_collections_since_last_major_collection > 10) {
		gc_major();
		GC.num_minor_collections_since_last_major_collection = 0;
	} else {
		gc_minor();
		++GC.num_minor_collections_since_last_major_collection;
	}
	
	
	snow_thread_returning_to_system_stack();
	_snow_gc_is_collecting = false;
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
	action((VALUE*)_snow_store_ptr(), false); // _snow_store_ptr() returns an SnArray**
	
	VALUE* types = (VALUE*)snow_get_basic_types();
	for (size_t i = 0; i < SN_TYPE_MAX; ++i) {
		action(&types[i], false);
	}
}

void gc_with_everything_do(SnGCAction action) {
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		void* stack_top;
		void* stack_bottom;
		snow_get_thread_stack_extents(i, &stack_top, &stack_bottom);
		ASSERT((stack_top && stack_bottom) || (!stack_top && !stack_bottom)); // stack extent calculation mismatch!
		if (stack_top && stack_bottom) {
			gc_with_stack_do((byte*)stack_bottom, (byte*)stack_top, action);
		}
	}
	
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
				MEMBER(SnFunction, desc);
				MEMBER(SnFunction, declaration_context);
				break;
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
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		gc_heap_clear_flags(GC.nurseries + i);
	}
	gc_heap_list_clear_flags(&GC.adults);
	gc_heap_list_clear_flags(&GC.biggies);
	gc_heap_list_clear_flags(&GC.unkillables);
}

static inline void gc_clear_heap(SnGCHeap* heap) {
	// first, call free funcs where necessary
	byte* p = heap->start;
	while (p < heap->current) {
		SnGCObjectHead* head = (SnGCObjectHead*)p;
		byte* object = p + sizeof(SnGCObjectHead);
		SnGCObjectTail* tail = (SnGCObjectTail*)(object + head->alloc_info.size);
		
		SnGCAllocInfo* alloc_info = &head->alloc_info;
		SnGCMetaInfo* meta_info = &tail->meta_info;
		
		SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);
		if (!(flags & GC_TRANSPLANTED)) {
			ASSERT(!(flags & GC_MARK)); // there were marked objects within the heap, and we're still trying to clear it. Clearly wrong.
			gc_finalize_object(object, alloc_info, meta_info);
		}
	}
	
	#ifdef DEBUG
	memset(heap->start, 0xcd, heap->end - heap->start);
	#endif
	
	heap->current = heap->start;
	heap->num_objects = 0;
	gc_heap_clear_flags(heap);
}

void gc_minor() {
	ASSERT(snow_all_threads_at_gc_barrier());
	
	gc_with_everything_do(gc_mark_root);
	
	size_t num_indefinites[SNOW_MAX_CONCURRENT_TASKS];
	memset(num_indefinites, 0, SNOW_MAX_CONCURRENT_TASKS*sizeof(size_t));
	
	/*
		Move or delete objects!
	*/
	uintx total_indefinites = 0;
	uintx total_survivors = 0;
	uintx total_objects = 0;
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		if (GC.nurseries[i].start == NULL) continue;
		
		uint32_t survivors = 0;
		
		SnGCHeap* heap = GC.nurseries + i;
		byte* p = heap->start;
		while (p < heap->current) {
			ASSERT(gc_looks_like_allocation(p)); // heap corruption?
			++total_objects;
			
			SnGCObjectHead* head = (SnGCObjectHead*)p;
			byte* object = p + sizeof(SnGCObjectHead);
			SnGCObjectTail* tail = (SnGCObjectTail*)(object + head->alloc_info.size);
			
			SnGCAllocInfo* alloc_info = &head->alloc_info;
			SnGCMetaInfo* meta_info = &tail->meta_info;
			
			SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);
			if (flags & GC_MARK) {
				++survivors;
				if (flags & GC_INDEFINITE) {
					// indefinitely referenced, can't transplant the object :(
					num_indefinites[i]++;
				} else {
					gc_transplant_adult(object, alloc_info, meta_info);
					gc_heap_set_flags(heap, alloc_info->object_index, GC_TRANSPLANTED);
				}
			}
			
			p = ((byte*)tail) + sizeof(SnGCObjectTail);
		}
		
		if (survivors == 0) {
			gc_clear_heap(heap);
		}
		
		total_survivors += survivors;
	}
	
	gc_with_everything_do(gc_update_root);
	
	debug("GC: Minor collection completed. Survivors: %lu (Indefinites: %lu), of %lu\n", total_survivors, total_indefinites, total_objects);
	
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		SnGCHeap* heap = GC.nurseries + i;
		
		if (num_indefinites[i] > 0) {
			gc_heap_list_push_heap(&GC.unkillables, heap);
			memset(heap, 0, sizeof(SnGCHeap));
		} else {
			gc_clear_heap(heap);
		}
	}
	
	gc_clear_flags();
}

static inline void gc_purge_heap_list(SnGCHeapList* list) {
	/*
		Delete all objects in heaps in the list that aren't GC_MARKed.
	*/
	
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		SnGCHeap* heap = &node->heap;
		uint32_t marked_objects = 0;
		
		byte* p = heap->start;
		while (p < heap->current) {
			ASSERT(gc_looks_like_allocation(p)); // heap corruption?
			SnGCObjectHead* head = (SnGCObjectHead*)p;
			byte* object = p + sizeof(SnGCObjectHead);
			SnGCObjectTail* tail = (SnGCObjectTail*)(object + head->alloc_info.size);
			
			SnGCFlags flags = gc_heap_get_flags(heap, head->alloc_info.object_index);
			if (flags & GC_MARK) {
				++marked_objects;
			}
			
			p = ((byte*)tail) + sizeof(SnGCObjectTail);
		}
		
		if (marked_objects == 0) {
			debug("heap %p has no more live objects, deleting...\n", heap);
			gc_clear_heap(heap);
			node = gc_heap_list_erase(list, node);
		} else {
			node = node->next;
		}
	}
}

void gc_major() {
	ASSERT(snow_all_threads_at_gc_barrier());
	
	gc_minor();
	
	// go through all heaps, count number of live objects, and remove empty heaps
	
	/*gc_purge_heap_list(&GC.adults); // TODO: Compact adults
	gc_purge_heap_list(&GC.biggies);
	gc_purge_heap_list(&GC.unkillables);*/
}

void gc_mark_root(VALUE* root_p, bool on_stack) {
	SnGCHeap* heap = gc_find_heap(*root_p);
	if (heap) {
		SnGCAllocInfo* alloc_info;
		SnGCMetaInfo* meta;
		byte* object = gc_find_object_start(heap, (const byte*)*root_p, &alloc_info, &meta);
		
		SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);

		SnGCFlags new_flags = flags | GC_MARK;
		if (on_stack) new_flags |= GC_INDEFINITE;
		gc_heap_set_flags(heap, alloc_info->object_index, new_flags);
		
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
		
		SnGCFlags flags = gc_heap_get_flags(heap, alloc_info->object_index);
		
		if (flags & GC_UPDATED) return;
		
		if (flags & GC_TRANSPLANTED) {
			ASSERT(!on_stack); // an indefinite pointer was transplanted!
			size_t diff = ((byte*)*root_p) - object;
			object = *((VALUE*)object);
			*root_p = (VALUE)(object + diff);
			
			heap = gc_find_heap(*root_p);
			gc_find_object_start(heap, object, &alloc_info, &meta);
		}
		
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
#define HEAP_SIZE (1 << 25) // 32 mb
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
