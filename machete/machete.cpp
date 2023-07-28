#include "huffman/huffman.h"
#include "util.h"

#include <vector>

typedef struct __attribute__((__packed__)) {
        int32_t data_len;
        union {
                struct {
                        int32_t huffman_len;
                        int32_t outier_cnt;
                        double first;
                        double error;
                        uint8_t payload[0];
                };
                uint8_t raw[0];
        };
} MacheteHeader;

typedef union {
        double d;
        int64_t i;
} DOUBLE;

size_t Machete_encode_diff_time;
size_t Machete_encode_freqtab_time;

size_t Machete_decode_restore_time;
size_t Machete_decode_total;

size_t Machete_sym_stat[5];

template<class HE>
int machete_compress_huffman(double* in, size_t len, uint8_t** out, double error) {
        if (unlikely(len < 10)) {
                int osize = sizeof(int32_t) + sizeof(double) * len;
                *out = (uint8_t*) malloc(osize);
                MacheteHeader *header = (MacheteHeader*) *out;
                header->data_len = len;
                __builtin_memcpy(header->raw, in, sizeof(double) * len);
                return osize;
        }

        int32_t *diff = (int32_t*) malloc(sizeof(int32_t) * len);
        DOUBLE e = {.d=error*0.999};
        double e2 = 2*e.d;
        double max_diff = e2 * (INT32_MAX - 1);
        std::vector<double> *v = new std::vector<double>();

        {TIK;
                double last = in[0];
                for (int i = 1; i < len; i++) {
                        // if (i == 96)
                        //         asm("nop");
                        if (in[i] > last) {
                                double _diff = in[i] - last;
                                if (unlikely(_diff > max_diff)) {
                                        diff[i] = INT32_MAX;
                                        v->push_back(in[i]);
                                } else {
                                        diff[i] = (int32_t) ((_diff + e.d) / e2);
                                }
                        } else {
                                double _diff = last - in[i];
                                if (unlikely(_diff > max_diff)) {
                                        diff[i] = INT32_MAX;
                                        v->push_back(in[i]);
                                } else {
                                        diff[i] = (int32_t) -((_diff + e.d) / e2);
                                }
                        }
                        last += e2 * diff[i];
                }
        TOK(Machete_encode_diff_time);}

        FreqTab table;
        RECORD(Machete_encode_freqtab_time, init_frequency_table(table, &diff[1], len-1));

        STAT_BLOCK({
                for (auto e: table) {
                        if (e.second < 5) {
                                Machete_sym_stat[e.second]+=e.second;
                        } else {
                                Machete_sym_stat[0]+=e.second;
                        }
                }
        });
        HE encoder;
        int huf_out = encoder.Huffman_prepare(&table, &diff[1], len-1);
        int out_size = sizeof(MacheteHeader) + huf_out;
        *out = (uint8_t*) malloc(out_size);
        // *out = (uint8_t*) malloc(len * 8 + 1000);
        MacheteHeader *header = (MacheteHeader*) *out;
        header->data_len = len;
        header->error = e.d;
        header->first = in[0];
        header->huffman_len = huf_out;
        header->outier_cnt = v->size();
        int tmp = sizeof(double)*header->outier_cnt;
        __builtin_memcpy(header->payload, v->data(), tmp);
        encoder.Huffman_encode(header->payload + tmp);
        free(diff);
        delete v;
        return out_size;
}

int debug_diff[1000];

