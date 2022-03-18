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
    ubignum_sub(a, b, &out1);
    ubignum_left_shift(a, 5, &out2);


    ubignum_free(out1);
    ubignum_free(out2);
    ubignum_free(b);
    ubignum_free(a);
    return 0;
}