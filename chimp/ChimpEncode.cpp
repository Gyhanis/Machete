#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "chimp.h"
#include "ChimpDef.h"

#include "BitStream/BitWriter.h"

static const uint16_t leadingRep[] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7  
};

static const uint16_t leadingRnd[] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        8, 8, 8, 8, 12, 12, 12, 12,
        16, 16, 18, 18, 20, 20, 22, 22,
        24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24
};

static inline void
write_long(BitWriter* writer, int64_t data, size_t len) {
        if (len <= 32) {
                write(writer, data, len);
        } else {
                write(writer, data >> 32, len-32);
                write(writer, data, 32);
        }
}

ssize_t chimp_encode(double* in, ssize_t len, uint8_t** out, double error) {
        assert(len > 0);

        size_t buffer_size = SIZE_IN_BIT((1 + 1 + 5 + 6 + 64) * len) * 4; 
        *out = (uint8_t*) malloc(4 + buffer_size);
        *(uint32_t*) *out = len;
        *(double*) (*out + 4) = in[0];
        BitWriter writer;
        initBitWriter(&writer, (uint32_t*) (*out+4+8), buffer_size / 4 - 2);
        int64_t *data = (int64_t*) in;

        int32_t storedLeadingZeros = INT32_MAX;

        int32_t index = 0;
        int32_t current = 0;

        size_t size = 0;
        int32_t previousValues = PREVIOUS_VALUES;
        int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
        int32_t threshold = 6 + previousValuesLog2;
        int32_t setLsb = (1 << (threshold + 1)) - 1;
        int32_t* indices = (int32_t*) calloc(sizeof(int32_t), (1 << (threshold + 1)));
        int64_t* storedValues = (int64_t*) calloc(sizeof(int64_t), PREVIOUS_VALUES);
        int32_t flagZeroSize = previousValuesLog2 + 2;
        int32_t flagOneSize = previousValuesLog2 + 11;

        storedValues[current] = data[0];
        indices[((int) in[0]) & setLsb] = index;
        size += 64;

        for (int i = 1; i < len; i++) {
                // if (i == 417) {
                //         asm("nop");
                // }
                int32_t key = (int) data[i] & setLsb;
                int64_t delta;
                int32_t previousIndex;
                int32_t trailingZeros = 0;
                int32_t currIndex = indices[key];
                if ((index - currIndex) < previousValues) {
                        delta = data[i] ^ storedValues[currIndex % previousValues];
                        trailingZeros = __builtin_ctzl(delta);
                        if (trailingZeros > threshold) {
                                previousIndex = currIndex % previousValues;
                        } else {
                                previousIndex = index % previousValues;
                                delta = storedValues[previousIndex] ^ data[i];
                        }
                } else {
                        previousIndex = index % previousValues;
                        delta = storedValues[previousIndex] ^ data[i];
                }

                if (delta == 0) {
                        write(&writer, previousIndex, flagZeroSize);
                        size += flagZeroSize;
                        storedLeadingZeros = 65;
                } else {
                        int32_t leadingZeros = leadingRnd[__builtin_clzl(delta)];

                        if (trailingZeros > threshold) {
                                int32_t significantBits = 64 - leadingZeros - trailingZeros;
                                write(&writer, ((previousValues + previousIndex) << 9) | 
                                        (leadingRep[leadingZeros] << 6) |
                                        significantBits, flagOneSize );

                                write_long(&writer, delta >> trailingZeros, significantBits);
                                size += significantBits + flagOneSize;
                                storedLeadingZeros = 65;
                        } else if (leadingZeros == storedLeadingZeros) {
                                write(&writer, 2, 2);
                                int32_t significantBits = 64 - leadingZeros;
                                write_long(&writer, delta, significantBits);
                                size += 2 + significantBits;
                        } else {
                                storedLeadingZeros = leadingZeros;
                                int significantBits = 64 - leadingZeros;
                                write(&writer, (0x3 << 3) | leadingRep[leadingZeros], 5);
                                write_long(&writer, delta, significantBits);
                                size += 5 + significantBits;
                        }
                }
                current = (current + 1) % previousValues;
                storedValues[current] = data[i];
                index++;
                indices[key] = index;
        }

        free(indices);
        free(storedValues);
        return flush(&writer) * 4 + 4 + 8;
};

