#pragma once

#include <stddef.h>
#include <stdint.h>

int chimp_encode(double* in, size_t len, uint8_t** out, double error);
int chimp_decode(uint8_t* in, size_t len, double* out, double error);
