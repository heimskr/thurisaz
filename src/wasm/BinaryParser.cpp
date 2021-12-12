#include "Kernel.h"
#include "Print.h"
#include "util.h"
#include "wasm/BinaryParser.h"
#include "wasm/Instructions.h"

namespace Wasmc {
	BinaryParser::BinaryParser(const std::vector<Long> &raw_): raw(raw_) {}

	BinaryParser::BinaryParser(const std::string &text) {
		for (const std::string &line: split<std::vector>(text, "\n", true)) {
			unsigned long parsed;
			if (!parseUlong(line, parsed, 16))
				Kernel::panicf("Invalid line: %s", line.c_str());
			raw.push_back(swap64(parsed));
		}
	}

	AnyBase * BinaryParser::parse(const Long instruction) {
		auto get = [&](int offset, int length) -> Long {
			return (instruction >> (64 - offset - length)) & ((1 << length) - 1);
		};

		const auto opcode = static_cast<Opcode>(get(0, 12));

		if (opcode == 0)
			return new AnyBase(0, 0, 0, 0);

		if (RTYPES.count(opcode) != 0) {
			const auto rs = get(19, 7);
			const auto rt = get(12, 7);
			const auto rd = get(26, 7);
			const auto function = get(52, 12);
			const auto condition = get(46, 4);
			const auto flags = get(50, 2);
			return new AnyR(opcode, rs, rt, rd, function, condition, flags);
		}
		
		if (ITYPES.count(opcode) != 0) {
			const auto rs = get(18, 7);
			const auto rd = get(25, 7);
			const auto immediate = instruction & 0xffffffff;
			const auto condition = get(12, 4);
			const auto flags = get(16, 2);
			return new AnyI(opcode, rs, rd, immediate, condition, flags);
		}

		if (JTYPES.count(opcode) != 0) {
			const auto rs = get(12, 7);
			const auto link = get(19, 1);
			const auto address = instruction & 0xffffffff;
			const auto condition = get(26, 4);
			const auto flags = get(30, 2);
			return new AnyJ(opcode, rs, link, address, condition, flags);
		}

		strprint("R:\n");
		for (const Opcode opc: RTYPES)
			printf("- %u\n", opc);

		strprint("I:\n");
		for (const Opcode opc: ITYPES)
			printf("- %u\n", opc);

		strprint("J:\n");
		for (const Opcode opc: JTYPES)
			printf("- %u\n", opc);

		Kernel::panicf("Invalid instruction (opcode 0x%x): 0x%016lx", opcode, instruction);
	}

	void BinaryParser::parse() {
		offsets = {
			getCodeOffset(), getDataOffset(), getSymbolTableOffset(), getDebugOffset(), getRelocationOffset(),
			getEndOffset()
		};

		rawMeta = slice(0, offsets.code / 8);

		const std::vector<Long> nva_longs = slice(7, offsets.code / 8);
		std::string nva_string;
		nva_string.reserve(8 * nva_longs.size());
		for (const Long piece: nva_longs)
			nva_string += toString(piece);
		
		const size_t first = nva_string.find('\0');
		if (first == std::string::npos)
			Kernel::panic("Invalid name-version-author string");
		const size_t second = nva_string.find('\0', first + 1);
		if (second == std::string::npos)
			Kernel::panic("Invalid name-version-author string");
		const size_t third = nva_string.find('\0', second + 1);
		if (third == std::string::npos)
			Kernel::panic("Invalid name-version-author string");

		orcid = toString(raw[5]) + toString(raw[6]);
		name = nva_string.substr(0, first);
		version = nva_string.substr(first + 1, second - first - 1);
		author = nva_string.substr(second + 1, third - second - 1);

		rawSymbols = slice(offsets.symbolTable / 8, offsets.debug / 8);
		extractSymbols();

		rawCode = slice(offsets.code / 8, offsets.data / 8);
		for (const Long instruction: rawCode)
			code.emplace_back(parse(instruction));

		rawData = slice(offsets.data / 8, offsets.symbolTable / 8);

		rawDebugData = slice(offsets.debug / 8, offsets.relocation / 8);
		debugData = getDebugData();

		rawRelocation = slice(offsets.relocation / 8, offsets.end / 8);
		relocationData = getRelocationData();
	}

