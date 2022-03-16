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

bool ubignum_assign(ubn *N, const char *input)  // may not be a good idea
{
    // '0' = 48
    if (!N)
        return false;
    int len = strlen(input);
    // back
    for (int i = 0; i < len / 32; i += 2) {
    }
}

bool ubignum_resize(ubn *N, int new_capacity)
{
    if (new_capacity < 0)
        return false;
#if DEBUG
    ubn *new = (ubn *) realloc(N->data, sizeof(ubn_unit) * new_capacity);
#else
    ubn *new =
        (ubn *) krealloc(N->data, sizeof(ubn_unit) * new_capacity, GFP_KERNEL);
#endif
    if (!new)
        return false;
    N = new;
    N->capacity = new_capacity;
    return true;
}

/* out = a + b
 * Aliasing arguments are acceptable.
 * If it return true, the result is put at @out.
 * If return false with alias input, the @out would be unchanged.
 * If return false without alias input, the @out would return neither answer nor
 * original out.
 */
bool ubignum_add(const ubn *a, const ubn *b, ubn **out)
{
    ubn *ans = *out;
    int alias = 0;
    if (a == *out)  // pointer aliasing
        alias ^= 1;
    if (b == *out)  // pointer aliasing
        alias ^= 2;
    if (alias) {  // if so, allocate space to store the result
        if (!ubignum_init(&ans))
            return false;
    }

    ubn_unit carry = 0;
    int i = 0;
    for (i = 0; i < min(a->size, b->size); i++) {
        if (i >= ans->size) {
            if (__builtin_expect(i >= ans->capacity, 0))
                if (!ubignum_resize(ans, ans->capacity * 2))
                    goto realoc_failed;
            ans->size++;
        }
        const ubn_unit_extend sum =
            (ubn_unit_extend) a->data[i] + b->data[i] + carry;
        ans->data[i] = sum;
        carry = sum >> ubn_unit_bit;
    }

    const ubn *remain = (i == a->size) ? b : a;
    for (; carry || i < remain->size; i++) {
        if (i >= ans->size) {
            if (__builtin_expect(i >= ans->capacity, 0))
                if (!ubignum_resize(ans, ans->capacity * 2))
                    goto realoc_failed;
            ans->size++;
        }
        if (__builtin_expect(i < remain->size, 1)) {
            const ubn_unit_extend sum =
                (ubn_unit_extend) remain->data[i] + carry;
            ans->data[i] = sum;
            carry = sum >> ubn_unit_bit;
        } else {  // the last carry out carries to a new ubn_unit
            ans->data[i] = carry;  // = 1
            carry = 0;
        }
    }
    *out = ans;  // no condition needed
    return true;

realoc_failed:
    if (alias)
        ubignum_free(ans);
    return false;
}
