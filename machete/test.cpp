#include "defs.h"
#include <stdio.h>

#define DLEN 1000

int32_t data[DLEN];
int32_t data2[DLEN];

double data3[DLEN];
double data4[DLEN];

bool check_data_int(int len) {
        if (len != DLEN) {
                printf("Length mismatch!\n");
                return false;
        } else {
                for (int i = 0; i < DLEN; i++) {
                        if (data[i] != data2[i]) {
                                printf("Data mismatch: %d: %d vs %d!\n", i, data[i], data2[i]);
                                return false;
                        }
                } 
        }
        return true;
}

bool check_data_double(int len, double error) {
        if (len != DLEN) {
                printf("Length mismatch!\n");
                return false;
        } else {
                for (int i = 0; i < DLEN; i++) {
                        double diff = data3[i] - data4[i];
                        if (diff < -error || diff > error) {
                                printf("Data mismatch: %d: %.16lf vs %.16lf (diff: %.16lf)\n", i, data3[i], data4[i], data3[i] - data4[i]);
                                return false;
                        }
                }
        }
        return true;
}

void test_encoder(Encoder e) {
        uint8_t* output;
        ssize_t compressed_size, decompressed_len;
        printf("-----------");
        switch (e) {
                case huffman: printf("Testing huffman"); break;
                case huffmanC: printf("Testing huffman(canonical)"); break;
                case ovlq: printf("Testing ovlq"); break;
                case hybrid: printf("Testing hybrid encoder"); break;
                default: printf("unknown encoder"); return;
        }
        printf("----------\n");
        switch (e) {
                case huffman: compressed_size = huffman_encode(data, DLEN, &output); break;
                case huffmanC: compressed_size = huffman_encode_canonical(data, DLEN, &output); break;
                case ovlq: compressed_size = ovlq_encode(data, DLEN, &output); break;
                case hybrid: compressed_size = hybrid_encode(data, DLEN, &output); break;
        }
        printf("compression ratio = %lf\n", static_cast<double>(sizeof(data)) / compressed_size);
        switch (e) {
                case huffman: decompressed_len = huffman_decode(output, compressed_size, data2); break;
                case huffmanC: decompressed_len = huffman_decode_canonical(output, compressed_size, data2); break;
                case ovlq: decompressed_len = ovlq_decode(output, compressed_size, data2); break;
                case hybrid: decompressed_len = hybrid_decode(output, compressed_size, data2); break;
        }
        
        if (check_data_int(decompressed_len)) {
                switch (e) {
                        case huffman: printf("huffman test passed\n"); break;
                        case huffmanC: printf("huffman(canonical) test passed\n"); break;
                        case ovlq: printf("ovlq test passed\n"); break;
                        case hybrid: printf("hybrid test passed\n"); break;
                }
        }
        free(output);
}

template<Predictor p, Encoder e>
void test_machete(double error) {
        printf("--------- Testing Machete ---------\n");
        uint8_t* output;
        ssize_t compressed_size = machete_compress<p, e>(data3, DLEN, &output, error);
        printf("compression ratio = %lf\n", static_cast<double>(sizeof(data)) / compressed_size);
        ssize_t decompressed_len = machete_decompress<p, e>(output, compressed_size, data4);
        if (check_data_double(decompressed_len, error))
                printf("Machete test passed\n");
        free(output);
}

int main() {
        __builtin_memset(data, 0, sizeof(data));
        test_encoder(huffman);
        for (int i = 0; i < 1000; i++) {
                data[i] = rand() % 20;
        }
        test_encoder(huffman);

        __builtin_memset(data, 0, sizeof(data));
        test_encoder(huffmanC);
        for (int i = 0; i < 1000; i++) {
                data[i] = rand() % 20;
        }
        test_encoder(huffmanC);

        __builtin_memset(data, 0, sizeof(data));
        test_encoder(ovlq);
        for (int i = 0; i < 1000; i++) {
                data[i] = rand() % 20;
        }
        test_encoder(ovlq);

        __builtin_memset(data, 0, sizeof(data));
        test_encoder(hybrid);
        for (int i = 0; i < 1000; i++) {
                data[i] = rand() % 20;
        }
        test_encoder(hybrid);

        FILE* fp;
        __builtin_memset(data3, 0, sizeof(data3));
        test_machete<lorenzo1, huffman>(1E-5);
        fp = fopen("tmp0.data", "r");
        fread(data3, sizeof(double), DLEN, fp);
        fclose(fp);
        test_machete<lorenzo1, huffman>(1E-6);

        __builtin_memset(data3, 0, sizeof(data3));
        test_machete<lorenzo1, ovlq>(1E-5);
        fp = fopen("tmp0.data", "r");
        fread(data3, sizeof(double), DLEN, fp);
        fclose(fp);
        test_machete<lorenzo1, ovlq>(1E-6);

        __builtin_memset(data3, 0, sizeof(data3));
        test_machete<lorenzo1, hybrid>(1E-5);
        fp = fopen("tmp0.data", "r");
        fread(data3, sizeof(double), DLEN, fp);
        fclose(fp);
        test_machete<lorenzo1, hybrid>(1E-6);

        return 0;
}