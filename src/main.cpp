#include "mal.h"
#include "P0Wrapper.h"
#include "Print.h"
#include "printf.h"

void pagefault();

void (*table[])() = {0, 0, 0, 0, pagefault, 0};
Memory memory;

template <typename T>
inline T upalign(T num, long alignment) {
	return num + ((alignment - (num % alignment)) % alignment);
}

int main() {
	strprint("Hello, world!\n");
	asm("%%rit table");
	prd(*((volatile long *) 1));
	prc('\n');

	char *wrapper_start;
	asm("$g -> %0" : "=r"(wrapper_start));
	wrapper_start = (char *) upalign((long) wrapper_start, 2048);

	P0Wrapper wrapper((uint64_t *) wrapper_start);
	strprint("wrapper.entries: ");
	prd((long) wrapper.entries);
	prc('\n');
	wrapper.set();
	uint64_t p5e;
	if (!wrapper.getP5E(wrapper_start, p5e)) {
		strprint("No\n");
	} else {
		strprint("P5E: ");
		prx(p5e);
		prc('\n');
	}

	uint64_t memsize;
	asm("? mem -> %0" : "=r"(memsize));
	memory.setBounds(wrapper_start + 2048, (char *) (memsize - 1));



	// uint64_t start = (uint64_t) wrapper_start;
	// printf("start = %lld\n", start);
	// *(uint64_t *) (start + 2048 * 0) = (start + 2048 * 1) | P0Wrapper::PRESENT; // P0E for 0x00xxxxxxxxxxxxxx
	// printf("*(uint64_t *) (%lld) == 0x%llx\n", start + 2048 * 0, *(uint64_t *) (start + 2048 * 0));
	// *(uint64_t *) (start + 2048 * 1) = (start + 2048 * 2) | P0Wrapper::PRESENT; // P1E for 0x0000xxxxxxxxxxxx
	// printf("P1E @ %lld\n", start + 2048 * 1);
	// *(uint64_t *) (start + 2048 * 2) = (start + 2048 * 3) | P0Wrapper::PRESENT; // P2E for 0x000000xxxxxxxxxx
	// *(uint64_t *) (start + 2048 * 3) = (start + 2048 * 4) | P0Wrapper::PRESENT; // P3E for 0x00000000xxxxxxxx
	// *(uint64_t *) (start + 2048 * 4) = (start + 2048 * 5) | P0Wrapper::PRESENT; // P4E for 0x0000000000xxxxxx
	// // P5E for 0x000000000000xxxx
	// *(uint64_t *) (start + 2048 * 5 + 0) = 0 | P0Wrapper::EXECUTABLE | P0Wrapper::WRITABLE | P0Wrapper::PRESENT;
	// *(uint64_t *) (start + 2048 * 5 + 1) = 0 | P0Wrapper::EXECUTABLE | P0Wrapper::WRITABLE | P0Wrapper::PRESENT;

	// void *stack = (void *) memsize;
	// printf("0x%p: [%u, %u, %u, %u, %u, %u : %u]\n", stack,
	// 	P0Wrapper::p0Offset(stack), P0Wrapper::p1Offset(stack), P0Wrapper::p2Offset(stack), P0Wrapper::p3Offset(stack),
	// 	P0Wrapper::p4Offset(stack), P0Wrapper::p5Offset(stack), P0Wrapper::pageOffset(stack));

	// *(uint64_t *) (wrapper_start + P0Wrapper::p0Offset(stack)) = uint64_t(wrapper_start + 2048 * 6) | P0Wrapper::PRESENT;

	int *foo = new int[42];
	int *bar = new int[666];
	printf("foo(0x%lx), bar(0x%lx)\n", foo, bar);


	asm("%%page on");
	asm("<prx %0>" :: "r"(*((volatile long *) 8)));
	asm("10 -> $m0; <prc $m0>");
}

void __attribute__((naked)) pagefault() {
	asm("63 -> $m0; <prc $m0>; 10 -> $m0; <prc $m0>; <halt>");
}
