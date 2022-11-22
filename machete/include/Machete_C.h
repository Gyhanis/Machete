#ifndef __MACHETE_C_H
#define __MACHETE_C_H

#include <stdint.h>

typedef int32_t Machete;

Machete* MachetePrepare(double * din, int32_t din_len, double error);

static inline int32_t MacheteGetSize(Machete* m) {
        return *m;
}

int32_t MacheteEncode(Machete* m, uint32_t* dout);

void MacheteDecode(uint32_t* din, uint32_t din_len, double* dout, uint32_t dout_len);

#endif 
