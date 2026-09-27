#include "main.h"
#include <gsl/gsl_cdf.h>
real chisq(real,int); real chisq_simple(real,int); real tdist(real,int);
real macheps; FILE *outputv; int quiet_mode=1;
int main(){ int dfs[]={1,2,3,5,10,29,30,31,50,100,200,400,800,1000,2000};
 double worst=0; for(int k=0;k<15;k++){int df=dfs[k]; for(double q=0.001;q<1;q+=0.001){double x=gsl_cdf_chisq_Pinv(q,df); double e=fabs(chisq(x,df)-q); if(e>worst){worst=e;} if(e>1e-3) printf("df=%d x=%g gsl=%g chisq=%g\n",df,x,q,chisq(x,df));}}
 printf("worst abs err %g\n",worst);
 printf("x=1001 df=1000: chisq=%g gsl=%g\n",chisq(1001,1000),gsl_cdf_chisq_P(1001,1000));
 for(int df=1;df<=7;df++) printf("chisq_simple x=5 df=%d: %g gsl %g\n",df,chisq_simple(5,df),gsl_cdf_chisq_P(5,df));
 for(int df=1;df<=6;df++) printf("tdist t=1.5 df=%d: %g gsl %g\n",df,tdist(1.5,df),gsl_cdf_tdist_P(1.5,df));
}
