#ifndef __SIMD_VECTOR_H
#define __SIMD_VECTOR_H

#include <stdint.h> 
#include <immintrin.h>

typedef uint64_t v4u64 __attribute__ ((vector_size (32)));
typedef int64_t v4i64 __attribute__ ((vector_size (32)));
typedef uint32_t v4u32 __attribute__ ((vector_size (16)));
typedef int32_t v4i32 __attribute__ ((vector_size (16)));

typedef union {
        __m256i data;
        v4u64   vu64;
        v4i64   vi64;
}V256;

typedef union {
        __m128i data;
        v4i32   vi32;
        v4u32   vu32;
}V128;

#define __force_inline static inline __attribute__((always_inline))

#endif 
