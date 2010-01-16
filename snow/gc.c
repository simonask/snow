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

#define DEFAULT_HEAP_SIZE (1<<20)

volatile bool _snow_gc_is_collecting = false;

HIDDEN SnArray** _snow_store_ptr(); // necessary for accessing global stuff

// internal functions
typedef void(*SnGCAction)(VALUE* root_pointer, bool on_stack);
static void gc_mark_root(VALUE* root, bool on_stack);
static void gc_update_root(VALUE* root, bool on_stack);

struct SnGCHeap;
struct SnGCHeader;
struct SnGCMetaInfo;

static void gc_minor();
static void gc_major();
static byte* gc_find_object_start(byte* ptr, struct SnGCHeader**, struct SnGCMetaInfo**);
static struct SnGCHeap* gc_find_heap(const VALUE root);
static bool gc_heap_contains(const struct SnGCHeap* heap, const VALUE root);
static void gc_mark_stack(byte* bottom, byte* top);
static void gc_compute_checksum(struct SnGCHeader* header);
static bool gc_check_checksum(const struct SnGCHeader* header);
static bool gc_looks_like_allocation(byte* ptr);
static void gc_transplant_adult(byte* object, struct SnGCHeader* header, struct SnGCMetaInfo* meta);
static void gc_finalize_object(byte* object, struct SnGCHeader* header, struct SnGCMetaInfo* meta);

#define GC_NURSERY_SIZE 0x20000 // 128K nurseries
#define GC_BIG_ALLOCATION_SIZE_BOUNDARY 0x1000 // 4K

typedef enum SnGCAllocType {
	GC_OBJECT = 0x1,
	GC_BLOB   = 0x2,
	GC_ATOMIC = 0x3,
	GC_ALLOC_TYPE_MAX
} SnGCAllocType;

typedef enum SnGCFlag {
	GC_NO_FLAGS      = 0,
	GC_MARK          = 1,
	GC_TRANSPLANTED  = 1 << 1,    // the object is no longer here
	GC_UPDATED       = 1 << 2,    // the object has had its roots updated
	GC_INDEFINITE    = 1 << 3,
	GC_FLAGS_MAX
} SnGCFlag;

typedef byte SnGCFlags;

typedef struct SnGCHeader {
	// Header info must never change throughout the lifetime of an object.
	// Only flag contents may change.
	// sizeof(SnGCHeader) must be == 8 bytes.
	unsigned size        : 32;
	unsigned flag_index  : 24;
	unsigned alloc_type  : 2;
	unsigned checksum    : 6; // sum of bits set in all the other fields
} PACKED SnGCHeader;

typedef struct SnGCMetaInfo {
	// sizeof(SnGCMetaInfo) must be == 8 bytes.
	SnGCFreeFunc free_func;
	#ifdef ARCH_IS_32_BIT
	byte padding[4];
	#endif
} PACKED SnGCMetaInfo;


typedef struct SnGCHeap {
	/*
		An incremental heap, with separate flags for better CoW-performance during GC.
	*/
	byte* start;
	byte* current;
	byte* end;
	
	SnGCFlags* flags;
	uint32_t num_objects;
} SnGCHeap;

#define FLAGS_PER_PAGE (SNOW_PAGE_SIZE / sizeof(SnGCFlags))

#define MAGIC_BEAD_1 0xbe4db3ad
#define MAGIC_BEAD_2 0xb3adbe4d

typedef uint64_t SnGCMagicBead;

struct {
	SnGCHeap nurseries[SNOW_MAX_CONCURRENT_TASKS];
	
	SnGCHeap* adult_heaps;
	uint32_t num_adult_heaps;
	
	SnGCHeap* big_allocations;
	uint32_t num_big_allocations;
	
	SnGCHeap* old_nurseries; // nurseries that contained indefinite roots, so cannot be deleted :(
	uint32_t num_old_nurseries;
	
