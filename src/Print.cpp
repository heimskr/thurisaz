#include "Print.h"

// extern "C" void __attribute__((naked)) strprint(const char *) {
// 	asm("@_strprint_loop          \
// 	     [$a0] -> $ma /b          \
// 	     : _strprint_print if $ma \
// 	     : _strprint_done         \
// 	     @_strprint_print         \
// 	     <prc $ma>                \
// 	     $a0 + 1 -> $a0           \
// 	     : _strprint_loop         \
// 	     @_strprint_done; : $rt");
// }
