#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_CAPACITY 2

#define DEBUG 1

#if DEBUG
#include <stdio.h>
#include <stdlib.h>
#else
#include <linux/compiler.h>
#endif

#if defined(__LP64__) || defined(__x86_64__) || defined(__amd64__) || \
    defined(__aarch64__)
typedef uint64_t ubn_unit;
typedef __uint128_t ubn_unit_extend;  // double length
#define ubn_unit_bit 64
#define CPU_64 1
#else
typedef uint32_t ubn_unit;
typedef uint64_t ubn_unit_extend;  // double length
#define ubn_unit_bit 32
#define CPU_64 0
#endif

#ifndef max
#define max(a, b)          \
    ({                     \
        typeof(a) _a = a;  \
        typeof(b) _b = b;  \
        _a > _b ? _a : _b; \
    })
#endif

#ifndef min
#define min(a, b)          \
    ({                     \
        typeof(a) _a = a;  \
        typeof(b) _b = b;  \
        _a < _b ? _a : _b; \
    })
#endif

/* unsigned big number
 * @data: MS:[size-1], LS:[0]
 * @size: sizeof(ubn_unit)
 * @capacity: allocated size
 */
typedef struct {
    ubn_unit *data;
    int size;
    int capacity;
} ubn;


#if DEBUG
void ubignum_show(ubn *N) {}
#endif

bool ubignum_init(ubn **N)
{
    if (!N)
        return false;
#if DEBUG
    *N = (ubn *) malloc(sizeof(ubn));
#else
    *N = (ubn *) kmalloc(sizeof(ubn), GFP_KERNEL);
#endif
    if (!*N)
        goto struct_aloc_failed;

#if DEBUG
    (*N)->data = (ubn_unit *) malloc(sizeof(ubn_unit) * DEFAULT_CAPACITY);
#else
    (*N)->data =
        (ubn_unit *) kmalloc(sizeof(ubn_unit) * DEFAULT_CAPACITY, GFP_KERNEL);
#endif
    if (!(*N)->data)
        goto data_aloc_failed;
    (*N)->size = 0;
    (*N)->capacity = DEFAULT_CAPACITY;
    return true;

data_aloc_failed:
    free(*N);
    *N = NULL;
struct_aloc_failed:
    return false;
}

void ubignum_free(ubn *N)
{
    if (!N)
        return;
#if DEBUG
    free(N->data);
    free(N);
#else
    kfree(N->data);
    kfree(N);
#endif
}

/* set the number to 0 */
void ubignum_zero(ubn *N)
{
    if (!N || !N->capacity)
        return;
    for (int i = 0; i < N->size; i++)
        N->data[i] = 0;
    N->size = 0;
}

// TODO
bool ubignum_assign(ubn *N, const char *input)  // may not be a good idea
{
    // '0' = 48
    if (!N)
        return false;
}

/* @*N remains unchanged if return false */
bool ubignum_resize(ubn **N, int new_capacity)
{
    if (new_capacity < 0) {
        return false;
    } else if (new_capacity == 0) {
#if DEBUG
        free((*N)->data);
#else
        kfree((*N)->data, GFP_KERNEL);
#endif
        (*N)->data = NULL;
        (*N)->size = 0;
    } else {
#if DEBUG
        ubn_unit *new =
            (ubn_unit *) realloc((*N)->data, sizeof(ubn_unit) * new_capacity);
#else
        ubn_unit *new = (ubn_unit *) krealloc(
            (*N)->data, sizeof(ubn_unit) * new_capacity, GFP_KERNEL);
#endif
        if (!new)
            return false;
        (*N)->data = new;
    }
    (*N)->capacity = new_capacity;
    return true;
}

/* */
int ubignum_compare(const ubn *a, const ubn *b)
{
    for (int i = max(a->size, b->size) - 1; i >= 0; i--) {
        if (a->data[i] > b->data[i])
            return 1;
        else if (a->data[i] < b->data[i])
            return -1;
    }
    return 0;
}

