#include "util.h"

bool parseUlong(const std::string &str, uint64_t &out, int base) {
	char *endptr;
	out = strtoul(str.c_str(), &endptr, base);
	return static_cast<unsigned long>(endptr - str.c_str()) == str.size();
}

bool parseLong(const std::string &str, int64_t &out, int base) {
	char *endptr;
	out = strtol(str.c_str(), &endptr, base);
	return static_cast<unsigned long>(endptr - str.c_str()) == str.size();
}
