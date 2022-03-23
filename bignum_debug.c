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
    const int target = 1000;
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

    ubn *fast[target + 1];
    for (int i = 0; i < target + 1; i++)
        ubignum_init(&fast[i]);
    ubignum_uint(fast[0], 0);
    ubignum_uint(fast[1], 1);
    int n = 1;
    ubn *tmp;
    ubignum_init(&tmp);
    for (int x = 1 << (32 - __builtin_clz(target) - 1 - 1); x; x = x >> 1) {
        ubignum_mult(fast[n - 1], fast[n - 1], &tmp);
        ubignum_mult(fast[n], fast[n], &fast[2 * n - 1]);
        ubignum_add(tmp, fast[2 * n - 1], &fast[2 * n - 1]);

        ubignum_left_shift(fast[n - 1], 1, &fast[2 * n]);
        ubignum_add(fast[2 * n], fast[n], &fast[2 * n]);
        ubignum_mult(fast[2 * n], fast[n], &fast[2 * n]);
        n *= 2;
        printf("%d is\t", n - 1);
        ubignum_show(fast[n - 1]);
        printf("%d is\t", n);
        ubignum_show(fast[n]);
        if (target & x) {
            ubignum_add(fast[n - 1], fast[n], &fast[n + 1]);
            n++;
            printf("%d is\t", n);
            ubignum_show(fast[n]);
        }
    }

    ubignum_free(tmp);
    for (int i = 0; i < target + 1; i++)
        ubignum_free(fast[i]);
    ubignum_free(out);
    ubignum_free(fib[0]);
    ubignum_free(fib[1]);
    return 0;
}