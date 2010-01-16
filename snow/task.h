#ifndef TASK_H_EVOMPGLT
#define TASK_H_EVOMPGLT

#include "snow/basic.h"
#include <unistd.h>

CAPI void snow_init_parallel(void* stack_top);

typedef void(*SnParallelForEachCallback)(void* data, size_t element_size, size_t index, void* userdata);
CAPI void snow_parallel_for_each(void* data, size_t element_size, size_t num_elements, SnParallelForEachCallback func, void* userdata);

CAPI uintx snow_get_current_task_id();
static inline void snow_task_yield() { /*usleep(1);*/ /* pthread_yield? */ }

struct SnContinuation;
CAPI struct SnContinuation* snow_get_current_continuation();

#endif /* end of include guard: TASK_H_EVOMPGLT */
