#include <stdio.h>
#include <stdlib.h>
#include "checkalloc.h"

void *checkalloc(int32 size)
{
	void *mem = _fmalloc(size);
	if (!mem)
	{
		printf("Failed to allocate %ld bytes!\n", size);
		exit(1);
	}
	return mem;
}