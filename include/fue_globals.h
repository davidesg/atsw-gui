#ifndef FUE_GLOBALS_H
#define FUE_GLOBALS_H

#include "nlutils.h"

/* Intervention structure (deterministic components) */
struct intervention {
    int type;
    int period;
    int year;
    int ma_order;
    double *ma_parameter;
    int *ma_fixed;
    int ar_order;
    double *ar_parameter;
    int *ar_fixed;
    int freq;
};

/* Operator structure (AR/MA) */
struct oper {
    int type;
    int order;
    double *op_parameter;
    int *op_fixed;
};

/* Fixed‑frequency operator structure */
struct freq_fix {
    int type;
    int freq;
    double op_parameter;
    int op_fixed;
};

/* Tree view column indices */
enum {
    COL_NUM = 0,
    COL_NAME,
    COL_SEASON,
    COL_YEAR,
    COL_MAOR,
    COL_AROR,
    NUM_COLS
};

/* Global model data (from core) */
extern struct Tseries Ts;
extern struct Tusmodel Tm;
extern struct intervention It[50];
extern struct oper Arr[20];
extern struct oper Ara[20];
extern struct oper Mar[20];
extern struct oper Maa[20];
extern struct freq_fix Ar2f[5];
extern struct freq_fix Ma2f[5];
extern int NdetVar, NopArr, NopAra, NopMar, NopMaa, NumAr2f, NumMa2f;
extern int ma_order_inc, ar_order_inc, new_det, type_op, new_op;
extern double **DataMat;
extern double Data[2000];

#endif /* FUE_GLOBALS_H */
