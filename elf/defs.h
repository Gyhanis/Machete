#pragma once 

#include <stdint.h>

union DOUBLE {
        double d;
        uint64_t i;
};

// Utils
int getFAlpha(int alpha);
int* getAlphaAndBetaStar(double v, int lastBetaStar);
double roundUp(double v, int alpha);
double get10iN(int i);
int getSP(double v);