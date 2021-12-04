#include "Kernel.h"
#include "util.h"
#include "wasm/SymbolTable.h"

namespace Wasmc {
	SymbolTableEntry::SymbolTableEntry(uint32_t id_, Long address_, SymbolEnum type_):
		id(id_), address(address_), type(type_) {}

	SymbolTableEntry::SymbolTableEntry(uint32_t id_, const std::string &label_, Long address_, SymbolEnum type_):
	id(id_), address(address_), type(type_), label(label_) {}

	std::vector<Long> SymbolTableEntry::encode(const std::string &name) const {
		std::vector<Long> out;
		size_t name_words = updiv(name.size(), 8ul);
		out.reserve(3 + name_words);
		out.push_back((uint64_t(id) << 32ul) | (uint32_t(type) << 16) | uint16_t(name_words));
		out.push_back(address);
		const std::vector<Long> name_longs = getLongs(name);
		out.insert(out.end(), name_longs.begin(), name_longs.end());
		return out;
	}

	std::vector<Long> SymbolTableEntry::encode() const {
		if (label.empty())
			Kernel::panic("Can't encode SymbolTableEntry: label is empty");
		return encode(label);
	}
}
