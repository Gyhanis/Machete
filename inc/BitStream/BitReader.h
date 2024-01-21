#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "BitDefine.h"



typedef struct {
        uint32_t* data;
        int64_t len;
        uint64_t buffer;
        int64_t cursor;
        int64_t bitcnt;
#ifdef DEBUG
        int64_t ptr;
#endif 
} BitReader;

static inline void 
initBitReader(BitReader* reader, uint32_t * input, size_t len)
{
        assert(len >= 1);
        reader->data = (uint32_t*) input;
        reader->buffer = ((uint64_t) input[0]) << 32;
        reader->cursor = 1;
        reader->bitcnt = 32;
        reader->len = len;
#ifdef DEBUG
        reader->ptr = 0;
#endif 
}

static inline uint64_t 
peek(BitReader* reader, size_t len) {
        assert(len <= 32);
        return reader->buffer >> 64 - len;
}

static inline void 
forward(BitReader* reader, size_t len) {
        assert(len <= 32);
        reader->bitcnt -= len;
        reader->buffer <<= len;
        if (reader->bitcnt < 32) {
                if (reader->cursor < reader->len) {
                        uint64_t data = reader->data[reader->cursor];
                        reader->buffer |= data << (32 - reader->bitcnt);
                        reader->bitcnt += 32;
                        reader->cursor ++;
                } else {
                        reader->bitcnt = 64;
                }
        }
#ifdef DEBUG
        reader->ptr += len;
#endif
}

static inline uint64_t 
readLong(BitReader* reader, size_t len) {
        if (len == 0) return 0;
        uint64_t result = 0;
        if (len > 32) {
                result = peek(reader, 32);
                forward(reader, 32);
                result <<= len - 32;
                len -= 32;
        } 
        result |= peek(reader, len);
        forward(reader, len);
        return result;
}
