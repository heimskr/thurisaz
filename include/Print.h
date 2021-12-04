#pragma once

inline void __attribute__((naked, always_inline)) prd(long) {
	asm("<prd $a0>");
}

inline void __attribute__((naked, always_inline)) prc(char) {
	asm("<prc $a0>");
}

inline void __attribute__((naked, always_inline)) prx(long) {
	asm("<prx $a0>");
}

inline void __attribute__((naked, always_inline)) prb(long) {
	asm("<prb $a0>");
}

extern "C" void strprint(const char *str);
