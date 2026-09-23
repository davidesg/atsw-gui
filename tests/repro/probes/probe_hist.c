#include "main.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
real macheps; FILE *outputv; int quiet_mode=1;
int data_freq=1,data_start_year=1,data_start_sub=1,trans_d=0,trans_D=0; char **series_names=NULL;
int main(int argc,char**argv){ int n=atoi(argv[1]); double out=atof(argv[2]); macheps=cmacheps(); outputv=stdout;
 gsl_rng*r=gsl_rng_alloc(gsl_rng_mt19937); gsl_rng_set(r,1);
 real *x=vector(1,n); for(int i=1;i<=n;i++) x[i]=gsl_ran_gaussian(r,1.0); x[n/2]=out;
 struct Tseries s; s.data=x; s.nobs=n; s.freq=1; s.begtime=1; s.begyear=1; s.name="r";
 File_StatSer(&s); File_HistSer(&s); return 0;}
