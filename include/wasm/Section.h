#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Kernel.h"
#include "wasm/Types.h"

namespace Wasmc {
	struct Section {
		using ValueType = uint8_t;

		std::vector<ValueType> bytes;
		std::map<size_t, const std::string *> labels;
		StringPtrSet *allLabels = nullptr;
		size_t counter = 0;
		std::string name;

		Section(const std::string &name_, StringPtrSet *all_labels = nullptr,
		size_t count = 0):
			bytes(count, 0), allLabels(all_labels), name(name_) {}

		ValueType & operator[](size_t);
		const ValueType & operator[](size_t) const;
		Section & operator+=(const std::string *label);
		Section & operator+=(size_t);

		Section & go(size_t);

		template <typename T>
		T * extend(size_t count, uint8_t value = 0) {
			const size_t old_size = bytes.size();
			size_t new_capacity, new_size = bytes.size() + count * sizeof(T);
			for (new_capacity = 1; new_capacity < new_size; new_capacity <<= 1);
			bytes.reserve(new_capacity);
			bytes.resize(new_size, value);
			return reinterpret_cast<T *>(&bytes[old_size]);
		}

		template <typename T>
		void append(T item) {
			*extend<T>(1) = item;
			*this += sizeof(T);
		}

		template <typename T, template <typename...> typename C>
		void appendAll(const C<T> &container) {
			T *pointer = extend<T>(container.size());
			for (const T &item: container)
				*pointer++ = item;
			*this += sizeof(T) * container.size();
		}

		void append(const std::string &string) {
			char *pointer = extend<char>(string.size());
			for (char ch: string)
				*pointer++ = ch;
			*this += string.size();
		}

		template <typename T>
		void insert(size_t offset, const T &item) {
			if (size() < offset + sizeof(T))
				Kernel::panicf("Can't insert %lu bytes into a Section of size %lu at offset %lu",
					sizeof(T), size(), offset);
			*reinterpret_cast<T *>(&bytes[offset]) = item;
		}

		template <typename T, template <typename...> typename C>
		void insertAll(size_t offset, const C<T> &container) {
			if (size() < offset + container.size() * sizeof(T))
				Kernel::panicf("Can't insert %lu * %lu bytes into a Section of size %lu at offset %lu",
					container.size(), sizeof(T), size(), offset);
			T *pointer = reinterpret_cast<T *>(&bytes[offset]);
			for (const T &item: container)
				*pointer++ = item;
		}

		size_t alignUp(size_t alignment);

		void clear();

		size_t size() const;

		/** Ensures each Section's size is divisible by eight and combines them into a vector of Longs. */
		static std::vector<Long> combine(std::initializer_list<std::reference_wrapper<Section>>);
	};
}
