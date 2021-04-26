#pragma once

inline void __attribute__((naked, always_inline)) prd(long x) {
	asm("<prd $a0>");
}

inline void __attribute__((naked, always_inline)) prc(char x) {
	asm("<prc $a0>");
}

inline void __attribute__((naked, always_inline)) prx(char x) {
	asm("<prx $a0>");
}

void strprint(const char *str);
