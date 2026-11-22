#include "random.h"

/* orig $001F50: the random number generator.  A 64-bit state (random_state_a, random_state_b) is doubled with add/addx on 16-bit words;
 * the new random_state_a is the result. */
uint32_t random_next(void)
{
    uint32_t a = random_state_a, b = random_state_b;
    uint32_t sum, carry, saved;

    sum = (a & 0xffff) + (a & 0xffff);                       /* add.w a,a */
    carry = sum >> 16;
    a = (a & 0xffff0000u) | (sum & 0xffff);
    saved = a & 0xffff;                                          /* move.w a,saved */
    sum = (a & 0xffff) + (b & 0xffff) + carry;               /* addx.w b,a */
    carry = sum >> 16;
    a = (a & 0xffff0000u) | (sum & 0xffff);
    b = (b & 0xffff0000u) | saved;                              /* move.w saved,b */
    a = (a << 16) | (a >> 16);                              /* swap a */
    b = (b << 16) | (b >> 16);                              /* swap b */
    saved = a & 0xffff;                                          /* move.w a,saved */
    sum = (a & 0xffff) + (b & 0xffff) + carry;               /* addx.w b,a */
    a = (a & 0xffff0000u) | (sum & 0xffff);
    b = (b & 0xffff0000u) | saved;                              /* move.w saved,b */
    a = (a << 16) | (a >> 16);
    b = (b << 16) | (b >> 16);
    random_state_a = a;
    random_state_b = b;
    return a;
}

/* orig $001FF6: a random number from 0 to n-1 (n is the bound; the result is the low word) */
uint16_t random_below(uint16_t n)
{
    uint32_t r = (random_next() >> 2) & 0xffff;

    return (uint16_t)(r % n);
}
