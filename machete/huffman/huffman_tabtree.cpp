#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <queue>
#include <stack>
#include <stdlib.h>
#include <assert.h>
#include "huffman.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

#define COST_OF_TABLE_WRITE 5
#define COST_OF_TABLE_LOOKUP 2



typedef struct __attribute__((__packed__)){
        uint16_t data_len;
        uint16_t sym_cnt;
        uint16_t max_bitlen;
        uint16_t min_bitlen;
        uint16_t breaking;
        uint16_t reserved;
        uint8_t  payload[0];
} HufHeader;

static bool less_on_lvl(HTNode *n0, HTNode* n1) {
        return n0->lvl > n1->lvl;
}

void HuffmanDEnc::Huffman_build_sorted_codebook() {
        huf_tree->lvl = 0;
        std::stack<HTNode*> stack;
        std::priority_queue<HTNode*, std::vector<HTNode*>, decltype(&less_on_lvl)> queue(less_on_lvl);
        stack.push(huf_tree);

        max_bitlen = 0;
        while (!stack.empty()) {
                HTNode* n = stack.top();
                stack.pop();
                if (n->left) {
                        n->left->lvl = n->lvl + 1;
                        n->right->lvl = n->lvl + 1;
                        stack.push(n->right);
                        stack.push(n->left);
                } else {
                        queue.push(n);
                }
        }


        int cnt = 0;
        int32_t code = 0;
        int code_len = 0;
        min_bitlen = queue.top()->lvl;
        HTNode* n;
        while (!queue.empty()) {
                n = queue.top();
                queue.pop();
                code <<= n->lvl - code_len;
                code_len = n->lvl;
                codebook[n->symbol] = std::make_pair(code, code_len);
                symbols[cnt++] = n->symbol;
                code++;
                code_size += n->lvl * n->cnt;
                bitlen_cnt_symbol[n->lvl] += n->cnt;
                bitlen_cnt_value[n->lvl] ++;
        }
        max_bitlen = n->lvl;
        free(node_pool);
}

void HuffmanDEnc::Huffman_search_optimal_breaking() {
        uint32_t bitlen_sum[32];
        bitlen_sum[min_bitlen] = bitlen_cnt_symbol[min_bitlen];
        for (int i = min_bitlen+1; i <= max_bitlen; i++) {
                bitlen_sum[i] = bitlen_sum[i-1] + bitlen_cnt_symbol[i];
        }

        int16_t _tabcnt[32];
        breaking = max_bitlen;
        int min_cost = (1 << max_bitlen) * COST_OF_TABLE_WRITE + bitlen_sum[max_bitlen] * COST_OF_TABLE_LOOKUP;
        int bec, blt;
        STAT_BLOCK(
                bec = (1 << max_bitlen);
                blt = (bitlen_sum[max_bitlen]);
        )
        for (int i = min_bitlen; i < max_bitlen; i++) {
                int entry_cnt = 1 << i;
                int remainder = 0;
                for (int j = i + 1; j <= max_bitlen; j++) {
                        remainder = remainder * 2 + bitlen_cnt_value[j];
                        int idx_len = j - i;
                        _tabcnt[j] = remainder >> idx_len;
                        entry_cnt += _tabcnt[j] << idx_len;
                        remainder = remainder - (_tabcnt[j] << idx_len);
                }
                int cost = entry_cnt * COST_OF_TABLE_WRITE;
                cost += (2*bitlen_sum[max_bitlen] - bitlen_sum[i]) * COST_OF_TABLE_LOOKUP;
                if (cost < min_cost) {
                        breaking = i;
                        min_cost = cost;
                        __builtin_memcpy(tabcnt, &_tabcnt[i+1], (max_bitlen - i)*2);
                        SET(bec, entry_cnt);
                        SET(blt, 2*bitlen_sum[max_bitlen] - bitlen_sum[i]);
                }
        }
        ADD(Huffman_table_len,  bec);
        ADD(Huffman_lookup_time, blt);
}

HuffmanDEnc::HuffmanDEnc(FreqTab *table, int32_t* data, size_t data_len, int32_t* workspace) 
:HuffmanEnc(table, data, data_len)
{
        sym_cnt = freq_tab->size();
        symbols = (int32_t*) calloc(sizeof(*this->symbols), data_len);
        code_size = 0;
        buffer = (int32_t*) calloc(sizeof(int32_t), 32 + 32 + 16);
        bitlen_cnt_symbol = buffer;
        bitlen_cnt_value = bitlen_cnt_symbol + 32;
        tabcnt = (int16_t*) (bitlen_cnt_value + 32);
        RECORD(Huffman_encode_tree_time, Huffman_build_tree());
        RECORD(Huffman_encode_codebook_time, Huffman_build_sorted_codebook());
        RECORD(Huffman_encode_search_time, Huffman_search_optimal_breaking());
        out_size = sizeof(HufHeader);
        out_size += codebook.size() * sizeof(int32_t);
        int tmp = max_bitlen - min_bitlen + 1;
        out_size += ALIGN_UP(tmp *sizeof(int16_t), 2);
        out_size += ALIGN_UP((max_bitlen - breaking) * sizeof(int16_t),2);
        out_size += ALIGN_UP(DIV_UP(code_size,3),  2);
}

