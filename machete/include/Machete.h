#ifndef __MACHETE_H
#define __MACHETE_H

#include "Huffman.h"

enum encoder {
        RLE,
        Huffman,
        Ranger,
        Raw,
};

typedef struct {
        int32_t         outsize;
        int32_t         din_len;
        double          first[10];
        double          error;
        enum encoder    encoder;
        int32_t*        quan_diff;
        HuffmanEncoder* huffman;
        RangerEnc *     ranger;
} Machete;

extern "C" {
        Machete* MachetePrepare(double * din, int32_t din_len, double error);

        static inline int32_t MacheteGetSize(Machete* m) {
                return m->outsize;
        }

        int32_t MacheteEncode(Machete* m, uint32_t* dout);

        void MacheteDecode(uint32_t* din, uint32_t din_len, double* dout, uint32_t dout_len);
}


#endif 
