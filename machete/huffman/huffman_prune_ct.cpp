#include "huffman.h"
#include "../vlq/vlqa.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

extern int32_t debug[1000];
extern int32_t Huffman_vlq_size;

typedef struct __attribute__((__packed__)){
        uint16_t data_len;
        uint16_t sym_cnt;
        
        uint16_t rare_cnt;
        uint8_t min_bitlen;
        uint8_t max_bitlen;

        uint16_t vlq_size;
        uint16_t code_len;
        
        uint32_t rare_sp;
        uint8_t  payload[0];
} HufHeader;

int32_t HuffmanPCEnc::calculate_out_size() {
        out_size = sizeof(HufHeader);

        vlqa = new VLQA(symbols, sym_cnt + rare_cnt);
        opt_set = vlqa->VLQA_search_optimal_set(&vlq_size);
        out_size += vlq_size;
        ADD(Huffman_vlq_size, vlq_size);
        ADD(Huffman_tree_size, vlq_size);
        
        int tmp = max_bitlen - min_bitlen + 1;
        out_size += ALIGN_UP(tmp *sizeof(int16_t), 2);
        ADD(Huffman_tree_size, ALIGN_UP(tmp*sizeof(int16_t), 2));

        out_size += ALIGN_UP(DIV_UP(code_size, 3), 2);

        return out_size;
}

void* HuffmanPCEnc::fill_header(void* p) {
        HufHeader *header = (HufHeader*) p;
        header->data_len = data_len;
        header->sym_cnt = sym_cnt;
        header->rare_cnt = rare_cnt;
        header->rare_sp = rare_cnt ? rare_sp : INT32_MIN;
        header->max_bitlen = max_bitlen;
        header->min_bitlen = min_bitlen;
        header->vlq_size = vlq_size;
        header->code_len = DIV_UP(code_size, 5);
        return header->payload;
}

void HuffmanPCDec::load_header(void* p) {
        HufHeader* header = (HufHeader*) p;
        data_len = header->data_len;
        sym_cnt = header->sym_cnt;
        rare_cnt = header->rare_cnt;
        rare_sp = header->rare_sp;
        max_bitlen = header->max_bitlen;
        min_bitlen = header->min_bitlen;
        vlq_size = header->vlq_size;
        code_len = header->code_len;
        vlq_code = header->payload;
        bitlen_cnt = (int16_t*) (vlq_code + ALIGN_UP(vlq_size,2));

        int tmp = max_bitlen - min_bitlen + 1;
        code = (uint32_t*) (bitlen_cnt + ALIGN_UP(tmp, 1));
}

void HuffmanPCDec::restore_codebook() {
        symbols = (int32_t*) malloc((sizeof(int32_t) + sizeof(int16_t)) * (sym_cnt) + sizeof(int32_t) * rare_cnt);
        rare_sym = symbols + sym_cnt;
        bitlen = (int16_t*) (rare_sym + rare_cnt);

        VLQA::VLQA_decode(vlq_code, vlq_size, symbols, sym_cnt + rare_cnt);

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
}
