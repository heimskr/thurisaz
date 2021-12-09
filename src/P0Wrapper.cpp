#include "P0Wrapper.h"

namespace Paging {
	P0Wrapper::P0Wrapper(void *entries_): entries((uint64_t *) entries_) {}

	void P0Wrapper::set() {
		asm("%%setpt %0" :: "r"(entries));
	}

	unsigned char p0Offset(void *addr) {
		return (((uintptr_t) addr) >> 56) & 0xff;
	}

	unsigned char p1Offset(void *addr) {
		return (((uintptr_t) addr) >> 48) & 0xff;
	}

	unsigned char p2Offset(void *addr) {
		return (((uintptr_t) addr) >> 40) & 0xff;
	}

	unsigned char p3Offset(void *addr) {
		return (((uintptr_t) addr) >> 32) & 0xff;
	}

	unsigned char p4Offset(void *addr) {
		return (((uintptr_t) addr) >> 24) & 0xff;
	}

	unsigned char p5Offset(void *addr) {
		return (((uintptr_t) addr) >> 16) & 0xff;
	}

	unsigned short pageOffset(void *addr) {
		return ((uintptr_t) addr) & 0xffff;
	}

	bool P0Wrapper::isPresent(uint64_t entry) {
		return (entry & 1) == 1;
	}

	uint64_t P0Wrapper::getP0E(void *addr) const {
		return entries[p0Offset(addr)];
	}

	bool P0Wrapper::getP1E(void *addr, uint64_t &out) {
		const uint64_t p0e = getP0E(addr);
		if (!isPresent(p0e))
			return false;
		out = ((uint64_t *) (p0e & ~0x7ff))[p1Offset(addr)];
		return true;
	}

	bool P0Wrapper::getP2E(void *addr, uint64_t &out) {
		uint64_t p1e;
		if (!getP1E(addr, p1e) || !isPresent(p1e))
			return false;
		out = ((uint64_t *) (p1e & ~0x7ff))[p2Offset(addr)];
		return true;
	}

	bool P0Wrapper::getP3E(void *addr, uint64_t &out) {
		uint64_t p2e;
		if (!getP2E(addr, p2e) || !isPresent(p2e))
			return false;
		out = ((uint64_t *) (p2e & ~0x7ff))[p3Offset(addr)];
		return true;
	}

	bool P0Wrapper::getP4E(void *addr, uint64_t &out) {
		uint64_t p3e;
		if (!getP3E(addr, p3e) || !isPresent(p3e))
			return false;
		out = ((uint64_t *) (p3e & ~0x7ff))[p4Offset(addr)];
		return true;
	}

	bool P0Wrapper::getP5E(void *addr, uint64_t &out) {
		uint64_t p4e;
		if (!getP4E(addr, p4e) || !isPresent(p4e))
			return false;
		out = ((uint64_t *) (p4e & ~0x7ff))[p5Offset(addr)];
		return true;
	}

	bool P0Wrapper::hasPage(void *addr) {
		uint64_t p5e;
		if (!getP5E(addr, p5e))
			return false;
		return isPresent(p5e);
	}
}
