
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
#define ubn_unit_bit 64
#define CPU_64 1
#else
typedef uint32_t ubn_unit;
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

/* swap two ubn */
void ubignum_swapptr(ubn **a, ubn **b)
{
    ubn *tmp = *a;
    *a = *b;
    *b = tmp;
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

/* set the number to 0, that is, size = 0 */
void ubignum_zero(ubn *N)
{
    if (!N || !N->capacity)
        return;
    for (int i = 0; i < N->capacity; i++)
        N->data[i] = 0;
    N->size = 0;
}

/* assign a unsigned number to N */
void ubignum_uint(ubn *N, const unsigned int n)
{
    if (!N || !N->capacity)
        return;
    ubignum_zero(N);
    if (__builtin_expect(n, 1)) {
        N->size = 1;
        N->data[0] = n;
    }
    return;
}

/* check if N is zero */
bool ubignum_iszero(const ubn *N)
{
    return N->capacity && !N->size;
}

/* count leading zero of the MS chunk
 * DO NOT INPUT ZERO
 */
static inline int ubignum_clz(const ubn *N)
{
#if CPU_64
    return __builtin_clzll(N->data[N->size - 1]);
#else
    return __builtin_clz(N->data[N->size - 1]);
#endif
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

/* Assign src to dest */
bool ubignum_copy(ubn *restrict dest, const ubn *restrict src)
{
    if (!dest || !src)
        return false;
    else if (dest == src)
        return true;
    if (dest->capacity < src->size)
        if (!ubignum_resize(&dest, src->size))
            return false;
    ubignum_zero(dest);
    dest->size = src->size;
    for (int i = 0; i < src->size; i++)
        dest->data[i] = src->data[i];
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
    } else if (ubignum_iszero(a)) {
        ubignum_zero(*out);
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

/* no checking for parameters */
static inline void ubn_unit_add(const ubn_unit a,
                                const ubn_unit b,
                                const int cin,
                                ubn_unit *sum,
                                int *cout)
{
#if CPU_64
    *cout = __builtin_uaddll_overflow(a, cin, sum);
    *cout |= __builtin_uaddll_overflow(*sum, b, sum);
#else
    *cout = __builtin_uadd_overflow(a, cin, sum);
    *cout |= __builtin_uadd_overflow(*sum, b, sum);
#endif
}

/* (*out) = a + b
 * Aliasing arguments are acceptable.
 * If false is returned, the values of (*out) remains unchanged.
 */
bool ubignum_add(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    /* compute new size */
    int new_size;
    if (ubignum_iszero(a) || ubignum_iszero(b))
        new_size = max(a->size, b->size);
    else if (a->size >= b->size && ubignum_clz(a) == 0)
        new_size = a->size + 1;
    else if (b->size >= a->size && ubignum_clz(b) == 0)
        new_size = b->size + 1;
    else
        new_size = max(a->size, b->size);
    while (__builtin_expect((*out)->capacity < new_size, 0))
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
 * Since the system is unsigned, a >= b should be guaranteed to get a positive
 * result.
 */
bool ubignum_sub(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out)
        return false;
    /* ones' complement of b */
    ubn *cmt = NULL;  // maybe there is way without memory allocation?
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
    if (!a || !a->capacity || !quo || !*quo || !rmd)
        return false;
    if (__builtin_expect(ubignum_iszero(a), 0)) {
        ubignum_zero(*quo);
        *rmd = 0;
        return true;
    }
    ubn *ans = NULL, *dvd = NULL, *suber = NULL, *ten = NULL;
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
    while (ubignum_compare(dvd, ten) >= 0) {  // if dvd >= 10
        int shift = dvd->size * ubn_unit_bit - ubignum_clz(dvd) - 4;
        ubignum_left_shift(ten, shift, &suber);
        if (ubignum_compare(dvd, suber) < 0)
            ubignum_left_shift(ten, --shift, &suber);
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
 * The pointers are assumed no aliasing.
 * @offset is assumed to be positive or 0.
 * The capacity of (*out) must be guaranteed in mult(). No resize is done here!
 */
static void ubignum_mult_add(const ubn *restrict a,
                             int offset,
                             ubn *restrict *out)
{
    if (ubignum_iszero(a))
        return;

    int carry = 0, oi;
    for (int ai = 0, oi = offset; ai < a->size; ai++, oi++)
        ubn_unit_add((*out)->data[oi], a->data[ai], carry, &(*out)->data[oi],
                     &carry);
    for (; carry; oi++)
        ubn_unit_add((*out)->data[oi], 0, carry, &(*out)->data[oi], &carry);

    (*out)->size = max(a->size + offset, (*out)->size);
    for (int i = (*out)->size - 1; !(*out)->data[i]; i--)
        (*out)->size--;
    return;
}

/* (*out) = a * b
 */
bool ubignum_mult(const ubn *a, const ubn *b, ubn **out)
{
    if (!a || !b || !out || !*out) {
        return false;
    } else if (ubignum_iszero(a) || ubignum_iszero(b)) {
        ubignum_zero(*out);
        return true;
    }
    /* keep mcand longer than mplier */
    const ubn *mcand = a->size > b->size ? a : b;
    const ubn *mplier = a->size > b->size ? b : a;
    ubn *ans = NULL;
    if (!ubignum_init(&ans))
        return false;
    if (!ubignum_resize(&ans, mcand->size + mplier->size))
        goto cleanup_ans;
    ubn *pprod = NULL;  // partial product
    if (!ubignum_init(&pprod))
        goto cleanup_ans;
    if (!ubignum_resize(&pprod, mcand->size + 1))
        goto cleanup_pprod;
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
            ubn_unit_add(low, overlap, carry, &pprod->data[j], &carry);
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
cleanup_pprod:
    ubignum_free(pprod);
cleanup_ans:
    ubignum_free(ans);
    return false;
}

/* convert the ubn to ascii string */
char *ubignum_2decimal(const ubn *N)
{
    if (!N)
        return NULL;
    if (ubignum_iszero(N)) {
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
    ubn *dvd = NULL;
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
