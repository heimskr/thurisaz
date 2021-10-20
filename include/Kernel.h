#include <string>

struct Kernel {
	static void __attribute__((noreturn)) panic(const std::string &);
	static void __attribute__((noreturn)) panic(const char *);
	static void __attribute__((noreturn)) panicf(const char *, ...);
};
