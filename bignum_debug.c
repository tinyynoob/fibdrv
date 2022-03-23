/* Try to debug in user space */
#include "bignum.h"

int main()
{
    ubn *fib[2], *out;
    ubignum_init(&fib[0]);
    ubignum_init(&fib[1]);
    ubignum_zero(fib[0]);
    ubignum_uint(fib[1], 1);
    ubignum_init(&out);
    const int target = 10000;
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

    ubn *fast[5];
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
            ubignum_copy(fast[2], fast[0]);
            ubignum_copy(fast[1], fast[4]);
        } else {
            ubignum_copy(fast[2], fast[4]);
            ubignum_copy(fast[1], fast[3]);
        }
        // printf("%d is\t", n - 1);
        // ubignum_show(fast[1]);
        // printf("%d is\t", n);
        // ubignum_show(fast[2]);
    }
    printf("%d is\t", n);
    ubignum_show(fast[2]);

    for (int i = 0; i < 5; i++)
        ubignum_free(fast[i]);
    ubignum_free(out);
    ubignum_free(fib[0]);
    ubignum_free(fib[1]);
    return 0;
}