#include "../include/Machete.h"
#include "../include/RLE.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unordered_map>

typedef union {
        double d;
        int64_t i;
} DOUBLE;

#define RLE_RATIO       10
#define RANGER_RATIO    4

char magic_char[] = "Mc01";
const uint32_t* magic = (uint32_t*) magic_char;

typedef struct {
        uint32_t magic; 
        enum encoder enc;
        double error;
        double first;
        uint32_t data[0];
} McHeader;

#define HEADER_SIZE (sizeof(McHeader)/sizeof(uint32_t))

Machete* MachetePrepare(double * din, int32_t din_len, double error) {
        Machete* m = (Machete*) malloc(sizeof(*m));
        m->din_len = din_len;
        if (din_len <= 10) {
                m->encoder = Raw;
                memcpy(m->first, din, sizeof(double) * din_len);
                m->outsize = 2 * din_len + 2;
                return m;
        }
        DOUBLE e = {.d = error};
        e.i &= -1L << 30;
        m->first[0] = din[0];
        m->error = e.d;
        m->quan_diff = (int32_t*) malloc(sizeof(int32_t)*din_len);
        double e2 = e.d * 2;
        double last = din[0];
        std::unordered_map<int32_t, uint32_t> map;
        for (int i = 1; i < din_len; i++) {
                DOUBLE diff = {.d = din[i] - last};
                DOUBLE comp = {.i = (diff.i & (1L << 63)) | e.i};
                m->quan_diff[i] = (int32_t) ((diff.d + comp.d) / e2);
                last += e2 * m->quan_diff[i];
                map[m->quan_diff[i]] ++;
        }

        int32_t tran = 0;
        for (int i = 2; i < din_len; i++) {
                if (m->quan_diff[i] != m->quan_diff[i-1]) {
                        tran ++;
                }
        }
        if (tran < din_len / (2 * RLE_RATIO)) {
                m->encoder = RLE;
                m->outsize = HEADER_SIZE + (tran+1) * 2;
        } else {
                if (map.size() > din_len / RANGER_RATIO) {
                        m->encoder = Ranger;
                        m->ranger = ranger_probe(&m->quan_diff[1], din_len-1);
                        m->outsize = HEADER_SIZE + m->ranger->out_len;
                } else {
                        m->encoder = Huffman;
                        m->huffman = HuffmanEncodePrepare(&m->quan_diff[1], din_len-1);
                        m->outsize = HEADER_SIZE + HuffmanGetSize(m->huffman);
                }
        }
        return m;
}

int32_t MacheteEncode(Machete* m, uint32_t* dout) {
        McHeader *header = (McHeader*) dout;
        header->magic = *magic;
        header->enc = m->encoder;
        
        if (m->encoder == Raw) {
                memcpy(&header->error, m->first, m->din_len * sizeof(double));
                return 2 * m->din_len + 2;
        }

        header->error = m->error;
        header->first = m->first[0];

        int hsize;
        switch (m->encoder)
        {
        case RLE:
                RLE_Encode(&m->quan_diff[1], m->din_len-1, (int32_t*) header->data, &hsize);
                break;
        
        case Huffman:
                m->huffman->output = header->data;
                hsize = HuffmanFinish(m->huffman);
                break;
        case Ranger:
                m->ranger->output = header->data;
                hsize = m->ranger->out_len;
                ranger_encode(m->ranger);
                break;
        default: 
                printf("%s Error: unknown type\n", __FUNCTION__);
                break;
        }
        free(m->quan_diff);
        free(m);
        return HEADER_SIZE + hsize;
}

void MacheteDecode(uint32_t* din, uint32_t din_len, double* dout, uint32_t dout_len) {
        clock_t start = clock();
        McHeader * header = (McHeader*) din;
        if (header->magic != *magic) {
                printf("Magic check failed\n");
                exit(-1);
        }
        if (header->enc == Raw) {
                memcpy(dout, &header->error, dout_len * sizeof(double));
                return ;
        }
        dout[0] = header->first;
        double e2 = header->error * 2;
        int32_t* qd = (int32_t*) malloc(sizeof(*qd) * dout_len);
        switch (header->enc)
        {
        case RLE:
                RLE_Decode((int32_t*)header->data, din_len - HEADER_SIZE, &qd[1], NULL);
                break;
        case Huffman:
                HuffmanDecode(header->data, din_len - HEADER_SIZE, &qd[1], dout_len - 1);
                break;
        case Ranger:
                ranger_decode(header->data, din_len - HEADER_SIZE, &qd[1], dout_len - 1);
                break;
        default:
                printf("%s Error: unknown type\n", __FUNCTION__);
                break;
        }
        for (int i = 1; i < dout_len; i++) {
                dout[i] = dout[i-1] + e2 * qd[i];
        }
        free(qd);
        clock_t dur = clock() - start;
        double dur_sec = (double) dur / CLOCKS_PER_SEC;
        // FILE* file = fopen("/home/sy/tstest3/machete_log", "a");
        // fprintf(file, "time: %lf\tlen:%d\tspeed:%lf\n", dur_sec, dout_len, dout_len*8.0/1024/1024 / dur_sec);
        // fclose(file);
}
