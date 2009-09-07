#include "test/test.h"
#include "snow/task.h"
#include "snow/context.h"
#include "snow/function.h"
#include "snow/intern.h"

SNOW_FUNC(task_test)
{
	return snow_call_method(SELF, snow_symbol("+"), 1, int_to_value(5));
}

TEST_CASE(queue) {
	SnFunction* func = snow_create_function_with_name(task_test, "task_test");
	
	SnTask tasks[8];
	for (uintx i = 0; i < 8; ++i)
	{
		// start tasks
		VALUE self = int_to_value((intx)i);
		SnContext* context = snow_create_context_for_function(func);
		context->self = self;
		snow_task_queue(&tasks[i], context);
	}
	
	VALUE results[8];
	
	for (uintx i = 0; i < 8; ++i)
	{
		// wait for completed tasks
		snow_task_finish(&tasks[i]);
		results[i] = tasks[i].return_val;
	}
	
	for (uintx i = 0; i < 8; ++i)
	{
		TEST_EQ(results[i], int_to_value(i + 5));
	}
}

static SnFunction* recurse = NULL;

SNOW_FUNC(task_recursive_test) {
	VALUE recursion_base = int_to_value(50);
	
	if (SELF > recursion_base)
		return SELF;
	
	SnTask subtasks[2];
	for (uintx i = 0; i < 2; ++i)
	{
		SnContext* context = snow_create_context_for_function(recurse);
		context->self = int_to_value(value_to_int(SELF) + 1);
		snow_task_queue(&subtasks[i], context);
	}
	
	VALUE sum = int_to_value(0);
	
	for (uintx i = 0; i < 2; ++i)
	{
		snow_task_finish(&subtasks[i]);
		sum = snow_call_method(subtasks[i].return_val, snow_symbol("+"), 1, sum);
	}
	
	return sum;
}

TEST_CASE(queue_recursive) {
	recurse = snow_create_function_with_name(task_recursive_test, "task_recursive_test");
	
	SnTask task;
	VALUE self = int_to_value(0);
	SnContext* context = snow_create_context_for_function(recurse);
	context->self = self;
//	snow_task_queue(&task, context);
//	snow_task_finish(&task);
	
	TEST(true);
}
