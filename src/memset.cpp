#include "memset.h"

extern "C" void * memset(void *ptr, int c, size_t len) {
	asm("memset %0 x %1 -> %2" :: "r"(len), "r"(c), "r"(ptr));
	return ptr;
}