/* left shift a->data by d bit */
bool ubignum_left_shift(const ubn *a, int d, ubn **out)
{
    if (!a || d < 0 || !out || !*out)
        return false;
    ubn *ans = *out;
    int alias = 0;
    if (a == *out)
        alias ^= 1;
    if (alias) {  // if alias, allocate space to store the result
        if (a->size == 0 || d == 0)
            return true;
        if (!ubignum_init(&ans))
            return false;
    }
    ubignum_zero(ans);
    const int chunk_shift = d / ubn_unit_bit;
    const int shift = d % ubn_unit_bit;
    const int new_size = a->size + chunk_shift + 1;
    if (__builtin_expect(new_size > ans->capacity, 0))
        if (!ubignum_resize(&ans, new_size))
            goto realoc_failed;
    ans->size = new_size;

    int ai = a->size;
    int oi = ai + chunk_shift;  // = new_size - 1
    if (shift) {                // copy data from a to ans
        ans->data[oi--] = a->data[ai - 1] >> ubn_unit_bit - shift;
        for (ai--; ai > 0; ai--)
            ans->data[oi--] = (a->data[ai] << shift) |
                              (a->data[ai - 1] >> ubn_unit_bit - shift);
        ans->data[oi--] = a->data[ai] << shift;  // ai is now 0
    } else {
        for (ai; ai >= 0; ai--)
            ans->data[oi--] = a->data[ai];
    }
    while (oi >= 0)  // set remaining part of ans to 0
        ans->data[oi--] = 0;
    if (ans->data[ans->size - 1] == 0)  // if MS chunk is 0
        ans->size--;
    if (alias)
        ubignum_free(*out);
    *out = ans;
    return true;
realoc_failed:
    if (alias)
        ubignum_free(ans);
    return false;
}

/* no checking, sum and cout input should be guarantee */
static inline void ubn_unit_add(const ubn_unit a,
                                const ubn_unit b,
                                const int cin,
                                ubn_unit *sum,
                                int *cout)
{
    const ubn_unit_extend SUM = (ubn_unit_extend) a + b + cin;
    *sum = SUM;
    *cout = SUM >> ubn_unit_bit;
}

/* (*out) = a + b
 * Aliasing arguments are acceptable.
 * If it return true, the result is put at @out.
 * If return false with alias input, the @out would be unchanged.
 * If return false without alias input, the @out would return neither answer nor
 * original out.
 */
bool ubignum_add(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    ubn *ans = *out;
    int alias = 0;
    if (a == *out)  // pointer aliasing
        alias ^= 1;
    if (b == *out)  // pointer aliasing
        alias ^= 2;
    if (alias) {  // if alias, allocate space to store the result
        if (!ubignum_init(&ans))
            return false;
    }

    int i = 0, carry = 0;
    for (i = 0; i < min(a->size, b->size); i++) {
        if (i >= ans->size) {
            if (__builtin_expect(i >= ans->capacity, 0))
                if (!ubignum_resize(&ans, ans->capacity * 2))
                    goto realoc_failed;
            ans->size++;
        }
        ubn_unit_add(a->data[i], b->data[i], carry, &ans->data[i], &carry);
    }

    const ubn *remain = (i == a->size) ? b : a;
    for (; carry || i < remain->size; i++) {
        if (i >= ans->size) {
            if (__builtin_expect(i >= ans->capacity, 0))
                if (!ubignum_resize(&ans, ans->capacity * 2))
                    goto realoc_failed;
            ans->size++;
        }
        ubn_unit_add(remain->data[i], 0, carry, &ans->data[i], &carry);
    }
    if (alias)
        ubignum_free(*out);
    *out = ans;
    return true;

realoc_failed:
    if (alias)
        ubignum_free(ans);
    return false;
}