	uint16_t num_minor_collections_since_last_major_collection;
	byte* lower_bound;
	byte* upper_bound;
} GC;

#define MY_NURSERY (&GC.nurseries[snow_get_current_thread_index()])

void snow_init_gc() {
	memset(&GC, 0, sizeof(GC));
}

void* gc_alloc_chunk(size_t size) {
	// allocate while updating bounds
	byte* ptr = (byte*)snow_malloc(size);
	if (ptr < GC.lower_bound || !GC.lower_bound)
		GC.lower_bound = ptr;
	if ((ptr + size) > GC.upper_bound)
		GC.upper_bound = ptr + size;
	return ptr;
}

static inline SnGCFlags gc_heap_get_flags(SnGCHeap* heap, uint32_t flag_index) {
	ASSERT(flag_index < heap->num_objects);
	
	if (heap->flags) {
		return heap->flags[flag_index];
	}
	
	return 0;
}

static inline void gc_heap_set_flags(SnGCHeap* heap, uint32_t flag_index, byte flags) {
	ASSERT(flag_index < heap->num_objects);
	
	if (!heap->flags) {
		heap->flags = snow_malloc(sizeof(SnGCFlags)*heap->num_objects);
		memset(heap->flags, 0, sizeof(SnGCFlags)*heap->num_objects);
	}
	
	heap->flags[flag_index] |= flags;
}

static inline void gc_heap_unset_flags(SnGCHeap* heap, uint32_t flag_index, byte flags) {
	ASSERT(flag_index < heap->num_objects);
	
	if (heap->flags) {
		heap->flags[flag_index] &= ~flags;
	}
}

static inline size_t gc_heap_available(SnGCHeap* heap) {
	return heap->end - heap->start;
}

static inline size_t gc_calculate_total_size(size_t size) {
	// returns the size + headers + metadata + alignment
	size_t extra_stuff = sizeof(SnGCHeader) + sizeof(SnGCMetaInfo) + 2 * sizeof(SnGCMagicBead);
	return size + extra_stuff;
}

static inline void* gc_heap_alloc(SnGCHeap* heap, size_t size, SnGCAllocType alloc_type) {
	size_t rounded_size = snow_gc_round(size);
	size_t total_size = gc_calculate_total_size(rounded_size);
	
	// XXX: Lock
	
	if (!heap->start) {
		heap->start = gc_alloc_chunk(GC_NURSERY_SIZE);
		heap->current = heap->start;
		heap->end = heap->start + GC_NURSERY_SIZE;
	}
	
	// see if we can hold the allocation
	if (heap->current + total_size >= heap->end) {
		return NULL;
	}
	
	byte* ptr = heap->current;
	heap->current += total_size;
	byte* object_end = heap->current;
	size_t flag_index = heap->num_objects++;
	// XXX: Unlock
	
	SnGCHeader* header = (SnGCHeader*)ptr;
	ptr += sizeof(SnGCHeader);
	SnGCMagicBead* bead1 = (SnGCMagicBead*)ptr;
	ptr += sizeof(SnGCMagicBead);
	byte* data = ptr;
	ptr += rounded_size;
	SnGCMetaInfo* metainfo = (SnGCMetaInfo*)ptr;
	memset(metainfo, 0, sizeof(SnGCMetaInfo));
	ptr += sizeof(SnGCMetaInfo);
	SnGCMagicBead* bead2 = (SnGCMagicBead*)ptr;
	ptr += sizeof(SnGCMagicBead);
	ASSERT(ptr == object_end);
	
	// set header info, and compute checksum
	header->size = rounded_size;
	header->flag_index = flag_index;
	header->alloc_type = alloc_type;
	gc_compute_checksum(header);
	
	// set beads
	*bead1 = MAGIC_BEAD_1;
	*bead2 = MAGIC_BEAD_2;
	
	memset(metainfo, 0, sizeof(SnGCMetaInfo));
	
	#ifdef DEBUG
	ASSERT(gc_looks_like_allocation((byte*)header));
	SnGCHeader* check_header;
	SnGCMetaInfo* check_meta;
	byte* start = gc_find_object_start(data, &check_header, &check_meta);
	ASSERT(start == data);
	ASSERT(check_header == header);
	ASSERT(check_meta == metainfo);
	#endif
	
	return data;
}

