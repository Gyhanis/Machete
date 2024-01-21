#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include <zstd.h>

ssize_t zstd_compress(double* in, ssize_t len, uint8_t** out, double error) {
        ssize_t max_size = ZSTD_compressBound(len * sizeof(double));
        *out = (uint8_t*) malloc(max_size);
        return ZSTD_compress(*out, max_size, in, len * sizeof(double), 3);
}

ssize_t zstd_decompress(uint8_t* in, ssize_t len, double* out, double error) {
        ssize_t out_size = ZSTD_getFrameContentSize(in, len);
        return ZSTD_decompress(out, out_size, in, len)/sizeof(double);
}

ssize_t zlib_compress(double* in, ssize_t len, uint8_t** out, double error) {
        ssize_t out_size = compressBound(len * sizeof(double));
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

ssize_t zlib_decompress(uint8_t* in, ssize_t len, double* out, double error) {
        ssize_t out_size = len * 4;
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

