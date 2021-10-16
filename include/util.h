#pragma once

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
