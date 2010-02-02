#ifndef LOCK_H_6MWOAQD4
#define LOCK_H_6MWOAQD4

#include "snow/basic.h"

typedef struct PACKED {
	uintx locked_by_task_id;
	byte locked;
} SnLock;

typedef struct PACKED {
	uintx locked_by_task_id;
	byte locked;
	byte recursive_count;
} SnRecursiveLock;

#endif /* end of include guard: LOCK_H_6MWOAQD4 */
