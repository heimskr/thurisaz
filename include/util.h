#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool parseUlong(const std::string &str, uint64_t &out, int base = 10);
bool parseLong(const std::string &str, int64_t &out, int base = 10);

template <typename T>
inline T upalign(T num, long alignment) {
	return num + ((alignment - (num % alignment)) % alignment);
}

template <typename T>
inline T updiv(T n, long d) {
	return n / d + (n % d? 1 : 0);
}

/** Swaps the endianness of a 64-bit value. */
template <typename T>
T swap64(T n) {
	uint64_t casted = uint64_t(n);
	return (T) (
		((casted & 0xff) << 56) |
		(((casted >>  8) & 0xff) << 48) |
		(((casted >> 16) & 0xff) << 40) |
		(((casted >> 24) & 0xff) << 32) |
		(((casted >> 32) & 0xff) << 24) |
		(((casted >> 40) & 0xff) << 16) |
		(((casted >> 48) & 0xff) << 8) |
		((casted >> 56) & 0xff)
	);
}

template <template <typename...> typename C>
C<std::string> split(const std::string &str, const std::string &delimiter, bool condense) {
	if (str.empty())
		return {};

	size_t next = str.find(delimiter);
	if (next == std::string::npos)
		return {str};

	C<std::string> out {};
	const size_t delimiter_length = delimiter.size();
	size_t last = 0;

	out.push_back(str.substr(0, next));

	while (next != std::string::npos) {
		last = next;
		next = str.find(delimiter, last + delimiter_length);
		std::string sub = str.substr(last + delimiter_length, next - last - delimiter_length);
		if (!sub.empty() || !condense)
			out.push_back(std::move(sub));
	}

	return out;
}

template <typename T>
static std::vector<uint64_t> getLongs(const T &str) {
	if (str.empty())
		return {0};

	std::vector<uint64_t> out;
	out.reserve(updiv(str.size(), 8ul));

	uint8_t count = 0;
	uint64_t next = 0;
	int index = 0;
	for (auto ch: str) {
		next |= uint64_t(ch) << (8 * index++);
		index = index == 8? 0 : index;

		if (++count == 8) {
			out.push_back(next);
			next = 0;
			count = 0;
		}
	}

	if (count != 0)
		out.push_back(next);

	return out;
}
