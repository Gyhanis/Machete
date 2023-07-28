#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

int machete_compress_huffman_naive(double* in, size_t len, uint8_t** out, double error);
int machete_decompress_huffman_naive(uint8_t* in, size_t len, double* out, double error);

int machete_compress_huffman_pruning(double* in, size_t len, uint8_t** out, double error);
int machete_decompress_huffman_pruning(uint8_t* in, size_t len, double* out, double error);

int machete_compress_huffman_pruning_vlq(double* in, size_t len, uint8_t** out, double error);
int machete_decompress_huffman_pruning_vlq(uint8_t* in, size_t len, double* out, double error);

int machete_compress_huffman_pruning_vlqa(double* in, size_t len, uint8_t** out, double error);
int machete_compress_huffman_pruning_ovlqa(double* in, size_t len, uint8_t** out, double error);
int machete_decompress_huffman_pruning_vlqa(uint8_t* in, size_t len, double* out, double error);

int machete_compress_huffman_tabtree(double* in, size_t len, uint8_t** out, double error);
int machete_decompress_huffman_tabtree(uint8_t* in, size_t len, double* out, double error);

#ifdef __cplusplus
}
#endif 
