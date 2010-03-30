#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"
#include "snow/continuation.h"
#include "snow/task-intern.h"
#include "snow/objects.h"

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

struct SnGCHeap;
struct SnGCHeapList;

// internal functions
static struct SnGCHeap* gc_find_heap(const SnAnyObject* root);
static bool gc_heap_contains(const struct SnGCHeap* heap, const void* root);
static void gc_mark_root(VALUE);
static void gc_mark_any_object(SnAnyObject* object);
static void gc_mark_object(struct SnObject*);
static void gc_mark_definite_roots();
extern void snow_finalize_object(SnAnyObject* object);
static void gc_clear_flags();
static void* gc_worker_thread(void*);

typedef enum SnGCFlag {
	GC_NO_FLAGS      = 0,
	GC_MARK          = 1,         // the object is referenced by reachable roots
	GC_UNALLOCATED   = 2,
	GC_FLAGS_MAX
} SnGCFlag;

#define GC_HEAP_SIZE 0x100000 // 1 MiB heaps
#define GC_DEFAULT_COLLECTION_THRESHOLD 0x4000000 // 64 MiB
typedef byte SnGCFlags;
#include "snow/gcheap.h"

typedef struct SnGCNursery {
	SnGCHeap* heap;
	volatile bool mark_request;
	volatile bool mark_done;
	struct SnGCNursery* next;
	struct SnGCNursery* previous;
} SnGCNursery;

struct {
	pthread_mutex_t gc_lock;
	
	pthread_key_t nursery_key;
	SnGCNursery* nursery_head;

	pthread_mutex_t heaps_lock;
	SnGCHeapList full_heaps;
	SnGCHeapList empty_heaps;
	SnGCHeapList sweeping;
	
	byte* lower_bound;
	byte* upper_bound;
	
	pthread_t worker_thread;
	
	struct {
		uint32_t survived;
		uint32_t freed;
		uint32_t total;
	} stats;
	
	struct {
		uintx freed_size;
		uintx total_mem_usage;
		
		uintx collection_threshold;
		uintx freed_by_last_collection;
	} info;
} GC;

static SnGCNursery* add_nursery() {
	SnGCNursery* nursery = (SnGCNursery*)snow_malloc(sizeof(SnGCNursery));
	nursery->heap = gc_heap_create(sizeof(SnAnyObject));
	pthread_setspecific(GC.nursery_key, nursery);
	nursery->mark_request = false;
	nursery->mark_done = false;
	nursery->previous = NULL;
	
	pthread_mutex_lock(&GC.gc_lock);
	nursery->next = GC.nursery_head;
	if (nursery->next) {
		ASSERT(nursery->next->previous == NULL);
		nursery->next->previous = nursery;
	}
	GC.nursery_head = nursery;
	pthread_mutex_unlock(&GC.gc_lock);
	return nursery;
}

static inline void gc_nursery_reboot() {
	SnGCNursery* nursery = (SnGCNursery*)pthread_getspecific(GC.nursery_key);
	pthread_mutex_lock(&GC.heaps_lock);
	gc_heap_list_push_heap(&GC.full_heaps, nursery->heap);
	GC.info.total_mem_usage += GC_HEAP_SIZE;
	SnGCHeap* new_heap_candidate = gc_heap_list_pop(&GC.empty_heaps);
	pthread_mutex_unlock(&GC.heaps_lock);
	if (new_heap_candidate != NULL)
		nursery->heap = new_heap_candidate;
	else
		nursery->heap = gc_heap_create(sizeof(SnAnyObject));
}

static inline SnGCHeap* gc_my_nursery() {
	SnGCNursery* nursery = (SnGCNursery*)pthread_getspecific(GC.nursery_key);
	if (!nursery) {
		nursery = add_nursery();
	}
	return nursery->heap;
}

static void gc_finalize_nursery(void* _nursery) {
	SnGCNursery* nursery = (SnGCNursery*)_nursery;
	
	pthread_mutex_lock(&GC.heaps_lock);
	// put all heaps in the adult heap list, then clear out this nursery
	gc_heap_list_push_heap(&GC.full_heaps, nursery->heap);
	pthread_mutex_unlock(&GC.heaps_lock);
	
	pthread_mutex_lock(&GC.gc_lock);
	if (nursery->previous) nursery->previous->next = nursery->next;
	if (nursery->next) nursery->next->previous = nursery->previous;
	if (GC.nursery_head == nursery) GC.nursery_head = nursery->next;
	pthread_mutex_unlock(&GC.gc_lock);
	snow_free(nursery); // nursery->heap ownership has been transferred to GC.adults
}


