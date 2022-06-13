#ifndef __ILS_H
#define __ILS_H

#include <stdint.h>

#define ils(x)                 \
    _Generic((x), uint32_t     \
             : ils32, int32_t  \
             : ils32, uint64_t \
             : ils64, int64_t  \
             : ils64, default  \
             : ils64)(x)

// isolate last set
uint32_t ils32(uint32_t n)
{
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n ^ (n >> 1);
}

// isolate last set
uint64_t ils64(uint64_t n)
{
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n ^ (n >> 1);
}
#endif