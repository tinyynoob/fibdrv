#include "ubignum.h"
#include "base.h"

#if KSPACE
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>  // memset
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
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
    ubn_t *N;
    if (unlikely(!(N = (ubn_t *) MALLOC(sizeof(ubn_unit_t)))))
        goto struct_aloc_failed;
    if (unlikely(
            !(N->data = (ubn_unit_t *) CALLOC(sizeof(ubn_unit_t), capacity))))
        goto data_aloc_failed;
    N->capacity = capacity;
    N->size = 0;
    return N;

data_aloc_failed:
    FREE(N);
struct_aloc_failed:
    return NULL;
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
        if (unlikely(!new))
            return false;
        N->data = new;
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

/* left shift a->data by d bit */
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
    const uint16_t new_size = a->size + chunk_shift + 1;
    a->size + chunk_shift + 1;
    if ((*out)->capacity < new_size)
        if (unlikely(!ubignum_recap(*out, new_size)))
            return false;

    int ai = new_size - chunk_shift - 1;  // note a->size may be changed due to
                                          // aliasing, should not use a->size
    int oi = new_size - 1;
    /* copy data from a to (*out) */
    if (shift) {
        (*out)->data[oi--] = a->data[ai - 1] >> (UBN_UNIT_BIT - shift);
        for (ai--; ai > 0; ai--)
            (*out)->data[oi--] = a->data[ai] << shift |
                                 a->data[ai - 1] >> (UBN_UNIT_BIT - shift);
        (*out)->data[oi--] = a->data[ai] << shift;  // ai is now 0
    } else {
        while (ai >= 0)
            (*out)->data[oi--] = a->data[ai--];
    }
    /* end copy */
    while (oi >= 0)  // set remaining part of (*out) to 0
        (*out)->data[oi--] = 0;

    (*out)->size = new_size;
    if ((*out)->data[(*out)->size - 1] == 0)  // if MS chunk is 0
        (*out)->size--;
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
        if (!ubignum_recap(*out, (*out)->capacity * 2))
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
 * Since the system is unsigned, a >= b should be guaranteed to get a positive
 * result.
 */
bool ubignum_sub(const ubn_t *a, const ubn_t *b, ubn_t **out)
{
    if (unlikely(ubignum_compare(a, b) < 0))
        return false;
    /* ones' complement of b */
    ubn_t *cmt =
        ubignum_init(a->size);  // maybe there is way without memory allocation?
    if (unlikely(!cmt))
        return false;
    for (int i = 0; i < b->size; i++)
        cmt->data[i] = ~b->data[i];
    for (int i = b->size; i < a->size; i++)
        cmt->data[i] = UBN_UNIT_MAX;

    int carry = 1;                     // to become two's complement
    for (int i = 0; i < a->size; i++)  // compute result and store in cmt
        carry = ubn_unit_add(a->data[i], cmt->data[i], carry, &cmt->data[i]);
    // the final carry is discarded
    cmt->size = a->size;
    for (int i = a->size - 1; i >= 0; i--)
        if (!cmt->data[i])
            cmt->size--;
    ubignum_free(*out);
    *out = cmt;
    return true;
}

/* a / 10 = (**quo)...(*rmd)
 */
