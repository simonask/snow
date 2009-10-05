#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"
#include "snow/continuation.h"
#include <pthread.h>
#include <stdlib.h>
#include <malloc/malloc.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

#define DEBUG_MALLOC 0 // set to 1 to override snow_malloc

// necessary for accessing global stuff
HIDDEN SnArray** _snow_store_ptr();

static void* gc_stack_top = NULL;
static bool gc_collecting = false;

struct heap_t {
	byte* data;
	uintx offset;
	uintx size;
	pthread_mutex_t mutex;
};

static struct heap_t gc_nursery = {.data = NULL, .offset = 0, .size = 0};

static SnContinuation gc_cc;

typedef enum GCFlag {
	GC_FLAG_MARK = 1,
	GC_FLAG_BLOB = 1 << 1,
	GC_FLAG_TRANSPLANTED = 1 << 2,
	GC_FLAG_MAX = 1 << 3
} GCFlag;

typedef enum GCOperation {
	GC_OP_MARK,
	GC_OP_UPDATE,
} GCOperation;

typedef struct __attribute__((packed)) GCHeader {
	unsigned magic_bead1 : 16;
	unsigned flags       : 16;
	
	SnGCFreeFunc free_func; // : 64 or 32
	unsigned size        : 16;
	
	#if ARCH_IS_32_BIT
	// compensate for the fact that SnGCFreeFunc is only 4 bytes
	byte padding[4];
	#endif
	
	unsigned magic_bead2 : 16;
} GCHeader;

static const int ALIGNMENT = 0x10;

/*
	The magic beads are used to distinguish pointers to the beginning of objects
	from pointers to the middle of objects, or more precisely, to find the actual
	header of any pointer.
*/
static const uint16_t MAGIC_BEAD = 0xbead;

static bool gc_contains(void*);
static void* gc_alloc(uintx size, GCHeader**) __attribute__((alloc_size(1)));
static VALUE gc_intern(SnContext*);
static inline void gc_scan_memory(byte* mem_start, size_t mem_size, GCOperation op);

static inline uintx gc_aligned_size(uintx size)
{
	return size + ((ALIGNMENT - (size % ALIGNMENT)) % ALIGNMENT);
}

static void gc_heap_init(struct heap_t* heap, uintx size)
{
	heap->size = gc_aligned_size(size);
	heap->data = (byte*)snow_malloc(heap->size);
	debug("gc: nursery is at %p\n", heap->data);
	heap->offset = 0;
	pthread_mutex_init(&heap->mutex, NULL);
	#ifdef DEBUG
	memset(heap->data, 0xcd, heap->size);
	#endif
}

static void* gc_heap_alloc(struct heap_t* heap, uintx size, GCHeader** header)
{
	pthread_mutex_lock(&heap->mutex);
	ASSERT(heap->data && heap->size);
	ASSERT(size < heap->size);
	uintx padded_size = gc_aligned_size(size);
	uintx new_offset = heap->offset + sizeof(GCHeader) + padded_size;
	ASSERT((new_offset % ALIGNMENT) == 0);
	void* data = NULL;
	if (new_offset > heap->size)
		goto out; // alloc failed, initiate gc
		
	*header = (GCHeader*)(heap->data + heap->offset);
	data = ((byte*)*header) + sizeof(GCHeader);
	heap->offset = new_offset;
	
	(*header)->size = padded_size;
	(*header)->flags = 0;
	(*header)->free_func = NULL;
	(*header)->magic_bead1 = MAGIC_BEAD;
	(*header)->magic_bead2 = MAGIC_BEAD;
	
	ASSERT(((uintx)data % ALIGNMENT) == 0);
	
	out:
	pthread_mutex_unlock(&heap->mutex);
	return data;
}

static void* gc_alloc(uintx size, GCHeader** header)
{
	ASSERT(!gc_collecting);
	ASSERT(size > 0);
	ASSERT(sizeof(GCHeader) == ALIGNMENT);
	
	if (!gc_nursery.data)
	{
		gc_heap_init(&gc_nursery, 1 << 23);
	}
	
	void* data = gc_heap_alloc(&gc_nursery, size, header);
	
	if (!data)
	{
		snow_gc();
		return gc_alloc(size, header);
	}
	
	return data;
}

