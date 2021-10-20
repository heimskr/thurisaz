#include <cstdarg>

#include "Kernel.h"
#include "Print.h"

void __attribute__((noreturn)) Kernel::panic(const std::string &message) {
	panic(message.c_str());
}

void __attribute__((noreturn)) Kernel::panic(const char *message) {
	strprint(message);
	for (;;)
		asm("<halt>");
}

void __attribute__((noreturn)) Kernel::panicf(const char *format, ...) {
	strprint("\e[2m[\e[22;31mPANIC\e[39;2m]\e[22m ");
	va_list var;
	va_start(var, format);
	vprintf(format, var);
	va_end(var);
	prc('\n');
	for (;;)
		asm("<halt>");
}