static inline void* gc_alloc_from_heap_list(size_t size, SnGCAllocType alloc_type, SnGCHeap** heaps_p, uint32_t* num_heaps_p) {
	size_t total_size = gc_calculate_total_size(snow_gc_round(size));
	
	SnGCHeap* heap = NULL;
	for (uint32_t i = 0; i < *num_heaps_p; ++i) {
		if (gc_heap_available(*heaps_p + i) >= total_size) heap = *heaps_p + i;
	}
	
	if (!heap) {
		*heaps_p = (SnGCHeap*)snow_realloc(*heaps_p, sizeof(SnGCHeap) * (*num_heaps_p + 1));
		SnGCHeap* heaps = *heaps_p;
		// swap first and last heap
		memcpy(&heaps[*num_heaps_p], &heaps[0], sizeof(SnGCHeap));
		memset(&heaps[0], 0, sizeof(SnGCHeap));
		(*num_heaps_p)++;
		heap = &heaps[0];
	}
	
	if (!heap->start && size > GC_NURSERY_SIZE) {
		heap->start = gc_alloc_chunk(total_size);
		heap->current = heap->start;
		heap->end = heap->start + total_size;
	}
	
	void* ptr = gc_heap_alloc(heap, size, alloc_type);
	ASSERT(ptr); // alloc_from_heap_list must never fail :(
	return ptr;
}

static inline void gc_transplant_adult(byte* object, SnGCHeader* old_header, SnGCMetaInfo* old_meta) {
	byte* new_ptr = (byte*)gc_alloc_from_heap_list(old_header->size, old_header->alloc_type, &GC.adult_heaps, &GC.num_adult_heaps);
	ASSERT(new_ptr);
	SnGCMetaInfo* meta = (SnGCMetaInfo*)(new_ptr + old_header->size);
	memcpy(new_ptr, object, old_header->size);
	memcpy(meta, old_meta, sizeof(SnGCMetaInfo));
	// place new pointer in the beginning of the old memory
	*(byte**)object = new_ptr;
}

static inline void gc_finalize_object(byte* object, SnGCHeader* header, SnGCMetaInfo* meta) {
	if (meta->free_func)
		meta->free_func(object);
}

static inline void* gc_alloc(size_t size, SnGCAllocType alloc_type) {
	ASSERT(size); // 0-allocations not allowed.
	snow_gc_barrier();
	void* ptr = NULL;
	
	if (size > GC_BIG_ALLOCATION_SIZE_BOUNDARY) {
		ptr = gc_alloc_from_heap_list(size, alloc_type, &GC.big_allocations, &GC.num_big_allocations);
		ASSERT(ptr); // big allocation failed!
	}
	else
	{
		ptr = gc_heap_alloc(MY_NURSERY, size, alloc_type);
		if (!ptr) {
			snow_gc();
			ptr = gc_heap_alloc(MY_NURSERY, size, alloc_type);
			ASSERT(ptr); // garbage collection didn't free up enough space!
		}
	}
	
	return ptr;
}

SnObjectBase* snow_gc_alloc_object(uintx size) {
	SnGCHeader* header;
	void* ptr = gc_alloc(size, GC_OBJECT);
	return (SnObjectBase*)ptr;
}

VALUE* snow_gc_alloc_blob(uintx size) {
	ASSERT(size % sizeof(VALUE) == 0); // blob allocation doesn't match size of roots
	void* ptr = gc_alloc(size, GC_BLOB);
	return (VALUE*)ptr;
}

