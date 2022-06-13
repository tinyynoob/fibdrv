#ifndef __LOG_H
#define __LOG_H

#include <stdint.h>
#include "fls.h"

#define log(x)                 \
    _Generic((x), uint64_t     \
             : log64, int64_t  \
             : log64, uint32_t \
             : log32, int32_t  \
             : log32, default  \
             : log64)(x)

/* rounding-down base-2 logarithm
 * log(0) is forced to -1
 */
int log32(uint32_t v)
{
    return (0 - !v) | (32 - fls(v));
}

/* rounding-down base-2 logarithm
 * log(0) is forced to -1
 */
int log64(uint64_t v)
{
    return (0 - !v) | (64 - fls(v));
}
#endif