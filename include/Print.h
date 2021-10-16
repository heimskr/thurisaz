#pragma once

inline void __attribute__((naked, always_inline)) prd(long x) {
	asm("<prd $a0>");
}

inline void __attribute__((naked, always_inline)) prc(char x) {
	asm("<prc $a0>");
}

inline void __attribute__((naked, always_inline)) prx(long x) {
	asm("<prx $a0>");
}

inline void __attribute__((naked, always_inline)) prb(long x) {
	asm("<prb $a0>");
}

extern "C" void strprint(const char *str);
