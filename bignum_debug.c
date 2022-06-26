/* Try to debug in user space */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "base.h"
#include "ubignum.h"

#define FIBSE 1
#define FAST 1
#define COMPARE 1

static inline void ubignum_show(const ubn_t *N)
{
    char *s = ubignum_2decimal(N);
    printf("%u\n", (unsigned) strlen(s));
    free(s);
}

static ubn_t *fib_sequence(uint32_t k);
static ubn_t *fib_fast(uint32_t k);

int main()
{
    const int target = 1000;
    ubn_t *a = NULL, *b = NULL, *out = NULL;
    out = ubignum_init(UBN_DEFAULT_CAPACITY);
    a = ubignum_init(UBN_DEFAULT_CAPACITY);
    b = ubignum_init(UBN_DEFAULT_CAPACITY);
#if FIBSE
    ubn_t *fib[2] = {NULL, NULL};
    fib[0] = ubignum_init(UBN_DEFAULT_CAPACITY);
    fib[1] = ubignum_init(UBN_DEFAULT_CAPACITY);
    ubignum_set_zero(fib[0]);
    ubignum_set_u64(fib[1], 1);
    for (int i = 2; i <= target; i++) {
        printf("%d is\t", i);
        ubignum_add(fib[0], fib[1], &fib[i & 1]);
        ubignum_show(fib[i & 1]);
    }
    ubignum_free(fib[0]);
    ubignum_free(fib[1]);
#endif

#if FAST
    for (int i = 0; i <= target; i++) {
        ubn_t *v = fib_fast(i);
        printf("%d is\t", i);
        ubignum_show(v);
        ubignum_free(v);
    }
#endif

#if COMPARE
    for (int i = 0; i <= target; i++) {
        ubn_t *s = fib_sequence(i);
        ubn_t *f = fib_fast(i);
        if (ubignum_compare(s, f)) {
            printf("i = %d\n", i);
            ubignum_show(s);
            ubignum_show(f);
        }
        ubignum_free(s);
        ubignum_free(f);
    }
#endif

    ubignum_free(out);
    ubignum_free(a);
    ubignum_free(b);
    return 0;
}

static ubn_t *fib_fast(uint32_t k)
{
    ubn_t *fast[5];
    if (k == 0) {
        fast[2] = ubignum_init(UBN_DEFAULT_CAPACITY);
        ubignum_set_zero(fast[2]);
        goto end;
    } else if (k == 1) {
        fast[2] = ubignum_init(UBN_DEFAULT_CAPACITY);
        ubignum_set_u64(fast[2], 1);
        goto end;
    }

    for (int i = 0; i < 5; i++)
        fast[i] = ubignum_init(UBN_DEFAULT_CAPACITY);
    ubignum_set_zero(fast[1]);
    ubignum_set_u64(fast[2], 1);
    int n = 1;
    for (int currbit = 1 << (32 - __builtin_clzll(k) - 1 - 1); currbit;
         currbit = currbit >> 1) {
        /* compute 2n-1 */
        ubignum_square(fast[1], &fast[0]);
        ubignum_square(fast[2], &fast[3]);
        // ubignum_mult(fast[1], fast[1], &fast[0]);
        // ubignum_mult(fast[2], fast[2], &fast[3]);
        ubignum_add(fast[0], fast[3], &fast[3]);
        /* compute 2n */
        ubignum_left_shift(fast[1], 1, &fast[4]);
        ubignum_add(fast[4], fast[2], &fast[4]);
        ubignum_mult(fast[4], fast[2], &fast[4]);
        n *= 2;
        if (k & currbit) {
            ubignum_add(fast[3], fast[4], &fast[0]);
            n++;
            ubignum_swapptr(&fast[2], &fast[0]);
            ubignum_swapptr(&fast[1], &fast[4]);
        } else {
            ubignum_swapptr(&fast[2], &fast[4]);
            ubignum_swapptr(&fast[1], &fast[3]);
        }
    }
    ubignum_free(fast[0]);
    ubignum_free(fast[1]);
    ubignum_free(fast[3]);
    ubignum_free(fast[4]);
end:;
    return fast[2];
}

static ubn_t *fib_sequence(uint32_t k)
{
    ubn_t *fib[2] = {NULL, NULL};
    fib[0] = ubignum_init(UBN_DEFAULT_CAPACITY);
    ubignum_set_zero(fib[0]);
    fib[1] = ubignum_init(UBN_DEFAULT_CAPACITY);
    ubignum_set_u64(fib[1], 1);
    for (int i = 2; i <= k; i++)
        ubignum_add(fib[0], fib[1], &fib[i & 1]);
    ubignum_free(fib[(k & 1) ^ 1]);
    return fib[k & 1];
}
