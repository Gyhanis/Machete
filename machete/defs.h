#pragma once

#include <stdint.h>
#include <unistd.h>
#include "mach_errors.h"

union DOUBLE {
        double d;
        int64_t i;
};

#define DOUBLE_SIGN_BIT (1L<<63)

#define READ_AS_UINT32(addr) (*reinterpret_cast<uint32_t*>(addr))

#define LIKELY(cond)    (__builtin_expect(cond, 1))
#define UNLIKELY(cond)  (__builtin_expect(cond, 0))

////////////////////////////////////////// Huffman Encoding //////////////////////
#include <unordered_map>

struct HufTree {
        int32_t val = 0;
        int32_t cnt = 0;
        int32_t lvl = 0;
        HufTree *left = NULL;
        HufTree *right = NULL;
};

struct CodebookEntry {
        union {
                int32_t code;
                int32_t val;
        };
        int32_t bitlen;
};

typedef std::unordered_map<int32_t, CodebookEntry> EncodeCodebook;
typedef CodebookEntry*  DecodeCodebook;

HufTree* huffman_build_tree(std::unordered_map<int32_t, size_t> &freq, HufTree* &nodes);
ssize_t huffman_build_encode_codebook(HufTree* root, EncodeCodebook &codebook, int32_t* vals);
ssize_t huffman_build_canonical_encode_codebook(HufTree* root, EncodeCodebook &codebook, int32_t* vals);
ssize_t huffman_store_codebook(EncodeCodebook &codebook, int32_t* vals, int32_t val_cnt, uint8_t* output);
ssize_t huffman_store_canonical_codebook(EncodeCodebook &codebook, int32_t* vals, int32_t val_cnt, uint8_t* output);
ssize_t huffman_store_code(EncodeCodebook &codebook, int32_t* input, int32_t len, uint8_t* output, ssize_t osize);
ssize_t huffman_encode(int32_t* input, ssize_t len, uint8_t** output);
ssize_t huffman_encode_canonical(int32_t* input, ssize_t len, uint8_t** output);

ssize_t huffman_build_decode_codebook(int32_t* vals, int16_t* bitlens, uint32_t val_cnt, DecodeCodebook &codebook);
ssize_t huffman_build_decode_codebook_canonical(int32_t* vals, int16_t* bitlens, uint32_t val_cnt, DecodeCodebook &codebook);
ssize_t huffman_decode_data(uint8_t* input, uint32_t code_size, DecodeCodebook codebook, ssize_t index_bitlen, int32_t* output, int32_t olen);
ssize_t huffman_decode(uint8_t* input, ssize_t len, int32_t* output);
ssize_t huffman_decode_canonical(uint8_t* input, ssize_t size, int32_t* output);

////////////////////////////////////////// Optimal VLQ //////////////////////

ssize_t ovlq_encode(int32_t* input, ssize_t len, uint8_t** output);
ssize_t ovlq_decode(uint8_t* input, ssize_t size, int32_t* output);

///////////////////////////////////////// Hybrid Encoder ///////////////////

ssize_t hybrid_encode(int32_t* input, ssize_t len, uint8_t** output);
ssize_t hybrid_decode(uint8_t* input, ssize_t size, int32_t* output);

enum Encoder {huffman, huffmanC, ovlq, hybrid};

ssize_t lorenzo1_diff(double* input, ssize_t len, int32_t* output, double error, uint8_t** predictor_out, ssize_t* psize);
ssize_t lorenzo1_correct(int32_t* input, ssize_t len, double* output, uint8_t* predictor_out, ssize_t psize);

enum Predictor {lorenzo1};

template<Predictor p, Encoder e>
ssize_t machete_compress(double* input, ssize_t len, uint8_t** output, double error);
template<Predictor p, Encoder e>
ssize_t machete_decompress(uint8_t* input, ssize_t size, double* output);