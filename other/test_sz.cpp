#include <stdint.h>
#include <float.h>
#include <math.h>
#include "../SZ/sz/include/sz.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

double * data0;
double * data2;

size_t ori_len;
size_t cmp_len;
int64_t encode_time;
int64_t decode_time;
double max, min;
double max_err, err_sq_sum;

//argv[1] data dir
//argv[2] error bound
//argv[3] slice length

int main(int argc, char* argv[]) {
        SZ_Init(NULL);
        double error = strtod(argv[2], NULL);
        int32_t slice_len = atoi(argv[3]);
        data0 = (double*) malloc(slice_len * sizeof(*data0));
        data2 = (double*) malloc(slice_len * sizeof(*data2));

        max = -DBL_MAX;
        min = DBL_MAX;

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
                        ori_len += act_len;
                        size_t tmp;
                        start = clock();
                        uint8_t* data1 = SZ_compress_args(SZ_DOUBLE, data0, &tmp, ABS, error, 0, 0, 0, 0, 0, 0, act_len);
                        encode_time += clock() - start;
                        cmp_len += tmp;
                        
                        start = clock();
                        SZ_decompress_args(SZ_DOUBLE, data1, tmp, data2, 0, 0, 0, 0, act_len);      
                        decode_time += clock() - start;
                        
                        for (int i = 0; i < act_len; i++) {
                                if (data0[i] > max) {
                                        max = data0[i];
                                }
                                if (data0[i] < min) {
                                        min = data0[i];
                                }
                                double diff = abs(data0[i] - data2[i]);
                                if (diff > max_err) {
                                        max_err = diff;
                                }
                                err_sq_sum += diff * diff;
                        }
                }
        }
        printf("Test finished                                    \n");
        printf("SZ compression ratio: %lf\n", ori_len * 8.0 / cmp_len);
        printf("SZ compression speed: %lf MiB/s\n", ori_len * 8.0 * CLOCKS_PER_SEC / 1024 / 1024 / encode_time);
        printf("SZ decompression speed: %lf MiB/s\n", ori_len * 8.0 * CLOCKS_PER_SEC / 1024/ 1024 / decode_time);
        printf("SZ maximum error: %.20lf\n", max_err);
        printf("SZ PSNR: %lf\n", 20*log10(max - min)-10*log10(err_sq_sum/ori_len));
        free(data0);
        free(data2);
        return 0;
}