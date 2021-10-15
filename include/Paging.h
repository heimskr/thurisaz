#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Paging {
	size_t getTableCount(size_t page_count);

	class Tables {
		private:
			void *tables;
			uint64_t *bitmap;
			size_t pageCount;

		public:
			Tables() = delete;
			Tables(void *tables_, uint64_t *bitmap_, size_t page_count):
				tables(tables_), bitmap(bitmap_), pageCount(page_count) {}

			// Keep in mind that on startup, it's safe to assume that memory past *$g will be zeroed out.
			void reset(bool zero_out_tables = false);
	};
}
