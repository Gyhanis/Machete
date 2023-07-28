#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <queue>
#include <stack>
#include <stdlib.h>
#include <time.h>
#include "huffman.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

size_t Huffman_encode_tree_time;
size_t Huffman_prune_time;
size_t Huffman_encode_codebook_time;
size_t Huffman_encode_search_time;
size_t Huffman_encode_time;

size_t Huffman_decode_codebook_time;
size_t Huffman_decode_time;

size_t Huffman_symbol_cnt;
size_t Huffman_rare_cnt;
size_t Huffman_tree_size;

size_t Huffman_table_len;
size_t Huffman_lookup_time;

size_t Huffman_max_bitlen[16];

typedef struct __attribute__((__packed__)){
        uint16_t data_len;
        uint16_t sym_cnt;
        uint16_t max_bitlen;
        uint16_t code_len;
        uint8_t  payload[0];
} HufHeader;

static bool great_on_cnt(HTNode *n0, HTNode *n1) {
        return n0->cnt > n1->cnt;
}

int32_t HuffmanEnc::Huffman_prepare(FreqTab *table, int32_t* data, size_t data_len) {
        freq_tab = table;
        this->data = data;
        this->data_len = data_len;
        code_size = 0;
        sym_cnt = freq_tab->size();
        symbols = (int32_t*) calloc(sizeof(*this->symbols), data_len);
        // SET(Huffman_symbol_cnt, sym_cnt);

        RECORD(Huffman_encode_tree_time,     build_tree());
        RECORD(Huffman_encode_codebook_time, build_codebook());
        ADD(Huffman_max_bitlen[max_bitlen], 1);
        return calculate_out_size();
}

void HuffmanEnc::build_tree() {
        std::priority_queue<HTNode*, std::vector<HTNode*>, decltype(&great_on_cnt)> queue(great_on_cnt);
        
        node_pool = (HTNode*) calloc(sizeof(HTNode),(sym_cnt * 2 - 1));

        int cur_node = 0;
        for (auto ent : *freq_tab) {
                node_pool[cur_node].symbol = ent.first;
                node_pool[cur_node].cnt = ent.second;
                queue.push(&node_pool[cur_node]);
                cur_node++;
        }

        while (queue.size() > 1) {
                HTNode* n1 = queue.top();
                queue.pop();
                HTNode* n2 = queue.top();
                queue.pop();
                node_pool[cur_node] = {0, n1->cnt + n2->cnt, 0, n1, n2};
                queue.push(&node_pool[cur_node]);
                cur_node++;
        }

        huf_tree = queue.top();
}

void HuffmanEnc::build_codebook() {
        huf_tree->lvl = 0;
        std::stack<HTNode*> stack;
        stack.push(huf_tree);
        int cnt = 0;
        int32_t code = 0;
        int code_len = 0;
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
                        if (code_len > n->lvl) {
                                code >>= code_len - n->lvl;
                        } else {
                                code <<= n->lvl - code_len;
                        }
                        code_len = n->lvl;
                        max_bitlen = code_len > max_bitlen ? code_len : max_bitlen;
                        codebook[n->symbol] = std::make_pair(code, n->lvl);
                        symbols[cnt++] = n->symbol;
                        code++;
                        code_size += n->lvl * n->cnt;
                }
        }
        free(node_pool);
}

int32_t HuffmanEnc::calculate_out_size() {
        out_size = sizeof(HufHeader);
        out_size += codebook.size() * (sizeof(int32_t) + sizeof(int16_t));
        out_size += (codebook.size() & 1) << 1;
        ADD(Huffman_tree_size, ALIGN_UP(codebook.size() * (sizeof(int32_t) + sizeof(int16_t)), 2));
        out_size += ALIGN_UP(DIV_UP(code_size, 3), 2);
        return out_size;
}

int32_t HuffmanEnc::Huffman_encode(void* output) {
        TIK;
        void* p = fill_header(output);
        p = store_codebook(p);
        store_data(p);
        free(symbols);
        ADD(Huffman_table_len, 1 << max_bitlen);
        ADD(Huffman_lookup_time, data_len);
        TOK(Huffman_encode_time);
        return out_size;
}


void* HuffmanEnc::fill_header(void* p) {
        HufHeader *header = (HufHeader*) p;
        header->data_len = data_len;
        header->sym_cnt = sym_cnt;
        header->max_bitlen = max_bitlen;
        header->code_len = DIV_UP(code_size, 5);
        return header->payload;
}

void* HuffmanEnc::store_codebook(void* p) {
        __builtin_memcpy(p, symbols, sizeof(int32_t) * sym_cnt);
        p = (int32_t*) p + sym_cnt;
        int16_t* code_len = (int16_t*) p;
        for (int i = 0; i < sym_cnt; i++) {
                code_len[i] = codebook[symbols[i]].second;                
        }
        return code_len + ALIGN_UP(sym_cnt,1);
}

void HuffmanEnc::store_data(void* p) {
        if (likely(sym_cnt > 1)) {
                BitWriter writer;
                initBitWriter(&writer, (uint32_t*)p, DIV_UP(code_size,5));

                for (int i = 0; i < data_len; i++) {
                        // if (i == 590) 
                        //         asm("nop");
                        auto e = codebook[data[i]];
                        write(&writer, e.first, e.second);
                }
                flush(&writer);
        }
}

int32_t HuffmanDec::Huffman_decode(void* data, size_t data_len, int32_t* output) {
        load_header(data);
        RECORD(Huffman_decode_codebook_time, restore_codebook());
        RECORD(Huffman_decode_time, restore_data(output));
        return this->data_len;
}

void HuffmanDec::load_header(void* p) {
        HufHeader* header = (HufHeader*) p;
        data_len = header->data_len;
        sym_cnt = header->sym_cnt;
        max_bitlen = header->max_bitlen;
        code_len = header->code_len;
        symbols = (int32_t*) header->payload;
        bitlen = (int16_t*) (symbols + sym_cnt);
        code = (uint32_t*) (bitlen + ALIGN_UP(sym_cnt, 1));
}

// HDEntry debug_codebook[1<<11];

void HuffmanDec::restore_codebook() {
        // codebook = (HDEntry*) malloc(sizeof(HDEntry) * (1<<11));
        codebook = (HDEntry*) malloc(sizeof(HDEntry) * (1 << max_bitlen));
        int32_t code = 0;
        for (int i = 0; i < sym_cnt; i++) {
                int times = 1 << (max_bitlen - bitlen[i]);
                for (int j = 0; j < times; j++) {
                        codebook[code++] = {symbols[i], bitlen[i]};
                }
        }
}

void HuffmanDec::restore_data(int32_t* output) {
        if (likely(sym_cnt > 1)) {
                BitReader reader;
                initBitReader(&reader, code, code_len);
                for (int i = 0; i < data_len; i++) {
                        // if (i == 590)
                        //         asm("nop");
                        int code = peek(&reader, max_bitlen);
                        output[i] = codebook[code].sym;
                        forward(&reader, codebook[code].len);
                }
        } else {
                for (int i = 0; i < data_len; i++) {
                        output[i] = symbols[0];
                }
        }
        free(codebook);
}