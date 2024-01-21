#pragma once 

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif 

int lfzip_init();
ssize_t lfzip_compress(double *in, ssize_t in_size, uint8_t** out, double error);
ssize_t lfzip_decompress(uint8_t *in, ssize_t in_size, double* out, double error);

#ifdef __cplusplus
}
#endif 
