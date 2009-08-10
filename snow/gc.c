#include "snow/gc.h"
#include "snow/arch.h"
#include "snow/intern.h"
#include "snow/continuation.h"

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
	
	#ifdef ARCH_IS_32_BIT
	byte padding[4];
	#endif
} GCHeader;

static const int ALIGNMENT = 0x10;

static bool gc_contains(void*);
static void* gc_alloc(uintx size, GCHeader**) __attribute__((alloc_size(1)));
static VALUE gc_intern(SnContext*);

static void* gc_alloc(uintx size, GCHeader** header)
{
	ASSERT(size > 0);
	ASSERT(sizeof(GCHeader) <= ALIGNMENT);
	
	if (!gc_nursery)
	{
		gc_nursery_size = 1 << 23;
		gc_nursery = (byte*)malloc(gc_nursery_size);
		gc_nursery_offset = 0;
		memset(gc_nursery, 0xcd, gc_nursery_size);
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
	header->flags |= SN_GC_BLOB;
	return data;
}

void snow_gc_set_free_func(void* data, SnGCFreeFunc func)
{
	ASSERT(gc_contains(data));
	ASSERT(((uintx)data & 0xf) == 0);
	GCHeader* header = (GCHeader*)((byte*)data - sizeof(GCHeader));
	header->free_func = func;
}

bool gc_contains(void* ptr)
{
	return (byte*)ptr >= &gc_nursery[ALIGNMENT] && (byte*)ptr < &gc_nursery[gc_nursery_offset];
}

static SnContinuation gc_cc;

void snow_gc()
{
	snow_continuation_init(&gc_cc, gc_intern, NULL, 0);
	uintx stack_size = 1 << 20; // 1Mb stack!
	gc_cc.stack_lo = (byte*)malloc(stack_size);
	gc_cc.stack_hi = gc_cc.stack_lo + stack_size;
	snow_continuation_call(&gc_cc, snow_get_current_continuation());
	free(gc_cc.stack_lo);
}

VALUE gc_intern(SnContext* ctx)
{
	puts("LOL");
}

void snow_gc_stack_top(void* top)
{
	gc_stack_top = top;
}