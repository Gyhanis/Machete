#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../include/Ranger.h"
#include "../include/Huffman.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <stack>

#define MAP_MAX_PREALLOC        8192

#define COST_OF_TABLE_WRITE     1
#define COST_OF_DECODE          1

#define RARE_SYM                0x80000000

#define USE_SCATTER
#include "../include/BitWriter.h"
#include "../include/BitReader.h"


struct great_on_cnt {
        bool operator() (const Node* a, const Node* b) {
                return a->cnt >= b->cnt;
        } 
};

struct great_on_lvl {
        bool operator() (const Node* a, const Node* b) {
                return a->lvl >= b->lvl;
        } 
};

HuffmanEncoder* HuffmanBuildTree(int32_t* data, size_t data_len) {
        HuffmanEncoder *encoder = (HuffmanEncoder*) calloc(1, sizeof(*encoder));
        encoder->symbols = (int32_t*) malloc(sizeof(int32_t) * data_len);
        encoder->rare_cnt = 0;
        encoder->sym_cnt = 0;
        encoder->codes = NULL;
        encoder->tables = NULL;
        encoder->data = data;
        encoder->data_len = data_len;
        encoder->enc_map = std::unordered_map<int32_t, EEntry> {};
        std::unordered_map<int32_t, uint32_t> map = {};
        map.reserve(data_len > MAP_MAX_PREALLOC ? MAP_MAX_PREALLOC : data_len); 
        for (int i = 0; i < data_len; i++) {
                map[data[i]] ++;
        }

        int32_t threshold = data_len >> 15;
        threshold = __builtin_expect(threshold > 1, 0) ? threshold : 1;

        for (int i = 0; i < data_len; i++) {
                if (map.count(data[i])) {
                        if (map[data[i]] <= threshold) {
                                encoder->symbols[encoder->rare_cnt++] = data[i];
                                map.erase(data[i]);
                                data[i] = RARE_SYM;
                        }
                } else {
                        encoder->symbols[encoder->rare_cnt++] = data[i];
                        data[i] = RARE_SYM;
                }
        }
        map[RARE_SYM] = encoder->rare_cnt;
        encoder->symbols += encoder->rare_cnt;

        std::priority_queue<Node*, std::vector<Node*>, great_on_cnt> queue;
        int rare_sym_cnt = 0;
        for (std::pair<int32_t, uint32_t> e : map) {
                Node * node = (Node*) malloc(sizeof(*node));
                node->sym = e.first;
                node->cnt = e.second;
                node->left = NULL;
                node->right = NULL;
                encoder->sym_cnt++;
                queue.push(node);
        }

        Node* n = queue.top();
        while (queue.size() > 1) {
                Node* n1 = queue.top();
                queue.pop();
                Node* n2 = queue.top();
                queue.pop();
                Node* n3 = (Node*) malloc(sizeof(*n3));
                n3->cnt = n1->cnt + n2->cnt;
                n3->left = n1; 
                n3->right = n2;
                queue.push(n3);
        }

        Node* root = queue.top();
        encoder->huffman_tree = root;
        return encoder;
}

