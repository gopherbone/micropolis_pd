#include "sim.h"

#include <sys/types.h>
#include <stdlib.h>

#define SIM_RAND_MAX 0xffff

static unsigned int next = 1;

int
sim_rand(void)
{
	next = next * 1103515245 + 12345;
	return ((next % ((SIM_RAND_MAX + 1) << 8)) >> 8);
}

void
sim_srand(unsigned int seed)
{
	next = seed;
}
