#pragma once

#include "stdint.h"

struct P0Wrapper {
	constexpr static uint64_t PRESENT    = 1;
	constexpr static uint64_t WRITABLE   = 2;
	constexpr static uint64_t EXECUTABLE = 4;
	constexpr static uint64_t USERPAGE   = 8;
	constexpr static uint64_t ACCESSED   = 16;
	constexpr static uint64_t MODIFIED   = 32;

	uint64_t *entries;

	P0Wrapper(uint64_t *entries_);
	void set();
	static unsigned char p0Offset(void *);
	static unsigned char p1Offset(void *);
	static unsigned char p2Offset(void *);
	static unsigned char p3Offset(void *);
	static unsigned char p4Offset(void *);
	static unsigned char p5Offset(void *);
	static unsigned short pageOffset(void *);

	static bool isPresent(uint64_t entry);

	uint64_t getP0E(void *) const;
	bool getP1E(void *, uint64_t &);
	bool getP2E(void *, uint64_t &);
	bool getP3E(void *, uint64_t &);
	bool getP4E(void *, uint64_t &);
	bool getP5E(void *, uint64_t &);
	bool hasPage(void *);

	uintptr_t assign(uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3, uint8_t index4, uint8_t index5,
	                 void *physical, uint8_t extra_meta = 0);
};

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
