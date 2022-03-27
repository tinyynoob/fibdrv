/* Try to debug in user space */
#include <time.h>
#include "bignum.h"

#define FIBSE 0
#define FAST 1

int main()
{
    const int target = 100000;
    struct timespec t1, t2;
    unsigned long long consume_time = 0;
    ubn *a = NULL, *b = NULL, *out = NULL;
    ubignum_init(&out);
    ubignum_init(&a);
    ubignum_init(&b);
#if FIBSE
    ubn *fib[2] = {NULL};
    ubignum_init(&fib[0]);
    ubignum_init(&fib[1]);
    ubignum_zero(fib[0]);
    ubignum_uint(fib[1], 1);
    int index = 0, counter = 0;
    for (; counter < target; counter++, index ^= 1) {
        // printf("%d is\t", counter);
        // ubignum_show(fib[index]);
        ubignum_add(fib[0], fib[1], &fib[index]);
    }
    printf("%d is\t", counter);
    ubignum_show(fib[index]);
    // printf("%d is\t", counter + 1);
    // ubignum_show(fib[index ^ 1]);
    // printf("\n");
    // ubignum_mult(fib[index], fib[index ^ 1], &out);
    // ubignum_show(out);
    ubignum_free(fib[0]);
    ubignum_free(fib[1]);
#endif

#if FAST
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ubn *fast[5] = {NULL};
    for (int i = 0; i < 5; i++)
        ubignum_init(&fast[i]);
    ubignum_zero(fast[1]);
    ubignum_uint(fast[2], 1);
    int n = 1;
    for (int currbit = 1 << (32 - __builtin_clz(target) - 1 - 1); currbit;
         currbit >>= 1) {
        /* compute 2n-1 */
        ubignum_mult(fast[1], fast[1], &fast[0]);
        ubignum_mult(fast[2], fast[2], &fast[3]);
        ubignum_add(fast[0], fast[3], &fast[3]);
        /* compute 2n */
        ubignum_left_shift(fast[1], 1, &fast[4]);  // * 2
        ubignum_add(fast[4], fast[2], &fast[4]);
        ubignum_mult(fast[4], fast[2], &fast[4]);
        n *= 2;
        if (target & currbit) {
            ubignum_add(fast[3], fast[4], &fast[0]);
            n++;
            ubignum_swapptr(&fast[2], &fast[0]);
            ubignum_swapptr(&fast[1], &fast[4]);
        } else {
            ubignum_swapptr(&fast[2], &fast[4]);
            ubignum_swapptr(&fast[1], &fast[3]);
        }
        // printf("%d is\t", n - 1);
        // ubignum_show(fast[1]);
        // printf("%d is\t", n);
        // ubignum_show(fast[2]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    consume_time += (unsigned long long) (t2.tv_sec * 1e9 + t2.tv_nsec) -
                    (t1.tv_sec * 1e9 + t1.tv_nsec);
    printf("%d consumes time %llu ns.\n", target, consume_time);
    printf("%d has chunk size %d and its decimal value is\n", target,
           fast[2]->size);
    ubignum_show(fast[2]);
    for (int i = 0; i < 5; i++)
        ubignum_free(fast[i]);
#endif

    ubignum_free(out);
    ubignum_free(a);
    ubignum_free(b);
    return 0;
}