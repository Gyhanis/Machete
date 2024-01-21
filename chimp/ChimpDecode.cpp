#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "chimp.h"
#include "ChimpDef.h"
#include "BitStream/BitReader.h"

static const int16_t leadingRep[] = {0, 8, 12, 16, 18, 20, 22, 24};

uint64_t read_long(BitReader* reader, size_t len) {
        uint64_t res;
        if (len > 32) {
                res = peek(reader, 32);
                forward(reader, 32);
                res <<= len - 32;
                res |= peek(reader, len - 32);
                forward(reader, len-32);
        } else {
                res = peek(reader, len);
                forward(reader, len);
        }
        return res;
}

ssize_t chimp_decode(uint8_t* in, ssize_t len, double* out, double error) {
        assert((len - 12) % 4 == 0);

        int32_t storedLeadingZeros = INT32_MAX;
        int32_t storedTrailingZeros = 0;
        
        size_t data_len = *(uint32_t*) in;
        out[0] = *(double*) (in + 4);
        int64_t *data = (int64_t*) out;
        BitReader reader;
        initBitReader(&reader, (uint32_t*) (in + 4 + 8), (len - 12) / 4);

        int32_t previousValues = PREVIOUS_VALUES;
        int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
        int32_t initialFill = previousValuesLog2 + 9;
        int64_t* storedValues = (int64_t*) calloc(sizeof(int64_t), previousValues);

        int64_t delta;
        storedValues[0] = data[0];

        for (int i = 1; i < data_len; i++) {
                int32_t flag = peek(&reader, 2);
                uint32_t tmp, fill, index, significantBits;
                forward(&reader, 2);
                switch (flag)
                {
                case 3:
                        tmp = peek(&reader, 3);
                        forward(&reader, 3);
                        storedLeadingZeros = leadingRep[tmp];
                        delta = read_long(&reader, 64 - storedLeadingZeros);
                        data[i] = data[i-1] ^ delta;
                        storedValues[i % previousValues] = data[i];
                        break;
                case 2:
                        delta = read_long(&reader, 64 - storedLeadingZeros);
                        data[i] = data[i-1] ^ delta;
                        storedValues[i % previousValues] = data[i];
                        break;
                case 1:
                        fill = initialFill;
                        tmp = peek(&reader, fill);
                        forward(&reader, fill);
                        fill -= previousValuesLog2;
                        index = (tmp >> fill) & ((1 << previousValuesLog2) - 1);
                        fill -= 3;
                        storedLeadingZeros = leadingRep[(tmp >> fill) & 0x7];
                        fill -= 6;
                        significantBits = (tmp >> fill) & 0x3f;
                        if (significantBits == 0) {
                                significantBits = 64;
                        }
                        storedTrailingZeros = 64 - significantBits - storedLeadingZeros;
                        delta = read_long(&reader, significantBits);
                        data[i] = storedValues[index] ^ (delta << storedTrailingZeros);
                        storedValues[i % previousValues] = data[i];
                        break;
                default:
                        data[i] = storedValues[read_long(&reader, previousValuesLog2)];
                        storedValues[i % previousValues] = data[i];
                        break;
                }
        }
        free(storedValues);
        return data_len;
}