#include <stdio.h>
void HuffmanSortNCode(HuffmanEncoder* encoder) {
        encoder->huffman_tree->lvl = 0;

        std::stack<Node*> stack;
        std::priority_queue<Node*, std::vector<Node*>, great_on_lvl> queue;
        stack.push(encoder->huffman_tree);
        while (!stack.empty()) {
                Node* n = stack.top();
                stack.pop();
                if (n->left) {
                        n->left->lvl = n->lvl + 1;
                        n->right->lvl = n->lvl + 1;
                        stack.push(n->right);
                        stack.push(n->left);
                        free(n);
                } else {
                        queue.push(n);
                }
        }
        encoder->huffman_tree = NULL;

        memset(encoder->bitlen_node_cnt, 0, sizeof(encoder->bitlen_node_cnt));
        memset(encoder->bitlen_sym_cnt, 0, sizeof(encoder->bitlen_sym_cnt));
        uint32_t sym_cnt = 0;
        uint32_t codes_size = 2 * ((encoder->sym_cnt + 1) & (-1 << 1));
        encoder->codes = (uint8_t*) malloc(codes_size);
        uint8_t codes_top = 0;

        uint32_t code = 0;
        uint32_t code_len = 0;
        uint32_t encode_len = 0;

        encoder->code_min_len = queue.top()->lvl;
        while (!queue.empty()) {
                Node* n = queue.top();
                queue.pop();

                encoder->symbols[sym_cnt++] = n->sym;
                if (code_len < n->lvl) {
                        if (encoder->bitlen_sym_cnt[code_len] > 0) {
                                encoder->codes[codes_top++] = code_len;
                                encoder->codes[codes_top++] = encoder->bitlen_node_cnt[code_len];
                        }
                        code <<= n->lvl - code_len;
                        code_len = n->lvl;
                }
                encoder->bitlen_sym_cnt[code_len] += n->cnt;
                encoder->bitlen_node_cnt[code_len]++;
                encoder->enc_map[n->sym] = EEntry{code, n->lvl};
                code++;
                encode_len += code_len * n->cnt;
                free(n);
        }
        if (encoder->bitlen_sym_cnt[code_len] > 0) {
                encoder->codes[codes_top++] = code_len;
                encoder->codes[codes_top++] = encoder->bitlen_node_cnt[code_len];
                encoder->code_max_len = code_len;
        }
        encoder->enc_size = 0;
        for (int i = encoder->code_min_len; i <= encoder->code_max_len; i++) {
                encoder->enc_size += encoder->bitlen_sym_cnt[i] * i; 
        }
        encoder->symbols -= encoder->rare_cnt;
        encoder->ranger = ranger_probe(encoder->symbols, encoder->sym_cnt + encoder->rare_cnt);
}

void HuffmanDecodeTabTreeSearch(HuffmanEncoder* encoder) {
        uint32_t unispace[32];
        memset(unispace, 0, 32);
        for (int i = encoder->code_min_len; i <= encoder->code_max_len; i++) {
                unispace[i] = unispace[i-1] + (encoder->bitlen_node_cnt[i] << (encoder->code_max_len - i));
        }
        for (int i = encoder->code_max_len; i >= encoder->code_min_len; i--) {
                encoder->bitlen_sym_cnt[i] += encoder->bitlen_sym_cnt[i+1];
        }
        uint32_t solution[32];
        uint32_t cost[32];
        cost[encoder->code_min_len] = (1 << encoder->code_min_len)*COST_OF_TABLE_WRITE + 
                encoder->bitlen_node_cnt[encoder->code_min_len] * COST_OF_DECODE;
        solution[encoder->code_min_len] = 1 << encoder->code_min_len;
        for (int i = encoder->code_min_len; i <= encoder->code_max_len; i++) {
                if (encoder->bitlen_node_cnt[i]) {
                        cost[i] = (1 << i) * COST_OF_TABLE_WRITE + 
                                encoder->bitlen_node_cnt[encoder->code_min_len] * COST_OF_DECODE;
                        solution[i] = 1 << i;
                        for (int j = encoder->code_min_len; j < i; j++) {
                                if (encoder->bitlen_node_cnt[j]) {
                                        uint32_t items = 0;
                                        uint32_t remainder = 0;
                                        for (int k = j + 1; k < i; k++) {
                                                uint32_t items0 = unispace[k] - unispace[k-1] + remainder;
                                                items += items0 >> (encoder->code_max_len - k);
                                                remainder = items0 & ~(-1 << ((encoder->code_max_len - k)));
                                        }
                                        items += (unispace[encoder->code_max_len] - unispace[i-1] + remainder) >> (encoder->code_max_len - i);
                                        uint32_t c = cost[j] + items*COST_OF_TABLE_WRITE + encoder->bitlen_sym_cnt[j+1]*COST_OF_DECODE;
                                        if (c < cost[i]) {
                                                cost[i] = c;
                                                solution[i] = solution[j] | (1 << i);
                                        }
                                }
                        }
                }
        }

        encoder->tables = (uint8_t*) malloc(2 * encoder->sym_cnt);
        encoder->table_cnt = 0;
        int remainder = 0;
        int left_solts;
        int code_len;
        int code_base_len;
        for (code_len = encoder->code_min_len; code_len <= encoder->code_max_len; code_len++) {
                remainder = remainder * 2 + encoder->bitlen_node_cnt[code_len];
                if (__builtin_expect(solution[encoder->code_max_len] & (1L << code_len), 1)) {
                        encoder->tables[encoder->table_cnt++] = code_len;
                        code_base_len = code_len;
                        left_solts = 1L << code_len;
                        left_solts -= remainder;
                        remainder = 0;
                        break;
                }
        }
        for (code_len++; code_len <= encoder->code_max_len; code_len++) {
                uint32_t item_cnt = encoder->bitlen_node_cnt[code_len] + 2 * remainder;
                if (solution[encoder->code_max_len] & (1L << code_len)) {
                        uint32_t sub_code_len = code_len - code_base_len;
                        for (int i = 0; i < left_solts; i++) {
                                encoder->tables[encoder->table_cnt++] = sub_code_len;
                        }
                        code_base_len = code_len;
                        left_solts <<= sub_code_len;
                        left_solts -= item_cnt;
                        remainder = 0;
                } else {
                        uint32_t sub_code_len = code_len - code_base_len;
                        uint32_t table_cnt = item_cnt >> sub_code_len;
                        remainder = item_cnt & ~(-1 << sub_code_len);
                        for (int j = 0; j < table_cnt; j++) {
                                encoder->tables[encoder->table_cnt++] = sub_code_len;
                        }   
                        left_solts -= table_cnt;
                }
        }
        assert(left_solts == 0 && remainder == 0);
        // }       
}

