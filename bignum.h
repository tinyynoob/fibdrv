#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>  //memmove()

#define DEFAULT_CAPACITY 16


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
// typedef unsigned __int128 bn_data_tmp; // gcc support __int128
#define ubn_unit_bit 64
#else
typedef uint32_t ubn_unit;
// typedef uint64_t bn_data_tmp;
#define ubn_unit_bit 32
#endif

// TODO:
#ifndef max
#define max
#endif

#ifndef min
#define min
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
    dest = (ubn_unit *) memmove(
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
    // wondering how to do
}

bool ubignum_resize(ubn *N, int new_size)
{
    if (new_size < 0)
        return false;
#if DEBUG
    ubn_unit *new = (ubn_unit *) realloc(N->data, sizeof(ubn_unit) * new_size);
#else
    ubn_unit *new =
        (ubn_unit *) krealloc(N->data, sizeof(ubn_unit) * new_size, GFP_KERNEL);
#endif
    if (!new)
        return false;
    N = new;
    N->capacity = new_size;
    return true;
}

bool ubignum_add(const ubn *a, const ubn *b, ubn *out)
{
    ubn *store = NULL;
    int alias = 0;
    if (a == out)  // pointer aliasing
        alias ^= 1;
    if (b == out)  // pointer aliasing
        alias ^= 2;
    if (alias) {
        store = ubignum_duplicate(store, out);
        if (!store)
            return false;
    }

    ubn_unit carry = 0;
    int i = 0;
    for (i = 0; i < min(a->size, b->size); i++) {
        if (__builtin_expect(i >= out->capacity, 0)) {
            if (!resize(out, out->capacity * 2)) {
                goto realoc_failed;
            }
        }
        ubn_unit an = a->data[i];
        ubn_unit bn = b->data[i];
        out->data[i] = an + bn + carry;
        // The following consideration does not include carry
        // fix it!
        carry =
            ((an ^ bn) >> 1 + (an & bn)) & ((ubn_unit) 1 << (ubn_unit_bit - 1));
    }
    ubn *remain = (i == a->size) ? b : a;
    for (; carry || i < remain->size; i++) {
        if (__builtin_expect(i >= out->capacity, 0)) {
            if (!resize(out, out->capacity * 2)) {
                goto realoc_failed;
            }
        }
        if (__builtin_expect(i < remain->size, 1)) {
            ubn_unit rn = remain->data[i];
            out->data[i] = rn + carry;
            carry = ((rn ^ carry) >> 1 ^ (rn & carry)) &
                    ((ubn_unit) 1 << (ubn_unit_bit - 1));
        } else {  // the last carry out carries to a new ubn_unit
            out->data[i] = carry;  // = 1
            carry = 0;
        }
    }
    return true;

realoc_failed:
    switch (alias) {
    case 1:
        swapptr(&a, &store);
        break;
    case 2:
    case 3:
        swapptr(&b, &store);
    case 0:
    }
    ubignum_free(store);
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
