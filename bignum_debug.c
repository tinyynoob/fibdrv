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

    ubn *fib1, *fib2;
    ubignum_init(&fib1);
    ubignum_init(&fib2);
    ubignum_zero(fib1);
    fib2->size = 1;
    fib2->data[0] = 1;
    for (int i = 2; i <= 1000; i++) {
        if (!ubignum_add(fib1, fib2, &fib1))
            break;
        printf("%d is\t", i++);
        ubignum_show(fib1);
        if (!ubignum_add(fib1, fib2, &fib2))
            break;
        printf("%d is\t", i);
        ubignum_show(fib2);
    }

    ubn *ten;
    ubignum_init(&ten);
    ten->data[0] = 10;
    ten->size = 1;

    ubignum_free(ten);
    ubignum_free(fib1);
    ubignum_free(fib2);
    ubignum_free(b);
    ubignum_free(a);
    return 0;
}