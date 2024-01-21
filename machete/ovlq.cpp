#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "defs.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

#define DIV_UP(x,b)     (((x) + (1<<(b)) - 1) >> (b))
#define ALIGN_UP(x,b)   (((x) + (1 << (b)) - 1) & ~((1 << (b))-1))

struct OVLQ_Header {
        uint32_t len;
        uint32_t mapping;
        uint8_t payload[0];
};


static inline int data_bitlen(int32_t data) {
        data = data < 0 ? ~data : data;
        return (data == 0) ? 1 : 33 - __builtin_clz(data);
}

int32_t ovlq_search_optimal_mapping(int32_t *data_min_bitlen, ssize_t len, ssize_t &total_bitlen) {
        int32_t *buffer = new int32_t[99] {0};
        int32_t *range_cnt = buffer;
        int32_t *range_sum = range_cnt + 33;
        int32_t *range = range_sum + 33;

        for (int i = 0; i < len; i++) {
                range_cnt[data_min_bitlen[i]]++;
        }

        int i;
        for (i = 1; i <= 32; i++) {
                if (range_cnt[i]) {
                        range_sum[0] = range_cnt[i];
                        range[0] = i;
                        break;
                }
        }
        int range_top = 1;
        for (i++; i <= 32; i++) {
                if (range_cnt[i]) {
                        range_sum[range_top] = range_sum[range_top-1] + range_cnt[i];
                        range[range_top++] = i;
                }
        }

        if (range_top == 1) {
                total_bitlen = range[0] * len;
                int32_t mapping = 1 << (range[0] - 1);
                delete[] buffer;
                return mapping;
        } 

        int32_t* buffer2 = new int32_t[range_top*3];
        int32_t* C = buffer2;
        int32_t* A = C + range_top;
        int32_t* M = A + range_top;

        C[0] = len * range[0];
        A[0] = len;
        M[0] = 1 << (range[0]-1);

        for (i = 1; i < range_top; i++) {
                C[i] = len * range[i];
                A[i] = len;
                M[i] = 1L << (range[i]-1);
                for (int j = 0; j < i; j++) {
                        int c = C[j] + A[j] + (range[i] - range[j]) * (len - range_sum[j]);
                        if (c < C[i]) {
                                C[i] = c;
                                A[i] = len - range_sum[j];
                                M[i] = (1 << (range[i]-1)) | M[j];
                        }
                }
        }

        int32_t optimal_mapping = M[range_top-1];
        total_bitlen = C[range_top-1];
        delete[] buffer;
        delete[] buffer2;
        return optimal_mapping;
}

struct OVLQ_EncodeTableEntry {
        int32_t flen;
        int32_t flag;
        int32_t dlen;
};

typedef OVLQ_EncodeTableEntry* OVLQ_EncodeTable;

OVLQ_EncodeTable ovlq_build_encode_table_with_mapping(int32_t mapping) {
        int level = __builtin_popcount(mapping);
        assert(level > 0);
        if (level > 1) {
                OVLQ_EncodeTable table = new OVLQ_EncodeTableEntry[33];
                int t = 1;
                int flag = 0;
                for (int i = 1; i < level; i++) {
                        int dlen = __builtin_ctz(mapping) + 1;
                        for (; t <= dlen; t++) {
                                table[t] = {i, flag, dlen};
                        }
                        mapping &= ~(1L << (dlen - 1));
                        flag |= 1 << i;
                }
                int dlen = __builtin_ctz(mapping) + 1;
                for (; t <= dlen; t++) {
                        table[t] = {level-1, flag >> 1, dlen};
                }
                return table;
        } else {
                return NULL;
        }
}

