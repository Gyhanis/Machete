#ifndef __BIT_READER_H
#define __BIT_READER_H

#include <immintrin.h>
#include <stdint.h>
#include <assert.h>
#include "vector.h"

typedef struct {
        uint32_t * input;
        uint64_t buffer;
        int64_t cursor;
        int64_t bitcnt;
}BitReader;

__force_inline BitReader 
initBitReader(BitReader reader, uint32_t * input, size_t len)
{
        assert(len >= 1);
        reader.input = (uint32_t*) input;
        reader.buffer = input[len-1];
        reader.cursor = len-2;
        reader.buffer <<= 32;
        reader.bitcnt = 32;
        return reader;
}

__force_inline uint64_t 
peek(BitReader reader, size_t len) {
        assert(len <= 32);
        return reader.buffer >>= 64 - len;
}

__force_inline BitReader 
forward(BitReader reader, size_t len) {
        assert(len <= 32);
        reader.bitcnt -= len;
        reader.buffer <<= len;
        if (__builtin_expect(reader.bitcnt < 32, 0)) {
                if (reader.cursor >= 0) {
                        uint64_t data = reader.input[reader.cursor];
                        reader.buffer |= data << (32 - reader.bitcnt);
                        reader.bitcnt += 32;
                        reader.cursor --;
                } else {
                        reader.bitcnt = 64;
                }
        }
        return reader;
}

typedef struct {
        uint32_t * input;
        v4u64 buffer;
        v4u64 start;
        v4u64 cursor;
        v4u64 bitcnt;
}BitReader4XV;

__force_inline BitReader4XV 
initBitReader4XV(BitReader4XV reader, 
                uint32_t * input, size_t len) 
{
        assert((len) >= 4);
        reader.input = input + 4;
        v4u64 sub_len = __builtin_convertvector(
                (v4u32)_mm_loadu_si128((__m128i*) input), v4u64);
        reader.start = v4u64{0, sub_len[0], 
                sub_len[0] + sub_len[1], sub_len[0] + sub_len[1] + sub_len[2]};
        reader.cursor = reader.start + sub_len - 1;
        __m128i mask = _mm256_cvtepi64_epi32(_mm256_cmpgt_epi64((__m256i) sub_len, _mm256_setzero_si256()));
        reader.buffer = __builtin_convertvector(
                (v4u32)_mm256_mask_i64gather_epi32(_mm_setzero_si128(), (int*) reader.input, 
                        (__m256i) reader.cursor, mask, 4), v4u64) << 32;

        reader.bitcnt = v4u64{32, 32, 32, 32};
        return reader;
}

__force_inline v4u32 
peek4XV(BitReader4XV reader, v4u64 len) 
{
        return __builtin_convertvector(reader.buffer >> (64 - len), v4u32);
}

__force_inline BitReader4XV 
forward(BitReader4XV reader, v4u64 len) 
{
        _mm_prefetch(&reader.input[reader.cursor[0]], _MM_HINT_T0);
        reader.bitcnt = (v4u64)(reader.bitcnt >= len) ? reader.bitcnt - len : 0;
        reader.buffer <<= len;
        v4i64 load = reader.bitcnt < 32 && reader.cursor > reader.start;
        reader.cursor = load ? reader.cursor - 1 : reader.cursor;
        reader.bitcnt = load ? reader.bitcnt + 32 : reader.bitcnt;
        v4u64 tmp = load ? 
                __builtin_convertvector(v4u32{
                        reader.input[reader.cursor[0]],
                        reader.input[reader.cursor[1]],
                        reader.input[reader.cursor[2]],
                        reader.input[reader.cursor[3]]
                }, v4u64) : 0;
        reader.buffer |= tmp << (64 - reader.bitcnt);
        return reader;
}

#endif