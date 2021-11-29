#include <map>
#include <memory>
#include <string.h>
#include <string>
#include <vector>

#include "Commands.h"
#include "Kernel.h"
#include "mal.h"
#include "P0Wrapper.h"
#include "Paging.h"
#include "Print.h"
#include "printf.h"
#include "util.h"
#include "storage/MBR.h"
#include "storage/Partition.h"
#include "storage/WhyDevice.h"
#include "fs/tfat/ThornFAT.h"
#include "fs/tfat/Util.h"

extern "C" {
	void timer();
	void pagefault();
	void inexec();
	void bwrite();
	void keybrd();
}

struct NoisyDestructor {
	const char *string;
	NoisyDestructor(const char *string_): string(string_) {}
	~NoisyDestructor() { strprint(string); }
};

long keybrd_index = 0;
unsigned long keybrd_queue[16] = {0};

void (*table[])() = {0, 0, timer, 0, pagefault, inexec, bwrite, keybrd};

extern "C" void map_loop(const std::map<std::string, int> &map) {
	for (const auto &[key, value]: map)
		printf("%s -> %d\n", key.c_str(), value);
}

extern "C" void stacktrace() {
	long m5 = 0, rt = 0;
	asm("$m5 -> %0" : "=r"(m5));
	size_t i = 0;
	asm("<p \"Stacktrace:\\n\">");
	while (m5 != 0) {
		rt = *(long *) (m5 + 16);
		printf("%2ld: %ld\n", i++, rt);
		m5 = *(long *) m5;
	}
}

extern "C" void kernel_main() {
	long rt;
	long *rt_addr = nullptr;
	asm("$rt -> %0" : "=r"(rt));
	strprint("Hello, world!\n");
	asm("%%rit table");
	--keybrd_index;

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
	const size_t page_count   = updiv(memsize, 65536);
	const size_t table_count  = Paging::getTableCount(page_count);
	const size_t tables_size  = table_count * 2048 + 2047;
	char * const page_tables_start  = bitmap_start + page_count / 8;
	char * const kernel_heap_start  = (char *) upalign((uintptr_t) page_tables_start + tables_size, 2048);
	char * const kernel_stack_start = (char *) (memsize * 2 / 5);
	char * const application_start  = (char *) (memsize / 2);

	Memory memory;
	memory.setBounds(kernel_heap_start, kernel_stack_start);

	// + 2047: hack to add enough space for alignment.
	uint64_t *tables = (uint64_t *) upalign((uintptr_t) page_tables_start, 2048);
	// The first table is P0.
	asm("%%setpt %0" :: "r"(tables));

	uint64_t * const bitmap = (uint64_t *) bitmap_start;
	Paging::Tables table_wrapper(tables, bitmap, page_count);
	table_wrapper.reset();
	table_wrapper.bootstrap();
	table_wrapper.initPMM();

	long mask;
	asm("$0 - 1 -> %0 \n lui: 0 -> %0" : "=r"(mask));

	for (int i = 0; bitmap[i]; ++i)
		printf("[%2d] %032b%032b\n", i, bitmap[i] >> 32, bitmap[i] & mask);
	strprint("Done.\n");

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
		long pmm_start = 0;
		asm("$k3 -> %0" : "=r"(pmm_start));
		Paging::Tables &wrapper_ref = *(Paging::Tables *) (tptr + pmm_start);
		Memory &memory = *(Memory *) (mptr + pmm_start);
		global_memory = (Memory *) ((char *) global_memory + pmm_start);
		memory.setBounds(memory.start + pmm_start, memory.high + pmm_start);

		std::string line;
		line.reserve(256);

		Thurisaz::Context context;
		asm("<io devcount> \n $r0 -> %0" : "=r"(context.driveCount));

		([] {
			std::map<std::string, int> map {{"hey", 42 | (43 << 8)}, {"there", 64}, {"friend", 100}};
			map.try_emplace("what", -10);
			printf("Map size: %lu\n", map.size());
			map_loop(map);
		})();

		strprint("\e[32m$\e[39;1m ");

		debug_enable = 0;
		debug_disable = 1;

		for (;;) {
			asm("<rest>");
			while (-1 < keybrd_index) {
				const long combined = keybrd_queue[keybrd_index--];
				const char key = combined & 0xff;
				long mask = 1l;
				mask <<= 32;
				const bool shift = (combined & mask) != 0;
				const bool alt   = (combined & (mask <<= 1)) != 0;
				const bool ctrl  = (combined & (mask <<= 1)) != 0;

				if (key == 'u' && ctrl) {
					line.clear();
					strprint("\e[2K\e[G\e[0;32m$\e[39;1m ");
				} else if (key == 0x7f) {
					if (!line.empty()) {
						strprint("\e[D \e[D");
						line.pop_back();
					}
				} else if (key == 0x0a) {
					if (!line.empty()) {
						strprint("\e[0m\n");
						const std::vector<std::string> pieces = split<std::vector>(line, " ", true);
						const std::string &cmd = pieces[0];
						const size_t size = pieces.size();
						line.clear();
						NoisyDestructor prompt("\e[32m$\e[39;1m ");
						if (!Thurisaz::runCommand(context, pieces))
							strprint("Unknown command.\n");
					}
				} else if (0x7f < key) {
					asm("<prx %0>" :: "r"(key));
				} else {
					line.push_back(key & 0xff);
					asm("<prc %0>" :: "r"(key & 0xff));
				}
			}
		}
	})((char *) &table_wrapper, (char *) &memory); //*/
}

extern "C" {
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
		asm("63 -> $m0               \n\
		     <prc $m0>               \n\
		     32 -> $m0               \n\
		     <prc $m0>               \n\
		     <prd $e0>               \n\
		     <prc $m0>               \n\
		     <prd $e1>               \n\
		     10 -> $m0               \n\
		     <prc $m0>               \n\
		     $k5 == 1 -> $k6         \n\
		     : pagefault_nope /* if $k6 */ \n\
		     1 -> $k5                \n\
		     :: stacktrace           \n\
		     @pagefault_nope         \n\
		     <halt>");
	}

	void __attribute__((naked)) keybrd() {
		asm("[keybrd_index] -> $e3 \n\
		     $e3 > 14 -> $e4       \n\
		     : $e0 if $e4          \n\
		     @keybrd_cont          \n\
		     keybrd_queue -> $e4   \n\
		     $e3 + 1 -> $e3        \n\
		     $e3 -> [keybrd_index] \n\
		     $e3 << 3 -> $e3       \n\
		     $e4 + $e3 -> $e3      \n\
		     $e2 -> [$e3]          \n\
		     : $e0                 ");
	}
}
