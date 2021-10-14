#pragma once

template <typename T>
inline T upalign(T num, long alignment) {
	return num + ((alignment - (num % alignment)) % alignment);
}

template <typename T>
inline T updiv(T n, long d) {
	return n / d + (n % d? 1 : 0);
}
