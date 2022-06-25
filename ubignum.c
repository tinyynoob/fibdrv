#include "ubignum.h"
#include "base.h"

#if KSPACE
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>  // memset, memcpy, memmove
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset, memcpy, memmove
#endif

static inline int ubignum_clz(const ubn_t *N);
static void ubignum_mult_add(const ubn_t *restrict a,
                             uint16_t offset,
                             ubn_t *restrict *out);



void ubignum_free(ubn_t *N)
{
    if (!N)
        return;
    FREE(N->data);
    FREE(N);
}

/* set the number to 0, that is, size = 0 */
void ubignum_set_zero(ubn_t *N)
{
    memset(N->data, 0, N->capacity * sizeof(ubn_unit_t));
    N->size = 0;
}

/* assign a u64 number to N */
void ubignum_set_u64(ubn_t *N, const uint64_t n)
{
    ubignum_set_zero(N);
    if (likely(n)) {
        N->size = 1;
        N->data[0] = n;
    }
}

/* count leading zero in the most significant chunk
 * -1 is returned if input is 0
 */
static inline int ubignum_clz(const ubn_t *N)
{
    if (unlikely(ubignum_iszero(N)))
        return -1;
#if CPU64
    return __builtin_clzll(N->data[N->size - 1]);
#else
    return __builtin_clz(N->data[N->size - 1]);
#endif
}

/* Initialize a big number and set its value to 0
 */
ubn_t *ubignum_init(uint16_t capacity)
{
    ubn_t *N = (ubn_t *) MALLOC(sizeof(ubn_t));
    if (unlikely(!N))
        return NULL;
    if (unlikely(
            !(N->data = (ubn_unit_t *) CALLOC(sizeof(ubn_unit_t), capacity)))) {
        FREE(N);
        return NULL;
    }
    N->capacity = capacity;
    N->size = 0;
    return N;
}

/*
 * Adjust capacity of *N.
 * If false is returned, (*N) remains unchanged.
 * Set (*N)->data to NULL if new_capacity is 0.
 */
bool ubignum_recap(ubn_t *N, uint16_t new_capacity)
{
    if (!new_capacity) {
        FREE(N->data);
        N->data = NULL;
        N->capacity = 0;
        N->size = 0;
        return true;
    } else {
        ubn_unit_t *new =
            (ubn_unit_t *) REALLOC(N->data, sizeof(ubn_unit_t) * new_capacity);
        if (unlikely(!new)) {
            FREE(new);
            return false;
        }
        N->data = new;
        if (new_capacity >= N->size)
            memset(N->data + N->size, 0,
                   (new_capacity - N->size) * sizeof(ubn_unit_t));
        N->capacity = new_capacity;
        N->size = MIN(N->size, N->capacity);
        return true;
    }
}

/* Compare two numbers
 * +1 a > b
 *  0 a = b
 * -1 a < b
 */
int ubignum_compare(const ubn_t *a, const ubn_t *b)
{
    if (a->size > b->size)
        return 1;
    else if (a->size < b->size)
        return -1;

    for (int i = (int) a->size - 1; i >= 0; i--) {
        if (a->data[i] > b->data[i])
            return 1;
        else if (a->data[i] < b->data[i])
            return -1;
    }
    return 0;
}

/* left shift a->data by d bit
 */
bool ubignum_left_shift(const ubn_t *a, uint16_t d, ubn_t **out)
{
    if (ubignum_iszero(a)) {
        ubignum_set_zero(*out);
        return true;
    } else if (d == 0) {
        if (a == *out)
            return true;
        if ((*out)->capacity < a->size)
            if (unlikely(!ubignum_recap(*out, a->size)))
                return false;
        memcpy((*out)->data, a->data, a->size * sizeof(ubn_unit_t));
        (*out)->size = a->size;
        return true;
    }

    const uint16_t chunk_shift = d / UBN_UNIT_BIT;
    const uint16_t shift = d % UBN_UNIT_BIT;
    const uint16_t new_size = a->size + chunk_shift + (shift > ubignum_clz(a));
    if ((*out)->capacity < new_size)
        if (unlikely(!ubignum_recap(*out, new_size)))
            return false;

    /* copy data from a to (*out)
     * We have to copy from higher to prevent overlaping @a in case there is
     * pointer aliasing.
     */
    if (shift) {
        int ai = a->size - 1, oi = a->size + chunk_shift - 1;
        if (shift > ubignum_clz(a))
            (*out)->data[oi + 1] = a->data[ai] >> (UBN_UNIT_BIT - shift);
        // merge the lower part from [ai] and the higher part from [ai - 1]
        for (; ai > 0; ai--)
            (*out)->data[oi--] = a->data[ai] << shift |
                                 a->data[ai - 1] >> (UBN_UNIT_BIT - shift);
        (*out)->data[oi] = a->data[ai] << shift;  // ai == 0
    } else {
        memmove((*out)->data + chunk_shift, a->data,
                a->size * sizeof(ubn_unit_t));
    }
    memset((*out)->data, 0,
           sizeof(ubn_unit_t) * chunk_shift);  // set the lowest to 0
    (*out)->size = new_size;
    /* end copy */
    return true;
}