HuffmanEncoder* HuffmanEncodePrepare(int32_t* data, size_t data_len) {
        HuffmanEncoder* huffman = HuffmanBuildTree(data, data_len);
        HuffmanSortNCode(huffman);
        HuffmanDecodeTabTreeSearch(huffman);
        return huffman;
}

uint32_t HuffmanGetSize(HuffmanEncoder *encoder) {
        return (3 + encoder->ranger->out_len)  // symbol array
                + (2 + (encoder->code_max_len - encoder->code_min_len + 2) / 2) // code len array 
                + (1 + (encoder->table_cnt + 3)/4) // table array
                + (4 + (encoder->enc_size)/32 + 4);
}

uint32_t HuffmanFinish(HuffmanEncoder *encoder) {
        uint32_t pout = 0;
        
        encoder->output[pout++] = encoder->table_cnt;
        memcpy(&encoder->output[pout], encoder->tables, encoder->table_cnt);
        pout += (encoder->table_cnt + 3) / 4;

        encoder->output[pout++] = encoder->rare_cnt;
        encoder->output[pout++] = encoder->sym_cnt;
        encoder->output[pout++] = encoder->ranger->out_len;
        encoder->ranger->output = &encoder->output[pout];
        pout += encoder->ranger->out_len;
        ranger_encode(encoder->ranger);

        encoder->output[pout++] = encoder->code_min_len;
        encoder->output[pout++] = encoder->code_max_len;
        uint32_t len = encoder->code_max_len - encoder->code_min_len + 1;
        uint16_t *p = (uint16_t*) &encoder->output[pout];
        for (int i = encoder->code_min_len; i <= encoder->code_max_len; i++) {
                *p = encoder->bitlen_node_cnt[i];
                p++;
        }
        pout += (len + 1) /2;


        BitWriter4XV writer = initBitWriter4XV(writer, (encoder->enc_size+31) / 32);
        int i;
        for (i = 0; i < encoder->data_len - 3; i += 4) {
                EEntry e[4];
                e[0] = encoder->enc_map[encoder->data[i+0]];
                e[1] = encoder->enc_map[encoder->data[i+1]];
                e[2] = encoder->enc_map[encoder->data[i+2]];
                e[3] = encoder->enc_map[encoder->data[i+3]];
                v4u64 data = v4u64{e[0].code, e[1].code, e[2].code, e[3].code};
                v4u64 len = v4u64{e[0].len, e[1].len, e[2].len, e[3].len};
                writer = write(writer, data, len);
        }

        while (i < encoder->data_len) {
                EEntry e = encoder->enc_map[encoder->data[i]];
                v4u64 data = v4u64{0,0,0,0};
                data[i&3] = e.code;
                v4u64 len = v4u64{0,0,0,0};
                len[i&3] = e.len;
                writer = write(writer, data, len);
                i++;
        }
        writer = flush(writer);
        pout += exportOutput(writer, encoder->output + pout);
        free(encoder->tables);
        free(encoder->symbols);
        free(encoder->codes);
        free(encoder);
        return pout;
}


struct dentry {
        int32_t write;
        int32_t symbol;
        int32_t bitlen;
        int32_t nexttab;
} __attribute__((aligned (16)));

