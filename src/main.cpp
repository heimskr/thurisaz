#include "mal.h"
#include "P0Wrapper.h"
#include "Paging.h"
#include "Print.h"
#include "printf.h"
#include "util.h"

void pagefault();

void (*table[])() = {0, 0, 0, 0, pagefault, 0};

extern "C" void kernel_main() {
	strprint("Hello, world!\n");
	asm("%%rit table");

	char *global_start;
	asm("$g -> %0" : "=r"(global_start));
	global_start = (char *) upalign((long) global_start, 2048);
	printf("Global start: %ld\n", global_start);

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
		strprint("\e[31mERROR\e[39m: Kernel is larger than 10% of main memory.\n"
			"       Allocate more memory and restart.\n");
		asm("<halt>");
	}

	char * const bitmap_start = (char *) (memsize / 10);
	const size_t page_count = updiv(memsize, 65536);
	char * const kernel_heap_start = bitmap_start + page_count / 8;
	char * const kernel_stack_start = (char *) (memsize * 2 / 5);
	char * const application_start = (char *) (memsize / 2);
	const size_t table_count = Paging::getTableCount(page_count);

	Memory memory;
	memory.setBounds(kernel_heap_start, kernel_stack_start);

	printf("Table count: %lu\n", table_count);

	const size_t tables_size = table_count * 2048;
	uint64_t *tables = (uint64_t *) malloc(tables_size);
	asm("%%setpt %0" :: "r"(tables));

	uint64_t * const bitmap = (uint64_t *) bitmap_start;
	Paging::Tables table_wrapper(tables, bitmap, page_count);
	table_wrapper.reset();

	for (int i = 0; bitmap[i]; ++i)
		printf("[%2d] %064b\n", i, bitmap[i]);
	strprint("Done.\n");

	P0Wrapper p0wrapper(tables);




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
	asm("63 -> $m0; <prc $m0>; 32 -> $m0; <prc $m0>; <prd $e0>; <prc $m0>; <prd $e1>; 10 -> $m0; <prc $m0>; <halt>");
}