/* (*out) = a + b
 * Aliasing arguments are acceptable.
 * If false is returned, the values of (*out) remains unchanged.
 */
bool ubignum_add(const ubn_t *a, const ubn_t *b, ubn_t **out)
{
    /* compute new size */
    uint16_t new_size;
    if (ubignum_iszero(a) || ubignum_iszero(b))
        new_size = MAX(a->size, b->size);
    else if (a->size >= b->size && ubignum_clz(a) == 0)
        new_size = a->size + 1;
    else if (b->size >= a->size && ubignum_clz(b) == 0)
        new_size = b->size + 1;
    else
        new_size = MAX(a->size, b->size);

    while (unlikely((*out)->capacity < new_size))
        if (unlikely(!ubignum_recap(*out, (*out)->capacity * 2)))
            return false;

    if (*out != a && *out != b)  // if no pointer aliasing
        ubignum_set_zero(*out);
    int i = 0, carry = 0;
    for (i = 0; i < MIN(a->size, b->size); i++)
        carry = ubn_unit_add(a->data[i], b->data[i], carry, &(*out)->data[i]);
    const ubn_t *const remain = (i == a->size) ? b : a;
    for (; i < remain->size; i++)
        carry = ubn_unit_add(remain->data[i], 0, carry, &(*out)->data[i]);
    (*out)->size = remain->size;
    if (unlikely(carry)) {
        (*out)->data[i] = 1;
        (*out)->size++;
    }
    return true;
}

/* *out = a - b
 * Since the system is unsigned, a >= b should be guaranteed to obtain a
 * positive result.
 */
bool ubignum_sub(const ubn_t *a, const ubn_t *b, ubn_t **out)
{
    if (unlikely(ubignum_compare(a, b) < 0)) {  // invalid
        return false;
    } else if (unlikely(ubignum_compare(a, b) == 0)) {
        ubignum_set_zero(*out);
        return true;
    }

    if ((*out)->capacity < a->size) {
        if (unlikely(!ubignum_recap(*out, a->size)))
            return false;
    } else if ((*out)->size > a->size) {
        memset((*out)->data + a->size, 0,
               sizeof(ubn_unit_t) * ((*out)->size - a->size));
    }

    // compute subtraction with adding the two's complement of @b
    int carry = 1;
    for (int i = 0; i < b->size; i++)
        carry = ubn_unit_add(a->data[i], ~b->data[i], carry, &(*out)->data[i]);
    for (int i = b->size; i < a->size; i++)
        carry = ubn_unit_add(a->data[i], UBN_UNIT_MAX, carry, &(*out)->data[i]);
    // the final carry is discarded

    (*out)->size = a->size;
    while ((*out)->data[(*out)->size - 1] == 0)
        (*out)->size--;

    return true;
}

/* dbt->dvd \div 10 = dbt->quo ... dbt->rmd
 * @dbt must be initialized with ubn_dbten_init() before calling this function.
 */
void ubignum_divby_ten(ubn_dbten_t *dbt)
{
    ubignum_set_zero(dbt->quo);
    ubignum_set_zero(dbt->subed);
    if (unlikely(ubignum_iszero(dbt->dvd))) {
        dbt->rmd = 0;
        return;
    }
    uint16_t dvd_sz = dbt->dvd->size;
    while (ubignum_compare(dbt->dvd, dbt->ten) >= 0) {  // if dvd >= 10
        uint16_t shift =
            dbt->dvd->size * UBN_UNIT_BIT - ubignum_clz(dbt->dvd) - 4;
        ubignum_left_shift(dbt->ten, shift, &dbt->subed);
        if (ubignum_compare(dbt->dvd, dbt->subed) < 0)
            ubignum_left_shift(dbt->ten, --shift, &dbt->subed);
        dbt->quo->data[shift / UBN_UNIT_BIT] |= (ubn_unit_t) 1
                                                << (shift % UBN_UNIT_BIT);
        ubignum_sub(dbt->dvd, dbt->subed, &dbt->dvd);
    }
    dbt->quo->size = dvd_sz;
    if (dbt->quo->data[dvd_sz - 1] == 0)
        dbt->quo->size--;
    dbt->rmd = (int) dbt->dvd->data[0];
}

