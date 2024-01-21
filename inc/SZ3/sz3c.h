//
// Created by Kai Zhao on 10/27/22.
//

#ifndef SZ3_SZ3C_H
#define SZ3_SZ3C_H

#include <unistd.h>
#include <stdint.h>
/** Begin errorbound mode in SZ2 (defines.h) **/
#define ABS 0
#define REL 1
#define VR_REL 1  //alternative name to REL
#define ABS_AND_REL 2
#define ABS_OR_REL 3
#define PSNR 4
#define NORM 5

#define PW_REL 10
#define ABS_AND_PW_REL 11
#define ABS_OR_PW_REL 12
#define REL_AND_PW_REL 13
#define REL_OR_PW_REL 14
/** End errorbound mode in SZ2 (defines.h) **/

/** Begin dataType in SZ2 (defines.h) **/
#define SZ_FLOAT 0
#define SZ_DOUBLE 1
#define SZ_UINT8 2
#define SZ_INT8 3
#define SZ_UINT16 4
#define SZ_INT16 5
#define SZ_UINT32 6
#define SZ_INT32 7
#define SZ_UINT64 8
#define SZ_INT64 9
/** End dataType in SZ2 (defines.h) **/

#ifdef __cplusplus
extern "C" {
#endif
int SZ_compress(double *in, size_t len, uint8_t** out, double absErrBound);
int SZ_compress_nocopy(double *in, size_t len, uint8_t** out, double absErrBound);
void SZ_free(uint8_t* data);

int SZ_decompress(uint8_t* in, size_t len, double* out, double error);

#ifdef __cplusplus
}
#endif

#endif //SZ3_SZ3C_H
