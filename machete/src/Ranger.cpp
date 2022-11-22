#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../include/BitWriter.h"
#include "../include/BitReader.h"
#include "../include/Ranger.h"

typedef struct {
        uint32_t part;
        uint32_t data[0];
} RangerHeader;

typedef union {
        uint64_t data;
        struct {
                uint32_t code;
                uint16_t clen;
                uint16_t dlen;
        };
        struct {
                uint32_t clen2;
                uint32_t dlen2;
        };
} RangeEntry;

RangerEnc* ranger_probe(int32_t *data, size_t data_len) {
        RangerEnc* ranger = (RangerEnc*) malloc(sizeof(*ranger));
        ranger->data = data;
        ranger->data_len = data_len;
        ranger->data_bits = (uint32_t*) malloc(sizeof(uint32_t) * data_len);
        
        // statistics
        int range_cnt[33];
        int range_sum[33];
        memset(range_cnt, 0, sizeof(range_cnt));
        memset(range_sum, 0, sizeof(range_cnt));
        for (int i = 0; i < data_len; i++) {
                int32_t tmp = data[i] < 0 ? ~data[i] : data[i];
                ranger->data_bits[i] = 33 - __builtin_clz(tmp);
                range_cnt[ranger->data_bits[i]] ++;
        }

        int i, j;
        int range_len = 0;
        int range[33];
        for (i = 1; i <=32; i++) {
                if (__builtin_expect(range_cnt[i], 1)) {
                        range_sum[0] = range_cnt[i];
                        range[0] = i;
                        break;
                }
        }
        range_len = 1;
        for (i++; i <= 32; i++) {
                if (range_cnt[i]) {
                        range_sum[range_len] = range_sum[range_len-1] + range_cnt[i];
                        range[range_len] = i;
                        range_len++;
                }
        }

        // if one range only
        if (range_len == 1) {
                ranger->out_len = (range[0] * ranger->data_len + 31) / 32;
                ranger->out_len += sizeof(RangerHeader) / sizeof(uint32_t);
                ranger->best_p = 1 << range[0];
                return ranger;
        }

        int len1 = range_len - 1;
        
        uint32_t C[range_len];  // cost of best partition
        uint32_t A[range_len];  // additional cost when adding new range      
        uint64_t P[range_len];  // best partition

        C[0] = range_sum[len1] * range[0];
        A[0] = range_sum[len1];
        P[0] = 1 << range[0];

        for (int i = 1; i < range_len; i++) {
                C[i] = range_sum[len1] * range[i];
                A[i] = range_sum[len1];
                P[i] = 1L << range[i];
                for (int j = 0; j < i; j++) {
                        int s = C[j] + A[j] + (range[i] - range[j])*(range_sum[len1] - range_sum[j]);
                        if (s < C[i]) {
                                C[i] = s;
                                A[i] = range_sum[len1] - range_sum[j];
                                P[i] = (1L << range[i]) | P[j];
                        }
                }
        }

        ranger->out_len = (C[len1] + 31) / 32 + (sizeof(RangerHeader) / sizeof(uint32_t));
        ranger->best_p = P[len1];
        return ranger;
}



uint32_t * ranger_encode(RangerEnc* ranger) {
        assert(ranger->best_p != 0);
        RangerHeader* header = (RangerHeader*) ranger->output;
        header->part = ranger->best_p >> 1;
        BitWriter writer = initBitWriter(writer, header->data, ranger->out_len - (sizeof(RangerHeader) / sizeof(uint32_t)));
        int min_len = __builtin_ctzll(ranger->best_p);
        
        if (__builtin_popcountll(ranger->best_p) == 1) {
                for (int i = 0; i < ranger->data_len; i++) {
                        writer = write(writer, ranger->data[i], min_len);
                }
                writer = flush(writer);
                uint32_t* result = ranger->output;
                ranger_free(ranger);
                return result;
        }

        RangeEntry entries[33];
        RangeEntry entry;
        entry.code = 0;
        entry.clen = 1;
        entry.dlen = min_len;
        int i = 1;
        while(__builtin_popcountll(ranger->best_p) > 1) {
                for (; i <= min_len; i++) {
                        entries[i].data = entry.data;
                }
                ranger->best_p &= ~(1L << min_len);
                min_len = __builtin_ctzll(ranger->best_p);
                entry.code |= 1;
                entry.code <<= 1;
                entry.clen += 1;
                entry.dlen = min_len;
        }

        entry.code >>= 1;
        entry.clen -= 1;
        for (; i <= min_len; i++) {
                entries[i].data = entry.data;
        }

        for (i = 0; i < ranger->data_len; i++) {
                entry.data = entries[ranger->data_bits[i]].data;
                writer = write(writer, entry.code, entry.clen);
                writer = write(writer, ranger->data[i], entry.dlen);
        }
        writer = flush(writer);

        uint32_t *result = ranger->output;
        ranger_free(ranger);
        return result;
}

// #include <time.h>
// #include <stdio.h>
void ranger_decode(uint32_t* input, size_t input_len, int32_t* data, size_t data_len) {
        // clock_t start, dur;
        // start = clock();
        RangerHeader * header = (RangerHeader*) input;
        uint64_t partition = header->part;
        partition <<= 1;
        assert(partition != 0);
        int range_cnt = __builtin_popcountll(partition);
        int min_len = __builtin_ctzll(partition);
        BitReader reader = initBitReader(reader, input, input_len);
        if (range_cnt == 1) {
                for (int i = 0; i < data_len; i++) {
                        int d = peek(reader, min_len);
                        reader = forward(reader, min_len);
                        data[i] = d << (32 - min_len) >> (32 - min_len);
                }
                return;
        } 

        int ent_cnt = 1 << (range_cnt - 1);
        RangeEntry entries[ent_cnt];
        RangeEntry entry;
        entry.clen2 = 1;
        entry.dlen2 = min_len;
        uint32_t mask = 1 << (range_cnt-2);
        for (uint32_t i = 0; i < ent_cnt; i++) {
                if (mask & i) {
                        mask >>= 1;
                        entry.clen2 ++;
                        partition &= ~(1L << min_len);
                        min_len = __builtin_ctzll(partition);
                        entry.dlen2 = min_len;
                }
                entries[i].data = entry.data;
        }
        entries[ent_cnt-1].clen2 --;
        size_t code_max_len = entries[ent_cnt-1].clen2;
        // printf("%d\n", clock() - start);
        
        // start = clock();
        for (int i = 0; i < data_len; i++) {
                int d = peek(reader, code_max_len);
                entry.data = entries[d].data;
                reader = forward(reader, entry.clen2);
                d = peek(reader, entry.dlen2);
                reader = forward(reader, entry.dlen2);
                data[i] = d << (32 - entry.dlen2) >> (32 - entry.dlen2);
        }
        // printf("%d\n", clock() - start);
        return;
}