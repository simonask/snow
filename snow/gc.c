#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"
#include "snow/continuation.h"

// necessary for accessing global stuff
HIDDEN SnArray** _snow_store_ptr();

static void* gc_stack_top = NULL;
static bool gc_collecting = false;

struct heap_t {
	byte* data;
	uintx offset;
	uintx size;
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
	heap->data = (byte*)valloc(heap->size);
	heap->offset = 0;
	#ifdef DEBUG
	memset(heap->data, 0xcd, heap->size);
	#endif
}

static void* gc_heap_alloc(struct heap_t* heap, uintx size, GCHeader** header)
{
	ASSERT(heap->data && heap->size);
	ASSERT(size < heap->size);
	uintx padded_size = gc_aligned_size(size);
	uintx new_offset = heap->offset + sizeof(GCHeader) + padded_size;
	ASSERT((new_offset % ALIGNMENT) == 0);
	if (new_offset > heap->size)
		return NULL; // alloc failed, initiate gc
		
	*header = (GCHeader*)(heap->data + heap->offset);
	void* data = ((byte*)*header) + sizeof(GCHeader);
	heap->offset = new_offset;
	
	(*header)->size = padded_size;
	(*header)->flags = 0;
	(*header)->free_func = NULL;
	(*header)->magic_bead1 = MAGIC_BEAD;
	(*header)->magic_bead2 = MAGIC_BEAD;
	
	ASSERT(((uintx)data % ALIGNMENT) == 0);
	
	return data;
}

static void* gc_alloc(uintx size, GCHeader** header)
{
	ASSERT(!gc_collecting);
	ASSERT(size > 0);
	ASSERT(sizeof(GCHeader) == ALIGNMENT);
	
	if (!gc_nursery.data)
	{
		gc_nursery.size = 1 << 23;
		gc_nursery.data = (byte*)valloc(gc_nursery.size);
		gc_nursery.offset = 0;
		memset(gc_nursery.data, 0xcd, gc_nursery.size);
	}

	ASSERT(size < gc_nursery.size); // TODO: give an option here
	
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
	gc_cc.stack_lo = (byte*)malloc(stack_size);
	gc_cc.stack_hi = gc_cc.stack_lo + stack_size;
	snow_continuation_call(&gc_cc, snow_get_current_continuation());
	free(gc_cc.stack_lo);
	
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
	
	#pragma omp parallel sections
	{
		#pragma omp section
		{
			gc_scan_memory((byte*)&gc_cc, sizeof(SnContinuation), GC_OP_MARK);
		}

		#pragma omp section
		{
			gc_scan_memory((byte*)_snow_store_ptr(), sizeof(SnArray*), GC_OP_MARK);
		}
		
		#pragma omp section
		{
			gc_scan_memory((byte*)snow_get_basic_types(), SN_TYPE_MAX*sizeof(VALUE), GC_OP_MARK);
		}
	}
	
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
		SnObjectType type = ((header->flags & GC_FLAG_BLOB) ? 0 : base->type);
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
	free(gc_nursery.data);
	memcpy(&gc_nursery, &new_nursery, sizeof(struct heap_t));
	
	return NULL;
}

void snow_gc_stack_top(void* top)
{
	gc_stack_top = top;
}