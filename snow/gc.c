#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"
#include "snow/continuation.h"

// necessary for accessing global stuff
HIDDEN SnArray** _snow_store_ptr();

static void* gc_stack_top = NULL;
static bool gc_collecting = false;

static byte* gc_nursery = NULL;
static uintx gc_nursery_offset = 0;
static uintx gc_nursery_size = 0;
static SnContinuation gc_cc;

typedef enum GCFlag {
	GC_MARK = 1,
	GC_BLOB = 1 << 1,
	GC_FLAGS_MAX = 1 << 2
} GCFlag;

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

static inline uintx gc_padded_size(uintx size)
{
	return size + (ALIGNMENT - (size % ALIGNMENT));
}

static void* gc_alloc(uintx size, GCHeader** header)
{
	ASSERT(!gc_collecting);
	ASSERT(size > 0);
	ASSERT(sizeof(GCHeader) == ALIGNMENT);
	
	if (!gc_nursery)
	{
		gc_nursery_size = 1 << 23;
		gc_nursery = (byte*)malloc(gc_nursery_size);
		gc_nursery_offset = 0;
		memset(gc_nursery, 0xcd, gc_nursery_size);
	}

	ASSERT(size < gc_nursery_size); // TODO: give an option here
	
	uintx padded_size = gc_padded_size(size);
	uintx new_offset = gc_nursery_offset + sizeof(GCHeader) + padded_size;
	
	if (new_offset > gc_nursery_size)
	{
		snow_gc();
		return gc_alloc(size, header);
	}
	
	*header = (GCHeader*)(gc_nursery + gc_nursery_offset);
	void* data = ((byte*)*header) + sizeof(GCHeader);
	gc_nursery_offset = new_offset;
	
	(*header)->size = padded_size;
	(*header)->flags = 0;
	(*header)->free_func = NULL;
	(*header)->magic_bead1 = MAGIC_BEAD;
	(*header)->magic_bead2 = MAGIC_BEAD;
	
	ASSERT(((uintx)data & 0xf) == 0);
	
//	debug("allocated object of size %d, total size %d, header at 0x%llx, data at 0x%llx\n", size, padded_size+ALIGNMENT, *header, data);
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
	header->flags |= GC_BLOB;
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

static inline bool gc_ptr_valid(void* ptr)
{
	// note: does not check if the pointer is actually in any heap
	GCHeader* header = (GCHeader*)(((byte*)ptr) - sizeof(GCHeader));
	if ((uintx)header % ALIGNMENT) return false;
	return header->magic_bead1 == MAGIC_BEAD
		&& header->magic_bead2 == MAGIC_BEAD
		&& header->flags < GC_FLAGS_MAX;
}

static bool gc_contains(void* ptr)
{
	return gc_ptr_in(ptr, &gc_nursery[ALIGNMENT], &gc_nursery[gc_nursery_offset]);
}

static inline GCHeader* gc_find_object(void* start_ptr, GCHeader** header)
{
	if (!gc_contains(start_ptr)) return NULL;
	uintx nptr = (uintx)start_ptr;
	nptr = nptr - (nptr % ALIGNMENT);
	void* ptr = (void*)nptr;
	
	while (!gc_ptr_valid(ptr))
	{
		nptr -= ALIGNMENT;
		ptr = (void*)nptr;
		
		#ifdef DEBUG
		if (!gc_contains(ptr))
			TRAP(); // massive heap corruption!
		#endif
	}
	
	*header = (GCHeader*)(nptr - sizeof(GCHeader));
	return ptr;
}

void snow_gc()
{
	gc_collecting = true;
	
	snow_continuation_init(&gc_cc, gc_intern, NULL, 0);
	uintx stack_size = 1 << 20; // 1Mb stack!
	gc_cc.stack_lo = (byte*)malloc(stack_size);
	gc_cc.stack_hi = gc_cc.stack_lo + stack_size;
	snow_continuation_call(&gc_cc, snow_get_current_continuation());
	free(gc_cc.stack_lo);
	
	gc_collecting = false;
}

static inline void gc_unmark_heap(byte* start, size_t size)
{
	byte* end = start + size;
	byte* i = start;
	while (i < end)
	{
		GCHeader* header = (GCHeader*)i;
		header->flags &= ~GC_MARK;
		i += sizeof(GCHeader) + gc_padded_size(header->size);
	}
}

static inline void gc_mark_memory(byte* mem_start, size_t mem_size)
{
	VALUE* words_start = (VALUE*)mem_start;
	VALUE* words_end = words_start + (mem_size / sizeof(VALUE));
	
	for (VALUE* i = words_start; i < words_end; ++i)
	{
		GCHeader* header = NULL;
		void* object_start = gc_find_object(*i, &header);
		if (object_start)
		{
			SnObjectBase* object = (SnObjectBase*)object_start;
			//debug("marking object: %p\n", object);
			ASSERT(gc_ptr_valid(object_start));
			if (!(header->flags & GC_MARK))
			{
				header->flags |= GC_MARK;
				gc_mark_memory(*i, header->size);
			}
		}
	}
}

static inline void gc_mark_object(SnObjectBase* object)
{
	GCHeader* header = NULL;
	object = gc_find_object(object, &header);
	
	if (header)
	{
		header->flags |= GC_MARK;
		gc_mark_memory((byte*)object, header->size);
	}
}

static VALUE gc_intern(SnContext* ctx)
{
	gc_unmark_heap(gc_nursery, gc_nursery_offset);
	
	#pragma omp parallel sections
	{
		#pragma omp section
		{
			byte* scan_root_start = (byte*)snow_get_current_continuation();
			gc_mark_memory(scan_root_start, sizeof(SnContinuation));
		}

		#pragma omp section
		{
			SnArray** scan_store_start = _snow_store_ptr();
			gc_mark_memory(scan_store_start, sizeof(SnArray*));
		}
	}
	
	uintx unreachable = 0;
	uintx reachable = 0;
	
	byte* end = gc_nursery + gc_nursery_offset;
	byte* i = gc_nursery;
	while (i < end)
	{
		GCHeader* header = (GCHeader*)i;
		SnObjectBase* base = ((SnObjectBase*)(i + sizeof(GCHeader)));
		ASSERT(gc_ptr_valid(base));
		SnObjectType type = ((header->flags & GC_BLOB) ? 0 : base->type);
		if (!(header->flags & GC_MARK))
		{
			//debug("unreachable object at %p, size %u, type %u\n", i + sizeof(GCHeader), header->size, type);
			++unreachable;
		}
		else
		{
			++reachable;
		}
		i += sizeof(GCHeader) + header->size;
	}
	
	debug("%llu reachable, %llu unreachable\n", reachable, unreachable);
	
	TRAP(); // TODO: create new nursery and move all pointers
}

void snow_gc_stack_top(void* top)
{
	gc_stack_top = top;
}