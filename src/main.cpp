#include "P0Wrapper.h"
#include "Print.h"

void pagefault();

void (*table[])() = {0, 0, 0, 0, pagefault, 0};

int main() {
	strprint("Hello, world!\n");
	asm("rit table");
	prd(*((volatile long *) 1));
	prc('\n');

	P0Wrapper wrapper((uint64_t *) 65536);
	prd((long) wrapper.entries);
	prc('\n');
	wrapper.set();
	uint64_t p5e;
	if (!wrapper.getP5E((void *) 65536, p5e)) {
		strprint("No\n");
	} else {
		strprint("P5E: ");
		prx(p5e);
		prc('\n');
	}

	asm("page on");
	prd(*((volatile long *) 1));
	prc('\n');
}

void __attribute__((naked)) pagefault() {
	asm("63 -> $m0; <prc $m0>; 10 -> $m0; <prc $m0>; <halt>");
}