void snow_gc_barrier() {
	SnGCNursery* nursery = (SnGCNursery*)pthread_getspecific(GC.nursery_key);
	if (nursery && nursery->mark_request && !nursery->mark_done) {
		SnExecutionState state;
		snow_save_execution_state(&state);
		
		snow_task_pause();
		SnTask* task = snow_get_current_task();
		while (task) {
			VALUE* p = (VALUE*)task->stack_bottom;
			VALUE* end = (VALUE*)task->stack_top;
			while (p < end) {
				gc_mark_root(*p);
				++p;
			}
			task = task->previous;
		}
		
		nursery->mark_done = true;
		snow_task_resume();
	}
}

void snow_init_gc() {
	memset(&GC, 0, sizeof(GC));
	pthread_mutex_init(&GC.gc_lock, NULL);
	pthread_key_create(&GC.nursery_key, gc_finalize_nursery);
	pthread_mutex_init(&GC.heaps_lock, NULL);
	gc_heap_list_init(&GC.full_heaps, sizeof(SnAnyObject));
	gc_heap_list_init(&GC.empty_heaps, sizeof(SnAnyObject));
	gc_heap_list_init(&GC.sweeping, sizeof(SnAnyObject));
	pthread_create(&GC.worker_thread, NULL, gc_worker_thread, NULL);
	GC.info.collection_threshold = GC_DEFAULT_COLLECTION_THRESHOLD;
}

static inline void gc_sweep_heap(SnGCHeap* heap) {
	SnAnyObject* objects = (SnAnyObject*)heap->start;
	size_t num_objects = (SnAnyObject*)heap->current - objects;

	heap->num_reachable = 0;
	
	// run through the freelist and mark unallocated objects, so they won't be re-added to the freelist
	SnAnyObject* f = (SnAnyObject*)heap->free_element;
	while (f) {
		gc_heap_set_flags(heap, f, GC_UNALLOCATED);
		f = *(SnAnyObject**)f;
	}

	// run through all objects in the heap and free the unreachable, allocated ones
	for (size_t i = 0; i < num_objects; ++i) {
		SnAnyObject* object = objects + i;
		byte flags = gc_heap_get_flags(heap, object);
		if (!(flags & GC_MARK) && !(flags & GC_UNALLOCATED)) {
			// unreachable and not already in the free list
			snow_finalize_object(object);
			gc_heap_free_element(heap, object);
		}
		else
		{
			++heap->num_reachable;
		}
	}
}

