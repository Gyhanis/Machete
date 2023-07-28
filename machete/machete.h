#pragma once 
#include <unistd.h>
#include <stdint.h>
#include "huffman/huffman.h"

extern size_t Huffman_encode_tree_time;
extern size_t Huffman_prune_time;
extern size_t Huffman_encode_codebook_time;
extern size_t Huffman_encode_search_time;
extern size_t Huffman_encode_time;

extern size_t Huffman_decode_codebook_time;
extern size_t Huffman_decode_time;

extern size_t Huffman_symbol_cnt;
extern size_t Huffman_rare_cnt;

extern size_t Huffman_table_len;
extern size_t Huffman_lookup_time;

extern int32_t Huffman_vlq_size;

extern size_t Machete_encode_diff_time;
extern size_t Machete_encode_freqtab_time;

extern size_t Machete_decode_restore_time;
extern size_t Machete_decode_total;

extern size_t Machete_sym_stat[5];

template<class HE>
int machete_compress_huffman(double* in, size_t len, uint8_t** out, double error);

template <class HD>
int machete_decompress_huffman(uint8_t* in, size_t len, double* out, double error);
