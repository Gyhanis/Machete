#include "defs.h"
#include <queue>
#include <stack>
#include <cstdlib>

#include "BitStream/BitReader.h"
#include "BitStream/BitWriter.h"

static bool great_on_cnt(HufTree *n0, HufTree *n1) {
        return n0->cnt > n1->cnt;
}

static bool less_on_lvl(HufTree *n0, HufTree* n1) {
        return n0->lvl > n1->lvl;
}

HufTree* huffman_build_tree(std::unordered_map<int32_t, size_t> &freq, HufTree* &nodes) {
        std::priority_queue<HufTree*, std::vector<HufTree*>, decltype(&great_on_cnt)> queue(great_on_cnt);
        
        nodes = new HufTree[freq.size() * 2-1];

        int cur_node = 0;
        for (auto ent : freq) {
                nodes[cur_node].val = ent.first;
                nodes[cur_node].cnt = ent.second;
                queue.push(&nodes[cur_node]);
                cur_node++;
        }

        while (queue.size() > 1) {
                HufTree* n1 = queue.top();
                queue.pop();
                HufTree* n2 = queue.top();
                queue.pop();
                nodes[cur_node] = {0, n1->cnt + n2->cnt, 0, n1, n2};
                queue.push(&nodes[cur_node]);
                cur_node++;
        }

        return queue.top();
}

ssize_t huffman_build_encode_codebook(HufTree* root, EncodeCodebook &codebook, int32_t* vals) {
        root->lvl = 0;
        std::stack<HufTree*> stack;
        stack.push(root);
        int cnt = 0;
        int32_t code = 0;
        int code_bitlen = 0;
        ssize_t total_bitlen = 0;
        while (!stack.empty()) {
                HufTree* n = stack.top();
                stack.pop();
                if (n->left) {
                        n->left->lvl = n->lvl + 1;
                        n->right->lvl = n->lvl + 1;
                        stack.push(n->right);
                        stack.push(n->left);
                } else {
                        if (code_bitlen > n->lvl) {
                                code >>= code_bitlen - n->lvl;
                        } else {
                                code <<= n->lvl - code_bitlen;
                        }
                        code_bitlen = n->lvl;
                        codebook[n->val] = CodebookEntry{code, n->lvl};
                        vals[cnt++] = n->val;
                        code++;
                        total_bitlen += n->lvl * n->cnt;
                }
        }
        return total_bitlen;
}

// A canonical codebook is a code book built on canonical huffman tree, so that the entries are sorted by the huffman code length.
ssize_t huffman_build_canonical_encode_codebook(HufTree* root, EncodeCodebook &codebook, int32_t* vals) {
        root->lvl = 0;
        std::stack<HufTree*> stack;
        std::priority_queue<HufTree*, std::vector<HufTree*>, decltype(&less_on_lvl)> queue(less_on_lvl);
        stack.push(root);

        while (!stack.empty()) {
                HufTree* n = stack.top();
                stack.pop();
                if (n->left) {
                        n->left->lvl = n->lvl + 1;
                        n->right->lvl = n->lvl + 1;
                        stack.push(n->right);
                        stack.push(n->left);
                } else {
                        queue.push(n);
                }
        }

        int cnt = 0;
        int32_t code = 0;
        int code_bitlen = 0;
        ssize_t total_bitlen = 0;
        while (!queue.empty()) {
                HufTree *n = queue.top();
                queue.pop();
                code <<= n->lvl - code_bitlen;
                code_bitlen = n->lvl;
                codebook[n->val] = CodebookEntry{code, n->lvl};
                vals[cnt++] = n->val;
                code++;
                total_bitlen += n->lvl * n->cnt;
        }
        return total_bitlen;
}

ssize_t huffman_store_codebook(EncodeCodebook &codebook, int32_t* vals, int32_t val_cnt, uint8_t* output) {
        __builtin_memcpy(output, vals, val_cnt * sizeof(vals[0]));
        uint16_t* bitlens = reinterpret_cast<uint16_t*> (output + val_cnt * sizeof(vals[0]));
        for (int i = 0; i < val_cnt; i++) {
                bitlens[i] = codebook[vals[i]].bitlen;
        }
        return 0;
}

