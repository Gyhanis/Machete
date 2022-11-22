#ifndef __BIT_WRITER_H
#define __BIT_WRITER_H

#include <immintrin.h>
#include <stdint.h>
#include <assert.h>
#include "vector.h"

typedef struct {
        uint32_t * output;
        uint64_t buffer;
        int64_t cursor;
        int64_t bitcnt;
}BitWriter;

__force_inline BitWriter
initBitWriter(BitWriter writer, uint32_t* output, uint64_t outsize){
        assert(outsize >= 1);
        writer.output = output;
        writer.buffer = 0;
        writer.cursor = outsize -1;
        writer.bitcnt = 0;
        return writer;
}

__force_inline BitWriter 
write(BitWriter writer, uint64_t data, uint64_t length)
{
        assert(length <= 32);
        data <<= (64 - length);
        writer.buffer |= data >> writer.bitcnt;
        writer.bitcnt += length;
        if (__builtin_expect(writer.bitcnt >= 32, 0)) {
                assert(writer.cursor >= 0);
                writer.output[writer.cursor] = writer.buffer >> 32;
                writer.cursor --;
                writer.buffer <<= 32;
                writer.bitcnt -= 32;
        }
        return writer;
}

__force_inline BitWriter
flush(BitWriter writer) {
        if (writer.bitcnt) {
                assert(writer.cursor >= 0);
                writer.output[writer.cursor] = writer.buffer >> 32;
                writer.buffer = 0;
                writer.bitcnt = 0;
        }
        return writer;
}

typedef struct {
        uint32_t * output;
        v4i64 start;
        v4i64 cursor;
        v4u64 buffer;
        v4i64 bitcnt;
}BitWriter4XV;

// __force_inline BitWriter4XV 
// initBitWriter4XV(BitWriter4XV writer, uint32_t * output, v4i64 outsize) 
// {
//         writer.output = output + 4;
//         _mm_store_si128((__m128i*) output, (__m128i) outsize);
//         writer.start = v4i32{0, outsize[1], outsize[1] + outsize[2], 
//                         outsize[1] + outsize[2] + outsize[3]};
//         writer.buffer = (v4u64)_mm256_setzero_si256();
//         writer.bitcnt = (v4u64)_mm256_setzero_si256();
//         writer.cursor = writer.start + outsize - 1;
//         return writer;
// }

__force_inline BitWriter4XV
initBitWriter4XV(BitWriter4XV writer, int64_t lane_size)
{
        writer.output =(uint32_t*) aligned_alloc(16, lane_size * sizeof(uint32_t) * 4);
        writer.start = v4i64{0, lane_size, lane_size * 2, lane_size * 3};
        writer.buffer = (v4u64)_mm256_setzero_si256();
        writer.bitcnt = (v4i64)_mm256_setzero_si256();
        writer.cursor = writer.start + lane_size - 1;
        return writer;
}

__force_inline BitWriter4XV
write(BitWriter4XV writer, v4u64 data, v4u64 length) 
{
        __mmask8 mask = _mm256_cmpgt_epi64_mask((__m256i) writer.start, (__m256i) writer.cursor);
        mask &= _mm256_cmpgt_epi64_mask((__m256i)length, _mm256_setzero_si256());
        mask |= _mm256_cmpgt_epi64_mask((__m256i) length, _mm256_set1_epi64x(32));
        assert(mask == 0);
        data <<= (64 - length);
        writer.buffer |= data >> (v4u64) writer.bitcnt;
        writer.bitcnt += (v4i64) length;
        v4i64 flush = writer.bitcnt > 32;
        __mmask8 flush_m = _mm256_movepi64_mask((__m256i)flush);
        __m128i data0 = (__m128i) __builtin_convertvector(writer.buffer >> 32, v4u32);
        _mm256_mask_i64scatter_epi32(writer.output, flush_m, (__m256i) writer.cursor, data0, 4);
        writer.cursor = flush ? writer.cursor - 1 : writer.cursor;
        writer.bitcnt = flush ? writer.bitcnt - 32 : writer.bitcnt;
        writer.buffer = flush ? writer.buffer << 32: writer.buffer;
        return writer;
}

__force_inline BitWriter4XV
flush(BitWriter4XV writer) 
{
        __mmask8 do_flush = _mm256_cmpgt_epi64_mask((__m256i) writer.bitcnt, _mm256_setzero_si256());
        __mmask8 mask = _mm256_cmpgt_epi64_mask((__m256i) writer.start, (__m256i) writer.cursor);
        assert((do_flush & mask) == 0);
        __m128i data = (__m128i) __builtin_convertvector(writer.buffer >> 32, v4u32);
        _mm256_mask_i64scatter_epi32(writer.output, do_flush, (__m256i) writer.cursor, data, 4);
        writer.cursor = (v4i64) _mm256_mask_sub_epi64((__m256i) writer.cursor, do_flush,(__m256i) writer.cursor, _mm256_set1_epi64x(1));
        return writer;
}

__force_inline int 
getOutputLen(BitWriter4XV writer) 
{
        uint32_t lane_len = writer.start[1];
        uint32_t len0 = lane_len - writer.cursor[0] - 1;
        uint32_t len1 = lane_len - writer.cursor[1] - 1;
        uint32_t len2 = lane_len - writer.cursor[2] - 1;
        uint32_t len3 = lane_len - writer.cursor[3] - 1;
        return 4 + len0 + len1 + len2 + len3;
}

__force_inline uint32_t 
exportOutput(BitWriter4XV writer, uint32_t *output) 
{
        uint32_t lane_len = writer.start[1];
        uint32_t len0 = lane_len - writer.cursor[0] - 1;
        uint32_t len1 = 2*lane_len - writer.cursor[1] - 1;
        uint32_t len2 = 3*lane_len - writer.cursor[2] - 1;
        uint32_t len3 = 4*lane_len - writer.cursor[3] - 1;
        output[0] = len0;
        output[1] = len1;
        output[2] = len2;
        output[3] = len3;
        uint32_t cur = 4;
        memcpy(&output[cur], &writer.output[writer.cursor[0] + 1], sizeof(uint32_t) * len0);
        cur += len0;
        memcpy(&output[cur], &writer.output[writer.cursor[1] + 1], sizeof(uint32_t) * len1);
        cur += len1;
        memcpy(&output[cur], &writer.output[writer.cursor[2] + 1], sizeof(uint32_t) * len2);
        cur += len2;
        memcpy(&output[cur], &writer.output[writer.cursor[3] + 1], sizeof(uint32_t) * len3);
        free(writer.output);
        return 4 + len0 + len1 + len2 + len3;
}

#endif