SnObjectBase* snow_gc_alloc_object(uintx size)
{
	GCHeader* header;
	SnObjectBase* obj = (SnObjectBase*)gc_alloc(size, &header);
	return obj;
}

void* snow_gc_alloc(uintx size)
{
	GCHeader* header;
	void* data = gc_alloc(size, &header);
	header->flags |= GC_FLAG_BLOB;
	return data;
}

void snow_gc_set_free_func(void* data, SnGCFreeFunc func)
{
	ASSERT(gc_contains(data));
	ASSERT(((uintx)data & 0xf) == 0);
	GCHeader* header = (GCHeader*)((byte*)data - sizeof(GCHeader));
	header->free_func = func;
}

static inline bool gc_ptr_in(const void* ptr, const void* start, const void* end)
{
	return (byte*)ptr >= (byte*)start && (byte*)ptr < (byte*)end;
}

static bool gc_heap_contains(const struct heap_t* heap, void* ptr)
{
	return gc_ptr_in(ptr, heap->data + sizeof(GCHeader), heap->data + heap->size);
}

static bool gc_contains(void* ptr)
{
	return gc_heap_contains(&gc_nursery, ptr);
}

static inline bool gc_ptr_valid(void* ptr)
{
	// note: does not check if the pointer is actually in any heap
	GCHeader* header = (GCHeader*)(((byte*)ptr) - sizeof(GCHeader));
	if ((uintx)header % ALIGNMENT) return false;
	return header->magic_bead1 == MAGIC_BEAD
		&& header->magic_bead2 == MAGIC_BEAD
		&& header->flags < GC_FLAG_MAX;
}

static inline void* gc_find_object_in_heap(const struct heap_t* heap, void* start_ptr, GCHeader** header)
{
	if (!gc_heap_contains(heap, start_ptr)) return NULL;
	uintx nptr = (uintx)start_ptr;
	nptr = nptr - (nptr % ALIGNMENT);
	void* ptr = (void*)nptr;
	
	while (!gc_ptr_valid(ptr))
	{
		nptr -= ALIGNMENT;
		ptr = (void*)nptr;
		
		#ifdef DEBUG
		if (!gc_heap_contains(heap, ptr))
			TRAP(); // massive heap corruption!
		#endif
	}
	
	*header = (GCHeader*)(nptr - sizeof(GCHeader));
	return ptr;
}

static inline void* gc_find_object(void* start_ptr, GCHeader** header)
{
	return gc_find_object_in_heap(&gc_nursery, start_ptr, header);
}

void snow_gc()
{
	gc_collecting = true;
	
	snow_continuation_init(&gc_cc, gc_intern, NULL);
	gc_cc.base.type = SN_CONTINUATION_TYPE;
	uintx stack_size = 1 << 20; // 1Mb stack!
	gc_cc.stack_lo = (byte*)snow_malloc(stack_size);
	gc_cc.stack_hi = gc_cc.stack_lo + stack_size;
	snow_continuation_call(&gc_cc, snow_get_current_continuation());
	snow_free(gc_cc.stack_lo);
	
	gc_collecting = false;
}

static inline void gc_unmark_heap(const struct heap_t* heap)
{
	byte* end = heap->data + heap->offset;
	byte* i = heap->data;
	while (i < end)
	{
		GCHeader* header = (GCHeader*)i;
		header->flags &= ~GC_FLAG_MARK;
		i += sizeof(GCHeader) + header->size;
	}
}

static void gc_scan_continuation(SnContinuation* cc, GCOperation op)
{
	gc_scan_memory((byte*)cc, sizeof(SnContinuation), op);
	
	byte* stack_lo;
	byte* stack_hi;
	snow_continuation_get_stack_bounds(cc, &stack_lo, &stack_hi);
	ASSERT(stack_hi > stack_lo);
	if (cc->return_to) // if return_to is NULL, cc wasn't called from anywhere, so it's the initial continuation
		gc_scan_memory(stack_lo, stack_hi - stack_lo, op);
}

