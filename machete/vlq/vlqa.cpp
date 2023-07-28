#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "../util.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

#include "vlqa.h"

int data_bitlen(int32_t data) {
        return 33 - __builtin_clz(data < 0 ? ~data : data);
}

VLQA::VLQA(int32_t* din, int32_t din_len) {
        this->din = din;
        this->din_len = din_len;
        this->bitlen = (int32_t*) malloc(sizeof(int32_t) * din_len);
        for (int i = 0; i < din_len; i++) {
                this->bitlen[i] = data_bitlen(din[i]);
        }
}

void VLQA::VLQA_build_table(int32_t set) {
        int level = __builtin_popcount(set);
        assert(level > 0);
        this->set = set;
        if (level == 1) {
                monolvl = __builtin_ctz(set) + 1;
        } else {
                monolvl = 0;
                int t = 1;
                int flag = 0;
                for (int i = 1; i < level; i++) {
                        int dlen = __builtin_ctz(set) + 1;
                        for (; t <= dlen; t++) {
                                table[t] = {i, flag, dlen};
                        }
                        set &= ~(1L << (dlen - 1));
                        flag |= 1 << i;
                }
                int dlen = __builtin_ctz(set) + 1;
                for (; t <= dlen; t++) {
                        table[t] = {level-1, flag >> 1, dlen};
                }
        }
}

// int32_t debug[1000];

size_t VLQA::VLQA_encode(void *out, int32_t out_size) {
        VLQA_Header * header = (VLQA_Header*) out;
        header->set = set;
        BitWriter writer;
        initBitWriter(&writer, header->payload, (out_size - sizeof(VLQA_Header))/4);
        if (monolvl) {
                for (int i = 0; i < din_len; i++) {
                        write(&writer, din[i], monolvl);
                }
        } else {
                for (int i = 0; i < din_len; i++) {
                        VLQA_EENT *entry = &table[bitlen[i]];
                        write(&writer, entry->flag, entry->flen);
                        write(&writer, din[i], entry->dlen);
                }
        }
        int len = flush(&writer);
        return sizeof(VLQA_Header) + len * sizeof(uint32_t);
}

int32_t VLQA::VLQA_search_optimal_set(size_t *outsize) {
        int32_t *buffer = (int32_t*) calloc(sizeof(int32_t), 99);
        int32_t *range_cnt = buffer;
        int32_t *range_sum = range_cnt + 33;
        int32_t *range = range_sum + 33;

        for (int i = 0; i < din_len; i++) {
                range_cnt[bitlen[i]]++;
        }

        int i;
        for (i = 1; i < 32; i++) {
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
                if (outsize) 
                        *outsize = sizeof(VLQA_Header) + ALIGN_UP(DIV_UP(range[0] * din_len,3),2);
                int32_t set = 1 << (range[0] - 1);
                free(buffer);
                return set;
        }

        int32_t C[range_top];
        int32_t A[range_top];
        int32_t S[range_top];

        C[0] = din_len * range[0];
        A[0] = din_len;
        S[0] = 1 << (range[0]-1);

        for (i = 1; i < range_top; i++) {
                C[i] = din_len * range[i];
                A[i] = din_len;
                S[i] = 1L << (range[i]-1);
                for (int j = 0; j < i; j++) {
                        int c = C[j] + A[j] + (range[i] - range[j]) * (din_len - range_sum[j]);
                        if (c < C[i]) {
                                C[i] = c;
                                A[i] = din_len - range_sum[j];
                                S[i] = (1 << (range[i]-1)) | S[j];
                        }
                }
        }

        free(buffer);
        if (outsize) *outsize = sizeof(VLQA_Header) + ALIGN_UP(DIV_UP(C[range_top-1], 3), 2);
        return S[range_top-1];
}

size_t VLQA::VLQA_encode_with_set(int32_t set, void* output, int32_t out_size) {
        VLQA_build_table(set);
        return VLQA_encode(output, out_size);
}

struct VLQA_DENT {
        int32_t flen;
        int32_t dlen;
};

size_t VLQA::VLQA_decode(void* in, int32_t in_size, int32_t *out, int32_t out_len) {
        VLQA_Header *header = (VLQA_Header*) in;
        uint64_t set = (uint64_t) header->set;
        set <<= 1;
        int32_t level = __builtin_popcountll(set);
        int32_t min_len = __builtin_ctzll(set);
        BitReader reader;
        initBitReader(&reader, header->payload, (in_size - sizeof(VLQA_Header))/sizeof(uint32_t));
        if (level == 1) {
                for (int i = 0; i < out_len; i++) {
                        int32_t d = peek(&reader, min_len);
                        forward(&reader, min_len);
                        out[i] = d << (32 - min_len) >> (32 - min_len);
                }
                return out_len;
        } else {
                int ent_cnt = 1 << (level - 1);
                VLQA_DENT *table = (VLQA_DENT*) malloc(sizeof(VLQA_DENT) * ent_cnt);
                uint32_t mask = 1 << (level - 2);
                VLQA_DENT entry = {1, min_len};
                for (int i = 0; i < ent_cnt; i++) {
                        if (mask & i) {
                                mask >>= 1;
                                entry.flen++;
                                set &= ~(1 << min_len);
                                min_len = __builtin_ctz(set);
                                entry.dlen = min_len;
                        }
                        table[i] = entry;
                }
                table[ent_cnt - 1].flen--;
                size_t flen_max = table[ent_cnt - 1].flen;

                for (int i = 0; i < out_len; i++) {
                        int d = peek(&reader, flen_max);
                        entry = table[d];
                        forward(&reader, entry.flen);
                        d = peek(&reader, entry.dlen);
                        forward(&reader, entry.dlen);
                        out[i] = d << (32 - entry.dlen) >> (32 - entry.dlen);
                }
                free(table);
        }
        return out_len;
}