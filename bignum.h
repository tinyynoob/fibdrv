
#define DEFAULT_CAPACITY 2

#define DEBUG 0

#if DEBUG
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#endif

/* consider x64 and treat others as 32-bit */
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

/* set the number to 0  with size to 0 */
void ubignum_zero(ubn *N)
{
    if (!N || !N->capacity)
        return;
    for (int i = 0; i < N->capacity; i++)
        N->data[i] = 0;
    N->size = 0;
}

/* Initialize (*N) and set its value to 0 */
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
    (*N)->data = (ubn_unit *) calloc(sizeof(ubn_unit), DEFAULT_CAPACITY);
#else
    (*N)->data =
        (ubn_unit *) kcalloc(sizeof(ubn_unit), DEFAULT_CAPACITY, GFP_KERNEL);
#endif
    if (!(*N)->data)
        goto data_aloc_failed;
    (*N)->capacity = DEFAULT_CAPACITY;
    (*N)->size = 0;
    return true;

data_aloc_failed:
    ubignum_free(*N);
    *N = NULL;
struct_aloc_failed:
    return false;
}

/* assign an unsigned number to N */
void ubignum_uint(ubn *N, const unsigned int n)
{
    if (!N || !N->capacity)
        return;
    ubignum_zero(N);
    N->size = 1;
    N->data[0] = n;
    return;
}

/*
 * Adjust capacity of (*N).
 * If false is returned, (*N) remains unchanged.
 * Set (*N)->data to NULL if new_capacity is 0.
 * No guarantee if new_capacity < (*N)->capacity except for 0.
 */
