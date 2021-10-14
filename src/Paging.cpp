#include <stdint.h>

#include "Paging.h"
#include "util.h"

namespace Paging {
	size_t getTableCount(size_t page_count) {
		size_t divisor = 256;
		size_t sum = updiv(page_count, divisor);
		for (unsigned char i = 0; i < 5; ++i)
			sum += updiv(page_count, divisor *= 256);
		return sum;
	}

	void Tables::reset() {
		asm("memset %0 x $0 -> %1" :: "r"(pageCount * 2048), "r"(tables));
		asm("memset %0 x $0 -> %1" :: "r"(updiv(pageCount, 8)), "r"(bitmap));
		uintptr_t global;
		asm("$g -> %0" : "=r"(global));
		const size_t global_pages = updiv(global, 65536);
		volatile size_t one = 1; // wasmc++ doesn't yet support "imm << $reg" (sllii) instructions.
		for (size_t i = 0; i < global_pages / 64; ++i)
			asm("~$0 -> %0" : "=r"(bitmap[i]));
		for (size_t i = 0; i < global_pages % 64; ++i)
			bitmap[global_pages / 64 + 1] |= one << i;
	}
}
