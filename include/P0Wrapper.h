#pragma once

#include <cstdint>

struct P0Wrapper {
	uint64_t *entries;

	P0Wrapper(uint64_t *entries_);
	void set();
	static unsigned char p0Offset(void *addr);
	static unsigned char p1Offset(void *addr);
	static unsigned char p2Offset(void *addr);
	static unsigned char p3Offset(void *addr);
	static unsigned char p4Offset(void *addr);
	static unsigned char p5Offset(void *addr);
	static unsigned short pageOffset(void *addr);

	static bool isPresent(uint64_t entry);

	uint64_t getP0E(void *addr);
	bool getP1E(void *addr, uint64_t &out);
	bool getP2E(void *addr, uint64_t &out);
	bool getP3E(void *addr, uint64_t &out);
	bool getP4E(void *addr, uint64_t &out);
	bool getP5E(void *addr, uint64_t &out);
};
