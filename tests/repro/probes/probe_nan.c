#include "main.h"
real macheps; FILE *outputv; int quiet_mode=1; long nf=0;
void raxopt( real (*func)( real [] ), real *fk, int n, real *xk, real **b, int maxits, int nrits, real gradtol, real steptol );
real f(real *x){ nf++; if (x[1] > 2.0) return NAN; return 0.1*(x[1]-3)*(x[1]-3)/0.9 + 0.0; }
int main(){ macheps=cmacheps(); outputv=stdout; real *x=vector(1,1); x[1]=0.0; real **b=matrix(1,1,1,1); real fk;
 /* objective normalised to 1 at x0 as in drvmlest */
 raxopt(f,&fk,1,x,b,100,1,1e-6,1e-8); printf("x=%g fk=%g nf=%ld\n",x[1],fk,nf); }
