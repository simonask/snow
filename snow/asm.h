#ifndef ASM_H_67QDIQDJ
#define ASM_H_67QDIQDJ

#include "snow/basic.h"

/*
	Definitions common to all assemblers.
*/

typedef struct Label {
	int32_t offset; // negative value is unbound label
} Label;

typedef struct LabelRef {
	int32_t offset;
	Label* label;
} LabelRef;

#endif /* end of include guard: ASM_H_67QDIQDJ */