void* snow_gc_alloc_atomic(uintx size) {
	SnGCHeader* header;
	void* ptr = gc_alloc(size, GC_ATOMIC);
	return ptr;
}

void snow_gc_set_free_func(const VALUE data, SnGCFreeFunc free_func) {
	#if DEBUG
	SnGCHeap* heap = gc_find_heap(data);
	ASSERT(heap); // not a GC-allocated pointer
	#endif
	SnGCHeader* header;
	SnGCMetaInfo* meta;
	byte* object = gc_find_object_start((byte*)data, &header, &meta);
	CAST_FUNCTION_TO_DATA(*meta, free_func);
}

uintx snow_gc_allocated_size(void* data) {
	#if DEBUG
	SnGCHeap* heap = gc_find_heap(data);
	ASSERT(heap); // not a GC-allocated pointer
	#endif
	SnGCHeader* header;
	SnGCMetaInfo* meta;
	byte* object = gc_find_object_start((byte*)data, &header, &meta);
	return header->size;
}

static inline bool gc_maybe_contains(const VALUE root) {
	return (const byte*)root >= GC.lower_bound && (const byte*)root <= GC.upper_bound;
}

SnGCHeap* gc_find_heap(const VALUE root) {
	if (!gc_maybe_contains(root)) return NULL;
	
	for (uint32_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		if (gc_heap_contains(GC.nurseries + i, root)) return GC.nurseries + i;
	}
	for (uint32_t i = 0; i < GC.num_adult_heaps; ++i) {
		if (gc_heap_contains(GC.adult_heaps + i, root)) return GC.adult_heaps + i;
	}
	for (uint32_t i = 0; i < GC.num_big_allocations; ++i) {
		if (gc_heap_contains(GC.big_allocations + i, root)) return GC.big_allocations + i;
	}
	for (uint32_t i = 0; i < GC.num_old_nurseries; ++i) {
		if (gc_heap_contains(GC.old_nurseries + i, root)) return GC.old_nurseries + i;
	}
	// not in any heap! :(
	return NULL;
}

