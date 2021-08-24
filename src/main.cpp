#include "P0Wrapper.h"
#include "Print.h"
#include "printf.h"

void pagefault();

void (*table[])() = {0, 0, 0, 0, pagefault, 0};

int main() {
	strprint("Hello, world!\n");
	asm("%%rit table");
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

	constexpr int start = 65536;
	*(uint64_t *) (start + 2048 * 0) = (start + 2048 * 1) | P0Wrapper::PRESENT; // P0E for 0x00xxxxxxxxxxxxxx
	*(uint64_t *) (start + 2048 * 1) = (start + 2048 * 2) | P0Wrapper::PRESENT; // P1E for 0x0000xxxxxxxxxxxx
	*(uint64_t *) (start + 2048 * 2) = (start + 2048 * 3) | P0Wrapper::PRESENT; // P2E for 0x000000xxxxxxxxxx
	*(uint64_t *) (start + 2048 * 3) = (start + 2048 * 4) | P0Wrapper::PRESENT; // P3E for 0x00000000xxxxxxxx
	*(uint64_t *) (start + 2048 * 4) = (start + 2048 * 5) | P0Wrapper::PRESENT; // P4E for 0x0000000000xxxxxx
	// P5E for 0x000000000000xxxx
	*(uint64_t *) (start + 2048 * 5 + 0) = 0 | P0Wrapper::EXECUTABLE | P0Wrapper::WRITABLE | P0Wrapper::PRESENT;
	*(uint64_t *) (start + 2048 * 5 + 1) = 0 | P0Wrapper::EXECUTABLE | P0Wrapper::WRITABLE | P0Wrapper::PRESENT;

	uint64_t memsize;
	asm ("? mem -> %0" : "=r"(memsize));
	void *stack = (void *) memsize;
	printf("[%u, %u, %u, %u, %u, %u, %u]\n",
		P0Wrapper::p0Offset(stack), P0Wrapper::p1Offset(stack), P0Wrapper::p2Offset(stack), P0Wrapper::p3Offset(stack),
		P0Wrapper::p4Offset(stack), P0Wrapper::p5Offset(stack), P0Wrapper::pageOffset(stack));


	// *(uint64_t *) (65536 + P0Wrapper::p0Offset(stack)) = (65536 + 2048 * 6) | P0Wrapper::PRESENT;


	asm("%%page on");
	prd(*((volatile long *) 1));
	prc('\n');
}

void __attribute__((naked)) pagefault() {
	asm("63 -> $m0; <prc $m0>; 10 -> $m0; <prc $m0>; <halt>");
}
