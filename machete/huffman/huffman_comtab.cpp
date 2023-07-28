#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <queue>
#include <stack>
#include <stdlib.h>
#include <assert.h>
#include "huffman.h"
#include "../vlq/vlqa.h"

int32_t Huffman_vlq_size;

typedef struct __attribute__((__packed__)) {
        uint16_t data_len;
        uint16_t sym_cnt;
        uint16_t max_bitlen;
        uint16_t min_bitlen;
        uint16_t code_len;
        uint16_t vlq_size;
        uint8_t  payload[0];
} HufHeader;

static bool less_on_lvl(HTNode *n0, HTNode* n1) {
        return n0->lvl > n1->lvl;
}

static inline size_t cvsize(int32_t data) {
        return ((33 - __builtin_clz(data<0 ? ~data : data)) + 6) / 7;
}

void HuffmanC0Enc::build_codebook() {
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
        }
        max_bitlen = n->lvl;
        free(node_pool);
}

int32_t HuffmanC0Enc::calculate_out_size() {
        out_size = sizeof(HufHeader);
        
        vlq_size = 0;
        for (int i = 0; i < sym_cnt; i++) {
                vlq_size += cvsize(symbols[i]);
        }
        vlq_size = ALIGN_UP(vlq_size, 2);
        out_size += vlq_size;

        int tmp = max_bitlen - min_bitlen + 1;
        out_size += ALIGN_UP(tmp *sizeof(int16_t), 2);

        out_size += ALIGN_UP(DIV_UP(code_size,3),  2);
        return out_size;
}

void* HuffmanC0Enc::fill_header(void* p) {
        HufHeader *header = (HufHeader*) p;
        header->data_len = data_len;
        header->sym_cnt = sym_cnt;
        header->max_bitlen = max_bitlen;
        header->min_bitlen = min_bitlen;
        header->vlq_size = vlq_size;
        header->code_len = DIV_UP(code_size, 5);
        return header->payload;
}

void* HuffmanC0Enc::store_codebook(void* p) {
        uint8_t *p1 = (uint8_t*) p;
        for (int i = 0; i < sym_cnt; i++) {
                // if (i == 97)
                //         asm("nop");
                p1 += VLQ_encode(symbols[i], p1);
        }
        p1 = (uint8_t*) p + ALIGN_UP(vlq_size, 2);

        int16_t* bitlen_cnt = (int16_t*) p1;
        int len = max_bitlen - min_bitlen + 1;
        __builtin_memset(bitlen_cnt, 0, len*sizeof(int16_t));
        for (auto e: codebook) {
                bitlen_cnt[e.second.second-min_bitlen]++;
        }
        return bitlen_cnt + ALIGN_UP(len, 1);
}

void HuffmanC0Dec::load_header(void* p) {
        HufHeader* header = (HufHeader*) p;
        data_len = header->data_len;
        sym_cnt = header->sym_cnt;
        max_bitlen = header->max_bitlen;
        min_bitlen = header->min_bitlen;
        vlq_size = header->vlq_size;
        code_len = header->code_len;
        vlq_code = header->payload;
        bitlen_cnt = (int16_t*) (vlq_code + ALIGN_UP(vlq_size,2));

        int tmp = max_bitlen - min_bitlen + 1;
        code = (uint32_t*) (bitlen_cnt + ALIGN_UP(tmp, 1));
}

void HuffmanC0Dec::restore_codebook() {
        symbols = (int32_t*) malloc((sizeof(int32_t) + sizeof(int16_t)) * (sym_cnt));
        bitlen = (int16_t*) (symbols + sym_cnt);

        uint8_t* p = vlq_code;
        for (int i = 0; i < sym_cnt; i++) {
                // if (i == 97) 
                //         asm("nop");
                p += VLQ_decode(p, symbols + i);
        }

        int t = 0;
        for (int i = min_bitlen; i <= max_bitlen; i++) {
                for (int j = 0; j < bitlen_cnt[i-min_bitlen]; j++) {
                        bitlen[t++] = i;
                }
        }

        codebook = (HDEntry*) malloc(sizeof(HDEntry) * (1 << max_bitlen));
        int32_t code = 0;
        for (int i = 0; i < sym_cnt; i++) {
                int times = 1 << (max_bitlen - bitlen[i]);
                for (int j = 0; j < times; j++) {
                        codebook[code++] = {symbols[i], bitlen[i]};
                }
        }
        s0 = symbols[0];
        free(symbols);
        symbols = &s0;
}