struct subtable {
        uint32_t off_len;
        uint32_t start;
} __attribute__((aligned (8)));

struct subtableE {
        struct subtable tab;
        uint32_t cur;
        uint32_t end;
} __attribute__((aligned (16)));

typedef struct {
        uint32_t * input;
        int32_t input_len;
        int32_t input_cur;
        int32_t subtab_cnt;
        int32_t table_len;
        int32_t rare_len;
        int32_t* rares;
        int32_t sym_len;
        int32_t* symbols;
        struct subtable* subtabs;
        struct dentry* table;
} HuffmanDecoder;

HuffmanDecoder * HuffmanReadTableList(uint32_t * input, int32_t input_len) {
        HuffmanDecoder* decoder = (HuffmanDecoder*) malloc(sizeof(*decoder));
        decoder->input = input;
        decoder->input_len = input_len;

        uint32_t cur = 0;
        decoder->subtab_cnt = input[cur++];
        decoder->subtabs = (struct subtable*) aligned_alloc(16, 
                sizeof(struct subtable) * ((decoder->subtab_cnt + 1) & ~1));
        uint8_t *p = (uint8_t*) (input + cur);
        uint32_t start = 0;
        for (int i = 0; i < decoder->subtab_cnt; i++) {
                decoder->subtabs[i].off_len = p[i];
                decoder->subtabs[i].start = start;
                start += 1 << p[i];
        }
        decoder->table_len = start;
        decoder->input_cur = cur + (decoder->subtab_cnt + 3) / 4;
        return decoder;
}

void HuffmanBuildDecodeTableDepthFirst(HuffmanDecoder * decoder) {
        decoder->rare_len = decoder->input[decoder->input_cur++];
        decoder->sym_len = decoder->input[decoder->input_cur++];
        decoder->rares = (int32_t*) malloc(sizeof(int32_t) * (decoder->sym_len + decoder->rare_len));
        decoder->symbols = decoder->rares + decoder->rare_len;

        uint32_t ranger_len = decoder->input[decoder->input_cur++];
        ranger_decode(&decoder->input[decoder->input_cur], ranger_len, decoder->rares, decoder->sym_len + decoder->rare_len);
        decoder->input_cur += ranger_len;
        uint32_t code_min_len = decoder->input[decoder->input_cur++];
        uint32_t code_max_len = decoder->input[decoder->input_cur++];
        
        decoder->table = (struct dentry*) aligned_alloc(16, sizeof(struct dentry) * decoder->table_len);
        std::stack<struct subtableE> stack;
        struct subtableE ste = {0, 0, 0, 1};
        stack.push(ste);
        ste.tab = decoder->subtabs[0];
        ste.cur = 0;
        ste.end = 1 << ste.tab.off_len;
        stack.push(ste);
        int used_subtable = 0;
        int code_len_base = 0;
        int cur_sym = 0;

        int cur = 0;
        uint16_t *p = (uint16_t*) &decoder->input[decoder->input_cur];
        for (int i = code_min_len; i <= code_max_len; i++) {
                int cnt = *p;
                p++;
                int sub_code_len = i - code_len_base;
                for (int j = 0;  j < cnt; j++) {
                        //TO-DO: less push and pop
                        while (sub_code_len > stack.top().tab.off_len) {
                                used_subtable++;
                                decoder->table[cur].write = 0;
                                decoder->table[cur].nexttab = used_subtable;
                                decoder->table[cur].bitlen = stack.top().tab.off_len;
                                cur++;
                                code_len_base += stack.top().tab.off_len;
                                sub_code_len = i - code_len_base;

                                stack.top().cur = cur;
                                ste.tab = decoder->subtabs[used_subtable];
                                cur = ste.tab.start;
                                ste.end = ste.tab.start + (1 << ste.tab.off_len);
                                stack.push(ste);
                        }
                        int times = 1 << (stack.top().tab.off_len - sub_code_len);
                        struct dentry de = {-1, decoder->symbols[cur_sym++], sub_code_len, 0};
                        for (int k = 0; k < times; k++) {
                                decoder->table[cur++] = de;
                        }

                        while (cur == stack.top().end) {
                                stack.pop();
                                code_len_base -= stack.top().tab.off_len;
                                sub_code_len = i - code_len_base;
                                cur = stack.top().cur;
                        }
                }
        }
        decoder->input_cur += (code_max_len - code_min_len + 2) / 2;
}

