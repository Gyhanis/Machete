#include "defs.h"
#include <stdlib.h>
#include <vector>

struct LorenzoConfig {
        double error;
        double first;
        double outiers[0];
};

static inline int32_t diff(const double &data, const double &predicted, const DOUBLE &e, const double &e2, const double &max_diff) {
        DOUBLE d = {.d = data - predicted};
        DOUBLE d_abs = {.i = d.i & ~DOUBLE_SIGN_BIT};
        if (UNLIKELY(d_abs.d > max_diff)) {
                return INT32_MIN;
        }
        DOUBLE comp = {.i = (d.i & DOUBLE_SIGN_BIT) | e.i};
        d.d += comp.d;
        return static_cast<int32_t>(d.d / e2);
}

ssize_t lorenzo1_diff(double* input, ssize_t len, int32_t* output, double error, uint8_t** predictor_out, ssize_t* psize) {
        std::vector<double> outier;
        DOUBLE e = {.d= error * 0.999};
        double e2 = e.d * 2;
        double max_diff = e2 * INT32_MAX;
        double predicted = input[0];
        for (int i = 1; i < len; i++) {
                *output = diff(input[i], predicted, e, e2, max_diff);
                if (UNLIKELY(*output == INT32_MIN)) {
                        outier.push_back(input[i]);
                        predicted = input[i];
                } else {
                        predicted += *output * e2;
                }
                output++;
        }
        *psize = sizeof(LorenzoConfig) + outier.size() * sizeof(double);
        *predictor_out = reinterpret_cast<uint8_t*>(malloc(*psize));
        LorenzoConfig* config = reinterpret_cast<LorenzoConfig*>(*predictor_out);
        config->error = error;
        config->first = input[0];
        __builtin_memcpy(config->outiers, &outier[0], outier.size() * sizeof(double));
        return len - 1;
}

ssize_t lorenzo1_correct(int32_t* input, ssize_t len, double* output, uint8_t* predictor_out, ssize_t psize) {
        LorenzoConfig* config = reinterpret_cast<LorenzoConfig*>(predictor_out);
        double e2 = config->error * 0.999 * 2;
        
        output[0] = config->first; 
        if (psize == sizeof(LorenzoConfig)) {
                for (int i = 0; i < len; i++) {
                        output[i+1] = output[i] + e2 * input[i];
                }
        } else {
                double* outier = config->outiers;
                for (int i = 0; i < len; i++) {
                        if (UNLIKELY(input[i] == INT32_MIN)) {
                                output[i+1] = *outier++;
                        } else {
                                output[i+1] = output[i] + e2 * input[i];
                        }
                }
        }
        return len + 1;
}




