#ifndef __RANGER_H
#define __RANGER_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
        uint32_t        data_len;
        uint32_t        out_len;
        int32_t*        data;
        uint32_t*       data_bits;
        uint64_t        best_p;
        uint32_t*       output;
} RangerEnc; 

RangerEnc* ranger_probe(int32_t *data, size_t data_len);

static __always_inline void ranger_free(RangerEnc* ranger) {
        free(ranger->data_bits);
        free(ranger);
}

static __always_inline void ranger_alloc_output(RangerEnc* ranger) {
        ranger->output = (uint32_t*) malloc(sizeof(uint32_t) * ranger->out_len);
}

// ranger_encode frees RangerEnc
uint32_t * ranger_encode(RangerEnc* ranger);

void ranger_decode(uint32_t* input, size_t input_len, int32_t* data, size_t data_len);

#endif