void HuffmanCEnc::build_codebook() {
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
        }
        max_bitlen = n->lvl;
        free(node_pool);
}

int32_t HuffmanCEnc::calculate_out_size() {
        out_size = sizeof(HufHeader);

        vlqa = new VLQA(symbols, sym_cnt);
        opt_set = vlqa->VLQA_search_optimal_set(&vlq_size);
        out_size += vlq_size;
        ADD(Huffman_vlq_size, vlq_size-sizeof(VLQA_Header));

        int tmp = max_bitlen - min_bitlen + 1;
        out_size += ALIGN_UP(tmp *sizeof(int16_t), 2);

        out_size += ALIGN_UP(DIV_UP(code_size,3),  2);
        return out_size;
}

void* HuffmanCEnc::fill_header(void* p) {
        HufHeader *header = (HufHeader*) p;
        header->data_len = data_len;
        header->sym_cnt = sym_cnt;
        header->max_bitlen = max_bitlen;
        header->min_bitlen = min_bitlen;
        header->vlq_size = vlq_size;
        header->code_len = DIV_UP(code_size, 5);
        return header->payload;
}

void* HuffmanCEnc::store_codebook(void* p) {
        vlqa->VLQA_encode_with_set(opt_set, p, vlq_size);
        delete vlqa;

        int16_t* bitlen_cnt = (int16_t*) ((uint8_t*)p + ALIGN_UP(vlq_size,2));
        int len = max_bitlen - min_bitlen + 1;
        __builtin_memset(bitlen_cnt, 0, len*sizeof(int16_t));
        for (auto e: codebook) {
                bitlen_cnt[e.second.second-min_bitlen]++;
        }
        return bitlen_cnt + ALIGN_UP(len, 1);
}

void HuffmanCDec::load_header(void* p) {
        HufHeader* header = (HufHeader*) p;
        data_len = header->data_len;
        sym_cnt = header->sym_cnt;
        max_bitlen = header->max_bitlen;
        min_bitlen = header->min_bitlen;
        vlq_size = header->vlq_size;
        code_len = header->code_len;
        vlq_code = header->payload;
        bitlen_cnt = (int16_t*) (vlq_code + ALIGN_UP(vlq_size,2));

        int tmp = max_bitlen - min_bitlen + 1;
        code = (uint32_t*) (bitlen_cnt + ALIGN_UP(tmp, 1));
}

void HuffmanCDec::restore_codebook() {
        symbols = (int32_t*) malloc((sizeof(int32_t) + sizeof(int16_t)) * (sym_cnt));
        bitlen = (int16_t*) (symbols + sym_cnt);

        VLQA::VLQA_decode(vlq_code, vlq_size, symbols, sym_cnt);

        int t = 0;
        for (int i = min_bitlen; i <= max_bitlen; i++) {
                for (int j = 0; j < bitlen_cnt[i-min_bitlen]; j++) {
                        bitlen[t++] = i;
                }
        }

        codebook = (HDEntry*) malloc(sizeof(HDEntry) * (1 << max_bitlen));
        int32_t code = 0;
        for (int i = 0; i < sym_cnt; i++) {
                int times = 1 << (max_bitlen - bitlen[i]);
                for (int j = 0; j < times; j++) {
                        codebook[code++] = {symbols[i], bitlen[i]};
                }
        }
        s0 = symbols[0];
        free(symbols);
        symbols = &s0;
}
