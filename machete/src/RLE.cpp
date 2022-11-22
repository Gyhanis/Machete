#include <stdint.h>

void RLE_Encode(int32_t* din, int32_t din_len, int32_t* dout, int32_t* dout_len) {
        int last = din[0];
        int cnt = 1;
        int pout = 0;
        for (int i = 1; i < din_len; i++) {
                if (__builtin_expect(din[i] == last, 1)) {
                        cnt++;
                } else {
                        dout[pout] = last;
                        dout[pout+1] = cnt;
                        pout += 2;
                        last = din[i];
                        cnt = 1;    
                }
        }
        dout[pout] = last;
        dout[pout+1] = cnt;
        *dout_len = pout+2;
}

void RLE_Decode(int32_t* din, int32_t din_len, int32_t* dout, int32_t* dout_len) {
        int pout = 0;
        for (int i = 0; i < din_len-1; i+=2) {
                for (int j = 0; j < din[i+1]; j++) {
                        dout[pout++] = din[i];
                }
        }
        if (dout_len)
                *dout_len = pout;
}