ssize_t huffman_store_canonical_codebook(EncodeCodebook &codebook, int32_t* vals, int32_t val_cnt, uint8_t* output) {
        __builtin_memcpy(output, vals, val_cnt * sizeof(vals[0]));
        uint16_t* bitlens = reinterpret_cast<uint16_t*> (output + val_cnt * sizeof(vals[0]));
        bitlens[0] = codebook[vals[0]].bitlen;
        bitlens[1] = codebook[vals[val_cnt-1]].bitlen;
        int bl = bitlens[0]-1;
        int i = 1;
        for (int j = 0; j < val_cnt; j++) {
                while (codebook[vals[j]].bitlen != bl) {
                        i++;
                        bl++;
                        bitlens[i] = 0;
                }
                bitlens[i] ++;
        }
        return 0;
}

ssize_t huffman_store_code(EncodeCodebook &codebook, int32_t* input, int32_t len, uint8_t* output, ssize_t osize) {
        if (LIKELY(codebook.size() > 1)) {
                BitWriter writer;
                initBitWriter(&writer, reinterpret_cast<uint32_t*>(output), osize/4);
                for (int i = 0; i < len; i++) {
                        auto e = codebook[input[i]];
                        write(&writer, e.code, e.bitlen);
                }
                flush(&writer);
        }
        return 0;
}

typedef struct __attribute__((__packed__)){
        uint32_t data_len;
        uint32_t val_cnt;
        uint32_t code_size;
        uint8_t  payload[0];
} HufHeader;

ssize_t huffman_encode(int32_t* input, ssize_t len, uint8_t** output) {
        std::unordered_map<int32_t, size_t> freq;
        for (int i = 0; i < len; i++) {
                freq[input[i]]++;
        }             
        HufTree* nodes;
        HufTree* root = huffman_build_tree(freq, nodes);
        EncodeCodebook codebook;
        int32_t* vals = new int32_t[freq.size()];
        ssize_t total_bitlen = huffman_build_encode_codebook(root, codebook, vals);
        delete[] nodes;

        ssize_t codebook_size = freq.size() * (sizeof(int32_t) + sizeof(int16_t));
        ssize_t code_size = (total_bitlen + 31) / 32 * 4;
        ssize_t osize = sizeof(HufHeader) + codebook_size + code_size;
        *output = reinterpret_cast<uint8_t*>(malloc(osize));
        HufHeader* header = reinterpret_cast<HufHeader*>(*output);
        header->data_len = len;
        header->val_cnt = freq.size();
        header->code_size = code_size;
        huffman_store_codebook(codebook, vals, header->val_cnt, header->payload);
        huffman_store_code(codebook, input, len, header->payload + codebook_size, code_size);
        delete[] vals;

        return osize;
}

ssize_t huffman_encode_canonical(int32_t* input, ssize_t len, uint8_t** output) {
        std::unordered_map<int32_t, size_t> freq;
        for (int i = 0; i < len; i++) {
                freq[input[i]]++;
        }             
        HufTree* nodes;
        HufTree* root = huffman_build_tree(freq, nodes);
        EncodeCodebook codebook;
        int32_t* vals = new int32_t[freq.size()];
        ssize_t total_bitlen = huffman_build_canonical_encode_codebook(root, codebook, vals);
        delete[] nodes;

        ssize_t codebook_size = freq.size() * sizeof(int32_t) + (codebook[vals[codebook.size()-1]].bitlen - codebook[vals[0]].bitlen + 3) * sizeof(int16_t);
        ssize_t code_size = (total_bitlen + 31) / 32 * 4;
        ssize_t osize = sizeof(HufHeader) + codebook_size + code_size;
        *output = reinterpret_cast<uint8_t*>(malloc(osize));
        HufHeader* header = reinterpret_cast<HufHeader*>(*output);
        header->data_len = len;
        header->val_cnt = freq.size();
        header->code_size = code_size;
        huffman_store_canonical_codebook(codebook, vals, header->val_cnt, header->payload);
        huffman_store_code(codebook, input, len, header->payload + codebook_size, code_size);
        delete[] vals;
        return osize;
}

