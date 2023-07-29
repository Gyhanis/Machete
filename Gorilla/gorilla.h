#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif 

int gorilla_encode(double* in, size_t len, uint8_t** out, double error);
int gorilla_decode(uint8_t* in, size_t len, double* out, double error);

#ifdef __cplusplus
}
#endif 
