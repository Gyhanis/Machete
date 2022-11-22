#include <stdint.h>
#include <float.h>
#include <math.h>
#include "include/Machete.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

double * data0;
uint32_t * data1;
double * data2;

int64_t machete_ori_len;
int64_t machete_cmp_len;
int64_t machete_encode_time;
int64_t machete_decode_time;
int64_t machete_encoder[3];
double machete_max, machete_min;
double machete_max_err, machete_err_sq_sum;

//argv[1] data dir
//argv[2] error bound
//argv[3] slice length
int main(int argc, char* argv[]) {
        double error = strtod(argv[2], NULL);
        int32_t slice_len = atoi(argv[3]);
        data0 = (double*) malloc(slice_len * sizeof(*data0));
        data1 = (uint32_t*) malloc(slice_len * 10);
        data2 = (double*) malloc(slice_len * sizeof(*data2));

        machete_max = -DBL_MAX;
        machete_min = DBL_MAX;

        DIR * dir = opendir(argv[1]);
        struct dirent* entry;
        char filename[128];

        while (entry = readdir(dir)) {
                if (entry->d_name[0] == '.') {
                        continue;
                }
                sprintf(filename, "%s/%s", argv[1], entry->d_name);
                fprintf(stderr,"Current file: %s\r", filename);
                
                clock_t start, dur;
                int64_t act_len;

                FILE* f = fopen(filename, "r");
                while (act_len = fread(data0, sizeof(*data0), slice_len, f)) {
                        machete_ori_len += act_len;

                        start = clock();
                        Machete *m = MachetePrepare(data0, act_len, error);
                        machete_encoder[m->encoder]++;
                        int64_t cmp_len = MacheteGetSize(m);
                        cmp_len = MacheteEncode(m, data1);
                        machete_encode_time += clock() - start;
                        machete_cmp_len += cmp_len;
                        
                        start = clock();
                        MacheteDecode(data1, cmp_len, data2, act_len);
                        machete_decode_time += clock() - start;
                        
                        for (int i = 0; i < act_len; i++) {
                                if (data0[i] > machete_max) {
                                        machete_max = data0[i];
                                }
                                if (data0[i] < machete_min) {
                                        machete_min = data0[i];
                                }
                                double diff = abs(data0[i] - data2[i]);
                                if (diff > machete_max_err) {
                                        machete_max_err = diff;
                                }
                                machete_err_sq_sum += diff * diff;
                        }
                }
        }
        printf("Test finished                                    \n");
        printf("Machete compression ratio: %lf\n", machete_ori_len * 2.0 / machete_cmp_len);
        printf("Machete compression speed: %lf MiB/s\n", machete_ori_len * 8.0 * CLOCKS_PER_SEC / 1024 / 1024 / machete_encode_time);
        printf("Machete decompression speed: %lf MiB/s\n", machete_ori_len * 8.0 * CLOCKS_PER_SEC / 1024/ 1024 / machete_decode_time);
        printf("Machete maximum error: %.20lf\n", machete_max_err);
        printf("Machete PSNR: %lf\n", 20*log10(machete_max - machete_min)-10*log10(machete_err_sq_sum/machete_ori_len));
        printf("Machete encoder useage (RLE/Huffman/VLQ): %d/%d/%d\n", machete_encoder[0], machete_encoder[1], machete_encoder[2]);
        free(data0);
        free(data1);
        free(data2);
        return 0;
}