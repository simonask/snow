#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"


static void* gc_stack_top = NULL;

static byte* gc_nursery = NULL;
static uintx gc_nursery_offset = 0;
static uintx gc_nursery_size = 0;

typedef enum GCFlag {
	SN_GC_MARK = 1,
	SN_GC_BLOB = 1 << 1,
} GCFlag;

typedef struct GCHeader {
	uint32_t size;
	uint32_t flags;
	SnGCFreeFunc free_func;
} GCHeader;

static const int ALIGNMENT = 0x10;

static bool gc_contains(void*);
static void* gc_alloc(uintx size, GCHeader**) __attribute__((alloc_size(1)));

static void* gc_alloc(uintx size, GCHeader** header)
{
	ASSERT(sizeof(GCHeader) <= ALIGNMENT);
	
	if (!gc_nursery)
	{
		gc_nursery_size = 1 << 23;
		gc_nursery = (byte*)malloc(gc_nursery_size);
		gc_nursery_offset = 0;
	}
	
	uintx padded_size = size + (ALIGNMENT - (size % ALIGNMENT));
	uintx new_offset = gc_nursery_offset + ALIGNMENT + padded_size;
	
	if (new_offset > gc_nursery_size)
	{
		snow_gc();
		// FIXME: if size > gc_nursery_size, inf. recursion
		return gc_alloc(size, header);
	}
	
	*header = (GCHeader*)&gc_nursery[gc_nursery_offset];
	void* data = &gc_nursery[gc_nursery_offset + ALIGNMENT];
	gc_nursery_offset = new_offset;
	
	(*header)->size = padded_size;
	(*header)->flags = 0;
	(*header)->free_func = NULL;
	
	ASSERT(((intx)data & 0xf) == 0);
	
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
	header->flags |= SN_GC_BLOB;
	return data;
}

void snow_gc_set_free_func(void* data, SnGCFreeFunc func)
{
	ASSERT(gc_contains(data));
	ASSERT(((uintx)data & 0xf) == 0);
	GCHeader* header = data - sizeof(GCHeader);
	header->free_func = func;
}

bool gc_contains(void* ptr)
{
	return (byte*)ptr >= &gc_nursery[ALIGNMENT] && (byte*)ptr < &gc_nursery[gc_nursery_offset];
}

void snow_gc()
{
	debug("nursery is at 0x%llx\n", gc_nursery);
	void** i;
	GET_BASE_PTR(i);
	void** end = (void**)gc_stack_top;
	for (; i < end; ++i)
	{
		if (gc_contains(*i))
		{
			debug("root: 0x%llx\n", *i);
		}
	}
}

void snow_gc_stack_top(void* top)
{
	gc_stack_top = top;
}