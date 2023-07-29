#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include <zstd.h>
#include "SZ3/api/sz.hpp"
#include "zfp/zfp.h"

extern "C" {
        int sz_compress(double* in, size_t len, uint8_t** out, double error);
        int sz_decompress(uint8_t* in, size_t len, double* out, double error);
        int zfp_compress_wrapper(double* in, size_t len, uint8_t** out, double error);
        int zfp_decompress_wrapper(uint8_t* in, size_t len, double* out, double error);
}

#define ZSTD_COMPRESSION_LEVEL ZSTD_maxCLevel()

int zfp_compress_wrapper(double* in, size_t len, uint8_t** out, double error) {
        zfp_field* field = zfp_field_1d(in, zfp_type_double, len);
        zfp_stream* stream = zfp_stream_open(NULL);
        zfp_stream_set_accuracy(stream, error);
        size_t bufsize = zfp_stream_maximum_size(stream, field);
        *out = (uint8_t*) malloc(bufsize+4);
        *(uint32_t*) *out = len;
        bitstream* bstream = stream_open(*out+4, bufsize);
        zfp_stream_set_bit_stream(stream, bstream);
        zfp_stream_rewind(stream);
        int out_size = zfp_compress(stream, field);
        if (out_size==0) {
                fprintf(stderr, "zfp compression failed\n");
        }
        zfp_field_free(field);
        zfp_stream_close(stream);
        stream_close(bstream);
        return out_size+4;
} 

int zfp_decompress_wrapper(uint8_t* in, size_t len, double* out, double error) {
        size_t out_len = *(uint32_t*) in;
        zfp_field* field = zfp_field_1d(out, zfp_type_double, out_len);
        zfp_stream* stream = zfp_stream_open(NULL);
        zfp_stream_set_accuracy(stream, error);
        bitstream* bstream = stream_open(in+4, len);
        zfp_stream_set_bit_stream(stream, bstream);
        zfp_stream_rewind(stream);
        if (!zfp_decompress(stream, field)) {
                fprintf(stderr, "zfp decompression failed\n");
        }
        zfp_field_free(field);
        zfp_stream_close(stream);
        stream_close(bstream);
        return out_len;
}

int zstd_compress(double* in, size_t len, uint8_t** out, double error) {
        size_t max_size = ZSTD_compressBound(len * sizeof(double));
        *out = (uint8_t*) malloc(max_size);

        return ZSTD_compress(*out, max_size, in, len * sizeof(double), ZSTD_COMPRESSION_LEVEL);
}

int zstd_decompress(uint8_t* in, size_t len, double* out, double error) {
        size_t out_size = ZSTD_getFrameContentSize(in, len);
        return ZSTD_decompress(out, out_size, in, len)/sizeof(double);
}

int zlib_compress(double* in, size_t len, uint8_t** out, double error) {
        size_t out_size = compressBound(len * sizeof(double));
        *out = (uint8_t*) malloc(out_size);

        z_stream stream = {
                (Bytef*)in, (uint32_t) (len*sizeof(double)), 0,
                *out, (uint32_t) out_size, 0,
                NULL, NULL,
                NULL, NULL, NULL,
                Z_BINARY, 0, 0
        };

        deflateInit(&stream, Z_DEFAULT_COMPRESSION);
        deflate(&stream, Z_FINISH);
        out_size -= stream.avail_out;
        deflateEnd(&stream);
        return out_size;
}

int zlib_decompress(uint8_t* in, size_t len, double* out, double error) {
        size_t out_size = len * 4;
        z_stream stream = {
                (Bytef*) in, (uint32_t) len, 0,
                (Bytef*) out, (uint32_t) out_size, 0,
                NULL, NULL,
                NULL, NULL, NULL,
                Z_BINARY,
                0, 0
        };

        inflateInit(&stream);
        int ret = inflate(&stream, Z_NO_FLUSH);
        while (ret != Z_STREAM_END) {
                if (__builtin_expect(stream.avail_out == 0, 0)) {
                        size_t cur_len = out_size;
                        out_size *= 2;
                        stream.next_out = ((Bytef*)out) + cur_len;
                        stream.avail_out = cur_len;
                }
                ret = inflate(&stream, Z_NO_FLUSH);
        }
        out_size -= stream.avail_out;
        inflateEnd(&stream);

        return out_size/sizeof(double);
}

int sz_compress(double* in, size_t len, uint8_t** out, double error) {
        if (len <= 1) {
                *out = (uint8_t*) malloc(len * sizeof(double) + 4);
                *(uint32_t*) *out = len;
                __builtin_memcpy(*out+4, in, len * sizeof(double));
                return len * sizeof(double) + 4;
        } else {
                SZ::Config config(len);
                config.errorBoundMode = SZ::EB_ABS;
                config.absErrorBound = error;
                // config.interpAlgo = SZ::INTERP_ALGO_LINEAR;
                size_t out_size;
                char* tmp = SZ_compress<double>(config, in, out_size);
                *out = (uint8_t*) malloc(out_size + 4);
                *(uint32_t*) *out = len;
                __builtin_memcpy(*out+4, tmp, out_size);
                delete[]tmp;
                return out_size+4;
        }
}

int sz_decompress(uint8_t* in, size_t len, double* out, double error) {
        uint32_t out_len = *(uint32_t*) in;
        if (out_len <= 1) {
                __builtin_memcpy(out, in + 4, out_len * sizeof(double));
                return out_len;
        } else {
                SZ::Config config(out_len);
                config.errorBoundMode = SZ::EB_ABS;
                config.absErrorBound = error;
                SZ_decompress<double>(config, (char*) (&in[4]), len-4, out);
                return out_len;
        }
}

