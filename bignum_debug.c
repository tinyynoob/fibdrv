/* Try to debug in user space */
#include "bignum.h"

int main()
{
    ubn *a, *b;
    ubignum_init(&a);
    ubignum_init(&b);

    ubignum_free(b);
    ubignum_free(a);
    return 0;
}