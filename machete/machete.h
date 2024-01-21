#include <unistd.h>
#include <stdint.h>

enum Encoder {huffman, huffmanC, ovlq, hybrid};
enum Predictor {lorenzo1};

template<Predictor p, Encoder e>
ssize_t machete_compress(double* input, ssize_t len, uint8_t** output, double error);
template<Predictor p, Encoder e>
ssize_t machete_decompress(uint8_t* input, ssize_t size, double* output);
