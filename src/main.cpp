#include <map>
#include <memory>
#include <string.h>
#include <string>
#include <vector>

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

void timer();
void pagefault();
void inexec();
void bwrite();

struct A {
	A() {
		// Uncomment for a surprise :)
		// b();
	}
	virtual void a() { strprint("A::a()\n"); }
	virtual void b() = 0;
};

struct B: public A {
	virtual void a() override { strprint("B::a()\n"); }
};

struct C: public B {
	virtual void b() override { strprint("C::b()\n"); }
};

struct NoisyDestructor {
	std::string name;
	NoisyDestructor(const std::string &name_): name(name_) {}
	~NoisyDestructor() { printf("~NoisyDestructor(%s)\n", name.c_str()); }
};

void (*table[])() = {0, 0, timer, 0, pagefault, inexec, bwrite};

extern "C" void kernel_main() {
	long rt;
	long *rt_addr = nullptr;
	asm("$rt -> %0" : "=r"(rt));
	strprint("Hello, world!\n");
	asm("%%rit table");

	C c;
	A &a = c;
	c.a();
	c.b();
	a.a();
	a.b();

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
		long pmm_start;
		asm("$k3 -> %0" : "=r"(pmm_start));
		Paging::Tables &wrapper_ref = *(Paging::Tables *) (tptr + pmm_start);
		Memory &memory = *(Memory *) (mptr + pmm_start);
		global_memory = (Memory *) ((char *) global_memory + pmm_start);
		memory.setBounds(memory.start + pmm_start, memory.high + pmm_start);

		long e0, r0;
		asm("<io devcount>");
		asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
		printf("Devcount: e0[%d], r0[%d]\n", e0, r0);
		long device_count = r0;

		std::vector<size_t> device_sizes;

		if (device_count == 0)
			strprint("No devices detected.\n");
		else
			for (long device_id = 0; device_id < device_count; ++device_id) {
				// strprint("\n\n");
				// char *name = new char[256];
				// asm("%0 -> $a1" :: "r"(device_id));
				// asm("%0 -> $a2" :: "r"(name));
				// asm("<io getname>");
				// asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// printf("After getname: e0[%d], r0[%d]\n", e0, r0);
				// if (name[255] != '\0')
				// 	name[255] = '\0';
				// printf("Device name: \"%s\"\n", name);

				// asm("%0 -> $a1" :: "r"(device_id));
				// asm("26 -> $a2" :: "r"(device_id));
				// asm("<io seekabs>");
				// asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// printf("After seek: e0[%d], r0[%d]\n", e0, r0);

				asm("%0 -> $a1" :: "r"(device_id));
				asm("<io getsize>");
				asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				printf("After getsize: e0[%d], r0[%d]\n", e0, r0);
				device_sizes.push_back(r0);

				// if (strcmp(name, "disk.img") == 0) {
				// 	asm("%0 -> $a1" :: "r"(device_id));
				// 	asm("%0 -> $a2 \n 4 -> $a3" :: "r"("Here"));
				// 	asm("<io write>");
				// 	asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// 	printf("After write: e0[%d], r0[%d]\n", e0, r0);
				// }

				// asm("%0 -> $a1" :: "r"(device_id));
				// asm("0 -> $a2");
				// asm("<io seekabs>");
				// asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// printf("After seek: e0[%d], r0[%d]\n", e0, r0);

				// char *buffer = new char[256];
				// asm("%0 -> $a1" :: "r"(device_id));
				// asm("%0 -> $a2 \n 256 -> $a3" :: "r"(buffer));
				// asm("<io read>");
				// asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// printf("After read: e0[%d], r0[%d]\n", e0, r0);
				// printf("Buffer: \"%s\"\n", buffer);

				// asm("0 -> $r0");
				// asm("%0 -> $a1" :: "r"(device_id));
				// asm("34 -> $a2");
				// asm("<io seekrel>");
				// asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// printf("After seekrel: e0[%d], r0[%d]\n", e0, r0);

				// asm("0 -> $r0");
				// asm("%0 -> $a1" :: "r"(device_id));
				// asm("<io getcursor>");
				// asm("$e0 -> %0 \n $r0 -> %1" : "=r"(e0), "=r"(r0));
				// printf("After getcursor: e0[%d], r0[%d]\n", e0, r0);

				// delete[] name;
				// delete[] buffer;

			}

			strprint("\n\nTesting stdlib.\n");
			std::string hello = "Hello, ";
			hello += "World!";
			printf("\"%s\"\n", hello.c_str());

			std::vector<std::string> strings;
			for (const char *literal: {"Foo", "Bar", "Baz"})
				strings.emplace_back(literal);

			printf("String count: %lu\n", strings.size());
			for (const std::string &str: strings)
				printf("- %s\n", str.c_str());

			prc('\n');

			std::map<std::string, int> map {{"hey", 42}, {"there", 64}, {"friend", 100}};
			map.try_emplace("what", -10);
			printf("Map size: %lu\n", map.size());
			for (const auto &[key, value]: map)
				printf("%s -> %d\n", key.c_str(), value);

			{
				printf("%s:%d\n", __FILE__, __LINE__);
				auto shared = std::make_shared<NoisyDestructor>("shared");
				printf("%s:%d\n", __FILE__, __LINE__);
				{
					printf("%s:%d\n", __FILE__, __LINE__);
					auto unique = std::make_unique<NoisyDestructor>("unique");
					printf("%s:%d use count: %lu\n", __FILE__, __LINE__, shared.use_count());
					auto shared_copy = shared;
					printf("%s:%d use count: %lu\n", __FILE__, __LINE__, shared.use_count());
				}
				printf("%s:%d\n", __FILE__, __LINE__);
			}

			if (0 < device_count) {
				WhyDevice device(0);
				MBR mbr;
				mbr.firstEntry = {0, 0xfa, 1, uint32_t(device_sizes.at(0) / 512 - 1)};
				ssize_t result = device.write(&mbr, sizeof(MBR), 0);
				if (result < 0)
					Kernel::panicf("device.write failed: %ld\n", result);
				Partition partition(device, mbr.firstEntry);
				char first24[24] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
				partition.read(&first24, 24, 0);
				for (int i = 0; i < 24; ++i) printf("%d ", first24[i]); prc('\n');

				ThornFAT::ThornFATDriver driver(&partition);
				strprint("ThornFAT driver instantiated.\n");
			}
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
	asm("63 -> $m0 \n <prc $m0> \n 32 -> $m0 \n <prc $m0> \n <prd $e0> \n <prc $m0> \n <prd $e1> \n 10 -> $m0");
	asm("<prc $m0> \n <halt>");
}
