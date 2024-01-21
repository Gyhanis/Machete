#include <assert.h>
#include <cmath>

#include "defs.h"

#define LENGTH_OF(x) (sizeof(x)/sizeof((x)[0]))

static const int f[] = 
{0, 4, 7, 10, 14, 17, 20, 24, 27, 30, 34, 37, 40, 
44, 47, 50, 54, 57, 60, 64, 67};

static const double map10iP[] = 
{1.0, 1.0E1, 1.0E2, 1.0E3, 1.0E4, 1.0E5, 1.0E6, 1.0E7, 
1.0E8, 1.0E9, 1.0E10, 1.0E11, 1.0E12, 1.0E13, 1.0E14,
1.0E15, 1.0E16, 1.0E17, 1.0E18, 1.0E19, 1.0E20};

static const double map10iN[] = 
{1.0, 1.0E-1, 1.0E-2, 1.0E-3, 1.0E-4, 1.0E-5, 1.0E-6, 1.0E-7,
1.0E-8, 1.0E-9, 1.0E-10, 1.0E-11, 1.0E-12, 1.0E-13, 1.0E-14,
1.0E-15, 1.0E-16, 1.0E-17, 1.0E-18, 1.0E-19, 1.0E-20};

static const long mapSPGreater1[] = 
{1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 
100000000, 1000000000};

static const double mapSPLess1[] = 
{1, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001,
0.000000001, 0.0000000001};

static const double LOG_2_10 = 3.321928095;

static int getSignificantCount(double v, int sp, int lastBetaStar);
static double get10iP(int i);
static int* getSPAnd10iNFlag(double v);

int getFAlpha(int alpha) {
        assert(alpha >= 0);
        if (alpha >= LENGTH_OF(f)) {
                return (int) ceilf64(alpha * LOG_2_10);
        } else {
                return f[alpha];
        }
}

int* getAlphaAndBetaStar(double v, int lastBetaStar) {
        v = v < 0 ? -v : v;
        int *alphaAndBetaStar = new int[2];
        int *spAnd10iNFlag = getSPAnd10iNFlag(v);
        int beta = getSignificantCount(v, spAnd10iNFlag[0], lastBetaStar);
        alphaAndBetaStar[0] = beta - spAnd10iNFlag[0] - 1;
        alphaAndBetaStar[1] = spAnd10iNFlag[1] == 1 ? 0 : beta;
        delete [] spAnd10iNFlag;
        return alphaAndBetaStar;
}

double roundUp(double v, int alpha) {
        double scale = get10iP(alpha);
        if (v < 0) {
                return floorf64(v * scale) / scale;
        } else {
                return ceil(v * scale) / scale;
        }
}

static int getSignificantCount(double v, int sp, int lastBetaStar) {
        int i;
        if(lastBetaStar != __INT32_MAX__ && lastBetaStar != 0) {
                i = lastBetaStar - sp - 1;
                i = i > 1 ? i : 1;
        } else if (lastBetaStar == __INT32_MAX__) {
                i = 17 - sp - 1;
        } else if (sp >= 0) {
                i = 1;
        } else {
                i = -sp;
        }

        double temp = v * get10iP(i);
        long tempLong = (long) temp;
        while (tempLong != temp) {
                i++;
                temp = v * get10iP(i);
                tempLong = (long) temp;
        }

        if (temp / get10iP(i) != v) {
                return 17;
        } else {
                while (i > 0 && tempLong % 10 == 0) {
                        i--;
                        tempLong = tempLong / 10;
                }
                return sp + i + 1;
        }
}

static double get10iP(int i) {
        assert(i >= 0);
        if (i >= LENGTH_OF(map10iP)) {
                return powf64(10, i);
        } else {
                return map10iP[i];
        }
}

double get10iN(int i) {
        assert(i >= 0);
        if (i >= LENGTH_OF(map10iN)) {
                return powf64(10, -i);
        } else {
                return map10iN[i];
        }
}

int getSP(double v) {
        if (v >= 1) {
                int i = 0;
                while (i < LENGTH_OF(mapSPGreater1) - 1) {
                        if (v < mapSPGreater1[i+1]) {
                                return i;
                        }
                        i++;
                }
        } else {
                int i = 1;
                while (i < LENGTH_OF(mapSPLess1)) {
                        if (v >= mapSPLess1[i]) {
                                return -i;
                        }
                        i++;
                }
        }
        return (int) floor(log10(v));
}

static int* getSPAnd10iNFlag(double v) {
        int *spAnd10iNFlag = new int[2];
        if (v >= 1) {
                int i = 0;
                while (i < LENGTH_OF(mapSPGreater1)-1) {
                        if (v < mapSPGreater1[i+1]) {
                                spAnd10iNFlag[0] = i;
                                return spAnd10iNFlag;
                        }
                        i++;
                }
        } else {
                int i = 1;
                while (i < LENGTH_OF(mapSPLess1)) {
                        if (v >= mapSPLess1[i]) {
                                spAnd10iNFlag[0] = -i;
                                spAnd10iNFlag[1] = v == mapSPLess1[i] ? 1 : 0;
                                return spAnd10iNFlag;
                        }
                        i++;
                }
        }
        double log10v = log10(v);
        spAnd10iNFlag[0] = (int) floor(log10v);
        spAnd10iNFlag[1] = log10v == (long) log10v ? 1 : 0;
        return spAnd10iNFlag;
}