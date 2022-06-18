#ifndef __UBIGNUM_H
#define __UBIGNUM_H

#define UBN_DEFAULT_CAPACITY 2

#include "base.h"

#if KSPACE
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#else
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#endif


#ifndef MAX
#define MAX(a, b)          \
    ({                     \
        typeof(a) _a = a;  \
        typeof(b) _b = b;  \
        _a > _b ? _a : _b; \
    })
#endif

#ifndef MIN
#define MIN(a, b)          \
    ({                     \
        typeof(a) _a = a;  \
        typeof(b) _b = b;  \
        _a < _b ? _a : _b; \
    })
#endif

/* unsigned big number
 * @data: MS:[size-1], LS:[0]
 * @size: used size in @data divided by sizeof(ubn_unit_t)
 * @capacity: allocated size of @data
 */
typedef struct {
    ubn_unit_t *data;
    uint16_t size;
    uint16_t capacity;
} ubn_t;

ubn_t *ubignum_init(uint16_t capacity);
bool ubignum_recap(ubn_t *N, uint16_t new_capacity);
void ubignum_free(ubn_t *N);
static inline void ubignum_swapptr(ubn_t **a, ubn_t **b);
static inline bool ubignum_iszero(const ubn_t *N);
void ubignum_set_zero(ubn_t *N);
void ubignum_set_u64(ubn_t *N, const uint64_t n);
int ubignum_compare(const ubn_t *a, const ubn_t *b);
bool ubignum_left_shift(const ubn_t *a, uint16_t d, ubn_t **out);
bool ubignum_add(const ubn_t *a, const ubn_t *b, ubn_t **out);
bool ubignum_sub(const ubn_t *a, const ubn_t *b, ubn_t **out);
bool ubignum_divby_ten(const ubn_t *a, ubn_t **quo, int *rmd);
bool ubignum_mult(const ubn_t *a, const ubn_t *b, ubn_t **out);
bool ubignum_square(const ubn_t *a, ubn_t **out);
char *ubignum_2decimal(const ubn_t *N);

/* return carry-out */
static inline int ubn_unit_add(const ubn_unit_t a,
                               const ubn_unit_t b,
                               const int cin,
                               ubn_unit_t *sum)
{
    int cout = 0;
#if CPU64
    cout = __builtin_uaddll_overflow(a, cin, sum);
    cout |= __builtin_uaddll_overflow(*sum, b, sum);
#else
    cout = __builtin_uadd_overflow(a, cin, sum);
    cout |= __builtin_uadd_overflow(*sum, b, sum);
#endif
    return cout;
}
/* swap two ubn_t */
static inline void ubignum_swapptr(ubn_t **a, ubn_t **b)
{
    ubn_t *tmp = *a;
    *a = *b;
    *b = tmp;
}

/* check if N is zero */
static inline bool ubignum_iszero(const ubn_t *N)
{
    return N->capacity && !N->size;
}

#endif