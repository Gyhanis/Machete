#ifndef __HUFFMAN_H
#define __HUFFMAN_H

#include "Ranger.h"
#include <unordered_map>

typedef struct node{
        int32_t sym;
        uint32_t cnt;
        uint32_t lvl;
        struct node * left;
        struct node * right;
} Node;

typedef struct {
        uint32_t code;
        uint32_t len;
} EEntry;

typedef struct {
        int32_t* data;
        size_t data_len;

        int32_t sym_cnt;
        int32_t rare_cnt;
        Node* huffman_tree;

        int32_t* symbols;
        uint8_t* codes;
        uint8_t* tables;
        uint32_t table_cnt;
        int32_t code_max_len;
        int32_t code_min_len;
        
        uint32_t bitlen_node_cnt[32];
        uint32_t bitlen_sym_cnt[32];
        
        RangerEnc * ranger;
        std::unordered_map<int32_t, EEntry> enc_map;
        uint32_t* output;
        uint32_t enc_size;
        
        uint32_t rare_sym;
} HuffmanEncoder;

HuffmanEncoder* HuffmanEncodePrepare(int32_t* data, size_t data_len);
uint32_t HuffmanGetSize(HuffmanEncoder *encoder);
uint32_t HuffmanFinish(HuffmanEncoder *encoder);

static inline void 
HuffmanFree(HuffmanEncoder* encoder) {
        if (encoder->symbols) free(encoder->symbols);
        if (encoder->codes) free(encoder->codes);
        if (encoder->tables) free(encoder->tables);
        if (encoder) free(encoder);
}

void HuffmanDecode(uint32_t * input, int32_t input_len, int32_t* output, uint32_t outlen);

#endif