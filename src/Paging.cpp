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
		uintptr_t global;
		asm("$g -> %0" : "=r"(global));
		const size_t global_pages = updiv(global, 65536);
		volatile size_t one = 1; // wasmc++ doesn't yet support "imm << $reg" (sllii) instructions.
		for (size_t i = 0; i < global_pages / 64; ++i)
			asm("~$0 -> %0" : "=r"(bitmap[i]));
		for (size_t i = 0; i < global_pages % 64; ++i)
			bitmap[global_pages / 64] |= one << i;
		strprint("Reset complete.\n");
	}

	long Tables::findFree(size_t start) {
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

	uintptr_t Tables::assign(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
	                         uint8_t index5, void *physical, uint8_t extra_meta) {
		return 0;
	}
}