/* (*out) = a - b
 * a >= b should be guarantee
 */
bool ubignum_sub(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    ubn *ans = *out;
    int alias = 0;
    if (a == *out)  // pointer aliasing
        alias ^= 1;
    if (b == *out)  // pointer aliasing
        alias ^= 2;
    if (alias) {  // if alias, allocate space to store the result
        if (!ubignum_init(&ans))
            return false;
    }
    /* ones' complement of b */
    ubn *cmt;
    ubignum_init(&cmt);
#if DEBUG
    cmt = (ubn *) malloc(sizeof(ubn));
#else
    cmt = (ubn *) kmalloc(sizeof(ubn), GFP_kernel);
#endif
    if (!cmt)
        goto cmt_failed;
#if DEBUG
    cmt->data = (ubn_unit *) malloc(sizeof(ubn_unit) * a->size);
#else
    cmt->data = (ubn_unit *) kmalloc(sizeof(ubn_unit) * a->size, GFP_KERNEL);
#endif
    if (!cmt->data)
        goto cmt_failed;
    cmt->capacity = a->size;
    cmt->size = a->size;
    for (int i = 0; i < b->size; i++)
        cmt->data[i] = ~b->data[i];
    for (int i = b->size; i < a->size; i++)
        cmt->data[i] = CPU_64 ? UINT64_MAX : UINT32_MAX;

    int carry = 1;
    for (int i = 0; i < a->size; i++)  // set ans->data
        ubn_unit_add(a->data[i], cmt->data[i], carry, &ans->data[i], &carry);
    for (int i = a->size; i; i--) {  // set ans->size
        if (ans->data[i - 1]) {
            ans->size = i;
            break;
        }
    }
    ubignum_free(cmt);
    if (alias)
        ubignum_free(*out);
    *out = ans;
    return true;
cmt_failed:
    ubignum_free(cmt);
    if (alias)
        ubignum_free(ans);
    return false;
}

/* developing */
bool ubignum_divby_ten(const ubn *a, ubn **quo, int *rmd)
{
    if (!a || !a->size || !quo || !*quo || !rmd)
        return false;
    ubn *ans = *quo;
    int alias = 0;
    if (a == *quo)  // pointer aliasing
        alias ^= 1;
    if (alias) {  // if alias, allocate space to store the result
        if (!ubignum_init(&ans))
            return false;
    }
    ubignum_zero(ans);

    ubn *dvd;
    if (!ubignum_init(&dvd))
        goto dvd_aloc_failed;
    if (!ubignum_resize(&dvd, a->capacity))
        goto dvd_resize_failed;
    for (int i = 0; i < a->size; i++) {
        dvd->data[i] = a->data[i];
        dvd->size++;
    }
    ubn *ten;  // const numbers
    if (!ubignum_init(&ten))
        goto ten_aloc_failed;
    ten->data[0] = 10;
    ten->size = 1;

    ubn *suber;
    if (!ubignum_init(&suber))
        goto suber_aloc_failed;
    if (!ubignum_resize(&suber, dvd->capacity + 1))
        goto suber_resize_failed;

    int shift = dvd->size * ubn_unit_bit - 3;
    ubignum_left_shift(ten, shift, &suber);
    while (ubignum_compare(dvd, ten) > 0) {
        while (ubignum_compare(dvd, suber) < 0) {
            shift--;
            ubignum_left_shift(ten, shift, &suber);
        }
        ans->data[shift / ubn_unit_bit] |= (ubn_unit) 1
                                           << (shift % ubn_unit_bit);
        ubignum_sub(dvd, suber, &dvd);
    }
    ans->size = a->size;
    if (ans->data[ans->size - 1] == 0)
        ans->size--;
    *rmd = (int) dvd->data[0];
    ubignum_free(ten);
    ubignum_free(dvd);
    ubignum_free(suber);
    if (alias)
        ubignum_free(*quo);
    *quo = ans;
    return true;

suber_resize_failed:
    ubignum_free(suber);
suber_aloc_failed:
    ubignum_free(ten);
ten_aloc_failed:
dvd_resize_failed:
    ubignum_free(dvd);
dvd_aloc_failed:
    if (alias)
        ubignum_free(ans);
    return false;
}

