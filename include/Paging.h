#pragma once

#include <cstddef>
#include <cstdint>

#include "P0Wrapper.h"

#define ADDR2ENTRY04(addr) ((((uintptr_t) addr) & ~Paging::Mask04) | Paging::Present)

namespace Paging {
	using Bitmap = uint64_t;
	using Entry = uint64_t;

	constexpr uint64_t Present    = 1;
	constexpr uint64_t Writable   = 2;
	constexpr uint64_t Executable = 4;
	constexpr uint64_t UserPage   = 8;
	constexpr uint64_t Accessed   = 16;
	constexpr uint64_t Modified   = 32;

	constexpr uint64_t PageSize = 65536;
	constexpr uint64_t TableEntries = 256;
	constexpr uint64_t TableSize = TableEntries * sizeof(Entry);
	constexpr uint64_t Mask04 = 0x7ff; // Equal to eleven ones in binary.
	constexpr uint64_t Mask5 = 0xffff; // Equal to sixteen ones in binary.
	constexpr uint64_t NotAssigned = 666; // Returned by assign functions when they don't need to assign a page.

	size_t getTableCount(size_t page_count);

	typedef Entry Table[TableEntries];

	class Tables {
		private:
			bool pmmReady = false;
			void *codeStart = nullptr, *dataStart = nullptr, *debugStart = nullptr;
			uintptr_t assign(Table *, ptrdiff_t, uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3,
			                 uint8_t index4, uint8_t index5, void *physical = nullptr, uint8_t extra_meta = 0);

		public:
			P0Wrapper p0;
			uintptr_t pmmStart;
			/** This should be a physical address. */
			Table *tables;
			Bitmap *bitmap;
			size_t pageCount;

			size_t p1count = 0, p2count = 0, p3count = 0, p4count = 0, p5count = 0, extracount = 0;

			Tables() = delete;
			Tables(const Tables &) = default;
			Tables(Tables &&) = default;

			Tables(Table *tables_, Bitmap *bitmap_, size_t page_count):
			p0(tables_), tables((Table *) tables_), bitmap(bitmap_), pageCount(page_count) {
				asm("[ 0] -> %0" : "=r"(codeStart));
				asm("[ 8] -> %0" : "=r"(dataStart));
				asm("[24] -> %0" : "=r"(debugStart));
			}

			// Keep in mind that on startup, it's safe to assume that memory past *$g will be zeroed out.
			void reset(bool zero_out_tables = false);
			/** Identity maps the first 1024 pages. */
			void bootstrap();
			void initPMM();
			Tables & setPMM(uintptr_t pmm_start, bool ready = true);

			long findFree(size_t start = 0) const;
			void mark(size_t index, bool used = true);
			bool isFree(size_t index) const;
			void * allocateFreePhysicalAddress(size_t consecutive_count = 1);
			size_t countFree() const;

			Tables &  setCodeStart(void *ptr) { codeStart  = ptr; return *this; }
			Tables &  setDataStart(void *ptr) { dataStart  = ptr; return *this; }
			Tables & setDebugStart(void *ptr) { debugStart = ptr; return *this; }
			Tables & setStarts(void *code, void *data, void *debug = nullptr) {
				codeStart = code;
				dataStart = data;
				if (debug)
					debugStart = debug;
				return *this;
			}

			/** Assigns addresses before the physical memory map is ready. */
			uintptr_t assignBeforePMM(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
			                          uint8_t index5, void *physical = nullptr, uint8_t extra_meta = 0);

			/** Assigns addresses once the physical memory map is ready. */
			uintptr_t assign(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4,
			                 uint8_t index5, void *physical = nullptr, uint8_t extra_meta = 0);

			uintptr_t assign(void *virtual_, void *physical = nullptr, uint8_t extra_meta = 0);

			Entry addr2entry5(void *, long code_offset = -1, long data_offset = -1) const;
	};
}
