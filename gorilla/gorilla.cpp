#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"
#include "gorilla.h"

ssize_t gorilla_encode(double* in, ssize_t len, uint8_t** out, double error) {
        assert(len > 0);

        size_t buffer_size = SIZE_IN_BIT((1 + 1 + 5 + 6 + 64) * len) * 4; 
        *out = (uint8_t*) malloc(4 + buffer_size);
        *(uint32_t*) *out = len;
        *(double*) (*out + 4) = in[0];
        BitWriter writer;
        initBitWriter(&writer, (uint32_t*) (*out+4+8), buffer_size / 4 - 2);
        
        uint64_t prevLeading = -1L;
        uint64_t prevTrailing = 0;
        uint64_t leading, trailing;
        uint64_t mask;
        double sum;

        uint64_t* data = (uint64_t*) in;
        for (int i = 1; i < len; i++) {
                uint64_t vDelta = data[i] ^ data[i-1];
                if (vDelta == 0) {
                        write(&writer, 0, 1);
                        continue;
                }

                // if (i == 930) {
                //         printf("!\n");
                // }

                leading = __builtin_clzl(vDelta);
                trailing = __builtin_ctzl(vDelta);

                leading = (leading >= 32) ? 31 : leading;
                uint64_t l;

                if (prevLeading != -1L && leading >= prevLeading && trailing >= prevTrailing) {
                        write(&writer, 2, 2);
                        l = 64 - prevLeading - prevTrailing;
                } else {
                        prevLeading = leading;
                        prevTrailing = trailing;
                        l = 64 - leading - trailing;

                        write(&writer, 3, 2);
                        write(&writer, leading, 5);
                        write(&writer, l-1, 6);
                }

                if (l <= 32) {
                        write(&writer, vDelta >> prevTrailing, l);
                } else {
                        write(&writer, vDelta >> 32, 32-prevLeading);
                        write(&writer, vDelta >> prevTrailing, 32-prevTrailing);
                }
        }

        return flush(&writer) * 4 + 4 + 8;
}

uint64_t read_delta(BitReader* reader, uint64_t leading, uint64_t meaningful) {
        uint64_t trailing = 64 - leading - meaningful;
        uint64_t delta;
        if (meaningful > 32) {
                delta = peek(reader, 32);
                forward(reader, 32);
                delta <<= meaningful - 32;
                delta |= peek(reader, meaningful - 32);
                forward(reader, meaningful-32);
        } else {
                delta = peek(reader, meaningful);
                forward(reader, meaningful);
        }
        return delta << trailing;
}

ssize_t gorilla_decode(uint8_t* in, ssize_t len, double* out, double error) {
        uint32_t data_len = *(uint32_t*) in;
        out[0] = *(double*) (in + 4);
        BitReader reader;
        assert((len - 4 - 8) % 4 == 0);
        initBitReader(&reader, (uint32_t*) (in + 4 + 8), (len - 4 - 8) / 4);

        uint64_t *data = (uint64_t*) out;
        uint64_t leading, meaningful, delta;
        for (int i = 1; i < data_len; i++) {
                // if (i == 930) {
                //         printf("!\n");
                // }
                switch (peek(&reader, 2))
                {
                case 0:
                case 1:
                        forward(&reader, 1);
                        data[i] = data[i-1];
                        break;
                case 2:
                        forward(&reader, 2);
                        delta = read_delta(&reader, leading, meaningful);
                        data[i] = data[i-1] ^ delta;
                        break;
                case 3:
                        forward(&reader, 2);
                        leading = peek(&reader, 5);
                        forward(&reader, 5);
                        meaningful = peek(&reader, 6) + 1;
                        forward(&reader, 6);
                        delta = read_delta(&reader, leading, meaningful);
                        data[i] = data[i-1] ^ delta;
                        break;
                default:
                        break;
                }
        }
        return data_len;
}