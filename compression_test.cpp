#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <dirent.h>

#include "machete/machete.h"

int debug;

enum ListError {
        SKIP = -2,
        EOL,
};

extern int compressor_list[];

enum Type {Lossy, Lossless};

typedef struct {
        uint64_t ori_size;
        uint64_t cmp_size;
        uint64_t cmp_time;
        uint64_t dec_time;
} Perf;

Perf empty = {0,0,0,0};

extern "C" {
        int zfp_compress_wrapper(double* in, size_t len, uint8_t** out, double error);
        int zfp_decompress_wrapper(uint8_t* in, size_t len, double* out, double error);
        int sz_compress (double* in, size_t len, uint8_t** out, double error);
        int sz_decompress (uint8_t* in, size_t len, double* out, double error);
}

int zlib_compress(double* in, size_t len, uint8_t** out, double error);
int zlib_decompress(uint8_t* in, size_t len, double* out, double error);
int zstd_compress(double* in, size_t len, uint8_t** out, double error);
int zstd_decompress(uint8_t* in, size_t len, double* out, double error);

struct {
        char name[16];
        Type type;
        int (*compress) (double* in, size_t len, uint8_t** out, double error);
        int (*decompress) (uint8_t* in, size_t len, double* out, double error);
        Perf perf;
} compressors[] = {
        { "Zlib",       Type::Lossless, zlib_compress,  zlib_decompress,        empty },        //0
        { "ZSTD",       Type::Lossless, zstd_compress,  zstd_decompress,        empty },
        // // { "Gorilla",    Type::Lossless, gorilla_encode, gorilla_decode,         empty },
        // // { "Chimp",      Type::Lossless, chimp_encode,   chimp_decode,           empty },        //3
        // // { "LFZip",      Type::Lossy,    lfzip_compress, lfzip_decompress,       empty },
        { "SZ",         Type::Lossy,   sz_compress,    sz_decompress,          empty },
        { "ZFP",        Type::Lossy,    zfp_compress_wrapper, zfp_decompress_wrapper, empty},  
        { "Machete",   Type::Lossy,    machete_compress_huffman<HuffmanEnc>,   machete_decompress_huffman<HuffmanDec>, empty},
        { "MacheteP",  Type::Lossy,    machete_compress_huffman<HuffmanPEnc>,  machete_decompress_huffman<HuffmanPDec>, empty},
        { "MachetePC", Type::Lossy,    machete_compress_huffman<HuffmanPCEnc>, machete_decompress_huffman<HuffmanPCDec>, empty},
};

struct {
        char name[16];
        const char* path;
        double error;
} datasets[] = {
        { "System",     "./example_data/System/bin"   , 1E-1},
        { "System",     "./example_data/System/bin"   , 5E-2}, 
        { "System",     "./example_data/System/bin"   , 1E-2},
        { "System",     "./example_data/System/bin"   , 5E-3},
        { "System",     "./example_data/System/bin"   , 1E-3},
};

int compressor_list[] = {0,1,2,3,4,EOL};
int dataset_list[] = {3,EOL};
int bsize_list[] = {2000, EOL};


int check(double *d0, double *d1, size_t len, double error) {
        if (error == 0) {
                for (int i = 0; i < len; i++) {
                        if (d0[i] != d1[i]) {
                                printf("%d: %.16lf(%lx) vs %.16lf(%lx)\n", i, d0[i], ((uint64_t*)d0)[i], d1[i], ((uint64_t*)d1)[i]);
                                return -1;
                        }
                }
        } else {
                for (int i = 0; i < len; i++) {
                        if (std::abs(d0[i] - d1[i]) > error) {
                                printf("%d(%lf): %lf vs %lf\n", i, d0[i] - d1[i], d0[i], d1[i]);
                                return -1;
                        }
                }
        }
        return 0;
}

