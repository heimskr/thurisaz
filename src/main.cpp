#include "mal.h"
#include "P0Wrapper.h"
#include "Paging.h"
#include "Print.h"
#include "printf.h"
#include "util.h"

void timer();
void pagefault();
void inexec();
void bwrite();

void (*table[])() = {0, 0, timer, 0, pagefault, inexec, bwrite};

extern "C" void kernel_main() {
	long rt;
	long *rt_addr = nullptr;
	asm("$rt -> %0" : "=r"(rt));
	strprint("Hello, world!\n");
	asm("%%rit table");

	for (size_t offset = 0; offset < 8 * 128; offset += 8) {
		// Crawl up the stack until we find the magical value of 0xcafef00d that @main in extra.wasm stuffed into $fp.
		// $rt is pushed right before $fp is pushed, so once we find 0xcafef00d, the next word up contains the original
		// value of $rt.
		long value;
		asm("$fp + %0 -> $k1" :: "r"(offset));
		asm("[$k1] -> %0" : "=r"(value));
		if (value == 0xcafef00d) {
			asm("$fp + %1 -> %0" : "=r"(rt_addr) : "r"(offset + 8));
			break;
		}
	}

	char *global_start;
	asm("$g -> %0" : "=r"(global_start));
	global_start = (char *) upalign((long) global_start, 2048);
	printf("Global start: %ld\n", global_start);

	uint64_t memsize;
	asm("? mem -> %0" : "=r"(memsize));

	if (memsize / 10 < (uintptr_t) global_start) {
		strprint("\e[31mERROR\e[39m: Kernel is larger than 10% of main memory.\n"
			"       Allocate more memory and restart.\n");
		asm("<halt>");
	}

	char * const bitmap_start = (char *) (memsize / 10);
	const size_t page_count = updiv(memsize, 65536);
	const size_t table_count = Paging::getTableCount(page_count);
	const size_t tables_size = table_count * 2048 + 2047;
	char * const page_tables_start = bitmap_start + page_count / 8;
	char * const kernel_heap_start = (char *) upalign((uintptr_t) page_tables_start + tables_size, 2048);
	char * const kernel_stack_start = (char *) (memsize * 2 / 5);
	char * const application_start = (char *) (memsize / 2);

	Memory memory;
	memory.setBounds(kernel_heap_start, kernel_stack_start);

	printf("Table count: %lu\n", table_count);

	// + 2047: hack to add enough space for alignment.
	uint64_t *tables = (uint64_t *) upalign((uintptr_t) page_tables_start, 2048);
	// The first table is P0.
	asm("%%setpt %0" :: "r"(tables));

	uint64_t * const bitmap = (uint64_t *) bitmap_start;
	Paging::Tables table_wrapper(tables, bitmap, page_count);
	table_wrapper.reset();
	table_wrapper.bootstrap();
	table_wrapper.initPMM();

	for (int i = 0; bitmap[i]; ++i)
		printf("[%2d] %064b\n", i, bitmap[i]);
	strprint("Done.\n");

	int foo = 42;

	asm("$sp -> $k1");
	asm("$fp -> $k2");
	asm("%0 -> $k3" :: "r"(table_wrapper.pmmStart));
	if (rt_addr) {
		*rt_addr += table_wrapper.pmmStart;
	} else {
		strprint("\e[31mERROR\e[39m: rt_addr not found!\n");
		asm("<halt>");
	}

	asm("%%page on");
	asm("$k1 + $k3 -> $sp");
	asm("$k2 + $k3 -> $fp");

	([](char *tptr, char *mptr) {
		long pmm_start;
		asm("$k3 -> %0" : "=r"(pmm_start));
		Paging::Tables &wrapper_ref = *(Paging::Tables *) (tptr + pmm_start);
		printf("k3[%lx], pmmStart[%lx]\n", pmm_start, wrapper_ref.pmmStart);

		Memory &memory = *(Memory *) (mptr + pmm_start);
		global_memory = (Memory *) ((char *) global_memory + pmm_start);
		memory.setBounds(memory.start + pmm_start, memory.high + pmm_start);

		asm("%time 2000000");
		for (;;);
	})((char *) &table_wrapper, (char *) &memory);
}

void __attribute__((naked)) inexec() {
	asm("<p \"IE\\n\">");
	asm("<halt>");
}

void __attribute__((naked)) bwrite() {
	asm("<p \"BW\\n\">");
	asm("<halt>");
}

void __attribute__((naked)) timer() {
	asm("<p \"TI\\n\">");
	asm("%time 2000000");
	asm("<rest>");
}

void __attribute__((naked)) pagefault() {
	asm("63 -> $m0; <prc $m0>; 32 -> $m0; <prc $m0>; <prd $e0>; <prc $m0>; <prd $e1>; 10 -> $m0; <prc $m0>; <halt>");
}
