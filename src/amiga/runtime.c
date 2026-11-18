/* The Amiga build has no libc and no libgcc: the helpers the compiler calls for what the 68000 has no instruction for
 * (32-bit multiply, divide, remainder).  Plain shift-and-subtract; no inline assembly. */
#include <stdint.h>

uint32_t __udivmodsi4(uint32_t n, uint32_t d, uint32_t *rem)
{
    uint32_t q = 0, r = 0;

    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= (uint32_t)1 << i;
        }
    }
    if (rem)
        *rem = r;
    return q;
}

uint32_t __udivsi3(uint32_t n, uint32_t d) { return __udivmodsi4(n, d, 0); }

uint32_t __umodsi3(uint32_t n, uint32_t d)
{
    uint32_t r;

    __udivmodsi4(n, d, &r);
    return r;
}

int32_t __divsi3(int32_t n, int32_t d)
{
    int neg = (n < 0) != (d < 0);
    uint32_t q = __udivmodsi4(n < 0 ? 0u - (uint32_t)n : (uint32_t)n, d < 0 ? 0u - (uint32_t)d : (uint32_t)d, 0);

    return neg ? (int32_t)(0u - q) : (int32_t)q;
}

int32_t __modsi3(int32_t n, int32_t d)
{
    uint32_t r;

    __udivmodsi4(n < 0 ? 0u - (uint32_t)n : (uint32_t)n, d < 0 ? 0u - (uint32_t)d : (uint32_t)d, &r);
    return n < 0 ? (int32_t)(0u - r) : (int32_t)r;
}

int32_t __mulsi3(int32_t a, int32_t b)
{
    uint32_t x = (uint32_t)a, y = (uint32_t)b, p = 0;

    while (y) {
        if (y & 1)
            p += x;
        x <<= 1;
        y >>= 1;
    }
    return (int32_t)p;
}
