#include <stddef.h>
#include <stdint.h>
#include "string.h"

/* Trivial C implementation of memcpy. See https://github.com/bavison/arm-mem/
 * for arm optimized asm versions.
 */

void * memcpy(void * restrict dest, const void * restrict src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while(n--) {
	*d++ = *s++;
    }
    return dest;
}
