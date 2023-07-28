#pragma once 

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

struct VLQA_EENT {
        int32_t flen;
        int32_t flag;
        int32_t dlen;
};

struct VLQA_Header {
        uint32_t set;
        uint32_t payload[0];
};

class VLQA {
private:
        int32_t *din;
        int32_t din_len;
        int32_t *bitlen;
        int32_t monolvl;
        VLQA_EENT table[33];
        uint32_t set;
        void VLQA_build_table(int32_t set);
        size_t VLQA_encode(void* output, int32_t out_size);
public: 
        VLQA(int32_t* din, int32_t din_len);
        ~VLQA() {free(bitlen);}
        int32_t VLQA_search_optimal_set(size_t *outsize);
        size_t VLQA_encode_with_set(int32_t set, void* out, int32_t out_size);
        static size_t VLQA_decode(void* in, int32_t in_size, int32_t *out, int32_t out_len);
};

int VLQ_encode(int32_t in, uint8_t *out);
int VLQ_decode(uint8_t *in, int32_t *out);
