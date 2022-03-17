#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_CAPACITY 16


#define DEBUG 1

#if DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //memmove()
#else
#include <linux/compiler.h>
#endif

#if defined(__LP64__) || defined(__x86_64__) || defined(__amd64__) || \
    defined(__aarch64__)
typedef uint64_t ubn_unit;
typedef __uint128_t ubn_unit_extend;  // double length
#define ubn_unit_bit 64
#else
typedef uint32_t ubn_unit;
typedef uint64_t ubn_unit_extend;  // double length
#define ubn_unit_bit 32
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
 * @data: MSB:[size-1], LSB:[0]
 * @size: sizeof(ubn_unit)
 * @capacity: allocated size
 */
typedef struct {
    ubn_unit *data;
    int size;
    int capacity;
} ubn;

static inline void swapptr(void **a, void **b)
{
    (*a) = (char *) ((__intptr_t)(*a) ^ (__intptr_t)(*b));
    (*b) = (char *) ((__intptr_t)(*b) ^ (__intptr_t)(*a));
    (*a) = (char *) ((__intptr_t)(*a) ^ (__intptr_t)(*b));
}

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

static ubn *ubignum_duplicate(ubn *dest, const ubn *src)
{
    if (!src)
        return NULL;
#if DEBUG
    dest = (ubn *) malloc(sizeof(ubn));
#else
    dest = (ubn *) kmalloc(sizeof(ubn));
#endif
    if (!dest)
        goto struct_aloc_failed;
    dest->capacity = src->capacity;

#if DEBUG
    dest->data = (ubn_unit *) malloc(sizeof(ubn_unit) * src->capacity);
#else
    dest->data =
        (ubn_unit *) kmalloc(sizeof(ubn_unit) * src->capacity, GFP_KERNEL);
#endif
    if (!dest->data)
        goto data_aloc_failed;
    dest->data = (ubn_unit *) memmove(
        dest->data, src->data,
        sizeof(ubn_unit) * src->size);  // for both user space and kernel space
    dest->size = src->size;
    return dest;
data_aloc_failed:
    free(dest);
struct_aloc_failed:
    return NULL;
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
    if (new_capacity < 0)
        return false;
#if DEBUG
    ubn *new = (ubn *) realloc((*N)->data, sizeof(ubn_unit) * new_capacity);
#else
    ubn *new = (ubn *) krealloc((*N)->data, sizeof(ubn_unit) * new_capacity,
                                GFP_KERNEL);
#endif
    if (!new)
        return false;
    *N = new;
    (*N)->capacity = new_capacity;
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
    *out = ans;  // no condition needed
    return true;

realoc_failed:
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
    /* keep mcand longer then mplier */
    const ubn *mcand = a->size > b->size ? a : b;
    const ubn *mplier = a->size > b->size ? b : a;
    ubn *prod;
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
#if defined(__LP64__) || defined(__x86_64__) || defined(__amd64__) || \
    defined(__aarch64__)
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

#if DEBUG
#else
#endif

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
