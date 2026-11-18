#include "game.h"

/* orig $001F50: the random number generator.  A 64-bit state (random_a, random_b) is doubled with add/addx on 16-bit words;
 * the new random_a is the result. */
uint32_t random_next(void)
{
    uint32_t d0 = random_a, d1 = random_b;
    uint32_t sum, carry, d2;

    sum = (d0 & 0xffff) + (d0 & 0xffff);                       /* add.w d0,d0 */
    carry = sum >> 16;
    d0 = (d0 & 0xffff0000u) | (sum & 0xffff);
    d2 = d0 & 0xffff;                                          /* move.w d0,d2 */
    sum = (d0 & 0xffff) + (d1 & 0xffff) + carry;               /* addx.w d1,d0 */
    carry = sum >> 16;
    d0 = (d0 & 0xffff0000u) | (sum & 0xffff);
    d1 = (d1 & 0xffff0000u) | d2;                              /* move.w d2,d1 */
    d0 = (d0 << 16) | (d0 >> 16);                              /* swap d0 */
    d1 = (d1 << 16) | (d1 >> 16);                              /* swap d1 */
    d2 = d0 & 0xffff;                                          /* move.w d0,d2 */
    sum = (d0 & 0xffff) + (d1 & 0xffff) + carry;               /* addx.w d1,d0 */
    d0 = (d0 & 0xffff0000u) | (sum & 0xffff);
    d1 = (d1 & 0xffff0000u) | d2;                              /* move.w d2,d1 */
    d0 = (d0 << 16) | (d0 >> 16);
    d1 = (d1 << 16) | (d1 >> 16);
    random_a = d0;
    random_b = d1;
    return d0;
}

/* orig $001FF6: a random number from 0 to n-1 (d0.w = n; the result is in d0.w) */
uint16_t random_below(uint16_t n)
{
    uint32_t r = (random_next() >> 2) & 0xffff;

    return (uint16_t)(r % n);
}
