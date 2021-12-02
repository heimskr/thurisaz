#include <cassert>

#include "wasm/Types.h"

namespace Wasmc {
	bool isUnknown(SymbolEnum type) {
		return type == SymbolEnum::Unknown || type == SymbolEnum::UnknownCode || type == SymbolEnum::UnknownData;
	}

	RelocationData::RelocationData(Long first, Long second, Long third):
		isData((first & 1) == 1), type(RelocationType((first >> 1) & 3)), symbolIndex(first >> 3), offset(second),
		sectionOffset(third) {}

	std::vector<Long> RelocationData::encode() const {
		return {Long(isData? 1 : 0) | ((Long(type) & 3) << 1) | (symbolIndex << 3), Long(offset), Long(sectionOffset)};
	}

	bool RelocationData::operator==(const RelocationData &other) const {
		return type == other.type && symbolIndex == other.symbolIndex && offset == other.offset &&
			sectionOffset == other.sectionOffset;
	}

	RelocationData::operator std::string() const {
		return "(" + std::string(isData? "data" : "code") + ": symbolIndex[" + std::to_string(symbolIndex) +
			"], offset[" + std::to_string(offset) + "], sectionOffset[" + std::to_string(sectionOffset) + "], label[" +
			(label? *label : "nullptr") + "])";
	}

	Long AnyR::encode() const {
		return uint64_t(function)
			| (uint64_t(flags) << 12)
			| (uint64_t(condition) << 14)
			| (uint64_t(rd) << 31)
			| (uint64_t(rs) << 38)
			| (uint64_t(rt) << 45)
			| (uint64_t(opcode) << 52);
	}

	Long AnyI::encode() const {
		return uint64_t(immediate)
			| (uint64_t(rd) << 32)
			| (uint64_t(rs) << 39)
			| (uint64_t(flags) << 46)
			| (uint64_t(condition) << 48)
			| (uint64_t(opcode) << 52);
	}

	Long AnyJ::encode() const {
		return uint64_t(immediate)
			| (uint64_t(flags) << 32)
			| (uint64_t(condition) << 34)
			| (uint64_t(link? 1 : 0) << 44)
			| (uint64_t(rs) << 45)
			| (uint64_t(opcode) << 52);
	}
}
