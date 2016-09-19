#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	bool bp;

	/* TODO: Add more members if necessary */
	char expr[1024];
	unsigned int old_value;
} WP;

#endif
