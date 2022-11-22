#ifndef __RLE_H
#define __RLE_H

#include <stdint.h>

void RLE_Encode(int32_t* din, int32_t din_len, int32_t* dout, int32_t* dout_len);

void RLE_Decode(int32_t* din, int32_t din_len, int32_t* dout, int32_t* dout_len);

#endif 
