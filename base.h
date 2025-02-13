#ifndef __BASE_H
#define __BASE_H

#ifndef KSPACE
#define KSPACE 1  // run in kernel space or user space
#endif

#if KSPACE
#include <linux/slab.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <stdlib.h>
#endif

/* consider x64 as 64-bit and treat others as 32-bit */
#if defined(__LP64__) || defined(__x86_64__) || defined(__amd64__) || \
    defined(__aarch64__)
#define CPU64 1
typedef uint64_t ubn_unit_t;
typedef __uint128_t ubn_extunit_t;
#define UBN_UNIT_BIT 64
#define UBN_UNIT_MAX 0xFFFFFFFFFFFFFFFFu
#define UBN_LTEN 10000000000000000u  // 10 ** 16
#define UBN_LTEN_EXP 16
#define UBN_LTEN_BIT 54
#else
#define CPU64 0
typedef uint32_t ubn_unit_t;
typedef uint64_t ubn_extunit_t;
#define UBN_UNIT_BIT 32
#define UBN_UNIT_MAX 0xFFFFFFFFu
#define UBN_LTEN 100000000u  // 10 ** 8
#define UBN_LTEN_EXP 8
#define UBN_LTEN_BIT 27
#endif

#define UBN_SUPERTEN_EXP 1024
#define UBN_ULTRATEN_EXP 65536

#if CPU64
#define UBN_SUPERTEN_CHUNK 54
#define UBN_ULTRATEN_CHUNK 3402
#else
#define UBN_SUPERTEN_CHUNK
#define UBN_ULTRATEN_CHUNK
#endif

#ifndef likely
#define likely(expr) __builtin_expect(expr, 1)
#endif

#ifndef unlikely
#define unlikely(expr) __builtin_expect(expr, 0)
#endif

#if KSPACE
#define MALLOC(sz) kmalloc(sz, GFP_KERNEL)
#define CALLOC(nmemb, sz) kcalloc(nmemb, sz, GFP_KERNEL)
#define REALLOC(ptr, sz) krealloc(ptr, sz, GFP_KERNEL)
#define FREE(ptr) kfree(ptr)
#else
#define MALLOC(sz) malloc(sz)
#define CALLOC(nmemb, sz) calloc(nmemb, sz)
#define REALLOC(ptr, sz) realloc(ptr, sz)
#define FREE(ptr) free(ptr)
#endif

#if CPU64
#ifndef ubn_unit_mult
#define ubn_unit_mult(a, b, hi, lo)                                \
    do {                                                           \
        __asm__("mulq %3" : "=a"(lo), "=d"(hi) : "a"(a), "rm"(b)); \
    } while (0);
#endif
#else
#ifndef ubn_unit_mult
#define ubn_unit_mult(a, b, hi, lo)                                \
    do {                                                           \
        __asm__("mull %3" : "=a"(lo), "=d"(hi) : "a"(a), "rm"(b)); \
    } while (0);
#endif
#endif


#endif