#include <stdint.h>

#include "Paging.h"
#include "Print.h"
#include "printf.h"
#include "util.h"

#define NOFREE() do { strprint("\e[31mERROR\e[39m: No free pages!\n"); asm("<halt>"); } while (false)

namespace Paging {
	size_t getTableCount(size_t page_count) {
		size_t divisor = TABLE_ENTRIES;
		size_t sum = updiv(page_count, divisor);
		for (unsigned char i = 0; i < 5; ++i)
			sum += updiv(page_count, divisor *= TABLE_ENTRIES);
		return sum;
	}

	void Tables::reset(bool zero_out_tables) {
		if (zero_out_tables) {
			printf("Resetting tables (%lu bytes).\n", pageCount * TABLE_SIZE);
			asm("memset %0 x $0 -> %1" :: "r"(pageCount * TABLE_SIZE), "r"(tables));
		}
		strprint("Resetting bitmap.\n");
		asm("memset %0 x $0 -> %1" :: "r"(updiv(pageCount, 8)), "r"(bitmap));
		strprint("Updating bitmap.\n");
		for (size_t i = 0; i < updiv(pageCount, 8 * sizeof(Bitmap)); ++i)
			asm("$0 -> %0" : "=r"(bitmap[i]));
		strprint("Reset complete.\n");
	}

	void Tables::bootstrap() {
		// Assumes that the kernel isn't larger than 1024 pages (128 MiB).
		tables[0][0] = ADDR2ENTRY04(&tables[1]);
		tables[1][0] = ADDR2ENTRY04(&tables[2]);
		tables[2][0] = ADDR2ENTRY04(&tables[3]);
		tables[3][0] = ADDR2ENTRY04(&tables[4]);
		tables[4][0] = ADDR2ENTRY04(&tables[5]);
		tables[4][1] = ADDR2ENTRY04(&tables[6]);
		tables[4][2] = ADDR2ENTRY04(&tables[7]);
		tables[4][3] = ADDR2ENTRY04(&tables[8]);
		for (short i = 0; i < 256; ++i) {
			tables[5][i] = addr2entry5((void *) (PAGE_SIZE * i));
			tables[6][i] = addr2entry5((void *) (PAGE_SIZE * (256 + i)));
			tables[7][i] = addr2entry5((void *) (PAGE_SIZE * (512 + i)));
			tables[8][i] = addr2entry5((void *) (PAGE_SIZE * (768 + i)));
		}
		for (unsigned i = 0; i < 16; ++i)
			asm("$0 - 1 -> %0" : "=r"(bitmap[i]));
	}

	void Tables::initPMM() {
		if (pmmReady)
			return;
		uintptr_t memsize;
		asm("? mem -> %0" : "=r"(memsize));
		asm("$0 - %1 -> %0" : "=r"(pmmStart) : "r"(memsize));
		printf("Mapping physical memory at 0x%lx...\n", pmmStart);
		for (uintptr_t i = 0; i < memsize; i += PAGE_SIZE) {
			void *virtual_address = (char *) pmmStart + i;
			void *physical = (void *) i;
			assignBeforePMM(p0Offset(virtual_address), p1Offset(virtual_address), p2Offset(virtual_address),
				            p3Offset(virtual_address), p4Offset(virtual_address), p5Offset(virtual_address), physical);
		}
		pmmReady = true;
		strprint("Finished mapping physical memory.\n");
	}

	Tables & Tables::setPMM(uintptr_t pmm_start, bool ready) {
		pmmStart = pmm_start;
		pmmReady = ready;
		return *this;
	}

	long Tables::findFree(size_t start) const {
		volatile unsigned long negative_one = 0;
		volatile long one = 1;
		asm("$0 - 1 -> %0" : "=r"(negative_one));
		for (size_t i = start / (8 * sizeof(Bitmap)); i < pageCount / (8 * sizeof(Bitmap)); ++i) {
			if (bitmap[i] != negative_one) {
				unsigned j = 0;
				// TODO: verify
				const ssize_t diff = ssize_t(start) - i * (8 * sizeof(Bitmap));
				if (0l < diff && diff < 64l)
					j = diff;
				for (; j < 8 * sizeof(Bitmap); ++j)
					if ((bitmap[i] & (one << j)) == 0)
						return i * 8 * sizeof(Bitmap) + j;
			}
		}
		return -1;
	}

	void Tables::mark(size_t index, bool used) {
		volatile long one = 1;
		if (used)
			bitmap[index / (8 * sizeof(Bitmap))] |= one << (index % (8 * sizeof(Bitmap)));
		else
			bitmap[index / (8 * sizeof(Bitmap))] &= ~(one << (index % (8 * sizeof(Bitmap))));
	}

	bool Tables::isFree(size_t index) const {
		volatile long one = 1;
		// NB: Change the math here if Bitmap changes in size.
		return (bitmap[index / 64] & (one << (index & 63))) == 0;
	}

