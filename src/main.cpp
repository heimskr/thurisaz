#include "mal.h"
#include "P0Wrapper.h"
#include "Print.h"
#include "printf.h"

void pagefault();

void (*table[])() = {0, 0, 0, 0, pagefault, 0};

template <typename T>
inline T upalign(T num, long alignment) {
	return num + ((alignment - (num % alignment)) % alignment);
}

template <typename T>
inline T updiv(T n, long d) {
	return n / d + (n % d? 1 : 0);
}

extern "C" void kernel_main() {
	strprint("Hello, world!\n");
	asm("%%rit table");
	prd(*((volatile long *) 1));
	prc('\n');

	char *global_start;
	asm("$g -> %0" : "=r"(global_start));
	global_start = (char *) upalign((long) global_start, 2048);

	// P0Wrapper wrapper((uint64_t *) wrapper_start);
	// strprint("wrapper.entries: ");
	// prd((long) wrapper.entries);
	// prc('\n');
	// wrapper.set();
	// uint64_t p5e;
	// if (!wrapper.getP5E(wrapper_start, p5e)) {
	// 	strprint("No\n");
	// } else {
	// 	strprint("P5E: ");
	// 	prx(p5e);
	// 	prc('\n');
	// }

	uint64_t memsize;
	asm("? mem -> %0" : "=r"(memsize));

	if (memsize / 10 < (uintptr_t) global_start) {
		strprint("\e[31mERROR\e[39m: Kernel is larger than 10% of main memory.\n       Allocate more memory and restart.\n");
		asm("<halt>");
	}

	char *bitmap_start = (char *) (memsize / 10);
	char *kernel_heap_start = bitmap_start + memsize / 65536 / 8;
	char *kernel_stack_start = (char *) (memsize * 2 / 5);
	char *application_start = (char *) (memsize / 2);

	Memory memory;
	memory.setBounds(kernel_heap_start, kernel_stack_start);

	printf("[%lx, %lx]\n", kernel_heap_start, kernel_stack_start);




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
	// printf("%ld: [%u, %u, %u, %u, %u, %u : %u]\n", stack,
	// 	P0Wrapper::p0Offset(stack), P0Wrapper::p1Offset(stack), P0Wrapper::p2Offset(stack), P0Wrapper::p3Offset(stack),
	// 	P0Wrapper::p4Offset(stack), P0Wrapper::p5Offset(stack), P0Wrapper::pageOffset(stack));

	// *(uint64_t *) (wrapper_start + P0Wrapper::p0Offset(stack)) = uint64_t(wrapper_start + 2048 * 6) | P0Wrapper::PRESENT;

	// size_t page_count = updiv(memsize, Memory::PAGE_LENGTH);
	// char *allocated = new char[page_count / 8];
	// printf("Allocated %ld char(s).\n", page_count / 8);

	// savePaging();
	// asm("%%page on");
	// asm("<prx %0>" :: "r"(*((volatile long *) 8)));
	// asm("10 -> $m0; <prc $m0>");
	// restorePaging();
}

void __attribute__((naked)) pagefault() {
	asm("63 -> $m0; <prc $m0>; 10 -> $m0; <prc $m0>; <halt>");
}
