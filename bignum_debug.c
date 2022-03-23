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

    int index = 0, counter = 0, target = 1000;
    for (; counter < target; counter++, index ^= 1) {
        // printf("%d is\t", counter);
        // ubignum_show(fib[index]);
        ubignum_add(fib[0], fib[1], &fib[index]);
    }
    printf("%d is\t", counter);
    ubignum_show(fib[index]);
    printf("%d is\t", counter + 1);
    ubignum_show(fib[index ^ 1]);
    printf("\n");
    ubignum_mult(fib[index], fib[index ^ 1], &out);
    ubignum_show(out);

    ubn *ten;
    ubignum_init(&ten);
    ubignum_uint(ten, 10);

    ubignum_free(ten);
    ubignum_free(out);
    ubignum_free(fib[0]);
    ubignum_free(fib[1]);
    return 0;
}