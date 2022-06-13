#ifndef __FLS_H
#define __FLS_H

#include <stdint.h>
#include "ils.h"

#define fls(x)                 \
    _Generic((x), uint64_t     \
             : fls64, int64_t  \
             : fls64, uint32_t \
             : fls32, int32_t  \
             : fls32, default  \
             : fls64)(x)

/* debruijn sequence = 0x07C4ACDDu */
static const int debruijn_fls32_table[32] = {
    32, 31, 22, 30, 21, 18, 10, 29, 2,  20, 17, 15, 13, 9, 6,  28,
    1,  23, 19, 11, 3,  16, 14, 7,  24, 12, 4,  8,  25, 5, 26, 27};

/* most significant = 1
 * least significant = 32
 * 0 = 0
 */
int fls32(uint32_t x)
{
    return (0 - !!x) & debruijn_fls32_table[(ils(x) * 0x07C4ACDDu) >> 27 & 31];
}

/* debruijn sequence = 0x07EDD5E59A4E28C2u */
static const int debruijn_fls64_table[64] = {
    1, 64, 6,  63, 5,  17, 11, 62, 4,  25, 16, 37, 10, 31, 22, 61,
    3, 13, 27, 24, 15, 46, 36, 44, 9,  34, 30, 53, 21, 50, 42, 60,
    2, 7,  18, 12, 26, 38, 32, 23, 14, 28, 47, 45, 35, 54, 51, 43,
    8, 19, 39, 33, 29, 48, 55, 52, 20, 40, 49, 56, 41, 57, 58, 59};

/* most significant = 1
 * least significant = 64
 * 0 = 0
 */
int fls64(uint64_t x)
{
    return (0 - !!x) &
           debruijn_fls64_table[(ils(x) * 0x07EDD5E59A4E28C2u) >> 58 & 63];
}
#endif