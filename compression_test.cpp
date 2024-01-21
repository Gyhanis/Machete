#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <dirent.h>

#include "machete/machete.h"
#include "lfzip/lfzip.h"
#include "SZ3/sz3c.h"
#include "gorilla/gorilla.h"
#include "chimp/chimp.h"
#include "elf/elf.h"

ssize_t zlib_compress   (double* in, ssize_t len, uint8_t** out, double error);
ssize_t zlib_decompress (uint8_t* in, ssize_t len, double* out, double error);
ssize_t zstd_compress   (double* in, ssize_t len, uint8_t** out, double error);
ssize_t zstd_decompress (uint8_t* in, ssize_t len, double* out, double error);

enum ListError {
        SKIP = -2,
        EOL,
};

enum Type {Lossy, Lossless};

typedef struct {
        ssize_t ori_size;
        ssize_t cmp_size;
        ssize_t cmp_time;
        ssize_t dec_time;
} Perf;

Perf empty = {0,0,0,0};

static inline ssize_t machete_decompress_lorenzo1_hybrid(uint8_t* input , ssize_t size, double* output, double error) {
        return machete_decompress<lorenzo1,hybrid>(input, size, output);
}

static inline ssize_t SZ_compress_wrapper(double* input, ssize_t len, uint8_t** output, double error) {
        return SZ_compress(input, len, output, error);
}

static inline ssize_t SZ_decompress_wrapper(uint8_t* input , ssize_t size, double* output, double error) {
        return SZ_decompress(input, size, output, error);
}

/*********************************************************************
 *                      Evaluation Settings 
*********************************************************************/
// Available compressors

struct {
        char name[16];
        Type type;
        ssize_t (*compress) (double* input, ssize_t len, uint8_t** output, double error);
        ssize_t (*decompress) (uint8_t* input, ssize_t size, double* output, double error);
        Perf perf;
} compressors[] = {
        { "Machete",    Type::Lossy,    machete_compress<lorenzo1, hybrid>,     machete_decompress_lorenzo1_hybrid,     empty},
        { "LFZip",      Type::Lossy,    lfzip_compress,                         lfzip_decompress,                       empty},
        { "SZ3",        Type::Lossy,    SZ_compress_wrapper,                    SZ_decompress_wrapper,                  empty},
        { "Gorilla",    Type::Lossless, gorilla_encode,                         gorilla_decode,                         empty},
        { "Chimp",      Type::Lossless, chimp_encode,                           chimp_decode,                           empty},
        { "Elf",        Type::Lossless, elf_encode,                             elf_decode,                             empty},
        { "Zlib",       Type::Lossless, zlib_compress,                          zlib_decompress,                        empty},
        { "ZSTD",       Type::Lossless, zstd_compress,                          zstd_decompress,                        empty},
};

// Available datasets
struct {
        char name[16];
        const char* path;
        double error;
} datasets[] = {
        // { "GeoLife",    "./example_data/Geolife"   , 1E-4}, 
        // { "GeoLife",    "./example_data/Geolife"   , 5E-5}, 
        // { "GeoLife",    "./example_data/Geolife"   , 1E-5}, 
        // { "GeoLife",    "./example_data/Geolife"   , 5E-6}, 
        // { "GeoLife",    "./example_data/Geolife"   , 1E-6}, 
        // { "GeoLife",    "./example_data/Geolife"   , 5E-7}, 
        { "System",     "./example_data/System"   , 1E-1},
        { "System",     "./example_data/System"   , 5E-2}, 
        { "System",     "./example_data/System"   , 1E-2},
        { "System",     "./example_data/System"   , 5E-3},
        { "System",     "./example_data/System"   , 1E-3},
        // { "REDD",       "./example_data/redd"      , 5E-2}, 
        // { "REDD",       "./example_data/redd"      , 1E-2},
        // { "REDD",       "./example_data/redd"      , 5E-3}, 
        // { "REDD",       "./example_data/redd"      , 1E-3}, 
        // { "Stock",      "./example_data/stock"     , 1E-2}, 
        // { "Stock",      "./example_data/stock"     , 5E-3},
        // { "Stock",      "./example_data/stock"     , 1E-3},
        // { "Stock",      "./example_data/stock"     , 5E-4},
};

// List of compressors to be evaluated (use indices in the "compressors" array above)
int compressor_list[] = {0, 1, 2, 3, 4, 5, 6, 7, EOL};
// List of datasets to be evaluated (use indices in the "datasets" array above)
int dataset_list[] = {0, 2, 4, EOL}; 
// List of slice lengths to be evaluated
int bsize_list[] = {500, 1000, 2000, EOL};

///////////////////////// Setting End ////////////////////////////


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
        double *d2 = (double*) malloc(2 * chunk_size * sizeof(double));
        int64_t start;

        // compress
        FILE* fc = fopen("tmp.cmp", "w");
        int block = 0;
        while(!feof(file)) {
                ssize_t len0 = fread(d0, sizeof(double), chunk_size, file);
                if (len0 == 0) break;

                start = clock();
                ssize_t len1 = compressors[c].compress(d0, len0, &d1, error);
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
                size_t len0 = fread(d0, sizeof(double), chunk_size, file);
                if (len0 == 0) break;
                
                size_t len1;
                (void)!fread(&len1, sizeof(len1), 1, fc);
                d1 = (uint8_t*) malloc(len1);
                (void)!fread(d1, 1, len1, fc);
                start = clock();
                ssize_t len2 = compressors[c].decompress(d1, len1, d2, error);
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
                        free(d0); fclose(fc);
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
        lfzip_init();

// #define DEBUG_LAST_FAILED
#ifdef DEBUG_LAST_FAILED
        FILE* fp = fopen("tmp.data", "r");
        if (fp == NULL) {
                printf("Failed to open tmp.data\n");
        }
        test_file(fp, 0, 1000, 1E-3);
        printf("Test finished\n");
#else 
        for (int i = 0; bsize_list[i] != EOL; i++) {
                printf("Current slice length: %d\n", bsize_list[i]);
                fflush(stdout);
                for (int j = 0; dataset_list[j] != EOL; j++) {
                        test_dataset(dataset_list[j], bsize_list[i]);
                        for (int k = 0; compressor_list[k] != EOL; k++) {
                                if (compressor_list[k] != SKIP) {
                                        report(compressor_list[k]);
                                }
                                __builtin_memset(&compressors[compressor_list[k]].perf, 0, sizeof(Perf));
                        }
                }
        }
        printf("Test finished\n");
#endif
        return 0;
}