int32_t HuffmanDEnc::Huffman_encode(void* output) {
        TIK;
        HufHeader* header = (HufHeader*) output;
        header->data_len = data_len;
        header->sym_cnt = sym_cnt;
        header->max_bitlen = max_bitlen;
        header->min_bitlen = min_bitlen;
        header->breaking = breaking;
        
        int32_t* p = (int32_t*) header->payload;
        __builtin_memcpy(p, symbols, sizeof(*symbols) * sym_cnt);
        p += sym_cnt;

        int16_t* code_len = (int16_t*) p;
        __builtin_memset(code_len, 0, (max_bitlen - min_bitlen + 1)*sizeof(int16_t));
        for (auto e: codebook) {
                code_len[e.second.second-min_bitlen]++;
        }
        p = (int32_t*) (code_len + ALIGN_UP(max_bitlen - min_bitlen + 1,1));

        __builtin_memcpy(p, tabcnt, ALIGN_UP((max_bitlen - breaking)*2,2));
        
        if (__builtin_expect(sym_cnt > 1, 1)) {
                p += DIV_UP((max_bitlen - breaking)*2, 2);

                BitWriter writer;
                initBitWriter(&writer, (uint32_t*)p, DIV_UP(code_size,5));

                for (int i = 0; i < data_len; i++) {
                        auto e = codebook[data[i]];
                        write(&writer, e.first, e.second);
                }
                flush(&writer);
        }
        free(symbols);
        free(buffer);
        TOK(Huffman_encode_time);
        return out_size;
}

void HuffmanDDec::Huffman_build_codebook() {
        int tablen = 1 << breaking;
        for (int i = breaking+1; i <= max_bitlen; i++) {
                tablen += tabcnt[i] << (i - breaking);
        }
        codebook = (HDEntry*) malloc(sizeof(HDEntry) * tablen);
        int next_table = 1 << breaking;
        int cur = 0;
        int tabsize = breaking+1;

        // fill root: symbol part
        int s;
        for (s = 0; s < sym_cnt && bitlen[s] <= breaking; s++) {
                int times = 1 << (breaking - bitlen[s]);
                for (int j = 0; j < times; j++) {
                        codebook[cur++] = {symbols[s], bitlen[s]};
                }
        }

        // fill root: table part
        for (int i = breaking+1; i <= max_bitlen; i++) {
                int tmp = i - breaking;
                int t0 = -tmp;
                int t1 = 1 << tmp;
                for (int j = 0; j < tabcnt[i]; j++) {
                        codebook[cur++] = {next_table, t0};
                        next_table += t1;
                }
        }
        assert(cur==(1<<breaking));
        assert(next_table == tablen);

        // fill children
        int taken = 0;
        for (int i = breaking+1; i <= max_bitlen; i++) {
                if (__builtin_expect(bitlen[s] > i, 0)) {
                        taken *= 2;
                        continue;
                }
                int bl = i - breaking;
                int cnt = (tabcnt[i] << bl) - taken;
                int j,k;
                for (j = 0; j < cnt; j++) {
                        codebook[cur++] = {symbols[s++], bl};
                }

                for (k = i + 1; k <= max_bitlen && tabcnt[k]==0; k++);
                int times = 1 << (k - i);
                taken = (bitlen_cnt[i-min_bitlen] - cnt) * 2;
                for (; j < bitlen_cnt[i-min_bitlen]; j++) {
                        for (int l = 0; l < times; l++) {
                                codebook[cur++] = {symbols[s], bl};
                        }
                        s++;
                }
        }
        assert(cur==tablen);
}

int32_t HuffmanDDec::Huffman_decode(void* data, size_t data_len, int32_t* output) {
        HuffmanDDec decoder;
        HufHeader *header = (HufHeader*) data;
        decoder.sym_cnt = header->sym_cnt;
        decoder.min_bitlen = header->min_bitlen;
        decoder.max_bitlen = header->max_bitlen;
        decoder.breaking = header->breaking;
        decoder.data_len = header->data_len;
        
        decoder.symbols = (int32_t*) header->payload;

        if (__builtin_expect(decoder.sym_cnt > 1, 1)) {
                decoder.bitlen_cnt = (int16_t*) (decoder.symbols + header->sym_cnt);
                decoder.bitlen = (int32_t*) malloc(sizeof(int32_t) * header->sym_cnt);
                int t = 0;
                for (int i = decoder.min_bitlen; i <= decoder.max_bitlen; i++) {
                        for (int j = 0; j < decoder.bitlen_cnt[i-decoder.min_bitlen]; j++) {
                                decoder.bitlen[t++] = i;
                        }
                }

                int16_t* tabcnt = (int16_t*) (decoder.bitlen_cnt 
                        + ALIGN_UP(decoder.max_bitlen - decoder.min_bitlen + 1, 1));
                __builtin_memcpy(&decoder.tabcnt[decoder.breaking+1], tabcnt, 
                        (decoder.max_bitlen - decoder.breaking)*2);
                RECORD(Huffman_decode_codebook_time, decoder.Huffman_build_codebook());

                TIK;
                uint32_t* code = (uint32_t*) (tabcnt + 
                        ALIGN_UP(decoder.max_bitlen - decoder.breaking, 1));
                BitReader reader;
                initBitReader(&reader, code, 
                        (data_len - ((int8_t*)code - (int8_t*)data)) / sizeof(uint32_t));
                for (int i = 0; i < decoder.data_len; i++) {
                        int code1 = peek(&reader, decoder.breaking);
                        if (decoder.codebook[code1].len > 0) {
                                output[i] = decoder.codebook[code1].sym;
                                forward(&reader, decoder.codebook[code1].len);
                        } else {
                                forward(&reader, decoder.breaking);
                                int code2 = peek(&reader, -decoder.codebook[code1].len);
                                HDEntry* subtab = decoder.codebook + decoder.codebook[code1].sym;
                                output[i] = subtab[code2].sym;
                                forward(&reader, subtab[code2].len);
                        }
                }
                TOK(Huffman_decode_time);
        } else {
                TIK;
                for (int i = 0; i < decoder.data_len; i++) {
                        output[i] = decoder.symbols[0];
                }
                TOK(Huffman_decode_time);
        }
        return decoder.data_len;
}