	void BinaryParser::applyRelocation(size_t code_offset, size_t data_offset) {
		std::vector<uint8_t> data_bytes;
		bool code_changed = false, data_changed = false;

		for (const RelocationData &relocation: relocationData) {
			if (relocation.symbolIndex < 0 || long(symbols.size()) <= relocation.symbolIndex)
				Kernel::panicf("Couldn't find symbol at index %ld\n", relocation.symbolIndex);

			const SymbolTableEntry &symbol = symbols.at(relocation.symbolIndex);
			long address = symbol.address;
			if (symbol.type == SymbolEnum::Data)
				address = address + data_offset;
			else
				address = address + code_offset;

			if (relocation.isData) {
				if (!data_changed) {
					for (Long data: rawData)
						for (int i = 0; i < 8; ++i)
							data_bytes.push_back((data >> (8 * i)) & 0xff);
					data_changed = true;
				}
				switch (relocation.type) {
					case RelocationType::Upper4:
						*(uint32_t *) &data_bytes[relocation.sectionOffset] = address >> 32;
						break;
					case RelocationType::Lower4:
						*(uint32_t *) &data_bytes[relocation.sectionOffset] = address & 0xffffffff;
						break;
					case RelocationType::Full:
						*(uint64_t *) &data_bytes[relocation.sectionOffset] = address;
					default:
						Kernel::panicf("Invalid RelocationType: %d\n", relocation.type);
				}
			} else {
				if (relocation.type == RelocationType::Upper4)
					address >>= 32;
				else if (relocation.type == RelocationType::Lower4)
					address &= 0xffffffff;
				if (AnyImmediate *any_imm = dynamic_cast<AnyImmediate *>(code.at(relocation.sectionOffset / 8).get())) {
					any_imm->immediate = address;
					code_changed = true;
				} else
					Kernel::panicf("No immediate value in instruction at %ld\n",
						relocation.sectionOffset + offsets.code);
			}
		}

		if (code_changed) {
			rawCode.clear();
			for (const auto &instruction: code)
				rawCode.push_back(instruction->encode());
		}

		if (data_changed) {
			rawData.clear();
			for (size_t i = 0, max = data_bytes.size(); i < max; i += 8)
				rawData.push_back(*(uint64_t *) &data_bytes[i]);
		}
	}

	decltype(BinaryParser::debugData) BinaryParser::copyDebugData() const {
		decltype(debugData) out;
		for (const auto &entry: debugData)
			out.emplace_back(entry->copy());
		return out;
	}

	Long BinaryParser::getMetaLength() const {
		return raw[0];
	}

	Long BinaryParser::getSymbolTableLength() const {
		return getDebugOffset() - getSymbolTableOffset();
	}

	Long BinaryParser::getCodeLength() const {
		return getDataOffset() - getCodeOffset();
	}

	Long BinaryParser::getDataLength() const {
		return getSymbolTableOffset() - getDataOffset();
	}

	Long BinaryParser::getDebugLength() const {
		return getRelocationOffset() - getDebugOffset();
	}

	Long BinaryParser::getRelocationLength() const {
		return getEndOffset() - getRelocationOffset();
	}

	Long BinaryParser::getMetaOffset() const {
		return 0;
	}

	Long BinaryParser::getCodeOffset() const {
		return raw[0];
	}

	Long BinaryParser::getDataOffset() const {
		return raw[1];
	}

	Long BinaryParser::getSymbolTableOffset() const {
		return raw[2];
	}

	Long BinaryParser::getDebugOffset() const {
		return raw[3];
	}

	Long BinaryParser::getRelocationOffset() const {
		return raw[4];
	}

