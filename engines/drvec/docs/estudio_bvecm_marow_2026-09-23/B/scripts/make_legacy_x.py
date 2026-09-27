import re
src=open('src/main.c',encoding='latin-1').read()
# 1. masks + helper after global_structural
hdr='''int global_structural = 0;   // 0 = no mostrar forma estructural, 1 = mostrar */'''
add=hdr+'''
/* ==== INSTRUMENT (study B): zero patterns on phi_k and theta_1 of Ybar_legacy = (W ; dY2) ==== */
int X_inst = 0;              /* 1 if DRV_AR or DRV_MA set */
int X_armask(int k, int i, int j) {   /* 1 = free */
   const char *a = getenv("DRV_AR"); if (!a) return 1;
   if (!strcmp(a,"vec")   && k == global_p && j == 2) return 0;   /* dY2 columns of last phi zero */
   if (!strcmp(a,"warma") && j == 2) return 0;                    /* dY2 columns of every phi zero */
   return 1;
}
int X_mamask(int i, int j) {          /* 1 = free */
   const char *a = getenv("DRV_MA"); if (!a || !strcmp(a,"leg")) return (i==2 && j==2);
   if (!strcmp(a,"none")) return 0;
   if (!strcmp(a,"free")) return 1;
   if (!strcmp(a,"tri"))  return !(i==2 && j==1);   /* drvec -matri   */
   if (!strcmp(a,"wrow")) return (i==1);            /* drvec -marow   */
   if (!strcmp(a,"ww"))   return (i==1 && j==1);    /* drvec -mawarma */
   if (!strcmp(a,"diag")) return (i==j);
   if (!strcmp(a,"dcol")) return (j==2);            /* both rows load on dY2 innovation */
   fprintf(stderr,"bad DRV_MA\\n"); exit(2);
}
int X_nar(void){int k,i,j,n=0;for(k=1;k<=global_p;k++)for(i=1;i<=2;i++)for(j=1;j<=2;j++)n+=X_armask(k,i,j);return n;}
int X_nma(void){int i,j,n=0;for(i=1;i<=2;i++)for(j=1;j<=2;j++)n+=X_mamask(i,j);return n;}
'''
assert hdr in src; src=src.replace(hdr,add,1)
# 2. npar
old='''   npar += 4 * global_p;  /* φ₁₁(k), φ₁₂(k), φ₂₁(k), φ₂₂(k) para k=1..p */'''.encode('utf-8').decode('latin-1')
assert old in src; src=src.replace(old,'   npar += X_nar();',1)
old='''   if (global_q >= 1) npar += 1;  /* θ₂₂(1) */'''.encode('utf-8').decode('latin-1')
assert old in src; src=src.replace(old,'   npar += X_nma();',1)
# 3. preest assembly
old='''    for (k = 1; k <= 4 * global_p; k++)
        x[idx++] = ar_coef[k];'''
new='''    for (k = 1; k <= global_p; k++) { int ii,jj; for (ii=1;ii<=2;ii++) for (jj=1;jj<=2;jj++)
        if (X_armask(k,ii,jj)) x[idx++] = ar_coef[4*(k-1)+2*(ii-1)+jj]; }'''
assert old in src; src=src.replace(old,new,1)
old='''    if (global_q >= 1)
        x[idx++] = theta22_est;'''
new='''    { int ii,jj; for (ii=1;ii<=2;ii++) for (jj=1;jj<=2;jj++) if (X_mamask(ii,jj)) {
        const char *sd = getenv("DRV_MASEED");
        x[idx++] = (ii==2&&jj==2) ? theta22_est : (sd ? atof(sd) : 0.0); } }'''
assert old in src; src=src.replace(old,new,1)
# 4. shootx reading
i0=src.index('   /* 2. Leer matrices AR */'); i1=src.index('   /* No hay MA(2) o superior - estructura fija */')
new='''   for (k = 1; k <= global_p; k++) { int ii,jj; for (ii=1;ii<=2;ii++) for (jj=1;jj<=2;jj++)
       phi1[k][ii][jj] = X_armask(k,ii,jj) ? x[idx++] : 0.0; }
   if (global_q >= 1) { int ii,jj; for (ii=1;ii<=2;ii++) for (jj=1;jj<=2;jj++)
       theta1[1][ii][jj] = X_mamask(ii,jj) ? x[idx++] : 0.0; }
'''
src=src[:i0]+new+src[i1:]
# 5. skip BEC if instrumented
old='   analisis_BEC(&varma1, x, dev, cov, npar, nobs, outputv);'
src=src.replace(old,'   if (!getenv("DRV_AR") && !getenv("DRV_MA")) '+old.strip(),1)
# 6. q=0 when MA none
old='   armax->q = global_q;'
src=src.replace(old,'   armax->q = (getenv("DRV_MA") && !strcmp(getenv("DRV_MA"),"none")) ? 0 : global_q;',1)
open('src/main.c','w',encoding='latin-1').write(src)
