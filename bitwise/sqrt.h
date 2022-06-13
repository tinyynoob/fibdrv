#ifndef __SQRT_H
#define __SQRT_H

#include <stdint.h>
#include "log.h"

#define sqrt(x)                 \
    _Generic((x), uint64_t      \
             : sqrt64, int64_t  \
             : sqrt64, uint32_t \
             : sqrt32, int32_t  \
             : sqrt32, default  \
             : sqrt64)(x)

// return rounding down answer
uint32_t sqrt32(uint32_t x)
{
    if (!x)
        return 0;
    unsigned shift = (log(x) & ~1u);
    x -= (uint32_t) 1u << shift;
    uint32_t ans = 1;
    while (shift > 0) {
        shift -= 2;
        uint32_t sub = ((ans << 2) | 1) << shift;
        if (x >= sub) {
            x -= sub;
            ans = (ans << 1) + 1;
        } else {
            ans = ans << 1;
        }
    }
    return ans;
}

// return rounding down answer
uint64_t sqrt64(uint64_t x)
{
    if (!x)
        return 0;
    unsigned shift = (log(x) & ~1u);
    x -= (uint64_t) 1u << shift;
    uint64_t ans = 1;
    while (shift > 0) {
        shift -= 2;
        uint64_t sub = ((ans << 2) | 1) << shift;
        if (x >= sub) {
            x -= sub;
            ans = (ans << 1) | 1;
        } else {
            ans = ans << 1;
        }
    }
    return ans;
}
#endif