ssize_t ovlq_encode(int32_t* input, ssize_t len, uint8_t** output) {
        int32_t *data_min_bitlen = new int32_t[len];
        for (int i = 0; i < len; i++) {
                data_min_bitlen[i] = data_bitlen(input[i]);
        }
        ssize_t optimal_total_bitlen;
        int32_t optimal_mapping = ovlq_search_optimal_mapping(data_min_bitlen, len, optimal_total_bitlen);
        OVLQ_EncodeTable table = ovlq_build_encode_table_with_mapping(optimal_mapping);

        ssize_t osize = sizeof(OVLQ_Header) + ALIGN_UP(DIV_UP(optimal_total_bitlen,3),2);
        *output = reinterpret_cast<uint8_t*>(malloc(osize));
        OVLQ_Header *header = reinterpret_cast<OVLQ_Header*>(*output);
        header->len = len;
        header->mapping = optimal_mapping;

        BitWriter writer;
        initBitWriter(&writer, reinterpret_cast<uint32_t*>(header->payload), (osize - sizeof(OVLQ_Header))/sizeof(uint32_t));
        if (table == nullptr) {
                int bitlen = __builtin_ctz(optimal_mapping) + 1;
                for (int i = 0; i < len; i++) {
                        write(&writer, input[i], bitlen);
                }
        } else {
                for (int i = 0; i < len; i++) {
                        auto entry = &table[data_min_bitlen[i]];
                        write(&writer, entry->flag, entry->flen);
                        write(&writer, input[i], entry->dlen);
                }
                delete[] table;
        }
        flush(&writer);
        delete[] data_min_bitlen;
        return osize;
}

struct OVLQ_DecodeTableEntry {
        int32_t flen;
        int32_t dlen;
};

typedef OVLQ_DecodeTableEntry* OVLQ_DecodeTable;

OVLQ_DecodeTable ovlq_build_decode_table_with_mapping(int64_t mapping, ssize_t &index_bitlen) {
        int32_t level = __builtin_popcountll(mapping);
        int32_t min_len = __builtin_ctzll(mapping);
        int ent_cnt = 1 << (level - 1);
        OVLQ_DecodeTable table = new OVLQ_DecodeTableEntry[ent_cnt];
        uint32_t mask = 1 << (level - 2);
        OVLQ_DecodeTableEntry entry = {1, min_len};
        for (int i = 0; i < ent_cnt; i++) {
                if (mask & i) {
                        mask >>= 1;
                        entry.flen++;
                        mapping &= ~(1 << min_len);
                        min_len = __builtin_ctz(mapping);
                        entry.dlen = min_len;
                }
                table[i] = entry;
        }
        table[ent_cnt - 1].flen--;
        index_bitlen = table[ent_cnt - 1].flen;
        return table;
}

ssize_t ovlq_decode(uint8_t* input, ssize_t size, int32_t* output) {
        OVLQ_Header *header = reinterpret_cast<OVLQ_Header*>(input);
        uint64_t mapping = static_cast<uint64_t>(header->mapping) << 1;
        BitReader reader;
        initBitReader(&reader, reinterpret_cast<uint32_t*>(header->payload), (size - sizeof(OVLQ_Header))/sizeof(uint32_t));
        int32_t level = __builtin_popcountll(mapping);
        if (level == 1) {
                int32_t bitlen = __builtin_ctzll(mapping);
                for (int i = 0; i < header->len; i++) {
                        int32_t d = peek(&reader, bitlen);
                        forward(&reader, bitlen);
                        output[i] = d << (32 - bitlen) >> (32 - bitlen);
                }
                return header->len;
        } else {
                ssize_t index_bitlen;
                OVLQ_DecodeTable table = ovlq_build_decode_table_with_mapping(mapping, index_bitlen);
                for (int i = 0; i < header->len; i++) {
                        int index = peek(&reader, index_bitlen);
                        auto entry = table[index];
                        forward(&reader, entry.flen);
                        int data = peek(&reader, entry.dlen);
                        forward(&reader, entry.dlen);
                        output[i] = data << (32 - entry.dlen) >> (32 - entry.dlen);
                }
                delete[] table;
                return header->len;
        }
}