#include "huffman.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

typedef struct __attribute__((__packed__)){
        uint16_t data_len;
        uint16_t sym_cnt;
        uint16_t rare_cnt;
        uint16_t max_bitlen;
        uint16_t code_len;
        uint16_t reserved;
        uint32_t rare_sp;
        uint8_t  payload[0];
} HufHeader;

int Huffman_prune_threshold = 1;

int32_t HuffmanPEnc::Huffman_prepare(FreqTab *table, int32_t* data, size_t data_len){
        freq_tab = table;
        this->data = data;
        this->data_len = data_len;
        code_size = 0;
        return Huffman_prepare_internal(Huffman_prune_threshold);
};

int32_t HuffmanPEnc::Huffman_prepare_internal(int threshold)
{
        rare_cnt = 0;
        sym_cnt = freq_tab->size();
        prune(threshold);
        build_tree();
        build_codebook();
        return calculate_out_size();
}

void HuffmanPEnc::prune(int threshold) {
        // count rare and set special symbol for rare
        int p = INT32_MAX;
        int n = INT32_MIN+1;
        for (auto e : *freq_tab) {
                if (e.second <= threshold) {
                        rare_cnt += e.second;
                        sym_cnt--;
                        if (e.first < 0) {
                                n = e.first > n ? e.first : n;
                        } else {
                                p = e.first < p ? e.first : p;
                        }
                }
        }
        rare_sp = p < -n ? p : n;

        // nothing to do if there is one rare symbol
        if (rare_cnt <= 1) {
                sym_cnt += rare_cnt;
                rare_cnt = 0;
                symbols = (int32_t*) malloc(sizeof(int32_t) * (rare_cnt + sym_cnt));
        } else {
                sym_cnt ++;
                symbols = (int32_t*) malloc(sizeof(int32_t) * (rare_cnt + sym_cnt));
                rare_sym = symbols + sym_cnt;
                int32_t rp = 0;
                for (int i = 0; i < data_len; i++) {
                        if ((*freq_tab)[data[i]] <= threshold) {
                              rare_sym[rp++] = data[i];
                              freq_tab->erase(data[i]);
                              data[i] = rare_sp;
                        }
                }
                (*freq_tab)[rare_sp] = rare_cnt;
        }
}

int32_t HuffmanPEnc::calculate_out_size() {
        out_size = sizeof(HufHeader);
        out_size += sym_cnt * sizeof(int32_t);
        out_size += rare_cnt * sizeof(int32_t);
        out_size += ALIGN_UP(sym_cnt * sizeof(int16_t), 2);
        ADD(Huffman_symbol_cnt, sym_cnt);
        ADD(Huffman_rare_cnt, rare_cnt);
        ADD(Huffman_tree_size, (sym_cnt + rare_cnt) * sizeof(int32_t));
        ADD(Huffman_tree_size, ALIGN_UP(sym_cnt * sizeof(int16_t), 2));
        out_size += ALIGN_UP(DIV_UP(code_size, 3), 2);
        return out_size;
}

void* HuffmanPEnc::fill_header(void* p) {
        HufHeader* header = (HufHeader*) p;
        header->data_len = data_len;
        header->sym_cnt = sym_cnt;
        header->rare_cnt = rare_cnt;
        header->rare_sp = rare_cnt ? rare_sp : INT32_MIN;
        header->max_bitlen = max_bitlen;
        header->code_len = DIV_UP(code_size, 5);
        return header->payload;
}

void* HuffmanPEnc::store_codebook(void* p) {
        __builtin_memcpy(p, symbols, sizeof(*symbols) * (sym_cnt+rare_cnt));
        int16_t* code_len = (int16_t*) ((int32_t*) p + sym_cnt + rare_cnt);
        for (int i = 0; i < sym_cnt; i++) {
                code_len[i] = codebook[symbols[i]].second;                
        }
        return code_len + ALIGN_UP(sym_cnt,1);
}

void HuffmanPDec::load_header(void* p) {
        HufHeader* header = (HufHeader*) p;
        data_len = header->data_len;
        sym_cnt = header->sym_cnt;
        rare_cnt = header->rare_cnt;
        max_bitlen = header->max_bitlen;
        code_len = header->code_len;
        symbols = (int32_t*) header->payload;
        rare_sym = symbols + sym_cnt;
        rare_sp = header->rare_sp;
        bitlen = (int16_t*) (symbols + sym_cnt + rare_cnt);
        code = (uint32_t*) (bitlen + ALIGN_UP(sym_cnt, 1));
}

void HuffmanPDec::restore_data(int32_t* output) {
        int rp = 0;
        if (likely(sym_cnt > 1)) {
                BitReader reader;
                initBitReader(&reader, code, code_len);
                for (int i = 0; i < data_len; i++) {
                        int code = peek(&reader, max_bitlen);
                        output[i] = codebook[code].sym == rare_sp ? 
                                rare_sym[rp++] : codebook[code].sym;
                        forward(&reader, codebook[code].len);
                }
        } else if (likely(symbols[0] != rare_sp)) {
                for (int i = 0; i < data_len; i++) {
                        output[i] = symbols[0];
                }
        } else {
                __builtin_memcpy(output, rare_sym, rare_cnt * sizeof(int32_t));
        }
        free(codebook);
}

