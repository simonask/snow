#ifndef GC_HEAP_H_94WS3GBG
#define GC_HEAP_H_94WS3GBG

typedef struct SnGCHeap {
	/*
		An incremental heap, with separate flags for better CoW-performance during GC.
	*/
	byte* start;
	byte* current;
	byte* end;
	
	uint32_t num_objects;
	
	SnGCFlags* flags;
	uint32_t num_flags;
} SnGCHeap;

static void gc_heap_clear_flags(SnGCHeap*);

static inline void gc_heap_init(SnGCHeap* heap) {
	heap->start = heap->current = heap->end = NULL;
	heap->num_objects = 0;
	heap->flags = NULL;
	gc_heap_clear_flags(heap);
}

static inline void gc_heap_finalize(SnGCHeap* heap) {
	gc_free_chunk(heap->start, heap->end - heap->start);
	snow_free(heap->flags);
}

static inline bool gc_heap_contains(const SnGCHeap* heap, const void* root) {
	const byte* data = (const byte*)root;
	return heap->start && (data >= heap->start + sizeof(SnGCObjectHead)) && (data < heap->end - sizeof(SnGCObjectTail));
}

static inline byte* gc_heap_alloc(SnGCHeap* heap, size_t total_size, uint32_t* out_object_index, size_t heap_size)
{
	ASSERT(total_size % SNOW_GC_ALIGNMENT == 0);
	
	// XXX: Lock
	
	if (!heap->start) {
		heap->start = gc_alloc_chunk(heap_size);
		heap->current = heap->start;
		heap->end = heap->start + heap_size;
	}
	
	// see if we can hold the allocation
	if (heap->current + total_size > heap->end) {
		return NULL;
	}
	
	byte* ptr = heap->current;
	heap->current += total_size;
	byte* object_end = heap->current;
	*out_object_index = heap->num_objects++;
	
	// XXX: Unlock
	
	return ptr;
}

static inline SnGCFlags gc_heap_get_flags(const SnGCHeap* heap, uint32_t flag_index) {
	ASSERT(flag_index < heap->num_objects);
	
	if (heap->flags) {
		return heap->flags[flag_index];
	}
	
	return 0;
}

static inline void gc_heap_set_flags(SnGCHeap* heap, uint32_t flag_index, byte flags) {
	ASSERT(flag_index < heap->num_objects);
	
	if (heap->num_objects > heap->num_flags) {
		uint32_t num = heap->num_flags;
		heap->num_flags = 2*heap->num_objects;
		heap->flags = snow_realloc(heap->flags, sizeof(SnGCFlags)*heap->num_flags);
		memset(heap->flags + num, 0, sizeof(SnGCFlags)*(heap->num_flags - num));
	}
	
	heap->flags[flag_index] |= flags;
}

static inline void gc_heap_clear_flags(SnGCHeap* heap) {
	snow_free(heap->flags);
	heap->flags = NULL;
	heap->num_flags = 0;
}

static inline size_t gc_heap_available(const SnGCHeap* heap) {
	return heap->end - heap->current;
}


// ----------------------------------------------------------------------------


typedef struct SnGCHeapListNode {
	SnGCHeap heap;
	struct SnGCHeapListNode* prev;
	struct SnGCHeapListNode* next;
} SnGCHeapListNode;

typedef struct SnGCHeapList {
	SnGCHeapListNode* head;
	SnGCHeapListNode* tail;
} SnGCHeapList;

static void gc_heap_list_init(SnGCHeapList* list) {
	list->head = list->tail = NULL;
}

static inline SnGCHeapListNode* gc_heap_list_push_heap(SnGCHeapList* list, SnGCHeap* heap) {
	SnGCHeapListNode* node = (SnGCHeapListNode*)snow_malloc(sizeof(SnGCHeapListNode));
	node->prev = NULL;
	node->next = list->head;
	list->head = node;
	if (!list->tail) list->tail = node;
	if (node->next) node->next->prev = node;
	
	if (heap) {
		memcpy(&node->heap, heap, sizeof(SnGCHeap));
	} else {
		gc_heap_init(&node->heap);
	}
	return node;
}

static inline SnGCHeapListNode* gc_heap_list_erase(SnGCHeapList* list, SnGCHeapListNode* node) {
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	if (node == list->head) list->head = node->next;
	if (node == list->tail) list->tail = node->prev;
	SnGCHeapListNode* next = node->next;
	gc_heap_finalize(&node->heap);
	snow_free(node);
	return next;
}

static byte* gc_heap_list_alloc(SnGCHeapList* list, size_t size, uint32_t* out_object_index, size_t heap_size)
{
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		if (gc_heap_available(&node->heap) >= size) break;
		node = node->next;
	}
	
	if (node == NULL) {
		node = gc_heap_list_push_heap(list, NULL);
	}
	
	byte* data = gc_heap_alloc(&node->heap, size, out_object_index, heap_size);
	ASSERT(data != NULL); // heap list allocations may not return NULL! Something is wrong.
	return data;
}

static SnGCHeap* gc_heap_list_find_heap(const SnGCHeapList* list, const void* ptr) {
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		if (gc_heap_contains(&node->heap, ptr)) return &node->heap;
		node = node->next;
	}
	return NULL;
}

static inline void gc_heap_list_clear_flags(SnGCHeapList* list) {
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		gc_heap_clear_flags(&node->heap);
		node = node->next;
	}
}

#endif /* end of include guard: GC_HEAP_H_94WS3GBG */