bool ubignum_divby_ten(const ubn_t *a, ubn_t **quo, int *rmd)
{
    if (!*quo || !rmd)
        return false;
    if (unlikely(ubignum_iszero(a))) {
        ubignum_set_zero(*quo);
        *rmd = 0;
        return true;
    }
    ubn_t *ans = NULL, *dvd = NULL, *suber = NULL, *ten = NULL;
    if (!(ans = ubignum_init(UBN_DEFAULT_CAPACITY)))
        return false;
    if (!ubignum_recap(ans, a->size))
        goto cleanup_ans;
    if (!(dvd = ubignum_init(UBN_DEFAULT_CAPACITY)))
        goto cleanup_ans;
    if (!ubignum_recap(dvd, a->size))
        goto cleanup_dvd;
    if (!(suber = ubignum_init(UBN_DEFAULT_CAPACITY)))
        goto cleanup_dvd;
    if (!ubignum_recap(suber, dvd->capacity + 1))
        goto cleanup_suber;
    if (!(ten = ubignum_init(1)))
        goto cleanup_suber;
    ubignum_set_u64(ten, 10);

    for (int i = 0; i < a->size; i++)
        dvd->data[i] = a->data[i];
    dvd->size = a->size;
    while (ubignum_compare(dvd, ten) >= 0) {  // if dvd >= 10
        uint16_t shift = dvd->size * UBN_UNIT_BIT - ubignum_clz(dvd) - 4;
        ubignum_left_shift(ten, shift, &suber);
        if (ubignum_compare(dvd, suber) < 0)
            ubignum_left_shift(ten, --shift, &suber);
        ans->data[shift / UBN_UNIT_BIT] |= (ubn_unit_t) 1
                                           << (shift % UBN_UNIT_BIT);
        ubignum_sub(dvd, suber, &dvd);
    }
    ans->size = a->size;
    if (ans->data[a->size - 1] == 0)
        ans->size--;
    *rmd = (int) dvd->data[0];
    ubignum_free(ten);
    ubignum_free(suber);
    ubignum_free(dvd);
    ubignum_free(*quo);
    *quo = ans;
    return true;
cleanup_suber:
    ubignum_free(suber);
cleanup_dvd:
    ubignum_free(dvd);
cleanup_ans:
    ubignum_free(ans);
    return false;
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

    int carry = 0, oi;
    for (int ai = 0, oi = offset; ai < a->size; ai++, oi++)
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
            ubn_unit_mult(mcand->data[j], mplier->data[i], high, low) carry =
                ubn_unit_add(low, overlap, carry, &pprod->data[j]);
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
    ubn_t *group = ubignum_init(a->size + 1 + 1);
    if (unlikely(!group))
        goto cleanup_ans;

    /*                  a   b   c   d
     *     *            a   b   c   d
     *    ------------------------------
     *                 ad  bd  cd  dd
     *             ac  bc  cc  cd
     *         ab  bb  bc  bd
     *     aa  ab  ac  cd
     *
     */
    for (int i = 0; i < a->size; i++)
        ubn_unit_mult(a->data[i], a->data[i], ans->data[2 * i + 1],
                      ans->data[2 * i]);
    ans->size = ans->data[a->size * 2 - 1] ? a->size * 2 : a->size * 2 - 1;
    for (int i = 0; i < a->size - 1; i++) {
        int carry = 0;
        ubn_unit_t overlap = 0;
        ubignum_set_zero(group);
        for (int j = i + 1; j < a->size; j++) {
            ubn_unit_t low, high;
            ubn_unit_mult(a->data[j], a->data[i], high, low);
            carry = ubn_unit_add(low, overlap, carry, &group->data[j]);
            overlap = high;  // update overlap
        }
        group->data[a->size] =
            overlap + carry;  // no carry out would be generated
        group->size = a->size + 1;
        ubignum_left_shift(group, 1, &group);
        if (group->data[a->size + 1])
            group->size++;
        ubignum_mult_add(group, i, &ans);
    }
    ubignum_free(group);
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
        if (!ans)
            return NULL;
        ans[0] = '0';
        ans[1] = '\0';
        return ans;
    }
    ubn_t *dvd = ubignum_init(N->size);
    if (unlikely(!dvd))
        return NULL;
    memcpy(dvd->data, N->data, N->size * sizeof(ubn_unit_t));
    dvd->size = N->size;

    /* Let n be the number.
     * digit = 1 + log_10(n) = 1 + \frac{log_2(n)}{log_2(10)}
     * log_2(10) \approx 3.3219 \approx 7/2,  we simply choose 3
     */
    uint32_t digit = (UBN_UNIT_BIT * N->size / 3) + 1;
    char *ans = (char *) CALLOC(sizeof(char), digit);
    if (!ans)
        goto cleanup_dvd;
    /* convert 2-base to 10-base */
    int index = 0;
    while (dvd->size) {
        int rmd;
        if (unlikely(!ubignum_divby_ten(dvd, &dvd, &rmd)))
            goto cleanup_ans;
        ans[index++] = rmd | '0';
    }
    int len = index;
    /* reverse string */
    index = len - 1;
    for (int i = 0; i < index; i++, index--) {
        char tmp = ans[i];
        ans[i] = ans[index];
        ans[index] = tmp;
    }
    ubignum_free(dvd);
    return ans;
cleanup_ans:
    FREE(ans);
cleanup_dvd:
    ubignum_free(dvd);
    return NULL;
}
