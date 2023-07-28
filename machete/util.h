#pragma once 

#include <unordered_map>

#define DIV_UP(x,b)     (((x) + (1<<(b)) - 1) >> (b))
#define ALIGN_UP(x,b)   (((x) + (1 << (b)) - 1) & ~((1 << (b))-1))

#define likely(condiction)      __builtin_expect((condiction), 1)
#define unlikely(condiction)    __builtin_expect((condiction), 0)

#define CHECK_WSP_LEN(wsp,size) assert(\
        ((wsp)) + ((size)) < \
        ((wsp ## _end)))

// #define STATISTICS

#ifdef STATISTICS
        #define TIK             size_t _cs = clock();
        #define TOK(var)        var += clock() - _cs;
        #define SET(var, val)   var = val;
        #define ADD(var, val)   var += val;
        #define STAT_BLOCK(code) {code}
#else 
        #define TIK 
        #define TOK(var) 
        #define SET(var, val)
        #define ADD(var, val)
        #define STAT_BLOCK(code) 
#endif 

#define RECORD(var, func) {TIK; func; TOK(var);}

typedef std::unordered_map<int32_t, int32_t> FreqTab;

static inline void 
init_frequency_table(FreqTab& table, int32_t* data, size_t data_len) {
        table.clear();
        for (int i = 0; i < data_len; i++) {
                table[data[i]]++;
        }
}