void HuffmanBuildDecodeTableBreadthFirst(HuffmanDecoder * decoder) {
        decoder->rare_len = decoder->input[decoder->input_cur++];
        decoder->sym_len = decoder->input[decoder->input_cur++];
        decoder->rares = (int32_t*) malloc(sizeof(int32_t) * (decoder->sym_len + decoder->rare_len));
        decoder->symbols = decoder->rares + decoder->rare_len;

        uint32_t ranger_len = decoder->input[decoder->input_cur++];
        ranger_decode(&decoder->input[decoder->input_cur], ranger_len, decoder->rares, decoder->sym_len + decoder->rare_len);
        decoder->input_cur += ranger_len;
        uint32_t code_min_len = decoder->input[decoder->input_cur++];
        uint32_t code_max_len = decoder->input[decoder->input_cur++];
        
        decoder->table = (struct dentry*) aligned_alloc(16, sizeof(struct dentry) * decoder->table_len);

        int used_table = 0;
        int curtab = 0;
        int nexttab = 1;
        int cursym = 0;
        int base_code_len = 0;
        int cur = 0;
        int end = 1 << decoder->subtabs[0].off_len;
        uint16_t *code_len_cnt = (uint16_t*) &decoder->input[decoder->input_cur];
        for (int code_len = code_min_len; code_len <= code_max_len; code_len++) {
                int sub_code_len = code_len - base_code_len;
                for (int i = 0; i < *code_len_cnt; i++) {
                        if (__builtin_expect(decoder->subtabs[curtab].off_len < sub_code_len, 0)) {
                                base_code_len += decoder->subtabs[curtab].off_len;
                                sub_code_len = code_len - base_code_len;
                                struct dentry de = {0, 0, (int32_t)decoder->subtabs[curtab].off_len, 0};
                                while (cur != end) {
                                        de.nexttab = ++used_table;
                                        decoder->table[cur++] = de;
                                }
                                for (curtab = nexttab; nexttab <= used_table; nexttab++ ) {
                                        if (decoder->subtabs[nexttab].off_len != decoder->subtabs[curtab].off_len) {
                                                break;
                                        }
                                }
                                end += (nexttab - curtab) << decoder->subtabs[curtab].off_len;
                        }
                        struct dentry de = {-1, decoder->symbols[cursym++], sub_code_len, 0};
                        int times = 1 << (decoder->subtabs[curtab].off_len - sub_code_len);
                        for (int i = 0; i < times; i++) {
                                decoder->table[cur++] = de;
                        }
                        if (__builtin_expect(cur == end, 0)) {
                                for (curtab = nexttab; nexttab <= used_table; nexttab++ ) {
                                        if (decoder->subtabs[nexttab].off_len != decoder->subtabs[curtab].off_len) {
                                                break;
                                        }
                                }
                                end += (nexttab - curtab) << decoder->subtabs[curtab].off_len;
                        }
                }
                code_len_cnt++;
        }
        decoder->input_cur += (code_max_len - code_min_len + 2) / 2;
}

