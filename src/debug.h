#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#ifndef DEBUG
	#define DEBUG 0
#endif

#if (DEBUG)
	#define DBG(_x) \
		( fprintf(stderr, "%s %d: ", __FILE__, __LINE__), (void) (_x) )
#else
	#define DBG(_x) \
		( (void) 0)
#endif

#endif /* DEBUG_H */
