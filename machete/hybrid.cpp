#include "defs.h"
#include <assert.h>
#include <vector>
#include <cstdlib>

ssize_t hybrid_data_partition(int32_t* input, ssize_t len, std::vector<int32_t> &low_redundancy_data, ssize_t &rare_cnt, int32_t &rare_sym, EncodeCodebook &codebook) {
        // ------------------- rare_extraction --------------------
        std::unordered_map<int32_t, size_t> freq;
        for (int i = 0; i < len; i++) {
                freq[input[i]]++;
        }  
        rare_cnt = 0;

        for (auto f : freq) {
                if (f.second == 1) {
                        rare_sym = rare_cnt == 0 ? f.first : rare_sym;
                        rare_cnt++;
                }
        }

        if (rare_cnt <= 1) { // rare extration won't help if there is no more than one rare symbol.
                rare_cnt = 0;
                low_redundancy_data.resize(freq.size());
        } else {
                low_redundancy_data.reserve(freq.size() + 1);
                low_redundancy_data.resize(freq.size() + 1 - rare_cnt);
                for (int i = 0; i < len; i++) {
                        if (freq[input[i]] == 1) {
                                low_redundancy_data.push_back(input[i]);
                                freq.erase(input[i]);
                                input[i] = rare_sym;
                        }
                }
                freq[rare_sym] = rare_cnt;
        }

        // ------------------------ build huffman tree ---------------
        HufTree* nodes;
        HufTree* root = huffman_build_tree(freq, nodes);
        ssize_t total_bitlen = huffman_build_canonical_encode_codebook(root, codebook, &low_redundancy_data[0]);
        delete[] nodes;
        return total_bitlen;
}

struct HybridHeader {
        int32_t len;
        int32_t rare_cnt;
        int32_t rare_sym;
        int32_t val_cnt;
        int32_t huffman_code_size;
        int32_t ovlq_size;
        uint8_t payload[0];
};

ssize_t hybrid_encode(int32_t* input, ssize_t len, uint8_t** output) {
        std::vector<int32_t> low_redundancy_data;
        int32_t rare_sym;
        ssize_t rare_cnt;
        EncodeCodebook codebook;
        ssize_t huffman_total_bitlen = hybrid_data_partition(input, len, low_redundancy_data, rare_cnt, rare_sym, codebook);
        ssize_t huffman_code_size = ((huffman_total_bitlen+31) / 32 * 4);
        int16_t min_code_bitlen = codebook[low_redundancy_data[0]].bitlen;
        int16_t max_code_bitlen = codebook[low_redundancy_data[codebook.size()-1]].bitlen;

        uint8_t *ovlq_out;
        ssize_t ovlq_size = ovlq_encode(&low_redundancy_data[0], low_redundancy_data.size(), &ovlq_out);
        
        // fill header
        ssize_t huffman_tree_st_size = (max_code_bitlen - min_code_bitlen + 3) * sizeof(int16_t);
        ssize_t osize = sizeof(HybridHeader) + huffman_tree_st_size + ovlq_size + huffman_code_size;
        *output = reinterpret_cast<uint8_t*>(malloc(osize));
        HybridHeader* header = reinterpret_cast<HybridHeader*>(*output);
        header->len = len;
        header->rare_cnt = rare_cnt;
        header->rare_sym = rare_sym;
        header->val_cnt = codebook.size();
        header->huffman_code_size = huffman_code_size;
        header->ovlq_size = ovlq_size;
        int16_t* huffman_tree_st = reinterpret_cast<int16_t*>(header->payload);
        
        // store huffman tree structure
        huffman_tree_st[0] = min_code_bitlen;
        huffman_tree_st[1] = max_code_bitlen;
        int bl = huffman_tree_st[0]-1;
        int i = 1;
        for (int j = 0; j < codebook.size(); j++) {
                while (codebook[low_redundancy_data[j]].bitlen != bl) {
                        i++;
                        bl++;
                        huffman_tree_st[i] = 0;
                }
                huffman_tree_st[i] ++;
        }
        
        // store ovlq result
        uint8_t* _ovlq_out = header->payload + huffman_tree_st_size;
        __builtin_memcpy(_ovlq_out, ovlq_out, ovlq_size);
        free(ovlq_out);

        // store huffman_code
        uint8_t* huffman_out = _ovlq_out + ovlq_size;
        huffman_store_code(codebook, input, len, huffman_out, huffman_code_size);
        return osize;
}

ssize_t hybrid_decode(uint8_t* input, ssize_t size, int32_t* output) {
        HybridHeader* header = reinterpret_cast<HybridHeader*>(input);
        int16_t *huffman_tree_st = reinterpret_cast<int16_t*>(header->payload);
        uint8_t *ovlq_out = header->payload + (huffman_tree_st[1] - huffman_tree_st[0] + 3) * sizeof(int16_t);
        uint8_t *huffman_out = ovlq_out + header->ovlq_size;
        
        int32_t *low_redundancy_data = new int32_t[header->val_cnt + header->rare_cnt];
        ovlq_decode(ovlq_out, header->ovlq_size, low_redundancy_data);
        
        DecodeCodebook codebook;
        ssize_t index_bitlen = huffman_build_decode_codebook_canonical(low_redundancy_data, huffman_tree_st, header->val_cnt, codebook);
        huffman_decode_data(huffman_out, header->huffman_code_size, codebook, index_bitlen, output, header->len);
        delete[] codebook;

        if (header->rare_cnt) {
                int32_t *rare = low_redundancy_data + header->val_cnt;
                for (int i = 0; i < header->len; i++) {
                        if (output[i] == header->rare_sym) {
                                output[i] = *rare++;
                        }
                }
        }
        delete[] low_redundancy_data;
        return header->len;
}
