#include <stddef.h>
#include <stdint.h>
#include "string.h"

/* Trivial C implementation of memcpy. See https://github.com/bavison/arm-mem/
 * for arm optimized asm versions.
 */

void * memcpy(void * __restrict__ dest, const void * __restrict__ src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while(n--) {
	*d++ = *s++;
    }
    return dest;
}