static inline void gc_scan_memory(byte* mem_start, size_t mem_size, GCOperation op)
{
	VALUE* words_start = (VALUE*)mem_start;
	VALUE* words_end = words_start + (mem_size / sizeof(VALUE));
	
	for (VALUE* i = words_start; i < words_end; ++i)
	{
		GCHeader* header = NULL;
		VALUE object_start = gc_find_object(*i, &header);
		if (object_start)
		{
			switch (op)
			{
				case GC_OP_MARK:
				{
					if (header->flags & GC_FLAG_MARK)
						continue;
					header->flags |= GC_FLAG_MARK;
					break;
				}
				case GC_OP_UPDATE:
				{
					VALUE new_object_start = *((VALUE*)object_start);
					ASSERT(new_object_start != object_start);
					uintx offset = (uintx)*i - (uintx)object_start;
					object_start = new_object_start;
					*i = (VALUE)((byte*)new_object_start + offset);
					
					header = (GCHeader*)((byte*)*i - sizeof(GCHeader)); // updated header
					break;
				}
			}
			
			SnObjectBase* object = (SnObjectBase*)object_start;
			
			SnObjectType type = 0;
			if (!(header->flags & GC_FLAG_BLOB))
				type = object->type;
			
			switch (type)
			{
				case SN_CONTINUATION_TYPE:
				{
					gc_scan_continuation((SnContinuation*)object, op);
					break;
				}
				default:
				{
					gc_scan_memory(object_start, header->size, op);
					break;
				}
			}
		}
	}
}

static void* gc_heap_transplant(struct heap_t* from, struct heap_t* to, void* data)
{
	GCHeader* old_header;
	void* old_data_start = gc_find_object_in_heap(from, data, &old_header);
	
	GCHeader* new_header;
	void* new_data = gc_heap_alloc(to, old_header->size, &new_header);
	memcpy(new_header, old_header, sizeof(GCHeader));
	memcpy(new_data, old_data_start, old_header->size);
	
	// place pointer to the new data in the first bytes of the old data
	*((void**)data) = new_data;
	old_header->flags |= GC_FLAG_TRANSPLANTED;
	
	return new_data;
}

static VALUE gc_intern(SnContext* ctx)
{
	ASSERT(snow_get_current_continuation() == &gc_cc);
	
	gc_unmark_heap(&gc_nursery);
	
	gc_scan_memory((byte*)&gc_cc, sizeof(SnContinuation), GC_OP_MARK);
	gc_scan_memory((byte*)_snow_store_ptr(), sizeof(SnArray*), GC_OP_MARK);
	gc_scan_memory((byte*)snow_get_basic_types(), SN_TYPE_MAX*sizeof(VALUE), GC_OP_MARK);
	
	uintx unreachable = 0;
	uintx reachable = 0;
	
	struct heap_t new_nursery;
	gc_heap_init(&new_nursery, gc_nursery.size);
	
	byte* end = gc_nursery.data + gc_nursery.offset;
	byte* i = gc_nursery.data;
	while (i < end)
	{
		GCHeader* header = (GCHeader*)i;
		SnObjectBase* base = ((SnObjectBase*)(i + sizeof(GCHeader)));
		ASSERT(gc_ptr_valid(base));
		if (!(header->flags & GC_FLAG_MARK))
		{
			if (header->free_func)
				header->free_func(base);
			++unreachable;
		}
		else
		{
			gc_heap_transplant(&gc_nursery, &new_nursery, base);
			++reachable;
		}
		i += sizeof(GCHeader) + header->size;
	}
	
	debug("%llu reachable, %llu unreachable\n", reachable, unreachable);
	
	// move all pointers
	gc_scan_memory((byte*)&gc_cc, sizeof(SnContinuation), GC_OP_UPDATE);
	gc_scan_memory((byte*)_snow_store_ptr(), sizeof(SnArray*), GC_OP_UPDATE);
	gc_scan_memory((byte*)snow_get_basic_types(), SN_TYPE_MAX*sizeof(VALUE), GC_OP_UPDATE);
	
	#ifdef DEBUG
	// poison the old nursery (how cruel!)
	memset(gc_nursery.data, 0xab, gc_nursery.size);
	#endif
	
	// set new nursery as the current nursery
	snow_free(gc_nursery.data);
	memcpy(&gc_nursery, &new_nursery, sizeof(struct heap_t));
	
	return NULL;
}

void snow_gc_stack_top(void* top)
{
	gc_stack_top = top;
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
	byte* data = (byte*)ptr;
	
	#if DEBUG_MALLOC
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
