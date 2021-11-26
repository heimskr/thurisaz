#include <string.h>
#include <stdint.h>
#include <limits.h>

#define ALIGN (sizeof(size_t)-1)
#define ONES ((size_t)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

char * strncpy(char *__restrict__ d, const char *__restrict__ s, size_t n) {
// #ifdef __GNUC__
	typedef size_t __attribute__((__may_alias__)) word;
	word *wd;
	const word *ws;
	if (((uintptr_t) s & ALIGN) == ((uintptr_t) d & ALIGN)) {
		for (; ((uintptr_t) s & ALIGN) && n && (*d = *s); n--, s++, d++);
		if (!n || !*s)
			goto tail;
		wd = (word *) d;
		ws = (const word *) s;
		for (; sizeof(size_t) <= n && !HASZERO(*ws); n -= sizeof(size_t), ws++, wd++)
			*wd = *ws;
		d = (char *) wd;
		s = (const char *) ws;
	}
// #endif
	for (; n && (*d = *s); n--, s++, d++);
tail:
	memset(d, 0, n);
	return d;
}
