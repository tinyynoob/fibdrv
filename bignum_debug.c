/* Try to debug in user space */
#include "bignum.h"

int main()
{
    ubn *a, *b;
    ubignum_init(&a);
    ubignum_init(&b);
    ubignum_zero(a);
    b->size = 1;
    b->data[0] = 1;

    ubn *fib[2];
    ubignum_init(&fib[0]);
    ubignum_init(&fib[1]);
    ubignum_zero(fib[0]);
    ubignum_uint(fib[1], 1);

    int index = 0, counter = 0, target = 8;
    for (; counter < target; counter++, index ^= 1) {
        ubignum_add(fib[0], fib[1], &fib[index]);
    }
    printf("index = %d, index^1 = %d\n", index, index ^ 1);
    printf("%d is\t", counter);
    ubignum_show(fib[index]);

    ubn *ten;
    ubignum_init(&ten);
    ubignum_uint(ten, 10);

    ubignum_free(ten);
    ubignum_free(fib[0]);
    ubignum_free(fib[1]);
    ubignum_free(b);
    ubignum_free(a);
    return 0;
}