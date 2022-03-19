/* Try to debug in user space */
#include "bignum.h"

int main()
{
    ubn *a, *b;
    ubignum_init(&a);
    ubignum_init(&b);
    a->size = 2;
    b->size = 1;
    a->data[0] = UINT64_MAX;
    a->data[1] = UINT64_MAX;
    b->data[0] = UINT64_MAX;
    ubn *out1, *out2;
    ubignum_init(&out1);
    ubignum_init(&out2);
    ubn *ten;
    ubignum_init(&ten);
    ten->data[0] = 10;
    ten->size = 1;

    ubignum_add(a, b, &out1);
    ubignum_show(out1);

    ubignum_mult(b, b, &out2);
    ubignum_show(out2);



    ubignum_free(out1);
    ubignum_free(out2);
    ubignum_free(b);
    ubignum_free(a);
    return 0;
}