int test_file(FILE* file, int c, int chunk_size, double error) {
        double* d0 = (double*) malloc(chunk_size * sizeof(double));
        uint8_t *d1;
        double *d2 = (double*) malloc(chunk_size * sizeof(double));
        int64_t start;

        // compress
        FILE* fc = fopen("tmp.cmp", "w");
        int block = 0;
        while(!feof(file)) {
                int len0 = fread(d0, sizeof(double), chunk_size, file);
                if (len0 == 0) break;

                start = clock();
                int len1 = compressors[c].compress(d0, len0, &d1, error);
                compressors[c].perf.cmp_time += clock() - start;
                compressors[c].perf.cmp_size += len1;
                
                fwrite(&len1, sizeof(len1), 1, fc);
                fwrite(d1, 1, len1, fc);
                free(d1);
                block++;
        }
        fclose(fc);

        // decompress
        rewind(file);
        fc = fopen("tmp.cmp", "r");
        block = 0;
        while(!feof(file)) {
                int len0 = fread(d0, sizeof(double), chunk_size, file);
                if (len0 == 0) break;
                
                int len1;
                (void)!fread(&len1, sizeof(len1), 1, fc);
                d1 = (uint8_t*) malloc(len1);
                (void)!fread(d1, 1, len1, fc);
                start = clock();
                int len2 = compressors[c].decompress(d1, len1, d2, error);
                compressors[c].perf.dec_time += clock() - start;
                compressors[c].perf.ori_size += len2 * sizeof(double);

                double terror;
                switch (compressors[c].type) {
                        case Lossy: terror = error; break;
                        case Lossless: terror = 0; break;
                }

                if (len0 != len2 || check(d0, d2, len0, terror)) {
                        printf("Check failed, dumping data to tmp.data\n");
                        FILE* dump = fopen("tmp.data", "w");
                        fwrite(d0, sizeof(double), len0, dump);
                        fclose(dump);

                        free(d1); free(d2);
                        free(d0); fclose(file);
                        return -1;
                }
                free(d1);
                block++;
        }
        fclose(fc);

        free(d0);
        free(d2);
        return 0;
}

void draw_progress(int now, int total, int len) {
        int count = now * len / total;
        int i;
        fprintf(stderr, "Progress: ");
        for (i = 0; i < count; i++) {
                fputc('#', stderr);
        }
        for (; i < len; i++) {
                fputc('-', stderr);
        }
        fputc('\r', stderr);
}

int test_dataset(int ds, int chunk_size) {
        printf("**************************************\n");
        printf("      Testing on %s(%.8lf)\n", datasets[ds].name, datasets[ds].error);
        printf("**************************************\n");
        fflush(stdout);
        DIR* dir = opendir(datasets[ds].path);
        char filepath[257];
        struct dirent *ent;
        
        int file_cnt = 0;
        while ((ent = readdir(dir)) != NULL) {
                if (ent->d_type == DT_REG) {
                        file_cnt++;
                }
        }
        rewinddir(dir);
        
        int cur_file = 0;
        draw_progress(cur_file, file_cnt, 80);
        while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.') {
                        continue;
                }
                sprintf(filepath, "%s/%s", datasets[ds].path, ent->d_name);
                FILE* file = fopen(filepath, "rb");
                for (int i = 0; compressor_list[i] != EOL; i++) {
                        if (compressor_list[i] == SKIP) {
                                continue;
                        }
                        fseek(file, 0, SEEK_SET);
                        if (test_file(file, compressor_list[i], chunk_size, datasets[ds].error)) {
                                printf("Error Occurred while testing %s, skipping\n", compressors[compressor_list[i]].name);
                                compressor_list[i] = SKIP;
                        }
                }
                fclose(file);
                cur_file++;
                draw_progress(cur_file, file_cnt, 80);
        }
        closedir(dir);
        printf("\n");
        fflush(stdout);
        return 0;
}

void report(int c) {
        printf("========= %s ==========\n", compressors[c].name);
        printf("Compression ratio: %lf\n",      
                (double)compressors[c].perf.ori_size / compressors[c].perf.cmp_size);
        printf("Compression speed: %lf MB/s\n", 
                (double)compressors[c].perf.ori_size/1024/1024 / 
                ((double)compressors[c].perf.cmp_time/CLOCKS_PER_SEC));
        printf("Decompression speed: %lf MB/s\n", 
                (double)compressors[c].perf.ori_size/1024/1024 / 
                ((double)compressors[c].perf.dec_time/CLOCKS_PER_SEC));
        printf("\n");
        fflush(stdout);
}

int main() {
        // lfzip_init();

        double data[1000];
        uint8_t* cmp;
        double *decmp;
        for (int i = 0; i < 1000; i++) {
                data[i] = (double) rand() / RAND_MAX;
        }        

        for (int i = 0; bsize_list[i] != EOL; i++) {
                printf("Current slice length: %d\n", bsize_list[i]);
                fflush(stdout);
                for (int j = 0; dataset_list[j] != EOL; j++) {
                        test_dataset(dataset_list[j], bsize_list[i]);
                        for (int k = 0; compressor_list[k] != EOL; k++) {
                                if (compressor_list[k] != SKIP) {
                                        report(compressor_list[k]);
                                }
                                Perf &p = compressors[compressor_list[k]].perf;
                                __builtin_memset(&compressors[compressor_list[k]].perf, 0, sizeof(Perf));
                        }
                }
        }
        printf("Test finished\n");
        return 0;
}