static void* gc_worker_thread(void* _unused_) {
	while (true) {
		usleep(100);
		
		if (GC.info.total_mem_usage > GC.info.collection_threshold) {
			gc_clear_flags();
			
			// Collect heaps that should be swept
			pthread_mutex_lock(&GC.heaps_lock);
			size_t memory_usage_before_collection = GC.info.total_mem_usage;
			
			for (SnGCHeapListNode* n = GC.full_heaps.head; n != NULL;) {
				gc_heap_list_push_heap(&GC.sweeping, n->heap);
				n->heap = NULL;
				n = gc_heap_list_erase(&GC.full_heaps, n);
			}
			pthread_mutex_unlock(&GC.heaps_lock);
			
			gc_mark_definite_roots();
			
			// Signal threads that they should be marked
			bool all_marked = false;
			while (!all_marked) {
				all_marked = true;
				pthread_mutex_lock(&GC.gc_lock);
				for (SnGCNursery* nursery = GC.nursery_head; nursery != NULL; nursery = nursery->next) {
					nursery->mark_request = true;
					if (!nursery->mark_done) all_marked = false;
				}
				pthread_mutex_unlock(&GC.gc_lock);
				usleep(1);
			}
			
			// Sweep the collected heaps
			for (SnGCHeapListNode* n = GC.sweeping.head; n != NULL;) {
				SnGCHeap* heap = n->heap;
				gc_sweep_heap(heap);
				
				
				if (heap->num_reachable == 0) {
					// nothing reachable in the heap, let's get rid of it
					n->heap = NULL;
					gc_heap_destroy(heap);
					GC.info.total_mem_usage -= GC_HEAP_SIZE;
					n = gc_heap_list_erase(&GC.sweeping, n);
				} else {
					n = n->next;
				}
			}
			
			// Done sweeping, put the swept heaps back in the game
			pthread_mutex_lock(&GC.heaps_lock);
			for (SnGCHeapListNode* n = GC.sweeping.head; n != NULL;) {
				SnGCHeap* heap = n->heap;
				size_t max_num_objects = (SnAnyObject*)heap->end - (SnAnyObject*)heap->start;
				if (heap->num_reachable < max_num_objects * 2 / 3) {
					gc_heap_list_push_heap(&GC.empty_heaps, heap);
				} else {
					gc_heap_list_push_heap(&GC.full_heaps, heap);
				}
				n->heap = NULL;
				n = gc_heap_list_erase(&GC.sweeping, n);
			}
			
			ASSERT(GC.sweeping.head == NULL); // some rubbish left in the sweep list?!
			
			size_t memory_usage_after_collection = GC.info.total_mem_usage;
			if (memory_usage_after_collection > GC.info.collection_threshold) {
				GC.info.collection_threshold = memory_usage_after_collection * 3 / 2;
			}
			pthread_mutex_unlock(&GC.heaps_lock);
			
			// Reset thread markings
			pthread_mutex_lock(&GC.gc_lock);
			for (SnGCNursery* nursery = GC.nursery_head; nursery != NULL; nursery = nursery->next) {
				nursery->mark_request = false;
				nursery->mark_done = false;
			}
			pthread_mutex_unlock(&GC.gc_lock);
		}
	}
	return NULL;
}

static inline void gc_clear_statistics() {
	memset(&GC.stats, 0, sizeof(GC.stats));
}

static inline void gc_print_statistics(const char* phase) {
	debug("GC: %s collection statistics: %u survived, %u freed, of %u objects.\n", phase, GC.stats.survived, GC.stats.freed, GC.stats.total);
}

static inline void* gc_alloc(SnValueType type) {
	snow_gc_barrier();
	
	SnAnyObject* ptr = NULL;
	
	DTRACE_PROBE(GC_ALLOC(type));
	
	ptr = (SnAnyObject*)gc_heap_alloc_element(gc_my_nursery());
	if (!ptr) {
		gc_nursery_reboot();
		ptr = (SnAnyObject*)gc_heap_alloc_element(gc_my_nursery());
		ASSERT(ptr); // couldn't make new heap for the nursery
	}
	ptr->type = type;
	++GC.stats.total;
	
	return ptr;
}

SnAnyObject* snow_gc_alloc_object(SnValueType type) {
	void* ptr = gc_alloc(type);
	return (SnAnyObject*)ptr;
}

struct SnGCRootArrayHeader {
	// TODO: Lock mechanism
	uintx dummy;
} SnGCRootArrayHeader;

VALUE* snow_alloc_gc_root_array(uintx n) {
	// TODO: header
	return snow_malloc(n*sizeof(VALUE));
}

VALUE* snow_realloc_gc_root_array(VALUE* ptr, uintx n) {
	// TODO: header
	return snow_realloc(ptr, n*sizeof(VALUE));
}

void snow_free_gc_root_array(VALUE* ptr) {
	snow_free(ptr);
}

static inline bool gc_maybe_contains(const void* root) {
	return (const byte*)root >= GC.lower_bound && (const byte*)root <= GC.upper_bound;
}

static inline SnGCHeap* gc_find_heap(const SnAnyObject* _root) {
	uintx root = (uintx)_root;
	uintx p = root;
	p -= p & (GC_HEAP_SIZE-1);
	return (root - p > sizeof(SnGCHeap)) ? (SnGCHeap*)p : NULL;
}

void gc_mark_definite_roots() {
	VALUE* store_ptr = (VALUE*)_snow_store_ptr();
	gc_mark_root(*store_ptr); // _snow_store_ptr() returns an SnArray**
}

static inline void gc_clear_flags() {
	pthread_mutex_lock(&GC.heaps_lock);
	gc_heap_list_clear_flags(&GC.full_heaps);
	gc_heap_list_clear_flags(&GC.empty_heaps);
	pthread_mutex_unlock(&GC.heaps_lock);
}

