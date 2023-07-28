#include "huffman.h"

template<class HD>
int32_t Huffman_decode(void* data, size_t data_len, int32_t* output) {
        HD decoder;
        decoder.load_header(data);
        RECORD(Huffman_decode_codebook_time, decoder.restore_codebook());
        RECORD(Huffman_decode_time, decoder.restore_data(output));
        return decoder.data_len;
}

auto Huffman_decode_naive = &Huffman_decode<HuffmanDec>;
auto Huffman_decode_comtab0 = &Huffman_decode<HuffmanC0Dec>;
