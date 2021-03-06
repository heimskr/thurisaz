#pragma once

#include <cstdint>

namespace Paging {
	unsigned char p0Offset(void *);
	unsigned char p1Offset(void *);
	unsigned char p2Offset(void *);
	unsigned char p3Offset(void *);
	unsigned char p4Offset(void *);
	unsigned char p5Offset(void *);
	unsigned short pageOffset(void *);

	struct P0Wrapper {
		uint64_t *entries;

		P0Wrapper(void *entries_);
		void set();

		static bool isPresent(uint64_t entry);

		uint64_t getP0E(void *) const;
		bool getP1E(void *, uint64_t &);
		bool getP2E(void *, uint64_t &);
		bool getP3E(void *, uint64_t &);
		bool getP4E(void *, uint64_t &);
		bool getP5E(void *, uint64_t &);
		bool hasPage(void *);
	};
}

inline void __attribute__((naked, always_inline)) savePaging() {
	asm("%%page -> $k0");
}

inline void __attribute__((naked, always_inline)) restorePaging() {
	asm(": restorePaging_on if $k0 \
	     %%page off                \
	     : restorePaging_end       \
	     @restorePaging_on         \
	     %%page on                 \
	     @restorePaging_end        \
	     <>");
}
