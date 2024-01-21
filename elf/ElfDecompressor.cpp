#include <cstdint>
#include <assert.h>
#include <math.h>

#include "elf.h"
#include "defs.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

class AbstractElfDecompressor {
private:
        int lastBetaStar = __INT32_MAX__;

        double nextValue() {
                double v;
                if (readInt(1) == 0) {
                        v = recoverVByBetaStar();
                } else if (readInt(1) == 0) {
                        v = xorDecompress();
                } else {
                        lastBetaStar = readInt(4);
                        v = recoverVByBetaStar();
                }
                return v;
        }

        double recoverVByBetaStar() {
                double v;
                double vPrime = xorDecompress();
                int sp = getSP(abs(vPrime));
                if (lastBetaStar == 0) {
                        v = get10iN(-sp - 1);
                        if (vPrime < 0) {
                                v = -v;
                        }
                } else {
                        int alpha = lastBetaStar - sp - 1;
                        v = roundUp(vPrime, alpha);
                }
                return v;
        }
protected:
        virtual double xorDecompress() = 0;
        virtual int readInt(int len) = 0;
        virtual int getLength() = 0;
public: 
        int decompress(double* output) {
                int len = getLength();
                for (int i = 0; i < len; i++) {
                        if (i == 4219) {
                                asm("nop");
                        }
                        output[i] = nextValue();
                }
                return len;
        }
};

static const short leadingRepresentation[] = 
{0, 8, 12, 16, 18, 20, 22, 24};

class ElfXORDecompressor {
private:
        DOUBLE storedVal = {.i = 0};
        int storedLeadingZeros = __INT32_MAX__;
        int storedTrailingZeros = __INT32_MAX__;
        bool first = true;

        BitReader reader;

        void next() {
                if (first) {
                        first = false;
                        int trailingZeros = peek(&reader, 7);
                        forward(&reader, 7);
                        if (trailingZeros < 64) {
                                storedVal.i = ((readLong(&reader, 63 - trailingZeros) << 1) + 1) << trailingZeros;
                        } else {
                                storedVal.i = 0;
                        }
                } else {
                        nextValue();
                }
        }

        void nextValue() {
                long value;
                int centerBits;
                uint32_t leadAndCenter;
                int flag = peek(&reader, 2);
                forward(&reader, 2);
                switch (flag) {
                case 3:
                        leadAndCenter = peek(&reader, 9);
                        forward(&reader, 9);
                        storedLeadingZeros = leadingRepresentation[leadAndCenter >> 6];
                        centerBits = leadAndCenter & 0x3f;
                        if (centerBits == 0) {
                                centerBits = 64;
                        }
                        storedTrailingZeros = 64 - storedLeadingZeros - centerBits;
                        value = ((readLong(&reader, centerBits - 1) << 1) + 1) << storedTrailingZeros;
                        value = storedVal.i ^ value;
                        storedVal.i = value;
                        break;
                case 2:
                        leadAndCenter = peek(&reader, 7);
                        forward(&reader, 7);
                        storedLeadingZeros = leadingRepresentation[leadAndCenter >> 4];
                        centerBits = leadAndCenter & 0xf;
                        if (centerBits == 0) {
                                centerBits = 16;
                        }
                        storedTrailingZeros = 64 - storedLeadingZeros - centerBits;
                        value = ((readLong(&reader, centerBits - 1) << 1 ) + 1) << storedTrailingZeros;
                        value = storedVal.i ^ value;
                        storedVal.i = value;
                        break;
                case 1:
                        break;
                default:
                        centerBits = 64 - storedLeadingZeros - storedTrailingZeros;
                        value = readLong(&reader, centerBits) << storedTrailingZeros;
                        value = storedVal.i ^ value;
                        storedVal.i = value;
                        break;
                }
        }
public: 
        size_t length = 0;

        void init(uint32_t* in, size_t len) {
                initBitReader(&reader, in+1, len-1);
                length = in[0];
        }

        double* getValues() {
                double* res = (double*) malloc(sizeof(double) * length);
                for (int i = 0; i < length; i++) {
                        res[i] = readValue();
                }
                return res;
        }

        BitReader* getReader() {
                return &reader;
        }

        double readValue() {
                next();
                return storedVal.d;
        }
};

class ElfDecompressor: public AbstractElfDecompressor {
private:
        ElfXORDecompressor xorDecompressor;
protected:

        double xorDecompress() override {
                return xorDecompressor.readValue();
        }

        int readInt(int len) override {
                int res = peek(xorDecompressor.getReader(), len);
                forward(xorDecompressor.getReader(), len);
                return res;
        }

        int getLength() override {
                return xorDecompressor.length;
        }
public: 
        ElfDecompressor(uint8_t* in, size_t len) {
                xorDecompressor.init((uint32_t*) in, len/4);
        }
};

ssize_t elf_decode(uint8_t* in, ssize_t len, double* out, double error) {
        ElfDecompressor decompressor(in, len);
        return decompressor.decompress(out); 
}