#include <stdint.h>

#include "Paging.h"
#include "Print.h"
#include "printf.h"
#include "util.h"

#define NOFREE() do { strprint("\e[31mERROR\e[39m: No free pages!\n"); asm("<halt>"); } while (false)

static char popcnt(uint64_t x) {
	// Thanks, Wikipedia :)
	static constexpr uint64_t m1 = 0x5555555555555555;
	static constexpr uint64_t m2 = 0x3333333333333333;
	static constexpr uint64_t m4 = 0x0f0f0f0f0f0f0f0f;
	static constexpr uint64_t h01 = 0x0101010101010101;
	x -= (x >> 1) & m1;
	x = (x & m2) + ((x >> 2) & m2);
	x = (x + (x >> 4)) & m4;
	return (x * h01) >> 56;
}

namespace Paging {
	size_t getTableCount(size_t page_count) {
		size_t divisor = TableEntries;
		size_t sum = updiv(page_count, divisor);
		for (unsigned char i = 0; i < 5; ++i)
			sum += updiv(page_count, divisor *= TableEntries);
		return sum;
	}

	void Tables::reset(bool zero_out_tables) {
		if (zero_out_tables) {
			printf("Resetting tables (%lu bytes).\n", pageCount * TableSize);
			asm("memset %0 x $0 -> %1" :: "r"(pageCount * TableSize), "r"(tables));
		}
		strprint("Resetting bitmap.\n");
		asm("memset %0 x $0 -> %1" :: "r"(updiv(pageCount, 8)), "r"(bitmap));
		strprint("Updating bitmap.\n");
		for (size_t i = 0; i < updiv(pageCount, 8 * sizeof(Bitmap)); ++i)
			asm("$0 -> %0" : "=r"(bitmap[i]));
		strprint("Reset complete.\n");
	}

	void Tables::bootstrap() {
		// Assumes that the kernel isn't larger than 1024 pages (64 MiB).
		tables[0][0] = ADDR2ENTRY04(&tables[1]);
		tables[1][0] = ADDR2ENTRY04(&tables[2]);
		tables[2][0] = ADDR2ENTRY04(&tables[3]);
		tables[3][0] = ADDR2ENTRY04(&tables[4]);
		tables[4][0] = ADDR2ENTRY04(&tables[5]);
		tables[4][1] = ADDR2ENTRY04(&tables[6]);
		tables[4][2] = ADDR2ENTRY04(&tables[7]);
		tables[4][3] = ADDR2ENTRY04(&tables[8]);
		for (short i = 0; i < 256; ++i) {
			tables[5][i] = addr2entry5((void *) (PageSize * i));
			tables[6][i] = addr2entry5((void *) (PageSize * (256 + i)));
			tables[7][i] = addr2entry5((void *) (PageSize * (512 + i)));
			tables[8][i] = addr2entry5((void *) (PageSize * (768 + i)));
		}

		uintptr_t g;
		asm("$g -> %0" : "=r"(g));
		const size_t max = updiv(g, 64 * Paging::PageSize);

		for (size_t i = 0; i < max; ++i)
			asm("$0 - 1 -> %0" : "=r"(bitmap[i]));
	}

	void Tables::initPMM() {
		if (pmmReady)
			return;
		uintptr_t memsize;
		asm("? mem -> %0" : "=r"(memsize));
		asm("$0 - %1 -> %0" : "=r"(pmmStart) : "r"(memsize));
		printf("Mapping physical memory at 0x%lx...\n", pmmStart);
		for (uintptr_t i = 0; i < memsize; i += PageSize) {
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
			return (void *) (free_index * PageSize);
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
			return (void *) (index * PageSize);
			nope:
			index += i;
		}

		return nullptr;
	}

	size_t Tables::countFree() const {
		size_t out = 0;
		for (size_t i = 0; i < pageCount; i += 64) {
			size_t ones = bitmap[i / 64];
			if (ones == -1ul)
				continue;
			if (pageCount - i < 64)
				ones <<= 64 + i - pageCount;
			out += 64 - popcnt(ones);
		}

		return out;
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
		if (!(usable[0][index0] & Present)) {
			// Allocate a new page for the P1 table if the P0 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress()) {
				++p1count;
				usable[0][index0] = ADDR2ENTRY04(free_addr);
			}
			else NOFREE();
		}

		Entry *p1 = (Entry *) ((char *) (usable[0][index0] & ~Mask04) + offset);
		if (!(p1[index1] & Present)) {
			// Allocate a new page for the P2 table if the P1 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress()) {
				++p2count;
				p1[index1] = ADDR2ENTRY04(free_addr);
			}
			else NOFREE();
		}

		Entry *p2 = (Entry *) ((char *) (p1[index1] & ~Mask04) + offset);
		if (!(p2[index2] & Present)) {
			// Allocate a new page for the P3 table if the P2 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress()) {
				++p3count;
				p2[index2] = ADDR2ENTRY04(free_addr);
			}
			else NOFREE();
		}

		Entry *p3 = (Entry *) ((char *) (p2[index2] & ~Mask04) + offset);
		if (!(p3[index3] & Present)) {
			// Allocate a new page for the P4 table if the P3 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress()) {
				++p4count;
				p3[index3] = ADDR2ENTRY04(free_addr);
			}
			else NOFREE();
		}

		Entry *p4 = (Entry *) ((char *) (p3[index3] & ~Mask04) + offset);
		if (!(p4[index4] & Present)) {
			// Allocate a new page for the P5 table if the P4 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress()) {
				++p5count;
				p4[index4] = ADDR2ENTRY04(free_addr);
			}
			else NOFREE();
		}

		Entry *p5 = (Entry *) ((char *) (p4[index4] & ~Mask04) + offset);
		if (!(p5[index5] & Present)) {
			// Allocate a new page if the P5 entry doesn't have the present bit set (or, optionally, use a provided
			// physical address).

			if (physical)
				return (p5[index5] = addr2entry5(physical) | extra_meta) & ~Mask5;

			if (void *free_addr = allocateFreePhysicalAddress()) {
				++extracount;
				return (p5[index5] = addr2entry5(free_addr) | extra_meta) & ~Mask5;
			}

			NOFREE();
		}

		return NotAssigned;
	}

	Entry Tables::addr2entry5(void *addr, long code_offset, long data_offset) const {
		const uintptr_t low = uintptr_t(addr) - uintptr_t(addr) % PageSize, high = low + PageSize;
		const uintptr_t code = code_offset < 0? uintptr_t(codeStart) : uintptr_t(code_offset);
		const uintptr_t data = data_offset < 0? uintptr_t(dataStart) : uintptr_t(data_offset);

		bool executable = code <= uintptr_t(addr) && high < data;
		if (!executable)
			executable = (low <= code && code < high) || (code <= low && low < data);

		const bool writable = data < high;

		// I'm aware that it's possible for a page to be both executable and writable if the code-data boundary
		// falls within the page. There's nothing I can really do about it.

		return (Entry(addr) & ~Mask5) | Present | (executable? Executable : 0) | (writable? Writable : 0);
	}
}

#undef ADDR2ENTRY04
#undef NOFREE
