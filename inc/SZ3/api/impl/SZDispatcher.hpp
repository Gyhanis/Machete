#ifndef SZ3_IMPL_SZDISPATCHER_HPP
#define SZ3_IMPL_SZDISPATCHER_HPP

#include "SZ3/utils/MemoryUtil.hpp"
#include "SZ3/utils/Statistic.hpp"
#include "SZ3/utils/Config.hpp"
#include "SZ3/api/impl/SZInterp.hpp"
#include "SZ3/api/impl/SZLorenzoReg.hpp"
#include <cmath>


template<class T, SZ::uint N>
char *SZ_compress_dispatcher(SZ::Config &conf, T *data, size_t &outSize) {

    assert(N == conf.N);
    SZ::calAbsErrorBound(conf, data);

    char *cmpData;
    if (conf.absErrorBound == 0) {
        auto zstd = SZ::Lossless_zstd();
        cmpData = (char *) zstd.compress((SZ::uchar *) data, conf.num * sizeof(T), outSize);
    } else if (conf.cmprAlgo == SZ::ALGO_LORENZO_REG) {
        // printf("ALGO_LORENZO_REG\n");
        cmpData = (char *) SZ_compress_LorenzoReg<T, N>(conf, data, outSize);
    } else if (conf.cmprAlgo == SZ::ALGO_INTERP) {
        // printf("ALGO_INTERP\n");
        cmpData = (char *) SZ_compress_Interp<T, N>(conf, data, outSize);
    } else if (conf.cmprAlgo == SZ::ALGO_INTERP_LORENZO) {
        // printf("ALGO_INTERP_LORENZO\n");
        cmpData = (char *) SZ_compress_Interp_lorenzo<T, N>(conf, data, outSize);
    }
    return cmpData;
}


template<class T, SZ::uint N>
void SZ_decompress_dispatcher(SZ::Config &conf, char *cmpData, size_t cmpSize, T *decData) {
    if (conf.absErrorBound == 0) {
        auto zstd = SZ::Lossless_zstd();
        auto zstdOut = zstd.decompress((SZ::uchar *) cmpData, cmpSize);
        memcpy(decData, zstdOut, conf.num * sizeof(T));
    } else if (conf.cmprAlgo == SZ::ALGO_LORENZO_REG) {
        SZ_decompress_LorenzoReg<T, N>(conf, cmpData, cmpSize, decData);
    } else if (conf.cmprAlgo == SZ::ALGO_INTERP) {
        SZ_decompress_Interp<T, N>(conf, cmpData, cmpSize, decData);
    } else {
        printf("SZ_decompress_dispatcher, Method not supported\n");
        exit(0);
    }
}

#endif