	void * Tables::allocateFreePhysicalAddress(size_t consecutive_count) {
		if (consecutive_count == 0)
			return nullptr;

		if (consecutive_count == 1) {
			long free_index = findFree();
			if (free_index == -1)
				return nullptr;
			mark(free_index, true);
			return (void *) (free_index * PAGE_SIZE);
		}

		// TODO: verify.
		size_t index = 0;
		for (; index + consecutive_count <= pageCount;) {
			const long new_index = findFree(index);
			size_t i = 1;
			if (new_index == -1)
				goto nope;
			index = new_index;
			for (; i < consecutive_count; ++i)
				if (!isFree(index + i))
					goto nope; // sorry
			for (size_t j = 0; j < consecutive_count; ++j)
				mark(index + j, true);
			return (void *) (index * PAGE_SIZE);
			nope:
			index += i;
		}

		return nullptr;
	}

	uintptr_t Tables::assignBeforePMM(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
	                                  uint8_t index5, void *physical, uint8_t extra_meta) {
		return assign(tables, 0, index0, index1, index2, index3, index4, index5, physical, extra_meta);
	}

	uintptr_t Tables::assign(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
	                         uint8_t index5, void *physical, uint8_t extra_meta) {
		return assign((Table *) ((char *) tables + pmmStart), pmmStart, index0, index1, index2, index3, index4, index5,
		              physical, extra_meta);
	}

	uintptr_t Tables::assign(void *virtual_, void *physical, uint8_t extra_meta) {
		auto out = assign(p0Offset(virtual_), p1Offset(virtual_), p2Offset(virtual_),
		              p3Offset(virtual_), p4Offset(virtual_), p5Offset(virtual_), physical, extra_meta);
		// printf("Assigned 0x%lx -> 0x%lx (%ld -> %ld): 0x%lx (%ld)\n",
		// 	virtual_, physical, virtual_, physical, out, out);
		return out;
	}

	uintptr_t Tables::assign(Table *usable, ptrdiff_t offset, uint8_t index0, uint8_t index1, uint8_t index2,
	                         uint8_t index3, uint8_t index4, uint8_t index5, void *physical, uint8_t extra_meta) {
		if (!(usable[0][index0] & PRESENT)) {
			// Allocate a new page for the P1 table if the P0 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				usable[0][index0] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p1 = (Entry *) ((char *) (usable[0][index0] & ~MASK04) + offset);
		if (!(p1[index1] & PRESENT)) {
			// Allocate a new page for the P2 table if the P1 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p1[index1] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p2 = (Entry *) ((char *) (p1[index1] & ~MASK04) + offset);
		if (!(p2[index2] & PRESENT)) {
			// Allocate a new page for the P3 table if the P2 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p2[index2] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p3 = (Entry *) ((char *) (p2[index2] & ~MASK04) + offset);
		if (!(p3[index3] & PRESENT)) {
			// Allocate a new page for the P4 table if the P3 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p3[index3] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p4 = (Entry *) ((char *) (p3[index3] & ~MASK04) + offset);
		if (!(p4[index4] & PRESENT)) {
			// Allocate a new page for the P5 table if the P4 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p4[index4] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p5 = (Entry *) ((char *) (p4[index4] & ~MASK04) + offset);
		if (!(p5[index5] & PRESENT)) {
			// Allocate a new page if the P5 entry doesn't have the present bit set (or, optionally, use a provided
			// physical address).

			if (physical)
				return (p5[index5] = addr2entry5(physical) | extra_meta) & ~MASK5;

			if (void *free_addr = allocateFreePhysicalAddress())
				return (p5[index5] = addr2entry5(free_addr) | extra_meta) & ~MASK5;

			NOFREE();
		}

		return NOT_ASSIGNED;
	}

	Entry Tables::addr2entry5(void *addr, long code_offset, long data_offset) const {
		const uintptr_t low = uintptr_t(addr) - uintptr_t(addr) % PAGE_SIZE, high = low + PAGE_SIZE;
		const uintptr_t code = code_offset < 0? uintptr_t(codeStart) : uintptr_t(code_offset);
		const uintptr_t data = data_offset < 0? uintptr_t(dataStart) : uintptr_t(data_offset);

		bool executable = code <= uintptr_t(addr) && high < data;
		if (!executable)
			executable = (low <= code && code < high) || (code <= low && low < data);

		const bool writable = data < high;

		// I'm aware that it's possible for a page to be both executable and writable if the code-data boundary
		// falls within the page. There's nothing I can really do about it.

		return (Entry(addr) & ~MASK5) | PRESENT | (executable? EXECUTABLE : 0) | (writable? WRITABLE : 0);
	}
}

#undef ADDR2ENTRY04
#undef NOFREE
