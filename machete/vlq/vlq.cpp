#include <stdint.h>

struct VLQ_BYTE{
        uint8_t flag:1;
        uint8_t data:7;
};

int VLQ_encode(int32_t in, uint8_t *out) {
        VLQ_BYTE* _out = (VLQ_BYTE*) out;
        int _in = in < 0 ? ~in : in;
        int blen = 33 - __builtin_clz(_in);
        int size = (blen - 1) / 7;
        for (int i = 0; i < size; i++) {
                _out[i].flag = 1;
                _out[i].data = in;
                in >>= 7;
        }
        _out[size].flag = 0;
        _out[size].data = in;
        return size+1;
}

int VLQ_decode(uint8_t *in, int32_t *out) {
        VLQ_BYTE* _in = (VLQ_BYTE*) in;
        uint32_t _out = 0;
        int size = 0;
        while(_in[size].flag) {
                _out >>= 7;
                _out |= _in[size++].data << (32 - 7);
        }
        int rest = 32 - size*7;
        if (rest>=7) {
                _out >>= 7;
                _out |= _in[size].data << (32 - 7);
                *out = _out;
                *out >>= 25 - size * 7;
        } else {
                _out >>= rest;
                _out |= _in[size].data << (32 - rest);
                *out = _out;
        }
        return size + 1;
}
