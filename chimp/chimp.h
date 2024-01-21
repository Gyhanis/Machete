#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

ssize_t chimp_encode(double* in, ssize_t len, uint8_t** out, double error);
ssize_t chimp_decode(uint8_t* in, ssize_t len, double* out, double error);

#ifdef __cplusplus
}
#endif 