/* (*out) += a << (offset * ubn_unit_bit)
 *
 * The pointers are assumed no aliasing.
 */
static bool ubignum_mult_add(const ubn *a, int offset, ubn **out)
{
    if (!a || !out || !*out || offset < 0)
        return false;
    if ((*out)->size < offset + a->size) {
        int new_size = offset + a->size;
        if (__builtin_expect(new_size >= (*out)->capacity, 0))
            if (!ubignum_resize(out, new_size + 1))
                return false;
        (*out)->size = new_size;
    }
    int carry = 0, oi;
    for (int ai = 0; ai < a->size; ai++) {
        oi = ai + offset;
        ubn_unit_add((*out)->data[oi], a->data[ai], carry, &(*out)->data[oi],
                     &carry);
    }
    for (; carry; oi++) {
        if (oi >= (*out)->size) {
            if (__builtin_expect(oi >= (*out)->capacity, 0))
                if (!ubignum_resize(out, (*out)->capacity * 2))
                    return false;
            (*out)->size++;
        }
        (*out)->data[oi] += carry;
    }
    return true;
}

/* (*out) = a * b
 *
 */
bool ubignum_mult(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    ubn *ans = *out;
    int alias = 0;
    if (a == *out)  // pointer aliasing
        alias ^= 1;
    if (b == *out)  // pointer aliasing
        alias ^= 2;
    if (alias) {  // if alias, allocate space to store the result
        if (!ubignum_init(&ans))
            return false;
    }
    /* keep mcand longer than mplier */
    const ubn *mcand = a->size > b->size ? a : b;
    const ubn *mplier = a->size > b->size ? b : a;
    ubn *prod;  // partial product
    if (!ubignum_init(&prod))
        goto prod_aloc_failed;
    if (prod->capacity < mcand->size + 1)
        if (!ubignum_resize(&prod, mcand->size + 1))
            goto prod_resize_failed;

    /* The outer loop */
    for (int i = 0; i < mplier->size; i++) {
        /* The inner loop explanation:
         *       a b c  = mcand->data[2, 1, 0]
         *     *     d  = mplier->data[0]
         *     --------
         *         c c  = (high, low) denotes result of c * d
         *       b b
         *   + a a
         *   ----------
         *     x y ? c  sum of bb and cc, it may be carry out to x
         *   + a a
         *
         * let x be recorded in @carry and y be @overlap
         * no realloc needed for @prod
         */
        int carry = 0;
        ubn_unit overlap = 0;
        for (int j = 0; j < mcand->size; j++) {
            ubn_unit low, high;
#if CPU_64
            __asm__("mulq %3"
                    : "=a"(low), "=d"(high)
                    : "a"(mcand->data[j]), "rm"(mplier->data[i]));
#else
            __asm__("mull %3"
                    : "=a"(low), "=d"(high)
                    : "a"(mcand->data[j]), "rm"(mplier->data[i]));
#endif
            int cout = 0;
            ubn_unit_add(low, overlap, carry & 2, &prod->data[j], &cout);
            carry = (carry << 1) | cout;  // update carry where cout is 0 or 1
            overlap = high;
        }
        prod->data[mcand->size] =
            overlap + (ubn_unit) carry & 2;  // no carry out would be generated

        if (!ubignum_mult_add(prod, i, &ans))
            goto multadd_failed;
    }
    if (alias)
        free(*out);
    *out = ans;
    return true;
multadd_failed:
prod_resize_failed:
    ubignum_free(prod);
prod_aloc_failed:
    if (alias)
        ubignum_free(ans);
    return false;
}