	Long BinaryParser::getEndOffset() const {
		return raw[5];
	}

	std::vector<Long> BinaryParser::slice(size_t begin, size_t end) {
		return {raw.begin() + begin, raw.begin() + end};
	}

	std::string BinaryParser::toString(Long number) {
		std::string out;
		out.reserve(sizeof(number));
		for (size_t i = 0; i < sizeof(number); ++i)
			out += static_cast<char>((number >> (8 * i)) & 0xff);
		return out;
	}

	void BinaryParser::extractSymbols() {
		symbols.clear();
		symbolIndices.clear();

		const size_t end = getDebugOffset() / 8;

		for (size_t i = getSymbolTableOffset() / 8, j = 0; i < end && j < 1'000'000; ++j) {
			const uint32_t id = raw[i] >> 32;
			const short length = raw[i] & 0xffff;
			const SymbolEnum type = static_cast<SymbolEnum>((raw[i] >> 16) & 0xffff);
			const Long address = raw[i + 1];

			std::string symbol_name;
			symbol_name.reserve(8ul * length);

			for (size_t index = i + 2; index < i + 2 + length; ++index) {
				Long piece = raw[index];
				size_t removed = 0;
				// Take the next long and ignore any null bytes at the least significant end.
				while (piece && (piece & 0xff) == 0) {
					piece >>= 8;
					++removed;
				}

				for (size_t offset = removed; offset < sizeof(piece); ++offset) {
					char ch = static_cast<char>((piece >> (8 * offset)) & 0xff);
					if (ch == '\0')
						break;
					symbol_name += ch;
				}
			}

			symbolIndices.try_emplace(symbol_name, symbols.size());
			symbols.emplace_back(id, symbol_name, address, type);
			i += 2 + length;
		}
	}

	std::vector<std::shared_ptr<DebugEntry>> BinaryParser::getDebugData() const {
		std::vector<std::shared_ptr<DebugEntry>> out;

		const size_t start = offsets.debug / 8, end = offsets.relocation / 8;
		Long piece;
		const auto get = [&piece](unsigned char index) -> size_t {
			return (piece >> (8 * index)) & 0xff;
		};

		for (size_t i = start; i < end; ++i) {
			piece = raw[i];
			const uint8_t type = get(0);
			if (type == 1 || type == 2) {
				const size_t name_length = get(1) | (get(2) << 8) | (get(3) << 16);
				std::string debug_name;
				for (size_t j = 4; j < name_length + 4; ++j) {
					const size_t mod = j % 8;
					if (mod == 0)
						piece = raw[++i];
					debug_name += static_cast<char>(get(mod));
				}
				if (type == 1)
					out.emplace_back(new DebugFilename(debug_name));
				else
					out.emplace_back(new DebugFunction(debug_name));
			} else if (type == 3) {
				const size_t file_index = get(1) | (get(2) << 8) | (get(3) << 16);
				const uint32_t line = get(4) | (get(5) << 8) | (get(6) << 16) | (get(7) << 24);
				piece = raw[++i];
				const uint32_t column = get(0) | (get(1) << 8) | (get(2) << 16);
				const uint8_t count = get(3);
				const uint32_t function_index = get(4) | (get(5) << 8) | (get(6) << 16) | (get(7) << 24);
				DebugLocation *location = new DebugLocation(file_index, line, column, function_index);
				out.emplace_back(location->setCount(count)->setAddress(raw[++i]));
			} else {
				Kernel::panicf("Invalid debug data entry type (%u) at line %lu of %lu in %s",
					type, i + 1, raw.size(), name.c_str());
			}
		}

		return out;
	}

	std::vector<RelocationData> BinaryParser::getRelocationData() const {
		std::vector<RelocationData> out;
		for (size_t i = 0, size = rawRelocation.size(); i < size; i += 3)
			out.emplace_back(rawRelocation[i], rawRelocation[i + 1], rawRelocation[i + 2]);
		return out;
	}
}
