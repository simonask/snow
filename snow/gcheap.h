#ifndef GC_HEAP_H_94WS3GBG
#define GC_HEAP_H_94WS3GBG

typedef struct SnGCHeap {
	/*
		An incremental heap, with separate flags for better CoW-performance during GC.
	*/
	byte* start;
	byte* current;
	byte* end;
	byte* free_element;
	
	SnGCFlags* flags;
	uintx element_size;
	uintx num_reachable;
	uintx num_allocated;
} SnGCHeap;

static inline SnGCHeap* gc_heap_create(uintx element_size) {
	ASSERT(GC_HEAP_SIZE > sizeof(SnGCHeap));
	ASSERT(element_size > sizeof(void*));
	ASSERT(sizeof(SnGCHeap) % SNOW_GC_ALIGNMENT == 0);
	byte* mem = (byte*)snow_malloc_aligned(GC_HEAP_SIZE, GC_HEAP_SIZE);
	SnGCHeap* heap = (SnGCHeap*)mem;
	size_t padding = snow_round_to_alignment(sizeof(SnGCHeap), element_size);
	heap->start = heap->current = mem + padding;
	uintx heap_size = GC_HEAP_SIZE - sizeof(SnGCHeap);
	heap_size -= heap_size % element_size;
	heap->end = heap->start + heap_size;
	heap->free_element = NULL;
	heap->flags = NULL;
	heap->element_size = element_size;
	heap->num_reachable = 0;
	heap->num_allocated = 0;
	return heap;
}

static inline void gc_heap_destroy(SnGCHeap* heap) {
	if (!heap) return;
	ASSERT(heap->num_reachable == 0);
	ASSERT(heap->num_allocated == 0);
	ASSERT(heap->free_element != NULL);
	snow_free(heap->flags);
	snow_free(heap);
}

static inline bool gc_heap_contains(const SnGCHeap* heap, const void* root) {
	const byte* data = (const byte*)root;
	return heap->start && data >= heap->start && data < heap->end;
}

static inline byte* gc_heap_alloc_element(SnGCHeap* heap)
{
	byte* ptr = NULL;
	
	if (heap->free_element != NULL)
	{
		ptr = heap->free_element;
		heap->free_element = *(byte**)ptr;
	}
	else if (heap->current < heap->end)
	{
		// no free element, extend heap bounds
		ptr = heap->current;
		heap->current += heap->element_size;
	}
	
	return ptr;
}

static inline void gc_heap_free_element(SnGCHeap* heap, void* element)
{
	ASSERT(gc_heap_contains(heap, element));          // not in the heap
	ASSERT(((uintx)element - (uintx)heap) % heap->element_size == 0); // pointer to the middle of an object
	*(void**)element = heap->free_element;
	heap->free_element = (byte*)element;
}

static inline SnGCFlags gc_heap_get_flags(const SnGCHeap* heap, void* element) {
	ASSERT(gc_heap_contains(heap, element));          // not in the heap
	ASSERT(((uintx)element - (uintx)heap) % heap->element_size == 0); // pointer to the middle of an object
	
	size_t flag_index = (uintx)((byte*)element - heap->start) / heap->element_size;
	if (heap->flags) {
		return heap->flags[flag_index];
	}
	return GC_NO_FLAGS;
}

static inline void gc_heap_set_flags(SnGCHeap* heap, void* element, byte flags) {
	ASSERT(gc_heap_contains(heap, element));          // not in the heap
	ASSERT(((uintx)element - (uintx)heap) % heap->element_size == 0); // pointer to the middle of an object
	
	if (!heap->flags) {
		uintx max_elements = (uintx)(heap->end - heap->start) / heap->element_size;
		heap->flags = (SnGCFlags*)snow_malloc(sizeof(SnGCFlags)*max_elements);
		memset(heap->flags, 0, sizeof(SnGCFlags)*max_elements);
	}
	
	size_t flag_index = (uintx)((byte*)element - heap->start) / heap->element_size;
	heap->flags[flag_index] |= flags;
}

static inline void gc_heap_clear_flags(SnGCHeap* heap) {
	snow_free(heap->flags);
	heap->flags = NULL;
	heap->num_reachable = 0;
}

static inline size_t gc_heap_available(const SnGCHeap* heap) {
	return heap->end - heap->current;
}


// ----------------------------------------------------------------------------


typedef struct SnGCHeapListNode {
	SnGCHeap* heap;
	struct SnGCHeapListNode* prev;
	struct SnGCHeapListNode* next;
} SnGCHeapListNode;

typedef struct SnGCHeapList {
	uintx element_size;
	SnGCHeapListNode* head;
	SnGCHeapListNode* tail;
} SnGCHeapList;

static void gc_heap_list_init(SnGCHeapList* list, uintx element_size) {
	list->element_size = element_size;
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
		ASSERT(heap->element_size == list->element_size);
		node->heap = heap;
	} else {
		node->heap = gc_heap_create(list->element_size);
	}
	return node;
}

static inline SnGCHeapListNode* gc_heap_list_erase(SnGCHeapList* list, SnGCHeapListNode* node) {
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	if (node == list->head) list->head = node->next;
	if (node == list->tail) list->tail = node->prev;
	SnGCHeapListNode* next = node->next;
	gc_heap_destroy(node->heap);
	snow_free(node);
	return next;
}

static inline SnGCHeap* gc_heap_list_pop(SnGCHeapList* list) {
	if (list->head) {
		SnGCHeap* heap = list->tail->heap;
		list->tail->heap = NULL;
		gc_heap_list_erase(list, list->tail);
		return heap;
	}
	return NULL;
}

static inline void gc_heap_list_clear(SnGCHeapList* list) {
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		SnGCHeapListNode* to_delete = node;
		node = node->next;
		gc_heap_destroy(node->heap);
		snow_free(to_delete);
	}
	list->head = list->tail = NULL;
}

static byte* gc_heap_list_alloc_element(SnGCHeapList* list)
{
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		byte* ptr = gc_heap_alloc_element(node->heap);
		if (ptr) return ptr;
		node = node->next;
	}
	
	// none of the existing heaps could allocate the data, so make a new one
	node = gc_heap_list_push_heap(list, NULL);
	byte* ptr = gc_heap_alloc_element(node->heap);
	ASSERT(ptr != NULL); // heap list allocations may not return NULL! Something is wrong.
	return ptr;
}

static inline void gc_heap_list_clear_flags(SnGCHeapList* list) {
	SnGCHeapListNode* node = list->head;
	while (node != NULL) {
		gc_heap_clear_flags(node->heap);
		node = node->next;
	}
}

static inline bool gc_heap_list_contains(const SnGCHeapList* list, const SnGCHeap* heap) {
	for (const SnGCHeapListNode* n = list->head; n != NULL; n = n->next) {
		if (n->heap == heap) return true;
	}
	return false;
}

#endif /* end of include guard: GC_HEAP_H_94WS3GBG */
