#pragma once 

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif 

int lfzip_init();
int lfzip_compress(double *in, size_t in_size, uint8_t** out, double error);
int lfzip_decompress(uint8_t *in, size_t in_size, double* out, double error);

#ifdef __cplusplus
}
#endif 
