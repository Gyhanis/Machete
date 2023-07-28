#pragma once

#include "../util.h"
#include "../vlq/vlqa.h"

#include <unistd.h>
#include <unordered_map>

extern size_t Huffman_encode_tree_time;
extern size_t Huffman_prune_time;
extern size_t Huffman_encode_codebook_time;
extern size_t Huffman_encode_search_time;
extern size_t Huffman_encode_time;

extern size_t Huffman_decode_codebook_time;
extern size_t Huffman_decode_time;
extern size_t Huffman_symbol_cnt;
extern size_t Huffman_rare_cnt;
extern size_t Huffman_tree_size;

extern size_t Huffman_table_len;
extern size_t Huffman_lookup_time;

extern size_t Huffman_max_bitlen[16];

struct HTNode {
        int32_t symbol;
        int32_t cnt;
        int32_t lvl;
        HTNode* left;
        HTNode* right;
};

class HuffmanEnc {
protected:
        int32_t*        data;
        size_t          data_len;
        FreqTab*        freq_tab;
        int32_t         sym_cnt;
        int32_t*        symbols;
        HTNode*         huf_tree;
        HTNode*         node_pool;

        std::unordered_map<int32_t, std::pair<int32_t, int32_t>> codebook;

        int32_t         out_size;
        int32_t         code_size;
        int32_t         max_bitlen;
        
        virtual void build_tree();
        virtual void build_codebook();
        virtual void* fill_header(void* header);
        virtual void* store_codebook(void* p);
        virtual void store_data(void* p);
        virtual int32_t calculate_out_size();
public:
        virtual int32_t Huffman_prepare(FreqTab *table, int32_t* data, size_t data_len);
        int32_t Huffman_encode(void* output);
        int32_t Huffman_outsize() {
                return out_size;
        }
};

struct HDEntry {
        int32_t sym;
        int32_t len;
};

class HuffmanDec {
protected: 
        int data_len;
        int sym_cnt;
        int max_bitlen;
        int code_len;
        int32_t*        symbols;
        int16_t*        bitlen;
        uint32_t*       code;
        HDEntry*        codebook;        

        virtual void load_header(void* header);
        virtual void restore_codebook();
        virtual void restore_data(int32_t* output);
public:
        static inline int32_t Huffman_decode_len(void* data) {
                return *(int32_t*)data;
        }
        int32_t Huffman_decode(void* data, size_t data_len, int32_t* output);
};





class HuffmanC0Enc: public HuffmanEnc {
protected: 
        int32_t min_bitlen;
        int32_t vlq_size;
        void build_codebook();
        int32_t calculate_out_size();
        void* fill_header(void* header);
        void* store_codebook(void* p);
};

class HuffmanC0Dec: public HuffmanDec {
protected:
        int32_t min_bitlen;
        size_t vlq_size;
        uint8_t* vlq_code;
        int16_t *bitlen_cnt;
        int32_t s0;
        void load_header(void* header);
        void restore_codebook();
};



class HuffmanCEnc: virtual public HuffmanEnc {
protected: 
        int32_t min_bitlen;
        VLQA *vlqa;
        int32_t opt_set;
        size_t vlq_size;
        void build_codebook();
        int32_t calculate_out_size();
        void* fill_header(void* header);
        void* store_codebook(void* p);
};

class HuffmanCDec: virtual public HuffmanDec {
protected:
        int32_t min_bitlen;
        VLQA *vlqa;
        int32_t vlq_size;
        uint8_t *vlq_code;
        int16_t *bitlen_cnt;
        int32_t s0;
        void load_header(void* header);
        void restore_codebook();
};





class HuffmanPEnc: virtual public HuffmanEnc {
protected: 
        int32_t rare_sp;
        int32_t *rare_sym;
        int32_t rare_cnt;
        void prune(int threshold);
        int32_t calculate_out_size();
        void* fill_header(void* header);
        void* store_codebook(void* p);
        virtual int32_t Huffman_prepare_internal(int threshold);
public:
        int32_t Huffman_prepare(FreqTab *table, int32_t* data, size_t data_len);
};

class HuffmanPDec: virtual public HuffmanDec {
protected:
        int32_t rare_sp;
        int32_t *rare_sym;
        int32_t rare_cnt;
        void load_header(void* header);
        void restore_data(int32_t* output);
};





class HuffmanPCEnc: public HuffmanPEnc, public HuffmanCEnc {
protected: 
        int32_t calculate_out_size();
        void* fill_header(void* header);
        void* store_codebook(void* p){return HuffmanCEnc::store_codebook(p);};
};

class HuffmanPCDec: public HuffmanPDec, public HuffmanCDec {
protected:
        void load_header(void* header);
        void restore_codebook();
        void restore_data(int32_t* output) {
                HuffmanPDec::restore_data(output);
                free(symbols);
        }
};


// class HuffmanDEnc: public HuffmanEnc {
// protected:
//         int32_t height_set;
//         int32_t min_bitlen;
//         int32_t *bitlen_cnt_value;
//         int32_t *bitlen_cnt_symbol;
//         int16_t *tabcnt;
//         int32_t entry_cnt;
//         int32_t breaking;
//         int32_t *buffer;

//         void Huffman_build_sorted_codebook();
//         void Huffman_search_optimal_breaking();
// public:
//         HuffmanDEnc(FreqTab *table, int32_t* data, size_t data_len, int32_t* workspace);
//         int32_t Huffman_encode(void* output);
// };

// class HuffmanDDec: public HuffmanDec {
// protected:
//         int32_t sym_cnt;
//         int32_t min_bitlen;
//         int32_t max_bitlen;
//         int32_t* symbols;
//         int32_t* bitlen;
//         int16_t* bitlen_cnt;
//         int32_t breaking;
//         int16_t tabcnt[32];
//         void Huffman_build_codebook();
// public: 
//         HuffmanDDec() {bitlen = NULL;};
//         ~HuffmanDDec() {if (bitlen) free(bitlen);}
//         static int32_t Huffman_decode(void* data, size_t data_len, int32_t* output);
// };

// class HuffmanPVaEnc: public HuffmanPEnc {
// protected:
//         int32_t vlqa_size;
//         VLQA* vlqa;
// public:
//         HuffmanPVaEnc(FreqTab *table, int32_t* data, size_t data_len, int32_t* workspace);
//         int32_t Huffman_encode(void* output);
// };


