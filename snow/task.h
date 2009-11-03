#ifndef TASK_H_EVOMPGLT
#define TASK_H_EVOMPGLT

#include "snow/basic.h"

CAPI void snow_init_parallel();

typedef void(*SnParallelForEachCallback)(void* data, size_t element_size, size_t index, void* userdata);
CAPI void snow_parallel_for_each(void* data, size_t element_size, size_t num_elements, SnParallelForEachCallback func, void* userdata);

typedef void(*SnParallelCallback)(size_t index, void* userdata);
CAPI void snow_parallel_call_each(SnParallelCallback* funcs, size_t num, void* userdata);

CAPI uintx snow_get_current_task_id();

struct SnContinuation;
CAPI struct SnContinuation* snow_get_current_continuation();

#endif /* end of include guard: TASK_H_EVOMPGLT */
