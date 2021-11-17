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

	// asm("$sp -> $k1");
	// asm("$fp -> $k2");
	// asm("%0 -> $k3" :: "r"(table_wrapper.pmmStart));
	// if (rt_addr) {
	// 	*rt_addr += table_wrapper.pmmStart;
	// } else {
	// 	strprint("\e[31mERROR\e[39m: rt_addr not found!\n");
	// 	asm("<halt>");
	// }

	//*

	// asm("%%page on");
	// asm("$k1 + $k3 -> $sp");
	// asm("$k2 + $k3 -> $fp");

	([](char *tptr, char *mptr) {
		long pmm_start = 0;
		// asm("$k3 -> %0" : "=r"(pmm_start));
		Paging::Tables &wrapper_ref = *(Paging::Tables *) (tptr + pmm_start);
		Memory &memory = *(Memory *) (mptr + pmm_start);
		global_memory = (Memory *) ((char *) global_memory + pmm_start);
		memory.setBounds(memory.start + pmm_start, memory.high + pmm_start);

		/*
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

			if (0 < device_count) {
				WhyDevice device(0);
				MBR mbr;
				mbr.firstEntry = {0, 0xfa, 1, uint32_t(device_sizes.at(0) / 512 - 1)};
				ssize_t result = device.write(&mbr, sizeof(MBR), 0);
				if (result < 0)
					Kernel::panicf("device.write failed: %ld\n", result);
				Partition partition(device, mbr.firstEntry);
				ThornFAT::ThornFATDriver driver(&partition);
				strprint("ThornFAT driver instantiated.\n");
				printf("ThornFAT creation %s.\n", driver.make(sizeof(ThornFAT::DirEntry) * 5)? "succeeded" : "failed");
			}
			//*/

		// for (;;) asm("<rest>");

		std::string line;
		line.reserve(256);
		long selected_drive = -1;
		long drive_count = 0;
		asm("<io devcount> \n $r0 -> %0" : "=r"(drive_count));

		std::unique_ptr<WhyDevice> device;
		std::unique_ptr<MBR> mbr;
		std::unique_ptr<Partition> partition;
		std::unique_ptr<ThornFAT::ThornFATDriver> driver;
		std::string cwd("/");

		([] {
			std::map<std::string, int> map {{"hey", 42 | (43 << 8)}, {"there", 64}, {"friend", 100}};
			map.try_emplace("what", -10);
			printf("Map size: %lu\n", map.size());
			map_loop(map);
		})();

		strprint("\e[32m$\e[39;1m ");

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
						if (cmd == "drives" || cmd == "count") {
							printf("Number of drives: %lu\n", WhyDevice::count());
						} else if (cmd == "drive") {
							if (selected_drive == -1)
								strprint("No drive selected.\n");
							else
								printf("Selected drive: %ld\n", selected_drive);
						} else if (cmd == "select") {
							if (size != 2) {
								strprint("Usage: select <drive>\n");
								continue;
							} else if (!parseLong(pieces[1], selected_drive) || selected_drive < 0 ||
							           drive_count <= selected_drive) {
								strprint("Invalid drive ID.\n");
								selected_drive = -1;
							} else {
								printf("Selected drive %ld.\n", selected_drive);
							}
						} else if (cmd == "getsize") {
							if (size != 1 && size != 2) {
								strprint("Usage: getsize [drive]\n");
								continue;
							}
							long drive = selected_drive;
							if (size == 2 && (!parseLong(pieces[1], drive) || drive < 0 || drive_count <= drive)) {
								strprint("Invalid drive ID.\n");
								continue;
							}
							long e0, r0;
							asm("%2 -> $a1    \n\
							     <io getsize> \n\
							     $e0 -> %0    \n\
							     $r0 -> %1    " : "=r"(e0), "=r"(r0) : "r"(drive));
							if (e0 != 0)
								printf("getsize failed: %ld\n", e0);
							else
								printf("Size of drive %ld: %lu byte%s\n", drive, r0, r0 == 1? "" : "s");
						} else if (cmd == "readmbr") {
							if (size != 1 && size != 2) {
								strprint("Usage: readmbr [drive]\n");
								continue;
							}
							long drive = selected_drive;
							if (size == 2 && (!parseLong(pieces[1], drive) || drive < 0 || drive_count <= drive)) {
								strprint("Invalid drive ID.\n");
								continue;
							}
							device = std::make_unique<WhyDevice>(drive);
							if (!mbr)
								mbr = std::make_unique<MBR>();
							ssize_t status = device->read(mbr.get(), sizeof(MBR), 0);
							if (status < 0) {
								printf("Couldn't read disk: %ld\n", status);
								continue;
							}
							printf("Disk ID: 0x%02x%02x%02x%02x\n",
								mbr->diskID[3], mbr->diskID[2], mbr->diskID[1], mbr->diskID[0]);
							for (int i = 0; i < 4; ++i) {
								const MBREntry &entry = (&mbr->firstEntry)[i];
								printf("%d: attributes(0x%02x), type(0x%02x), startLBA(%u), sectors(%u) @ 0x%lx\n",
									i, entry.attributes, entry.type, entry.startLBA, entry.sectors, &entry);
							}
							printf("Signature: 0x%02x%02x @ 0x%lx\n", mbr->signature[1], mbr->signature[0], &mbr->signature);
							printf("MBR @ 0x%lx\n", mbr.get());
						} else if (cmd == "make") {
							if (!device || selected_drive < 0) {
								printf("No device selected. Use \e[3mselect\e[23m.\n");
								continue;
							}

							if (!mbr) {
								strprint("MBR hasn't been read yet. Use \e[3mreadmbr\e[23m.\n");
								continue;
							}

							printf("MBR @ 0x%lx\n", mbr.get());

							long e0, r0;
							asm("%2 -> $a1    \n\
							     <io getsize> \n\
							     $e0 -> %0    \n\
							     $r0 -> %1    " : "=r"(e0), "=r"(r0) : "r"(selected_drive));
							if (e0 != 0) {
								printf("getsize failed: %ld\n", e0);
								continue;
							}

							mbr->firstEntry.debug();
							mbr->firstEntry = MBREntry(0, 0xfa, 1, uint32_t(r0 / 512 - 1));
							printf("r0: %ld -> %u\n", r0, uint32_t(r0 / 512 - 1));
							printf("Number of blocks: %u\n", mbr->firstEntry.sectors);
							mbr->firstEntry.debug();
							ssize_t result = device->write(mbr.get(), sizeof(MBR), 0);
							if (result < 0)
								Kernel::panicf("device.write failed: %ld\n", result);
							partition = std::make_unique<Partition>(*device, mbr->firstEntry);
							driver = std::make_unique<ThornFAT::ThornFATDriver>(partition.get());
							strprint("ThornFAT driver instantiated.\n");
							printf("ThornFAT creation %s.\n", driver->make(sizeof(ThornFAT::DirEntry) * 5)? "succeeded"
								: "failed");
						} else if (cmd == "driver") {
							if (!device || selected_drive < 0) {
								strprint("No device selected. Use \e[3mreadmbr\e[23m.\n");
								continue;
							}

							if (!mbr) {
								strprint("MBR hasn't been read yet. Use \e[3mreadmbr\e[23m.\n");
								continue;
							}

							partition = std::make_unique<Partition>(*device, mbr->firstEntry);
							driver = std::make_unique<ThornFAT::ThornFATDriver>(partition.get());
							strprint("ThornFAT driver instantiated.\n");
						} else if (cmd == "ls") {
							if (!driver) {
								strprint("Driver not initialized. Use \e[3mdriver\e[23m.\n");
								continue;
							}

							const std::string path = 1 < size? cwd + "/" + pieces[1] : cwd;
							const int status = driver->readdir(path.c_str(), [](const char *item, off_t) {
								printf("- %s\n", item);
							});
							if (status != 0)
								printf("Error: %ld\n", long(status));
						} else if (cmd == "create") {
							if (!driver) {
								strprint("Driver not initialized. Use \e[3mdriver\e[23m.\n");
								continue;
							}

							if (size != 2) {
								strprint("Usage: create <path>\n");
								continue;
							}

							const int status = driver->create(pieces[1].c_str(), 0666, 0, 0);
							if (status != 0)
								printf("create error: %ld\n", long(status));
						} else if (cmd == "write") {
							if (!driver) {
								strprint("Driver not initialized. Use \e[3mdriver\e[23m.\n");
								continue;
							}

							if (size < 2) {
								strprint("Usage: write <path> [data...]\n");
								continue;
							}

							const std::string path = pieces[1];
							std::string data = pieces.size() == 2? "" : pieces[2];
							for (size_t i = 3; i < pieces.size(); ++i)
								data += " " + pieces[i];

							ssize_t status = driver->truncate(path.c_str(), data.size());
							if (status != 0) {
								printf("truncate error: %ld\n", long(status));
								continue;
							}

							status = driver->write(path.c_str(), data.c_str(), data.size(), 0);
							if (status < 0)
								printf("write error: %ld\n", long(status));
						} else if (cmd == "read") {
							if (!driver) {
								strprint("Driver not initialized. Use \e[3mdriver\e[23m.\n");
								continue;
							}

							if (size != 2) {
								strprint("Usage: read <path>\n");
								continue;
							}

							const std::string path = pieces[1][0] == '/'? pieces[1] : FS::simplifyPath(cwd, pieces[1]);
							size_t size;
							ssize_t status = driver->getsize(path.c_str(), size);
							if (status != 0) {
								printf("getsize failed: %ld\n", status);
								continue;
							}

							std::string data;
							data.resize(size);
							status = driver->read(path.c_str(), &data[0], size, 0);
							if (status < 0)
								printf("read failed: %ld\n", status);
							else
								printf("Read %lu byte%s:\n%s\n", status, status == 1? "" : "s", data.c_str());
						} else {
							strprint("Unknown command.\n");
						}
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