static inline bool gc_heap_contains(const SnGCHeap* heap, const VALUE root) {
	const byte* data = (const byte*)root;
	return data >= heap->start && data < heap->end;
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

void gc_with_object_do(VALUE object, SnGCHeader* header, SnGCMetaInfo* meta, SnGCAction action) {
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

void gc_minor() {
	ASSERT(snow_all_threads_at_gc_barrier());
	
	gc_with_everything_do(gc_mark_root);
	
	size_t num_indefinites[SNOW_MAX_CONCURRENT_TASKS];
	memset(num_indefinites, 0, SNOW_MAX_CONCURRENT_TASKS*sizeof(size_t));
	
	uintx total_indefinites = 0;
	uintx total_survivors = 0;
	uintx total_objects = 0;
	
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		if (GC.nurseries[i].start == NULL) continue;
		
		SnGCHeap* heap = GC.nurseries + i;
		byte* p = heap->start;
		while (p < heap->current) {
			ASSERT(gc_looks_like_allocation(p));
			++total_objects;
			
			SnGCHeader* header = (SnGCHeader*)p;
			byte* object = p + sizeof(SnGCHeader*) + sizeof(SnGCMagicBead);
			SnGCMetaInfo* meta = (SnGCMetaInfo*)(object + header->size);
			
			SnGCFlags flags = gc_heap_get_flags(heap, header->flag_index);
			if (flags & GC_MARK) {
				++total_survivors;
				if (flags & GC_INDEFINITE) {
					// indefinitely referenced, can't transplant the object :(
					++num_indefinites[i];
				} else {
					gc_transplant_adult(object, header, meta);
					gc_heap_set_flags(heap, header->flag_index, GC_TRANSPLANTED);
				}
			} else {
				gc_finalize_object(object, header, meta);
			}
			
			p = object + header->size + sizeof(SnGCMetaInfo) + sizeof(SnGCMagicBead);
		}
	}
	
	gc_with_everything_do(gc_update_root);
	
	
	
	// clean up the nurseries
	for (size_t i = 0; i < SNOW_MAX_CONCURRENT_TASKS; ++i) {
		SnGCHeap* heap = GC.nurseries + i;
		if (num_indefinites[i] == 0) {
			// no indefinites, so just reset the current pointer
			snow_free(heap->flags);
			heap->current = heap->start;
			heap->num_objects = 0;
		} else {
			total_indefinites += num_indefinites[i];
			// heap was referenced by indefinite pointers, so copy it and reset the nursery
			GC.old_nurseries = (SnGCHeap*)snow_realloc(GC.old_nurseries, sizeof(SnGCHeap)*(GC.num_old_nurseries+1));
			memcpy(GC.old_nurseries + GC.num_old_nurseries, heap, sizeof(SnGCHeap));
			++GC.num_old_nurseries;
			
			// nulling the heap will cause a reinitialization the next time someone allocates from it
			memset(heap, 0, sizeof(SnGCHeap));
		}
	}
	
	debug("GC: Minor collection completed. Survivors: %lu (Indefinites: %lu), of %lu\n", total_survivors, total_indefinites, total_objects);
}

void gc_major() {
	ASSERT(snow_all_threads_at_gc_barrier());
	
	gc_minor();
	
/*	gc_with_everything_do(gc_mark_root);
	
	gc_with_everything_do(gc_update_root);*/
}

void gc_mark_root(VALUE* root_p, bool on_stack) {
	SnGCHeap* heap = gc_find_heap(*root_p);
	if (heap) {
		SnGCHeader* header;
		SnGCMetaInfo* meta;
		byte* object = gc_find_object_start(*root_p, &header, &meta);
		
		SnGCFlags flags = gc_heap_get_flags(heap, header->flag_index);

		flags |= GC_MARK;
		if (on_stack) flags |= GC_INDEFINITE;
		gc_heap_set_flags(heap, header->flag_index, flags);
		
		if (!(flags & GC_MARK)) {
			switch (header->alloc_type) {
				case GC_OBJECT:
				{
					gc_with_object_do(object, header, meta, gc_mark_root);
					break;
				}
				case GC_BLOB:
				{
					VALUE* blob = (VALUE*)object;
					for (size_t i = 0; i < header->size / sizeof(VALUE); ++i) {
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
		SnGCHeader* header;
		SnGCMetaInfo* meta;
		byte* object = gc_find_object_start(*root_p, &header, &meta);
		
		SnGCFlags flags = gc_heap_get_flags(heap, header->flag_index);
		
		if (flags & GC_UPDATED) return;
		
		if (flags & GC_TRANSPLANTED) {
			ASSERT(!on_stack); // an indefinite pointer was transplanted!
			size_t diff = ((byte*)*root_p) - object;
			object = *((VALUE*)object);
			*root_p = (VALUE)(object + diff);
			
			heap = gc_find_heap(*root_p);
			gc_find_object_start(object, &header, &meta);
		}
		
		gc_heap_set_flags(heap, header->flag_index, GC_UPDATED);
		
		switch (header->alloc_type) {
			case GC_OBJECT:
			{
				gc_with_object_do(object, header, meta, gc_update_root);
				break;
			}
			case GC_BLOB:
			{
				VALUE* blob = (VALUE*)object;
				for (size_t i = 0; i < header->size / sizeof(VALUE); ++i) {
					gc_update_root(blob + i, false);
				}
				break;
			}
			case GC_ATOMIC:
			break; // do nothing; it's atomic
		}
	}
}

bool gc_looks_like_allocation(byte* ptr) {
	const SnGCHeader* header = (SnGCHeader*)ptr;
	const SnGCMagicBead* bead1 = (SnGCMagicBead*)(ptr + sizeof(SnGCHeader));
	const byte* data = ((byte*)bead1) + sizeof(SnGCMagicBead);
	const byte* data_end = data + header->size;
	const SnGCMetaInfo* meta = (SnGCMetaInfo*)(data + header->size);
	const SnGCMagicBead* bead2 = (SnGCMagicBead*)((byte*)meta + sizeof(SnGCMetaInfo));
	return *bead1 == MAGIC_BEAD_1
	    && gc_check_checksum(header)
	    && gc_maybe_contains((const VALUE)data_end) // mostly necessary to avoid crashes on false positives, when checking for bead 2.
	    && *bead2 == MAGIC_BEAD_2;
}

static inline byte* gc_find_object_start(byte* data, SnGCHeader** header_p, SnGCMetaInfo** meta_p) {
	byte* ptr = data;
	ptr -= (uintx)ptr % SNOW_GC_ALIGNMENT;
	ptr -= SNOW_GC_ALIGNMENT; // start 16 bytes before
	while (!gc_looks_like_allocation(ptr))
	{
		ptr -= SNOW_GC_ALIGNMENT;
		ASSERT(gc_maybe_contains(ptr));
	}
	byte* object_start = ptr + sizeof(SnGCHeader) + sizeof(SnGCMagicBead);
	*header_p = (SnGCHeader*)ptr;
	*meta_p = (SnGCMetaInfo*)(object_start + (*header_p)->size);
	return object_start;
}


static inline void gc_compute_checksum(SnGCHeader* header) {
	header->checksum = 0;
	header->checksum = snow_popcount64(*((uint64_t*)header));
}

static inline bool gc_check_checksum(const SnGCHeader* header) {
	SnGCHeader input = *header;
	input.checksum = 0;
	uint8_t checksum = snow_popcount64(*((uint64_t*)&input));
	return checksum == header->checksum;
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

static void verify_header(void* ptr, struct alloc_header* header)
{
	ASSERT(header == ((byte*)ptr - sizeof(struct alloc_header)));
	ASSERT(header->ptr == ptr);
	ASSERT(header->magic_bead1 == MAGIC_BEAD); // overrun of previous
	ASSERT(header->magic_bead2 == MAGIC_BEAD); // underrun
	ASSERT((header->freed && header->free_rip) || (!header->freed && !header->free_rip));
}

static void verify_heap()
{
	byte* data = (byte*)heap;
	uintx offset = 0;
	struct alloc_header* previous_header = NULL;
	while (offset < heap_offset)
	{
		struct alloc_header* header = (struct alloc_header*)(data + offset);
		offset += sizeof(struct alloc_header);
		void* ptr = data + offset;
		offset += header->size;
		verify_header(ptr, header);
		previous_header = header;
	}
	ASSERT(offset == heap_offset);
}
#endif // DEBUG_MALLOC

static inline uintx allocated_size_of(void* ptr) {
	#if DEBUG_MALLOC
	byte* data = (byte*)ptr;
	struct alloc_header* header = (struct alloc_header*)(data - sizeof(struct alloc_header));
	return header->size;
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
	struct alloc_header* header = (struct alloc_header*)((byte*)heap + heap_offset);
	heap_offset += sizeof(struct alloc_header);
	void* ptr = (byte*)heap + heap_offset;
	heap_offset += size;
	
	header->magic_bead1 = MAGIC_BEAD;
	header->ptr = ptr;
	header->size = size;
	header->freed = false;
	void(*rip)();
	GET_RETURN_PTR(rip);
	header->alloc_rip = rip;
	header->free_rip = NULL;
	header->magic_bead2 = MAGIC_BEAD;
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
	uintx old_size = allocated_size_of(ptr);
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
		
		struct alloc_header* header = ((byte*)ptr - sizeof(struct alloc_header));
		verify_header(ptr, header);
		header->freed = true;
		void(*rip)();
		GET_RETURN_PTR(rip);
		header->free_rip = rip;
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