ssize_t huffman_build_decode_codebook(int32_t* vals, int16_t* bitlens, uint32_t val_cnt, DecodeCodebook &codebook) {
        int16_t max_bitlen = 0;
        for (int i = 0; i < val_cnt; i++) {
                max_bitlen = max_bitlen > bitlens[i] ? max_bitlen : bitlens[i];
        }
        CodebookEntry* p = codebook = new CodebookEntry[1 << max_bitlen];
        for (int i = 0; i < val_cnt; i++) {
                size_t times = 1UL << (max_bitlen - bitlens[i]);
                for (int j = 0; j < times; j++) {
                        *p++ = {vals[i], bitlens[i]};
                }
        }
        return max_bitlen;
}

ssize_t huffman_build_decode_codebook_canonical(int32_t* vals, int16_t* bitlens, uint32_t val_cnt, DecodeCodebook &codebook) {
        int16_t max_bitlen = bitlens[1];
        int bitlen = bitlens[0];
        int bitlen_cnt = 2;
        CodebookEntry* p = codebook = new CodebookEntry[1 << max_bitlen];
        for (int i = 0; i < val_cnt; i++) {
                while (bitlens[bitlen_cnt] == 0) {
                        bitlen_cnt++;
                        bitlen++;
                }
                bitlens[bitlen_cnt]--;
                size_t times = 1UL << (max_bitlen - bitlen);
                for (int j = 0; j < times; j++) {
                        *p++ = {vals[i], bitlen};
                }
        }
        return max_bitlen;
}

ssize_t huffman_decode_data(uint8_t* input, uint32_t code_size, DecodeCodebook codebook, ssize_t index_bitlen, int32_t* output, int32_t olen) {
        if (LIKELY(index_bitlen)) {
                BitReader reader;
                initBitReader(&reader, reinterpret_cast<uint32_t*>(input), code_size/4);
                for (int i = 0; i < olen; i++) {
                        int code = peek(&reader, index_bitlen);
                        output[i] = codebook[code].val;
                        forward(&reader, codebook[code].bitlen);
                }
        } else {
                for (int i = 0; i < olen; i++) {
                        output[i] = codebook[0].val;
                }
        }
        return 0;
}

ssize_t huffman_decode(uint8_t* input, ssize_t size, int32_t* output) {
        HufHeader* header = reinterpret_cast<HufHeader*>(input);
        DecodeCodebook codebook;
        int32_t* vals = reinterpret_cast<int32_t*>(header->payload);
        int16_t* bitlens = reinterpret_cast<int16_t*>(vals + header->val_cnt);
        ssize_t index_bitlen = huffman_build_decode_codebook(vals, bitlens, header->val_cnt, codebook);
        ssize_t codebook_size = header->val_cnt * (sizeof(int32_t) + sizeof(int16_t));
        huffman_decode_data(header->payload + codebook_size, header->code_size, codebook, index_bitlen, output, header->data_len);
        delete[] codebook;
        return header->data_len;
}

ssize_t huffman_decode_canonical(uint8_t* input, ssize_t size, int32_t* output) {
        HufHeader* header = reinterpret_cast<HufHeader*>(input);
        DecodeCodebook codebook;
        int32_t* vals = reinterpret_cast<int32_t*>(header->payload);
        int16_t* bitlens = reinterpret_cast<int16_t*>(vals + header->val_cnt);
        ssize_t index_bitlen = huffman_build_decode_codebook_canonical(vals, bitlens, header->val_cnt, codebook);
        ssize_t codebook_size = header->val_cnt * sizeof(int32_t) + (bitlens[1] - bitlens[0] + 3) * sizeof(int16_t);
        huffman_decode_data(header->payload + codebook_size, header->code_size, codebook, index_bitlen, output, header->data_len);
        delete[] codebook;
        return header->data_len;
}