void HuffmanDecodeStep(HuffmanDecoder* decoder, int32_t* output, uint32_t outlen) {
        BitReader4XV reader = initBitReader4XV(reader, &decoder->input[decoder->input_cur], decoder->input_len - decoder->input_cur);
        
        v4u32 index = v4u32{0,1,2,3};
        v4u32 curtab = v4u32{0,0,0,0};
        v4u32 out_lenv = v4u32{outlen, outlen,
                outlen, outlen};

        __mmask8 done;
        do {
#ifdef USE_GATHER
                v4u64 tables = (v4u64) _mm256_i32gather_epi64((long long*) decoder->subtabs, (__m128i) curtab, 8);
#else 
                uint64_t* subtables = (uint64_t*) decoder->subtabs;
                v4u64 tables = (v4u64) {subtables[curtab[0]], subtables[curtab[1]],
                        subtables[curtab[2]], subtables[curtab[3]]};
#endif 
                v4u64 peek_len = tables & 0xffffffff;
                v4u32 offset = peek4XV(reader, peek_len);

                offset += __builtin_convertvector(tables >> 32, v4u32);
                _mm_prefetch(&decoder->table[offset[0]], _MM_HINT_T0);
                _mm_prefetch(&decoder->table[offset[1]], _MM_HINT_T0);
                _mm_prefetch(&decoder->table[offset[2]], _MM_HINT_T0);
                _mm_prefetch(&decoder->table[offset[3]], _MM_HINT_T0);

                v4u32 tmp = offset * 4;
#ifdef USE_GATHER
                v4i32 do_write = (v4i32) _mm_i32gather_epi32((int*) decoder->table, (__m128i) tmp, 4);
#else 
                v4i32 do_write = (v4i32) {decoder->table[offset[0]].write,
                        decoder->table[offset[1]].write,
                        decoder->table[offset[2]].write,
                        decoder->table[offset[3]].write};
#endif 
                do_write = do_write && index < out_lenv;
                tmp += 1;
#ifdef USE_GATHER
                v4i32 symbols = (v4i32) _mm_i32gather_epi32((int*) decoder->table, (__m128i) tmp, 4);
#else 
                v4i32 symbols = (v4i32) {decoder->table[offset[0]].symbol,
                        decoder->table[offset[1]].symbol,
                        decoder->table[offset[2]].symbol,
                        decoder->table[offset[3]].symbol};
#endif

#ifdef USE_SCATTER
                _mm_mask_i32scatter_epi32(output, _mm_movepi32_mask((__m128i) do_write), 
                        (__m128i) index, (__m128i) symbols, 4);
#else 
                if (do_write[0]) output[index[0]] = symbols[0];
                if (do_write[1]) output[index[1]] = symbols[1];
                if (do_write[2]) output[index[2]] = symbols[2];
                if (do_write[3]) output[index[3]] = symbols[3];
#endif 

                index = do_write ? index + 4 : index;
                tmp += 1;
#ifdef USE_GATHER
                v4u64 shift = __builtin_convertvector(
                        (v4u32)_mm_i32gather_epi32((int*)decoder->table, (__m128i) tmp, 4), v4u64);
#else 
                v4u64 shift = (v4u64) {(uint64_t) decoder->table[offset[0]].bitlen,
                        (uint64_t) decoder->table[offset[1]].bitlen,
                        (uint64_t) decoder->table[offset[2]].bitlen,
                        (uint64_t) decoder->table[offset[3]].bitlen};
#endif 
                reader = forward(reader, shift);
                tmp += 1;
#ifdef USE_GATHER
                curtab = (v4u32) _mm_i32gather_epi32((int*)decoder->table, (__m128i) tmp, 4);
#else 
                curtab = (v4u32) {(uint32_t) decoder->table[offset[0]].nexttab,
                        (uint32_t) decoder->table[offset[1]].nexttab,
                        (uint32_t) decoder->table[offset[2]].nexttab,
                        (uint32_t) decoder->table[offset[3]].nexttab};
#endif 
                done = _mm_cmpge_epi32_mask((__m128i)index, (__m128i)out_lenv);
        } while (done != 0xf);
        
        int rare_cur = 0;
        for (int i = 0; i < outlen; i++) {
                if (output[i] == RARE_SYM) {
                        output[i] = decoder->rares[rare_cur++];
                }
        }
        free(decoder->subtabs);
        free(decoder->table);
        free(decoder->rares);
        free(decoder);
}

#include <time.h>
int huffman_decode_time[3];

// void HuffmanDecode(uint32_t * input, int32_t input_len, int32_t* output, uint32_t outlen) {
//         clock_t start = clock();
//         HuffmanDecoder * decoder = HuffmanReadTableList(input, input_len);
//         huffman_decode_time[0] += clock() - start;
//         start = clock();
//         // HuffmanBuildDecodeTableDepthFirst(decoder);
//         HuffmanBuildDecodeTableBreadthFirst(decoder);
//         huffman_decode_time[1] += clock() - start;
//         start = clock();
//         HuffmanDecodeStep(decoder, output, outlen);
//         huffman_decode_time[2] += clock() - start;
// }

void HuffmanDecode(uint32_t * input, int32_t input_len, int32_t* output, uint32_t outlen) {
        HuffmanDecoder * decoder = HuffmanReadTableList(input, input_len);
        // HuffmanBuildDecodeTableDepthFirst(decoder);
        HuffmanBuildDecodeTableBreadthFirst(decoder);
        HuffmanDecodeStep(decoder, output, outlen);
}