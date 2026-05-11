/* model_globals.c - Definitions of global structures */
#include "nlutils.h"
#include "fue_globals.h"

struct Tseries Ts;
struct Tusmodel Tm;
struct intervention It[50];
struct oper Arr[20];
struct oper Ara[20];
struct oper Mar[20];
struct oper Maa[20];
struct freq_fix Ar2f[5];
struct freq_fix Ma2f[5];
int NdetVar = 0;
int NopArr = 0;
int NopAra = 0;
int NopMar = 0;
int NopMaa = 0;
int NumAr2f = 0;
int NumMa2f = 0;
int ma_order_inc = 0;
int ar_order_inc = 0;
int new_det = 0;
int type_op = 0;
int new_op = 0;
double **DataMat = NULL;
double Data[2000];