bool ubignum_resize(ubn **N, int new_capacity)
{
    if (new_capacity < 0) {
        return false;
    } else if (new_capacity == 0) {
#if DEBUG
        free((*N)->data);
#else
        kfree((*N)->data);
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
    for (int i = (*N)->size; i < (*N)->capacity; i++)
        (*N)->data[i] = 0;
    return true;
}

/* compare two numbers */
int ubignum_compare(const ubn *a, const ubn *b)
{
    if (a->size > b->size)
        return 1;
    else if (a->size < b->size)
        return -1;

    for (int i = a->size - 1; i >= 0; i--) {
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
    else if (d == 0) {
        if (a == *out)
            return true;
        if (__builtin_expect((*out)->capacity < a->size, 0))
            if (!ubignum_resize(out, a->size))
                return false;
        for (int i = 0; i < a->size; i++)
            (*out)->data[i] = a->data[i];
        (*out)->size = a->size;
        return true;
    }
    const int chunk_shift = d / ubn_unit_bit;
    const int shift = d % ubn_unit_bit;
    const int new_size = a->size + chunk_shift + 1;
    if (__builtin_expect((*out)->capacity < new_size, 0))
        if (!ubignum_resize(out, new_size))
            return false;

    int ai = new_size - chunk_shift - 1;  // note a->size may be changed due to
                                          // aliasing, should not use a->size
    int oi = new_size - 1;
    /* copy data from a to (*out) */
    if (shift) {
        (*out)->data[oi--] = a->data[ai - 1] >> (ubn_unit_bit - shift);
        for (ai--; ai > 0; ai--)
            (*out)->data[oi--] = (a->data[ai] << shift) |
                                 (a->data[ai - 1] >> (ubn_unit_bit - shift));
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
 * If false is returned, the values of (*out) remains unchanged.
 */
bool ubignum_add(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    while (__builtin_expect((*out)->capacity < max(a->size, b->size) + 1, 0))
        if (!ubignum_resize(out, (*out)->capacity * 2))
            return false;
    if (*out != a && *out != b)  // if no pointer aliasing
        ubignum_zero(*out);
    int i = 0, carry = 0;
    for (i = 0; i < min(a->size, b->size); i++)
        ubn_unit_add(a->data[i], b->data[i], carry, &(*out)->data[i], &carry);
    const ubn *const remain = (i == a->size) ? b : a;
    for (; i < remain->size; i++)
        ubn_unit_add(remain->data[i], 0, carry, &(*out)->data[i], &carry);
    (*out)->size = remain->size;
    if (carry) {
        (*out)->data[i] = 1;
        (*out)->size++;
    }
    return true;
}

/* (*out) = a - b
 * Since the system is unsigned, a >= b should be guarantee to get a positive
 * result.
 */
bool ubignum_sub(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    /* ones' complement of b */
    ubn *cmt;  // maybe there is way without memory allocation?
    if (!ubignum_init(&cmt))
        goto cmt_aloc_failed;
    if (__builtin_expect(cmt->capacity < a->size, 0))
        if (!ubignum_resize(&cmt, a->size))
            goto cmt_realoc_failed;
    cmt->size = a->size;
    for (int i = 0; i < b->size; i++)
        cmt->data[i] = ~b->data[i];
    for (int i = b->size; i < a->size; i++)
        cmt->data[i] = CPU_64 ? 0xFFFFFFFFFFFFFFFF : 0xFFFFFFFF;

    int carry = 1;
    for (int i = 0; i < a->size; i++)  // compute result and store in cmt
        ubn_unit_add(a->data[i], cmt->data[i], carry, &cmt->data[i], &carry);
    // the last carry is discarded
    for (int i = a->size - 1; i >= 0; i--)
        if (!cmt->data[i])
            cmt->size--;
    ubignum_free(*out);
    *out = cmt;
    return true;
cmt_realoc_failed:
    ubignum_free(cmt);
cmt_aloc_failed:
    return false;
}

/* a / 10 = (*quo)...rmd */
bool ubignum_divby_ten(const ubn *a, ubn **quo, int *rmd)
{
    if (!a || !a->size || !quo || !*quo || !rmd)
        return false;
    ubn *ans, *dvd, *suber, *ten;
    if (!ubignum_init(&ans))
        return false;
    if (!ubignum_resize(&ans, a->size))
        goto cleanup_ans;
    if (!ubignum_init(&dvd))
        goto cleanup_ans;
    if (!ubignum_resize(&dvd, a->size))
        goto cleanup_dvd;
    if (!ubignum_init(&suber))
        goto cleanup_dvd;
    if (!ubignum_resize(&suber, dvd->capacity + 1))
        goto cleanup_suber;
    if (!ubignum_init(&ten))
        goto cleanup_suber;
    ubignum_uint(ten, 10);

    for (int i = 0; i < a->size; i++)
        dvd->data[i] = a->data[i];
    dvd->size = a->size;
    int shift = dvd->size * ubn_unit_bit - 4;  // can be improved by clz()
    ubignum_left_shift(ten, shift, &suber);
    while (ubignum_compare(dvd, ten) >= 0) {  // if dvd >= 10
        while (ubignum_compare(dvd, suber) < 0) {
            shift--;
            ubignum_left_shift(ten, shift, &suber);
        }
        ans->data[shift / ubn_unit_bit] |= (ubn_unit) 1
                                           << (shift % ubn_unit_bit);
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
    prod->size = mcand->size + 1;

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
            overlap +
            ((ubn_unit) carry & 2);  // no carry out would be generated

        if (!ubignum_mult_add(prod, i, &ans))
            goto multadd_failed;
    }
    if (alias)
        ubignum_free(*out);
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

/* convert the ubn to ascii string */
char *ubignum_2decimal(const ubn *N)
{
    if (!N)
        return NULL;
    if (N->capacity && !N->size) {
#if DEBUG
        char *ans = (char *) calloc(sizeof(char), 2);
#else
        char *ans = (char *) kcalloc(sizeof(char), 2, GFP_KERNEL);
#endif
        if (!ans)
            return NULL;
        ans[0] = '0';
        ans[1] = 0;
        return ans;
    }
    ubn *dvd;
    if (!ubignum_init(&dvd))
        return NULL;
    if (!ubignum_resize(&dvd, N->size))
        goto cleanup_dvd;
    for (int i = 0; i < N->size; i++)
        dvd->data[i] = N->data[i];
    dvd->size = N->size;
    /* Let n be the number.
     * digit = 1 + log_10(n) = 1 + \frac{log_2(n)}{log_2(10)}
     * log_2(10) \approx 3.3219 \approx 7/2,  we simply choose 3
     */
    unsigned digit = (ubn_unit_bit * N->size / 3) + 1;
#if DEBUG
    char *ans = (char *) calloc(sizeof(char), digit);
#else
    char *ans = (char *) kcalloc(sizeof(char), digit, GFP_KERNEL);
#endif
    if (!ans)
        goto cleanup_dvd;
    /* convert 2-base to 10-base */
    int index = 0;
    while (dvd->size) {
        int rmd;
        if (__builtin_expect(!ubignum_divby_ten(dvd, &dvd, &rmd), 0))
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
#if DEBUG
    free(ans);
#else
    kfree(ans);
#endif
cleanup_dvd:
    ubignum_free(dvd);
    return NULL;
}

#if DEBUG
void ubignum_show(ubn *N)
{
    char *dec = ubignum_2decimal(N);
    if (!dec)
        return;
    printf("%s\n", dec);
    free(dec);
}
#endif
