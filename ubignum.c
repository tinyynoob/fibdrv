#include "ubignum.h"
#include "base.h"

#if KSPACE
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>  // memset, memcpy, memmove
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset, memcpy, memmove
#include "list.h"
#endif

typedef struct {
    ubn_div_t *dit;
    char str[UBN_SUPERTEN_EXP + 1];
    struct list_head list;
} ubn_2dec_l2_t;

static char *ubignum_2decimal_large(const ubn_t *N);
static char *ubignum_2decimal_medium(const ubn_t *N);
static void ubignum_2decimal_l1(ubn_div_t *const dit, char *const str);
static void ubignum_2decimal_l2(ubn_2dec_l2_t *const node);
static inline int ubignum_clz(const ubn_t *N);
static void ubignum_mult_add(const ubn_t *restrict a,
                             uint32_t offset,
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
ubn_t *ubignum_init(uint32_t capacity)
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
bool ubignum_recap(ubn_t *N, uint32_t new_capacity)
{
    if (unlikely(!new_capacity)) {
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
bool ubignum_left_shift(ubn_t *a, uint32_t d, ubn_t **out)
{
    if (ubignum_iszero(a)) {
        ubignum_set_zero(*out);
        return true;
    } else if (unlikely(d == 0)) {
        if (a == *out)
            return true;
        if ((*out)->capacity < a->size)
            if (unlikely(!ubignum_recap(*out, a->size)))
                return false;
        memcpy((*out)->data, a->data, a->size * sizeof(ubn_unit_t));
        (*out)->size = a->size;
        return true;
    }

    const uint32_t chunk_shift = d / UBN_UNIT_BIT;
    const uint32_t shift = d % UBN_UNIT_BIT;
    const uint32_t new_size = a->size + chunk_shift + (shift > ubignum_clz(a));
    if (unlikely((*out)->capacity < new_size))
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
           sizeof(ubn_unit_t) * chunk_shift);  // set the remaining to 0
    (*out)->size = new_size;
    /* end copy */
    return true;
}

/* (*out) = a + b
 * Aliasing arguments are acceptable.
 * If false is returned, the values of (*out) remains unchanged.
 */
bool ubignum_add(ubn_t *a, ubn_t *b, ubn_t **out)
{
    /* compute new size */
    uint32_t new_size;
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
    ubn_t *const remain = (i == a->size) ? b : a;
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
 * non-negative result.
 */
bool ubignum_sub(ubn_t *a, ubn_t *b, ubn_t **out)
{
    if (unlikely(ubignum_compare(a, b) < 0)) {  // invalid
        return false;
    } else if (unlikely(ubignum_compare(a, b) == 0)) {
        ubignum_set_zero(*out);
        return true;
    }

    if (unlikely((*out)->capacity < a->size)) {
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

/* Division for unsigned big numbers
 * @dit must be initialized with ubn_div_init() before calling this
 * function.
 */
bool ubignum_div(ubn_div_t *dit, const ubn_t *restrict dvs)
{
    ubignum_set_zero(dit->quo);
    ubignum_set_zero(dit->subed);
    if (unlikely(ubignum_iszero(dvs))) {  // divided by zero
        return false;
    } else if (unlikely(dit->dvd->size < dvs->size)) {
        return true;
    } else if (unlikely(dit->quo->capacity < dit->dvd->size - dvs->size + 1)) {
        if (unlikely(!ubignum_recap(dit->quo, dit->dvd->size - dvs->size + 1)))
            return false;
    }

    dit->quo->size = dit->dvd->size - dvs->size + 1;
    while (likely(ubignum_compare(dit->dvd, dvs) >= 0)) {  // if dvd >= dvs
        uint32_t shift =
            (dit->dvd->size * UBN_UNIT_BIT - ubignum_clz(dit->dvd)) -
            (dvs->size * UBN_UNIT_BIT - ubignum_clz(dvs));
        ubignum_left_shift((ubn_t *) dvs, shift, &dit->subed);
        if (ubignum_compare(dit->dvd, dit->subed) < 0)  // if dvd < subed
            ubignum_left_shift((ubn_t *) dvs, --shift, &dit->subed);
        ubignum_sub(dit->dvd, dit->subed, &dit->dvd);
        dit->quo->data[shift / UBN_UNIT_BIT] |= (ubn_unit_t) 1
                                                << (shift % UBN_UNIT_BIT);
    }
    if (dit->quo->data[dit->quo->size - 1] == 0)
        dit->quo->size--;
    /* update dit->dvd->size */
    while (dit->dvd->data[dit->dvd->size - 1] == 0)
        dit->dvd->size--;
    return true;
}

/* dit->dvd \div UBN_LTEN = dit->quo ... dit->sh_rmd
 */
void ubignum_divby_Lten(ubn_div_t *const dit)
{
    ubignum_set_zero(dit->quo);
    if (unlikely(ubignum_iszero(dit->dvd))) {
        dit->sh_rmd = 0;
        return;
    }
    const uint32_t dvd_ori_sz = dit->dvd->size;

    while (likely(dit->dvd->size >= 2)) {
        const uint32_t clz = ubignum_clz(dit->dvd);
        ubn_extunit_t m = (ubn_extunit_t) dit->dvd->data[dit->dvd->size - 1]
                              << UBN_UNIT_BIT |
                          dit->dvd->data[dit->dvd->size - 2];
        ubn_extunit_t subed = (ubn_extunit_t) UBN_LTEN
                              << (2 * UBN_UNIT_BIT - UBN_LTEN_BIT - clz);
        const uint32_t quo_shift =
            dit->dvd->size * UBN_UNIT_BIT - UBN_LTEN_BIT - clz - !(m >= subed);
        subed = subed >> !(m >= subed);
        m -= subed;
        dit->dvd->data[dit->dvd->size - 1] = m >> UBN_UNIT_BIT;
        dit->dvd->data[dit->dvd->size - 2] = (ubn_unit_t) m;
        while (likely(dit->dvd->size) &&
               dit->dvd->data[dit->dvd->size - 1] == 0)
            dit->dvd->size--;

        dit->quo->data[quo_shift / UBN_UNIT_BIT] |=
            (ubn_unit_t) 1 << (quo_shift % UBN_UNIT_BIT);
    }
    while (likely(dit->dvd->size) &&
           likely(dit->dvd->data[0] >=
                  UBN_LTEN)) {  // if dvd->size == 1 && dvd >= UBN_LTEN
        const uint32_t clz = ubignum_clz(dit->dvd);
        ubn_unit_t subed = (ubn_unit_t) UBN_LTEN
                           << (UBN_UNIT_BIT - UBN_LTEN_BIT - clz);
        const uint32_t quo_shift =
            UBN_UNIT_BIT - UBN_LTEN_BIT - clz - !(dit->dvd->data[0] >= subed);
        subed = subed >> !(dit->dvd->data[0] >= subed);
        dit->dvd->data[0] -= subed;
        if (unlikely(dit->dvd->data[0] == 0))
            dit->dvd->size = 0;
        dit->quo->data[0] |= (ubn_unit_t) 1 << quo_shift;
    }

    dit->quo->size =
        (dit->quo->data[dvd_ori_sz - 1] == 0) ? dvd_ori_sz - 1 : dvd_ori_sz;
    dit->sh_rmd = dit->dvd->data[0];  // \in [0, UBN_LTEN - 1]
}

/* (*out) += a << (offset * UBN_UNIT_BIT)
 * The capacity of (*out) must be guaranteed in mult(). No recap is done here!
 */
static void ubignum_mult_add(const ubn_t *restrict a,
                             uint32_t offset,
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
bool ubignum_mult(ubn_t *a, ubn_t *b, ubn_t **out)
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
bool ubignum_square(ubn_t *a, ubn_t **out)
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

/* convert the unsigned big number to ascii string
 */
char *ubignum_2decimal(const ubn_t *N)
{
    if (ubignum_iszero(N)) {
        char *ans = (char *) MALLOC(sizeof(char) * 2);
        if (unlikely(!ans))
            return NULL;
        ans[0] = '0';
        ans[1] = '\0';
        return ans;
    } else if (N->size == 1) {
#if CPU64
        char *ans = (char *) MALLOC(sizeof(char) * (20 + 1));
#else
        char *ans = (char *) MALLOC(sizeof(char) * (10 + 1));
#endif
        if (unlikely(!ans))
            return NULL;
#if CPU64
        snprintf(ans, 20 + 1, "%llu", (unsigned long long) N->data[0]);
#else
        snprintf(ans, 10 + 1, "%u", (unsigned) N->data[0]);
#endif
        return ans;
    }

    const uint32_t threshold = UBN_SUPERTEN_CHUNK * 2;
    if (N->size >= threshold)
        return ubignum_2decimal_large(N);
    return ubignum_2decimal_medium(N);
}

static char *ubignum_2decimal_large(const ubn_t *N)
{
    ubn_div_t *dit = ubn_div_init(N, (uint32_t) UBN_SUPERTEN_CHUNK);
    /* obtain SUPERTEN from LTEN */
    ubn_t *super_ten = ubignum_init(1);
    ubignum_set_u64(super_ten, UBN_LTEN);
    for (uint32_t e = UBN_LTEN_EXP; e < UBN_SUPERTEN_EXP; e <<= 1)
        ubignum_square(super_ten, &super_ten);
    /* divided by super_ten, which is 10 ** 1024 */
    struct list_head *h = (struct list_head *) MALLOC(sizeof(struct list_head));
    INIT_LIST_HEAD(h);
    uint32_t list_len = 0;
    do {
        ubignum_div(dit, super_ten);
        ubn_2dec_l2_t *l2node = (ubn_2dec_l2_t *) MALLOC(sizeof(ubn_2dec_l2_t));
        l2node->dit = ubn_div_init(dit->dvd, 0);  // assigns remainder
        l2node->str[UBN_SUPERTEN_EXP] = '\0';
        list_add(&l2node->list, h);
        list_len++;
        ubignum_swapptr(&dit->dvd, &dit->quo);
    } while (likely(!ubignum_iszero(dit->dvd)));
    ubn_div_free(dit);
    dit = NULL;
    ubignum_free(super_ten);
    super_ten = NULL;
    char *ans =
        (char *) MALLOC(sizeof(char) * (list_len * UBN_SUPERTEN_EXP + 1));
    struct list_head *it;
    uint32_t index = 0;
    bool start = false;
    list_for_each (it, h) {
        ubn_2dec_l2_t *const l2node = list_entry(it, ubn_2dec_l2_t, list);
        ubignum_2decimal_l2(l2node);
        ubn_div_free(l2node->dit);
        l2node->dit = NULL;
        if (unlikely(!start)) {
            uint32_t stridx = 0;
            while (stridx < UBN_SUPERTEN_EXP &&
                   likely(l2node->str[stridx] == '0'))
                stridx++;
            // stridx achieves end or stridx != '0'
            if (l2node->str[stridx] != '\0') {
                start = true;
                memcpy(ans, l2node->str + stridx,
                       sizeof(char) * (UBN_SUPERTEN_EXP - stridx));
                index += UBN_SUPERTEN_EXP - stridx;
            }
        } else {
            memcpy(ans + index, l2node->str, sizeof(char) * UBN_SUPERTEN_EXP);
            index += UBN_SUPERTEN_EXP;
        }
    }
    ans[index] = '\0';
    while (!list_empty(h)) {
        ubn_2dec_l2_t *l2node = list_entry(h->next, ubn_2dec_l2_t, list);
        FREE(l2node);
        list_del(h->next);
    }
    FREE(h);
    h = NULL;
    return ans;
    // TODO: aloc fail handle
}

static char *ubignum_2decimal_medium(const ubn_t *N)
{
    /* Estimate the digit needed.
     * Let n be the number.
     * digit = 1 + log_10(n) = 1 + \frac{log_2(n)}{log_2(10)}
     * log_2(10) \approx 3.3219, we may choose 3.25
     * (x * 13) >> 2 is equivalent to x / 3.25
     */
    uint64_t digit = (UBN_UNIT_BIT * N->size - ubignum_clz(N)) * 13;
    digit = (digit >> 2) + 1;  // +1 for containing '\0'
    ubn_div_t *dit = ubn_div_init(N, 0);
    char *str = (char *) MALLOC(sizeof(char) * (digit + UBN_LTEN_EXP));
    /* convert 2-base to 10-base */
    uint32_t stridx = 0;
    while (likely(!ubignum_iszero(dit->dvd))) {
        ubignum_2decimal_l1(dit, str + stridx);
        stridx += UBN_LTEN_EXP;
    }
    str[stridx] = '\0';
    char *ans = (char *) MALLOC(sizeof(char) * (stridx + 1));
    stridx -= UBN_LTEN_EXP;
    uint32_t index = UBN_LTEN_EXP;
    while (likely(str[stridx] == '0')) {
        stridx++;
        index--;
    }
    memcpy(ans, str + stridx, sizeof(char) * index);
    stridx += index - UBN_LTEN_EXP - UBN_LTEN_EXP;
    while (stridx) {
        memcpy(ans + index, str + stridx, sizeof(char) * UBN_LTEN_EXP);
        index += UBN_LTEN_EXP;
        stridx -= UBN_LTEN_EXP;
    }
    memcpy(ans + index, str + stridx, sizeof(char) * UBN_LTEN_EXP);
    index += UBN_LTEN_EXP;
    ans[index] = '\0';
    FREE(str);
    ubn_div_free(dit);
    dit = NULL;
    return ans;
    // TODO: aloc fail handle
}

/* no allocation
 * dit->dvd = dit->dvd / UBN_LTEN
 * copy the string format of (dit->dvd % UBN_LTEN) to str without '\0'
 */
static void ubignum_2decimal_l1(ubn_div_t *const dit, char *const str)
{
    char s[UBN_LTEN_EXP + 1];
    ubignum_divby_Lten(dit);
#if CPU64
    snprintf(s, UBN_LTEN_EXP + 1, "%0*llu", UBN_LTEN_EXP,
             (unsigned long long) dit->sh_rmd);
#else
    snprintf(s, UBN_LTEN_EXP + 1, "%0*u", UBN_LTEN_EXP, (unsigned) rmd);
#endif
    memcpy(str, s, sizeof(char) * UBN_LTEN_EXP);
    ubignum_swapptr(&dit->dvd, &dit->quo);
}

static void ubignum_2decimal_l2(ubn_2dec_l2_t *const node)
{
    uint16_t str_offset = UBN_SUPERTEN_EXP - UBN_LTEN_EXP;
    while (likely(!ubignum_iszero(node->dit->dvd))) {
        ubignum_2decimal_l1(node->dit, node->str + str_offset);
        str_offset -= UBN_LTEN_EXP;
    }
    str_offset += UBN_LTEN_EXP;
    if (unlikely(str_offset))
        memset(node->str, '0', sizeof(char) * str_offset);  // pad '0'
}

/* Allocate space for members and copy dividend->data to ()->dvd->data.
 * @dvs_level represents the expected divisor size.
 * If dvs_level is 0, @rmd and @subed won't allocate space.
 */
ubn_div_t *ubn_div_init(const ubn_t *dividend, uint32_t dvs_level)
{
    ubn_div_t *dit = (ubn_div_t *) MALLOC(sizeof(ubn_div_t));
    if (unlikely(!dit))
        return NULL;
    if (unlikely(!(dit->dvd = ubignum_init(dividend->size))))
        goto cleanup_struct;
    if (unlikely(!(dit->quo = ubignum_init((dividend->size > dvs_level)
                                               ? dividend->size - dvs_level + 1
                                               : 1))))
        goto cleanup_dvd;
    if (dvs_level) {
        if (unlikely(!(dit->subed = ubignum_init(dividend->size + 1))))
            goto cleanup_quo;
    } else {
        dit->subed = NULL;
    }
    dit->sh_rmd = 0;
    dit->dvd->size = dividend->size;
    memcpy(dit->dvd->data, dividend->data, sizeof(ubn_unit_t) * dividend->size);
    return dit;
cleanup_quo:
    ubignum_free(dit->quo);
cleanup_dvd:
    ubignum_free(dit->dvd);
cleanup_struct:
    FREE(dit);
    return NULL;
}

void ubn_div_free(ubn_div_t *dit)
{
    if (!dit)
        return;
    ubignum_free(dit->subed);
    ubignum_free(dit->quo);
    ubignum_free(dit->dvd);
    FREE(dit);
}
