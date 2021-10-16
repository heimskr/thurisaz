#include <stdint.h>

#include "Paging.h"
#include "Print.h"
#include "printf.h"
#include "util.h"

namespace Paging {
	size_t getTableCount(size_t page_count) {
		size_t divisor = 256;
		size_t sum = updiv(page_count, divisor);
		for (unsigned char i = 0; i < 5; ++i)
			sum += updiv(page_count, divisor *= 256);
		return sum;
	}

	void Tables::reset(bool zero_out_tables) {
		if (zero_out_tables) {
			printf("Resetting tables (%lu bytes).\n", pageCount * 2048);
			asm("memset %0 x $0 -> %1" :: "r"(pageCount * 2048), "r"(tables));
		}
		strprint("Resetting bitmap.\n");
		asm("memset %0 x $0 -> %1" :: "r"(updiv(pageCount, 8)), "r"(bitmap));
		strprint("Updating bitmap.\n");
		for (size_t i = 0; i < updiv(pageCount, 8 * sizeof(Bitmap)); ++i)
			asm("$0 -> %0" : "=r"(bitmap[i]));
		strprint("Reset complete.\n");
	}

	void Tables::initPMM() {
		if (pmmReady)
			return;
		uintptr_t memsize;
		asm("? mem -> %0" : "=r"(memsize));
		// For some reason, the binary format is still primarily big endian.
		codeStart = (void *) swap64((size_t) codeStart);
		dataStart = (void *) swap64((size_t) dataStart);
		asm("$0 - %1 -> %0" : "=r"(pmmStart) : "r"(memsize));
		strprint("Mapping physical memory...\n");
		for (uintptr_t i = 0; i < memsize; i += PAGE_SIZE) {
			void *virtual_address = (char *) pmmStart + i;
			void *physical = (void *) i;
			assignBeforePMM(wrapper.p0Offset(virtual_address), wrapper.p1Offset(virtual_address),
				wrapper.p2Offset(virtual_address), wrapper.p3Offset(virtual_address), wrapper.p4Offset(virtual_address),
				wrapper.p5Offset(virtual_address), physical);
		}
		pmmReady = true;
		strprint("Finished mapping physical memory.\n");
	}

	long Tables::findFree(size_t start) const {
		volatile long negative_one = 0;
		volatile long one = 1;
		// +r constraints don't seem to work as of this writing.
		asm("%1 - 1 -> %0" : "=r"(negative_one) : "r"(negative_one));
		for (size_t i = start; i < pageCount / (8 * sizeof(Bitmap)); ++i)
			if (bitmap[i] != negative_one)
				for (unsigned int j = 0; j < 8 * sizeof(Bitmap); ++j)
					if ((bitmap[i] & (one << j)) == 0)
						return i * 8 * sizeof(Bitmap) + j;
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
		return (bitmap[index >> 6] & (one << (index & 63))) != 0;
	}

	void * Tables::allocateFreePhysicalAddress(size_t consecutive_count) {
		if (consecutive_count == 0)
			return nullptr;

		if (consecutive_count == 1) {
			int free_index = findFree();
			if (free_index == -1)
				return nullptr;
			mark(free_index, true);
			return (void *) (pmmStart + free_index * PAGE_SIZE);
		}

		int index = -1;
		for (;;) {
			index = findFree(index + 1);
			for (size_t i = 1; i < consecutive_count; ++i)
				if (!isFree(index + i))
					goto nope; // sorry
			mark(index, true);
			return (void *) (pmmStart + index * PAGE_SIZE);
			nope:
			continue;
		}
	}

#define ACCESS(ptr) ((uint64_t *) ((uintptr_t) pmmStart + (uintptr_t) (ptr)))
#define NOFREE() do { strprint("\e[31mERROR\e[39m: No free pages!\n"); asm("<halt>"); } while (false)
#define ADDR2ENTRY04(addr) ((((uintptr_t) addr) & ~MASK04) | PRESENT)
	uintptr_t Tables::assignBeforePMM(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
	                                  uint8_t index5, void *physical, uint8_t extra_meta) {
		if (!(tables[0][index0] & PRESENT)) {
			// Allocate a new page for the P1 table if the P0 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				tables[0][index0] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p1 = (Entry *) (tables[0][index0] & ~MASK04);
		if (!(p1[index1] & PRESENT)) {
			// Allocate a new page for the P2 table if the P1 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p1[index1] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p2 = (Entry *) (p1[index1] & ~MASK04);
		if (!(p2[index2] & PRESENT)) {
			// Allocate a new page for the P3 table if the P2 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p2[index2] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p3 = (Entry *) (p2[index2] & ~MASK04);
		if (!(p3[index3] & PRESENT)) {
			// Allocate a new page for the P4 table if the P3 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p3[index3] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p4 = (Entry *) (p3[index3] & ~MASK04);
		if (!(p4[index4] & PRESENT)) {
			// Allocate a new page for the P5 table if the P4 entry doesn't have the present bit set.
			if (void *free_addr = allocateFreePhysicalAddress())
				p4[index4] = ADDR2ENTRY04(free_addr);
			else NOFREE();
		}

		Entry *p5 = (Entry *) (p4[index4] & ~MASK04);
		if (!(p5[index5] & PRESENT)) {
			// Allocate a new page if the P5 entry doesn't have the present bit set (or, optionally, use a provided
			// physical address).

			auto addr2entry5 = [this](void *addr) -> Entry {
				const uintptr_t low = (uintptr_t) addr - (uintptr_t) addr % PAGE_SIZE, high = low + PAGE_SIZE;
				const uintptr_t code = (uintptr_t) codeStart, data = (uintptr_t) dataStart;

				bool executable = codeStart <= addr && addr < dataStart;
				if (!executable) {
					executable = (low <= code && code < high) || (code <= low && low < data);
				}

				const bool writable = data < high;

				// I'm aware that it's possible for a page to be both executable and writable if the code-data boundary
				// falls within the page. There's nothing I can really do about it.

				return ((Entry) addr) & ~MASK5 | PRESENT | (executable? EXECUTABLE : 0) | (writable? WRITABLE : 0);
			};

			if (physical)
				return (p5[index5] = addr2entry5(physical)) & ~MASK5;

			if (void *free_addr = allocateFreePhysicalAddress())
				return (p5[index5] = addr2entry5(free_addr) | extra_meta) & ~MASK5;

			NOFREE();
		}

		return NOT_ASSIGNED;
	}
#undef ADDR2ENTRY04
#undef NOFREE
#undef ACCESS
}
