#include "defs.h"
#include <stdlib.h>

typedef struct __attribute__((__packed__)) {
        uint32_t data_len;
        union {
                struct {
                        uint16_t esize;
                        uint16_t psize;
                        uint8_t payload[0];
                };
                uint8_t raw[0];
        };
} MacheteHeader;

template<Predictor p>
ssize_t predict_diff_phase(double* input, ssize_t len, int32_t* output, double error, uint8_t** predictor_out, ssize_t* psize) {
        switch (p) {
                case lorenzo1: return lorenzo1_diff(input, len, output, error, predictor_out, psize);
        }
        return -1;
}
template<Predictor p>
ssize_t predict_correct_phase(int32_t* input, ssize_t len, double* output, uint8_t* predictor_out, ssize_t psize) {
        switch (p) {
                case lorenzo1: return lorenzo1_correct(input, len, output, predictor_out, psize);
        }
        return -1;
}

template<Encoder e>
ssize_t encode_phase(int32_t* input, ssize_t len, uint8_t** output) {
        switch (e) {
                case huffman: return huffman_encode(input, len, output);
                case ovlq: return ovlq_encode(input, len, output);
                case hybrid: return hybrid_encode(input, len, output);
        }
        return -1;
}
template<Encoder e>
ssize_t decode_phase(uint8_t* input, ssize_t size, int32_t* output) {
        switch (e)
        {
                case huffman: return huffman_decode(input, size, output);
                case ovlq: return ovlq_decode(input, size, output);
                case hybrid: return hybrid_decode(input, size, output);
        }
        return -1;
}

template<Predictor p, Encoder e>
ssize_t machete_compress(double* input, ssize_t len, uint8_t** output, double error) {
        if (UNLIKELY(len < 10)) {//do not compress if too short, headers are too costy in this case.
                ssize_t data_size = sizeof(double) * len;
                *output = reinterpret_cast<uint8_t*>(malloc(sizeof(uint32_t) + data_size));
                MacheteHeader* header = reinterpret_cast<MacheteHeader*>(*output);
                header->data_len = len;
                __builtin_memcpy(header->raw, input, data_size);
                return sizeof(uint32_t) + data_size;
        }

        if (len > INT16_MAX) {
                printf("Warning: input length too long, don't forget to check the return value in case of errors.");
        }

        int32_t *delta = reinterpret_cast<int32_t*>(malloc(sizeof(int32_t) * len));
        uint8_t *predictor_out;
        ssize_t psize;
        ssize_t dlen = predict_diff_phase<p>(input, len, delta, error, &predictor_out, &psize);
        if (UNLIKELY(dlen < 0)) { // never triggered in current version
                free(delta);
                return PREDICTION_ERROR;
        }

        uint8_t *encoder_out;
        ssize_t esize = encode_phase<e>(delta, dlen, &encoder_out);
        free(delta);
        if (UNLIKELY(esize < 0)) { // never triggered in current version
                return ENCODING_ERROR;
        }
        ssize_t osize = sizeof(MacheteHeader) + psize + esize;
        *output = reinterpret_cast<uint8_t*>(malloc(osize));
        MacheteHeader *header = reinterpret_cast<MacheteHeader*>(*output);
        header->data_len = len;
        if (UNLIKELY(esize > UINT16_MAX || psize > UINT16_MAX)) { // check for overflow
                free(predictor_out);
                free(encoder_out);
                return SIZE_ERROR;
        }
        header->esize = static_cast<uint16_t>(esize);
        header->psize = static_cast<uint16_t>(psize);
        __builtin_memcpy(header->payload, predictor_out, psize);
        __builtin_memcpy(header->payload+psize, encoder_out, esize);
        free(predictor_out);
        free(encoder_out);
        return osize;
}

ssize_t machete_getlen(uint8_t* compressed) {
        return READ_AS_UINT32(compressed);
}

template<Predictor p, Encoder e>
ssize_t machete_decompress(uint8_t* input, ssize_t size, double* output) {
        MacheteHeader* header = reinterpret_cast<MacheteHeader*>(input);
        if (UNLIKELY(header->data_len < 10)) {
                __builtin_memcpy(output, input+4, sizeof(double) * header->data_len);
                return header->data_len;
        }

        uint8_t *predictor_out = header->payload;
        uint8_t *encoder_out = header->payload + header->psize;
        ssize_t dlen = READ_AS_UINT32(encoder_out);
        int32_t *delta = reinterpret_cast<int32_t*>(malloc(sizeof(int32_t) * dlen));
        decode_phase<e>(encoder_out, header->esize, delta);
        predict_correct_phase<p>(delta, dlen, output, predictor_out, header->psize);
        free(delta);
        return header->data_len;
}

decltype(&machete_compress<lorenzo1,huffman>) _func_compress[] = {
        machete_compress<lorenzo1, huffman>,
        machete_compress<lorenzo1, ovlq>,
        machete_compress<lorenzo1, hybrid>,
};

decltype(&machete_decompress<lorenzo1, huffman>) _func_decompress[] = {
        machete_decompress<lorenzo1, huffman>,
        machete_decompress<lorenzo1, ovlq>,
        machete_decompress<lorenzo1, hybrid>,
};