template <class HD>
int machete_decompress_huffman(uint8_t* in, size_t len, double* out, double error) {
        TIK;
        MacheteHeader *header = (MacheteHeader*) in;
        // double* dout = *out = (double*) malloc(sizeof(double) * header->data_len);
        double* dout = out;
        if (unlikely(header->data_len < 10)) {
                __builtin_memcpy(dout, header->raw, sizeof(double) * header->data_len);
                return header->data_len;
        }

        int32_t *diff = (int32_t*) malloc(sizeof(int32_t) * header->data_len);
        // int32_t* diff = debug_diff;
        HD decoder;
        TOK(Machete_decode_total);
        if (likely(header->outier_cnt==0)) {
                decoder.Huffman_decode(header->payload, header->huffman_len, &diff[1]);
                TIK;
                dout[0] = header->first;
                double e2 = header->error*2;
                for (int i = 1; i < header->data_len; i++) {
                        // if (i == 96)
                        //         asm("nop");
                        dout[i] = dout[i-1] + e2 * diff[i];
                }
                TOK(Machete_decode_restore_time);
        } else {
                double* outier = (double*) header->payload;
                decoder.Huffman_decode(header->payload + sizeof(double) * header->outier_cnt, header->huffman_len, &diff[1]);
                TIK;
                dout[0] = header->first;
                double e2 = header->error*2;
                for (int i = 1; i < header->data_len; i++) {
                        // if (i == 96)
                        //         asm("nop");
                        dout[i] = diff[i] != INT32_MAX ? dout[i-1] + e2 * diff[i] : *outier++;
                }
                TOK(Machete_decode_restore_time);
        }
        free(diff);
        return header->data_len;
}

decltype(&machete_compress_huffman<HuffmanEnc>) func_com[] = {
        &machete_compress_huffman<HuffmanEnc>,
        &machete_compress_huffman<HuffmanPEnc>,
        &machete_compress_huffman<HuffmanC0Enc>,
        &machete_compress_huffman<HuffmanCEnc>,
        &machete_compress_huffman<HuffmanPCEnc>,
};

decltype(&machete_decompress_huffman<HuffmanDec>) func_dec[] = {
        &machete_decompress_huffman<HuffmanDec>,
        &machete_decompress_huffman<HuffmanPDec>,
        &machete_decompress_huffman<HuffmanC0Dec>,
        &machete_decompress_huffman<HuffmanCDec>,
        &machete_decompress_huffman<HuffmanPCDec>,
};

// C wrapper
#include "machete_c.h"

int machete_compress_huffman_naive(double* in, size_t len, uint8_t** out, double error) {
        return machete_compress_huffman<HuffmanEnc>(in, len, out, error);
}
int machete_decompress_huffman_naive(uint8_t* in, size_t len, double* out, double error) {
        return machete_decompress_huffman<HuffmanDec>(in, len, out, error);
}

int machete_compress_huffman_pruning(double* in, size_t len, uint8_t** out, double error) {
        return machete_compress_huffman<HuffmanPEnc>(in, len, out, error);
}
int machete_decompress_huffman_pruning(uint8_t* in, size_t len, double* out, double error) {
        return machete_decompress_huffman<HuffmanPDec>(in, len, out, error);
}

int machete_compress_huffman_pruning_vlq(double* in, size_t len, uint8_t** out, double error) {
        return machete_compress_huffman<HuffmanC0Enc>(in, len, out, error);
}
int machete_decompress_huffman_pruning_vlq(uint8_t* in, size_t len, double* out, double error) {
        return machete_decompress_huffman<HuffmanC0Dec>(in, len, out, error);
}

int machete_compress_huffman_pruning_vlqa(double* in, size_t len, uint8_t** out, double error) {
        return machete_compress_huffman<HuffmanCEnc>(in, len, out, error);
}
int machete_decompress_huffman_pruning_vlqa(uint8_t* in, size_t len, double* out, double error) {
        return machete_decompress_huffman<HuffmanCDec>(in, len, out, error);
}

int machete_compress_huffman_tabtree(double* in, size_t len, uint8_t** out, double error) {
        return machete_compress_huffman<HuffmanPCEnc>(in, len, out, error);
}
int machete_decompress_huffman_tabtree(uint8_t* in, size_t len, double* out, double error) {
        return machete_decompress_huffman<HuffmanPCDec>(in, len, out, error);
}