void gc_mark_root(VALUE root) {
	SnGCHeap* heap = gc_find_heap(root);
	if (heap && (((uintx)root - (uintx)heap) % sizeof(SnAnyObject) == 0) && gc_heap_list_contains(&GC.sweeping, heap)) {
		SnGCFlags flags = gc_heap_get_flags(heap, root);

		SnGCFlags new_flags = flags | GC_MARK;
		
		if (flags != new_flags) {
			gc_heap_set_flags(heap, root, new_flags);
			// was not already marked
			++heap->num_reachable;
			++GC.stats.survived;
			gc_mark_any_object(root);
		}
	}
}

static void gc_mark_any_object(SnAnyObject* object) {
	// special cases (Array, Map need locks)
	switch (object->type) {
		case SnArrayType: {
			SnArray* array = SNOW_CAST_OBJECT(object, SnArray);
			snow_rwlock_read(array->lock);
			// TODO: Maybe copy the array into local storage so the lock can be released quicker?
			for (size_t i = 0; i < array->size; ++i) {
				gc_mark_root(array->data[i]);
			}
			snow_rwlock_unlock(array->lock);
			return;
		}
		case SnMapType: {
			SnMap* map = SNOW_CAST_OBJECT(object, SnMap);
			snow_rwlock_read(map->lock);
			for (size_t i = 0; i < map->size*2; ++i) {
				gc_mark_root(map->data[i]);
			}
			snow_rwlock_unlock(map->lock);
			return;
		}
		case SnObjectType: {
			gc_mark_object(SNOW_CAST_OBJECT(object, SnObject));
			return;
		}
		default: break;
	}
	
	// all other objects
	#define SN_BEGIN_THIN_CLASS(NAME)  case NAME ## Type: { NAME* casted = SNOW_CAST_OBJECT(object, NAME);
	#define SN_BEGIN_CLASS(NAME)       case NAME ## Type: { NAME* casted = SNOW_CAST_OBJECT(object, NAME); gc_mark_object(SNOW_CAST_OBJECT(object, SnObject));
	#define SN_MEMBER_ROOT(TYPE, NAME)     gc_mark_root(casted->NAME);
	#define SN_END_CLASS()             break; }
	
	switch (object->type) {
		#include "objects-decl.h"
		default:
		{
			ASSERT(false); // ERROR! Unknown or illegal object type!
			return;
		}
	}
}

static void gc_mark_object(SnObject* object) {
	gc_mark_root(object->prototype);
	gc_mark_root(object->members);
	gc_mark_root(object->property_names);
	gc_mark_root(object->property_data);
	gc_mark_root(object->included_modules);
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
	
	//#if defined(_POSIX_ADVISORY_INFO) && _POSIX_ADVISORY_INFO > 0
	int r = posix_memalign(&ptr, alignment, size);
	ASSERT(r != EINVAL); // alignment not a power of 2, or not a multiple of sizeof(void*)
	ASSERT(r == 0); // other error (out of memory?)
	//#else
	//ASSERT(alignment <= 4096); // alignment must be less than 4K on systems without posix_memalign
	//ASSERT((alignment & (alignment - 1)) == 0); // alignment is not a power of 2
	//ptr = valloc(size);
	//ASSERT(ptr); // memory allocation failed
	//#endif
	
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

void* snow_malloc_gc_root(uintx size) {
	void* ptr = snow_malloc(size);
	snow_gc_add_root(ptr, size);
	return ptr;
}

void* snow_calloc_gc_root(uintx count, uintx size) {
	void* ptr = snow_calloc(count, size);
	snow_gc_add_root(ptr, count*size);
	return ptr;
}

void* snow_realloc_gc_root(void* ptr, uintx new_size) {
	snow_gc_remove_root(ptr);
	void* new_ptr = snow_realloc(ptr, new_size);
	snow_gc_add_root(new_ptr, new_size);
	return new_ptr;
}

void snow_free_gc_root(void* ptr) {
	snow_gc_remove_root(ptr);
	snow_free(ptr);
}

inline void snow_gc_add_root(void* ptr, uintx size) {
	// TODO: !!
}

inline void snow_gc_remove_root(void* ptr) {
	// TODO: !!
}
