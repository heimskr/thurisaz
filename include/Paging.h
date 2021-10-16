#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Paging {
	size_t getTableCount(size_t page_count);

	using Bitmap = uint64_t;

	class Tables {
		private:
			void *tables;
			Bitmap *bitmap;
			size_t pageCount;

		public:
			Tables() = delete;
			Tables(void *tables_, Bitmap *bitmap_, size_t page_count):
				tables(tables_), bitmap(bitmap_), pageCount(page_count) {}

			// Keep in mind that on startup, it's safe to assume that memory past *$g will be zeroed out.
			void reset(bool zero_out_tables = false);

			long findFree(size_t start = 0);

			uintptr_t assign(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
			                 uint8_t index5, void *physical, uint8_t extra_meta = 0);
	};
}