/* (*out) += a << (offset * UBN_UNIT_BIT)
 * The capacity of (*out) must be guaranteed in mult(). No recap is done here!
 */
static void ubignum_mult_add(const ubn_t *restrict a,
                             uint16_t offset,
                             ubn_t *restrict *out)
{
    if (ubignum_iszero(a))
        return;

    int carry = 0, oi = offset;
    for (int ai = 0; ai < a->size; ai++, oi++)
        carry = ubn_unit_add((*out)->data[oi], a->data[ai], carry,
                             &(*out)->data[oi]);
    for (; carry; oi++)
        carry = ubn_unit_add((*out)->data[oi], 0, carry, &(*out)->data[oi]);

    (*out)->size = MAX(a->size + offset, (*out)->size);
    for (int i = (*out)->size - 1; !(*out)->data[i]; i--)
        (*out)->size--;
    return;
}


/* *out = a * b
 */
bool ubignum_mult(const ubn_t *a, const ubn_t *b, ubn_t **out)
{
    if (ubignum_iszero(a) || ubignum_iszero(b)) {
        ubignum_set_zero(*out);
        return true;
    }
    /* keep mcand longer than mplier */
    const ubn_t *mcand = a->size > b->size ? a : b;
    const ubn_t *mplier = a->size > b->size ? b : a;
    ubn_t *ans = ubignum_init(mcand->size + mplier->size);
    if (unlikely(!ans))
        return false;
    ubn_t *pprod = ubignum_init(mcand->size + 1);  // partial product
    if (unlikely(!pprod))
        goto cleanup_ans;
    pprod->size = mcand->size + 1;

    /* Let a, b, c, d, e, f be chunks.
     * Suppose that we are going to mult (a, b, c, d) and (e, f).
     * The outer loop goes from f to e and add the partial products.
     */
    for (int i = 0; i < mplier->size; i++) {
        /* The inner loop do (a, b, c, d)*(e) and (a, b, c, d)*(f)
         *          a   b   c   d
         *    *                 f
         * ---------------------------
         *                 df  df       in the form of (high, low)
         *             cf  cf
         *         bf  bf
         *  +  af  af
         * ---------------------------
         *            pprod
         */
        int carry = 0;
        ubn_unit_t overlap = 0;
        for (int j = 0; j < mcand->size; j++) {
            ubn_unit_t low, high;
            ubn_unit_mult(mcand->data[j], mplier->data[i], high, low);
            carry = ubn_unit_add(low, overlap, carry, &pprod->data[j]);
            overlap = high;  // update overlap
        }
        pprod->data[mcand->size] =
            overlap + carry;  // no carry out would be generated

        ubignum_mult_add(pprod, i, &ans);
    }
    ubignum_free(pprod);
    ubignum_free(*out);
    *out = ans;
    return true;
cleanup_ans:
    ubignum_free(ans);
    return false;
}

