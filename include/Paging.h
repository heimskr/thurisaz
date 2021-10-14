#pragma once

#include <stddef.h>

namespace Paging {
	size_t getTableCount(size_t page_count);

	class Tables {
		private:
			void *tables;
			long *bitmap;
			size_t pageCount;

		public:
			Tables() = delete;
			Tables(void *tables_, long *bitmap_, size_t page_count):
				tables(tables_), bitmap(bitmap_), pageCount(page_count) {}

			void reset();
	};
}
