#include <cmath>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "bsc/libbsc.h"
#include "lfzip_predictor.cpp"
#include "lfzip.h"

#define MIN_BIN_IDX -32767
#define MAX_BIN_IDX 32767

int lfzip_init() {
        return bsc_init(LIBBSC_FEATURE_FASTMODE);
}

ssize_t lfzip_compress(double *in, ssize_t in_size, uint8_t** out, double error) {
        NLMS_predictor* predictor = new NLMS_predictor(32, 0.5);
        std::vector<double> reconstruction(in_size);
        uint8_t *tmp = (uint8_t*) malloc((sizeof(int16_t)+sizeof(double)) * in_size); 
        int16_t* bin_idx_array = (int16_t*) tmp;
        double* overflow = (double*) (tmp + sizeof(int16_t) * in_size);

        uint32_t data_len = in_size;
        int of_size = 0;
        for (int i = 0; i < in_size; i++) {
                double dataval = in[i];
                double predval = predictor->predict(reconstruction, i);
                // double predval = i == 0 ? 0 : reconstruction[i-1];
                double diff = dataval - predval;
                int64_t bin_idx = int64_t(std::round((diff / (2.0 * error))));
                if (MIN_BIN_IDX <= bin_idx && bin_idx <= MAX_BIN_IDX) {
                        reconstruction[i] = predval + (double)(error * bin_idx * 2.0);
                        if (std::abs(reconstruction[i] - dataval) <= error) {
                                bin_idx_array[i] = (int16_t)bin_idx;
                                continue;
                        }
                }
                bin_idx_array[i] = (int16_t)(MIN_BIN_IDX - 1);
                overflow[of_size++] = dataval;
                reconstruction[i] = dataval;
        }
        delete predictor;

        *out = (uint8_t*) malloc(4 + LIBBSC_HEADER_SIZE + sizeof(int16_t) * in_size + sizeof(double) * of_size);
        int res = bsc_compress(tmp, *out + 4, sizeof(int16_t) * in_size + sizeof(double) * (of_size), 
                LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, 
                LIBBSC_DEFAULT_BLOCKSORTER, LIBBSC_DEFAULT_CODER, LIBBSC_FEATURE_FASTMODE);
        *(uint32_t*) *out = data_len;
        free(tmp);
        return res + 4;
}

ssize_t lfzip_decompress(uint8_t *in, ssize_t in_size, double* out, double error) {
        uint32_t len = *(uint32_t*) in;

        int tmp_size = (sizeof(double) + sizeof(int16_t)) * len;
        uint8_t* tmp = (uint8_t*) malloc(tmp_size);
        bsc_decompress(in+4, in_size - 4, tmp, tmp_size, LIBBSC_FEATURE_FASTMODE);

        int16_t* bin_idx_array = (int16_t*) tmp;
        double* overflow = (double*) (tmp + sizeof(int16_t) * len);
        int of_top = 0;
        NLMS_predictor* predictor = new NLMS_predictor(32, 0.5);
        std::vector<double> reconstruction(len);
        for (int i = 0; i < len; i++) {
                double predval = predictor->predict(reconstruction, i);
                int64_t bin_idx = bin_idx_array[i];
                if (bin_idx == MIN_BIN_IDX-1) {
                        reconstruction[i] = overflow[of_top++];
                } else {
                        reconstruction[i] = predval + (double)(error * bin_idx * 2.0);
                }
        }
        __builtin_memcpy(out, &reconstruction[0], sizeof(double) * len);
        delete predictor;
        free(tmp);
        return len;
}