/* (*out) = a * a */
bool ubignum_square(const ubn_t *a, ubn_t **out)
{
    if (ubignum_iszero(a)) {
        ubignum_set_zero(*out);
        return true;
    }
    ubn_t *ans = ubignum_init(a->size * 2);
    if (unlikely(!ans))
        return false;
    ubn_t *dprod = ubignum_init(a->size + 1 + 1);
    if (unlikely(!dprod))
        goto cleanup_ans;

    /*                  a   b   c   d
     *     *            a   b   c   d
     *    ------------------------------
     *                 ad  bd  cd  dd       @dprod is (ad bd cd 0)
     *             ac  bc  cc  cd                     (ac bc  0 0)
     *         ab  bb  bc  bd                         (ab  0  0 0)
     *     aa  ab  ac  ad
     *
     * Don't be messed by the sketch.
     * The entries usually have overlap, since multiplication doubles the
     * length. For exmaple, the dd occupies the two rightmost chunks.
     */
    // compute aa, bb, cc, dd parts
    for (int i = 0; i < a->size; i++)
        ubn_unit_mult(a->data[i], a->data[i], ans->data[2 * i + 1],
                      ans->data[2 * i]);
    ans->size = ans->data[a->size * 2 - 1] ? a->size * 2 : a->size * 2 - 1;
    // compute multiplications of different chunks
    for (int i = 0; i < a->size - 1; i++) {
        int carry = 0;
        ubn_unit_t overlap = 0;
        ubignum_set_zero(dprod);
        for (int j = i + 1; j < a->size; j++) {
            ubn_unit_t low, high;
            ubn_unit_mult(a->data[j], a->data[i], high, low);
            carry = ubn_unit_add(low, overlap, carry, &dprod->data[j]);
            overlap = high;  // update overlap
        }
        dprod->data[a->size] =
            overlap + carry;  // no carry-out would be generated
        dprod->size = dprod->data[a->size] ? a->size + 1 : a->size;
        ubignum_left_shift(dprod, 1, &dprod);  // * 2
        ubignum_mult_add(dprod, i, &ans);
    }
    ubignum_free(dprod);
    ubignum_free(*out);
    *out = ans;
    return true;
cleanup_ans:
    ubignum_free(ans);
    return false;
}

/* convert the unsigned big number to ascii string */
char *ubignum_2decimal(const ubn_t *N)
{
    if (ubignum_iszero(N)) {
        char *ans = (char *) CALLOC(sizeof(char), 2);
        if (unlikely(!ans))
            return NULL;
        ans[0] = '0';
        ans[1] = '\0';
        return ans;
    }
    ubn_dbten_t *dbt = ubn_dbten_init(N);
    if (unlikely(!dbt))
        return NULL;
    /* Let n be the number.
     * digit = 1 + log_10(n) = 1 + \frac{log_2(n)}{log_2(10)}
     * log_2(10) \approx 3.3219 \approx 7/2,  we simply choose 3
     */
    uint32_t digit = (UBN_UNIT_BIT * N->size / 3) + 1;
    char *ans = (char *) CALLOC(sizeof(char), digit);
    if (unlikely(!ans)) {
        ubn_dbten_free(dbt);
        return NULL;
    }

    /* convert 2-base to 10-base */
    int index = 0;
    while (dbt->dvd->size) {
        ubignum_divby_ten(dbt);
        ans[index++] = dbt->rmd | '0';  // digit to ascii
        ubignum_swapptr(&dbt->dvd, &dbt->quo);
    }
    /* reverse the string */
    index--;
    for (int i = 0; i < index; i++, index--) {
        char tmp = ans[i];
        ans[i] = ans[index];
        ans[index] = tmp;
    }
    ubn_dbten_free(dbt);
    return ans;
}

/* Allocate space for members and copy dividend->data to ()->dvd->data.
 */
ubn_dbten_t *ubn_dbten_init(const ubn_t *dividend)
{
    ubn_dbten_t *dbt = (ubn_dbten_t *) MALLOC(sizeof(ubn_dbten_t));
    if (unlikely(!dbt))
        return NULL;
    if (unlikely(!(dbt->dvd = ubignum_init(dividend->size))))
        goto cleanup_struct;
    if (unlikely(!(dbt->quo = ubignum_init(dividend->size))))
        goto cleanup_dvd;
    if (unlikely(!(dbt->subed = ubignum_init(dividend->size + 1))))
        goto cleanup_quo;
    if (unlikely(!(dbt->ten = ubignum_init(1))))
        goto cleanup_subed;
    ubignum_set_u64(dbt->ten, 10);
    dbt->dvd->size = dividend->size;
    memcpy(dbt->dvd->data, dividend->data, sizeof(ubn_unit_t) * dividend->size);
    return dbt;
cleanup_subed:
    ubignum_free(dbt->subed);
cleanup_quo:
    ubignum_free(dbt->quo);
cleanup_dvd:
    ubignum_free(dbt->dvd);
cleanup_struct:
    FREE(dbt);
    return NULL;
}

void ubn_dbten_free(ubn_dbten_t *dbt)
{
    if (!dbt)
        return;
    ubignum_free(dbt->ten);
    ubignum_free(dbt->subed);
    ubignum_free(dbt->quo);
    ubignum_free(dbt->dvd);
    FREE(dbt);
}
