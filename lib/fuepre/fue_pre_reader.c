/*****************************************************************************/
/*  fue_pre_reader.c -- el lector del .pre de fue, y lo que se hace con el
 *  modelo leido: contar, empaquetar y desempaquetar sus coeficientes libres,
 *  y comprobar que varias series se pueden cruzar.
 *
 *  Nacio en drtran (engines/drtran/src). Vive en lib/fuepre desde que
 *  drvarma 5.0 lee tambien los .pre: lo enlazan drtran, su GUI y drvarma,
 *  y lo que este lector acepte es lo que aceptan los tres.
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/*****************************************************************************/
/*  Lector de archivos .pre (DRVUS/FUE)                                      */
/*  Parsea el modelo completo: deterministas (Omega/Delta), factores ARMA,   */
/*  media (valor + flag de estimación), Box-Cox, diferencias y la serie.     */
/*                                                                           */
/*  The host provides main.h: `real`, struct Tseries (with numbering and    */
/*  refactor), struct Tusmodel (by including tusmodel.h), the nlatools       */
/*  allocators and Easter; chekma only if it links fuepre_motor.c.           */
/*  DateToObs and ObsToDate come from lib/dates.                             */
/*****************************************************************************/

#include "main.h"
#include "fue_pre_reader.h"
#include "dates.h"
#include <string.h>
#include <math.h>

/* La linea del lector es SUYA, no del anfitrion. Usaba FUEPRE_LINE, que vale 200
   en drtran y 80 en drvarma: con 80, una linea larga de un .pre se partia en
   dos lecturas y todo lo de detras se desplazaba un renglon.               */
#define FUEPRE_LINE 512

/* ─── CalcNonsOp (from fue.c) ─────────────────────────────────────────── */
static void CalcNonsOp( int sp, int d, int ds, int *ifds, int ord, real *op )
{
    real *pol1, *pol2, *pol3, *pol4;
    int  i, j, k, pp, pp1, pp2;

    pp1 = d + ds * sp;
    pol1 = vector( 0, pp1 );
    pol2 = vector( 0, pp1 );
    for ( i = 1; i <= pp1; i++ ) pol1[i] = 0.0;
    pol1[0] = -1.0;
    pp      = 0;

    if ( ds > 0 )
        for ( k = 1; k <= ds; k++ )
        {
            for ( i = 0; i <= pp1; i++ ) pol2[i] = 0.0;
            for ( j = 0; j <= pp + sp; j++ )
            {
                if ( (j >= 0) && (j < sp) )
                    pol2[j] = pol1[j];
                else if ( (j >= sp) && (j <= pp) )
                    pol2[j] = pol1[j] - pol1[j-sp];
                else if ( (j > pp) && (j <= pp + sp) )
                    pol2[j] = -pol1[j-sp];
            }
            pp += sp;
            for ( i = 0; i <= pp; i++ ) pol1[i] = pol2[i];
        }

    if ( d > 0 )
        for ( k = 1; k <= d; k++ )
        {
            for ( i = 0; i <= pp1; i++ ) pol2[i] = 0.0;
            for ( j = 0; j <= pp + 1; j++ )
            {
                if ( (j >= 0) && (j < 1) )
                    pol2[j] = pol1[j];
                else if ( (j >= 1) && (j <= pp) )
                    pol2[j] = pol1[j] - pol1[j-1];
                else if ( (j > pp) && (j <= pp + 1) )
                    pol2[j] = -pol1[j-1];
            }
            pp += 1;
            for ( i = 0; i <= pp; i++ ) pol1[i] = pol2[i];
        }

    free_vector( pol2, 0, pp1 );

    pp2 = ord - pp1;
    pol2 = vector( 0, pp2 );
    pol3 = vector( 0, pp2 );
    pol4 = vector( 0, 2 );
    for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
    pol3[0] = -1.0;
    pp      = 0;

    if ( ((sp == 12) && (ifds[0] == 1)) || ((sp == 4) && (ifds[0] == 1)) )
    {
        pol4[0] = -1.0; pol4[1] = 1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 1; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 1; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }
    if ( (sp == 12) && (ifds[1] == 1) )
    {
        pol4[0] = -1.0; pol4[1] = sqrt(3.0); pol4[2] = -1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 2; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 2; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }
    if ( (sp == 12) && (ifds[2] == 1) )
    {
        pol4[0] = -1.0; pol4[1] = 1.0; pol4[2] = -1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 2; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 2; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }
    if ( ((sp == 12) && (ifds[3] == 1)) || ((sp == 4) && (ifds[1] == 1)) )
    {
        pol4[0] = -1.0; pol4[1] = 0.0; pol4[2] = -1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 2; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 2; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }
    if ( (sp == 12) && (ifds[4] == 1) )
    {
        pol4[0] = -1.0; pol4[1] = -1.0; pol4[2] = -1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 2; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 2; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }
    if ( (sp == 12) && (ifds[5] == 1) )
    {
        pol4[0] = -1.0; pol4[1] = -sqrt(3.0); pol4[2] = -1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 2; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 2; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }
    if ( ((sp == 12) && (ifds[6] == 1)) || ((sp == 4) && (ifds[2] == 1)) )
    {
        /* Nyquist (f=s/2): factor (1 + B), root at B=-1  =>  pol4 = -(1+B).
           Al portar de fue.c se copio aqui el (1 - B) de la frecuencia CERO
           (pol4[1] = +1.0), que sobre-diferencia en f=0 y NO diferencia en
           Nyquist: rompia toda serie con raiz estacional en pi (EA/EP/EC de m6,
           sigma ~8x, el MA se iba a la raiz unitaria). El legacy trae -1.0.     */
        pol4[0] = -1.0; pol4[1] = -1.0;
        for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0; pol2[0] = -1.0; pol3[0] = -1.0;
        for ( i = 0; i <= pp; i++ ) for ( j = 0; j <= 1; j++ ) pol2[j+i] -= pol4[j] * pol3[i];
        pp += 1; for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
    }

    /* El operador no estacionario es el PRODUCTO del polinomio de diferencias
       regulares/estacionales (pol1, orden pp1) por el de factores de frecuencia
       fija (pol3, orden pp). Ambos se almacenan con signo cambiado
       (pol1 = -A, pol3 = -B), luego op = -(A*B) = -(pol1 * pol3).          */
    for ( i = 0; i <= ord; i++ ) op[i] = 0.0;
    for ( i = 0; i <= pp1; i++ )
        for ( j = 0; j <= pp; j++ )
            if ( i + j <= ord ) op[i+j] -= pol1[i] * pol3[j];
    free_vector( pol4, 0, 2 );
    free_vector( pol3, 0, pp2 );
    free_vector( pol2, 0, pp2 );
    free_vector( pol1, 0, pp1 );
}

/* DateToObs and ObsToDate come from lib/dates; Easter from the host's
   nlatools.c. The same routines as fue.                                    */


/* ─── Genera una variable determinista a partir de su línea del .pre ─────
   Replica exactamente los tipos que fue admite (fue.c):
     impulse <per> <año>   compimp <per> <año>   step <per> <año>
     ramp <per> <año>      easter (freq=12)      trend
     cos <r>               sin <r>               alter
   En series anuales (freq=1) las intervenciones llevan solo el año.
   Devuelve 1 si reconoció el tipo, 0 si es no estándar.                  */
static int gen_detvar( const char *line, struct Tseries *Ts, real *v )
{
    const double PI = 3.14159265358979323846;
    int  j, i1, i2, obs;
    double r1;

    for (j = 1; j <= Ts->nobs; j++) v[j] = 0.0;

    /* --- Intervenciones fechadas --- */
    {
        char kind[32];
        int  nread;

        nread = sscanf(line, "%31s %d %d", kind, &i1, &i2);

        if (nread >= 2 &&
            (strcmp(kind, "impulse") == 0 || strcmp(kind, "compimp") == 0 ||
             strcmp(kind, "step")    == 0 || strcmp(kind, "ramp")    == 0)) {

            if (Ts->freq == 1) { i2 = i1; i1 = 1; }   /* anual: solo el año */
            else if (nread < 3) return 0;             /* falta el año       */

            DateToObs(Ts->begyear, Ts->begtime, i2, i1, Ts->freq, &obs);

            if (strcmp(kind, "impulse") == 0) {
                if (obs >= 1 && obs <= Ts->nobs) v[obs] = 1.0;
            } else if (strcmp(kind, "compimp") == 0) {
                if (obs >= 1 && obs <= Ts->nobs)     v[obs]   =  1.0;
                if (obs >= 0 && obs + 1 <= Ts->nobs) v[obs+1] = -1.0;
            } else if (strcmp(kind, "step") == 0) {
                if (obs >= 1 && obs <= Ts->nobs)
                    for (j = obs; j <= Ts->nobs; j++) v[j] = 1.0;
            } else {   /* ramp */
                if (obs >= 1 && obs <= Ts->nobs)
                    for (j = obs; j <= Ts->nobs; j++) v[j] = j - obs + 1;
            }
            return 1;
        }
    }

    /* The keyword is the WHOLE first word, compared with strcmp as fue does
       (fue.c, "easter"/"trend"/"alter"). A prefix comparison read a variable
       called "timeshift" as a linear trend, silently (REGLAS-NO-ESCRITAS 50);
       and "time" is not a word fue accepts at all: only "trend".          */
    char word[32] = "";
    sscanf(line, "%31s", word);

    /* --- Semana Santa (solo mensual) --- */
    if (strcmp(word, "easter") == 0 && Ts->freq == 12) {
        int year, month, eday, emonth;
        for (j = 1; j <= Ts->nobs; j++) {
            ObsToDate(Ts->begyear, Ts->begtime, j, Ts->freq, &year, &month);
            Easter(&eday, &emonth, year);
            if (emonth == 4 && month == emonth && eday >= 4)
                v[j] = 1.0;
            else if (emonth == 4 && month == emonth && eday < 4) {
                v[j] = 0.5;
                if (j > 1) v[j-1] = 0.5;
            } else if (emonth == 3 && month == emonth)
                v[j] = 1.0;
        }
        return 1;
    }

    /* --- Armónicos y componentes simples --- */
    if (sscanf(line, "cos %lf", &r1) == 1) {
        for (j = 1; j <= Ts->nobs; j++)
            v[j] = cos(2.0 * PI * r1 / Ts->freq * j);
        return 1;
    }
    if (sscanf(line, "sin %lf", &r1) == 1) {
        for (j = 1; j <= Ts->nobs; j++)
            v[j] = sin(2.0 * PI * r1 / Ts->freq * j);
        return 1;
    }
    if (strcmp(word, "alter") == 0) {
        for (j = 1; j <= Ts->nobs; j++) v[j] = pow(-1.0, (double)j);
        return 1;
    }
    if (strcmp(word, "trend") == 0) {
        for (j = 1; j <= Ts->nobs; j++) v[j] = j;
        return 1;
    }

    return 0;   /* no estándar: se leerá como columna extra de datos */
}

/* ─── Lee una sección de factores ARMA del .pre ─────────────────────────
   Puerto FIEL del lector de fue (fue.c [3.3.1]-[3.3.2]). fue usa fscanf, que es
   agnóstico al espaciado: los coeficientes pueden venir uno por línea (como los
   escribe fue) o varios en la misma. Gramática, idéntica para AR regular, AR
   anual, MA regular y MA anual:

       ** Number and orders of ... :        <- cabecera (fgets)
       <Num> <ord_1> <ord_2> ...            <- fscanf
       **                                   <- separador (fgets), por factor
       <coef> <flag> ...                    <- fscanf, ord_i coeficientes

   El "**" final de cada sección se fusiona con la cabecera de la siguiente.  */
static void read_arma_section(FILE *f, char *line, int *Num, int **ord,
                              real ***coef, int ***flag)
{
    int i, j;

    fgets(line, FUEPRE_LINE, f);                     /* cabecera */

    *Num = 0;
    if (fscanf(f, "%d", Num) != 1 || *Num <= 0) {
        *Num = 0;
        fscanf(f, "\n");
        return;
    }

    *ord  = ivector(1, *Num);
    *coef = (real **)malloc((size_t)(*Num) * sizeof(real *)) - 1;
    *flag = (int  **)malloc((size_t)(*Num) * sizeof(int  *)) - 1;

    for (i = 1; i <= *Num; i++)
        if (fscanf(f, "%d", &(*ord)[i]) != 1) (*ord)[i] = 0;
    fscanf(f, "\n");

    for (i = 1; i <= *Num; i++) {
        (*coef)[i] = vector(0, (*ord)[i]);
        (*flag)[i] = ivector(0, (*ord)[i]);

        fgets(line, FUEPRE_LINE, f);                 /* "**" */
        for (j = 1; j <= (*ord)[i]; j++) {
            if (fscanf(f, "%lf", &(*coef)[i][j]) != 1) (*coef)[i][j] = 0.0;
            if (fscanf(f, "%d\n", &(*flag)[i][j]) != 1) (*flag)[i][j] = 0;
        }
    }
}

/* ─── Lee una sección de factores de FRECUENCIA FIJA ────────────────────
   Puerto fiel de fue.c [3.3.9]-[3.3.10]. Cada factor es de orden 2 con un único
   coeficiente libre (el término en B²), y la primera línea lleva FRECUENCIAS
   (que fue lee como reales: "f = 3.0").                                      */
static void read_fixfreq_section(FILE *f, char *line, int *Num, int **fre,
                                 real ***coef, int **flag)
{
    int i;

    fgets(line, FUEPRE_LINE, f);                     /* cabecera */

    *Num = 0;
    if (fscanf(f, "%d", Num) != 1 || *Num <= 0) {
        *Num = 0;
        fscanf(f, "\n");
        return;
    }

    *fre  = ivector(1, *Num);
    *coef = (real **)malloc((size_t)(*Num) * sizeof(real *)) - 1;
    *flag = ivector(1, *Num);

    for (i = 1; i <= *Num; i++) {
        double fr = 0.0;
        if (fscanf(f, "%lf", &fr) != 1) fr = 0.0;
        (*fre)[i] = (int)fr;
    }
    fscanf(f, "\n");

    for (i = 1; i <= *Num; i++) {
        (*coef)[i] = vector(0, 2);
        fgets(line, FUEPRE_LINE, f);                 /* "**" */
        if (fscanf(f, "%lf", &(*coef)[i][2]) != 1) (*coef)[i][2] = 0.0;
        if (fscanf(f, "%d\n", &(*flag)[i]) != 1) (*flag)[i] = 0;
    }
}

/* ─── Componente determinista sobre un horizonte EXTENDIDO ──────────────
   Regenera cada variable determinista en t = 1..nobs_ext (son funciones del
   tiempo: armonicos, tendencias, intervenciones fechadas...), le aplica su
   filtro racional Omega(B)/Delta(B) y devuelve la contribucion total.

   Es lo que permite PREVER el nivel: la parte determinista del futuro se
   conoce exactamente, no se prevé.                                         */
void build_det_component(struct Tusmodel *Tm, struct Tseries *Ts,
                         int nobs_ext, real *det_out)
{
    struct Tseries Text = *Ts;      /* misma fecha de inicio, mas observaciones */
    real *v, *filt_num, *filt_out;
    int i, j, t;

    for (t = 1; t <= nobs_ext; t++) det_out[t] = 0.0;
    if (Tm->NdetVar <= 0) return;

    Text.nobs = nobs_ext;

    v        = vector(1, nobs_ext);
    filt_num = vector(1, nobs_ext);
    filt_out = vector(1, nobs_ext);

    for (i = 1; i <= Tm->NdetVar; i++) {
        int nw = Tm->Nomega[i];
        int nd = Tm->Ndelta[i];

        gen_detvar(Tm->detspec[i], &Text, v);

        /* numerador Omega(B) = ω₀ - ω₁B - ... (Box-Jenkins, como fue calcnu):
           el lider suma, los demas RESTAN. */
        for (t = 1; t <= nobs_ext; t++) {
            real sum = 0.0;
            for (j = 0; j <= nw; j++)
                if (t - j >= 1)
                    sum += (j == 0 ? Tm->Omega[i][j] : -Tm->Omega[i][j]) * v[t - j];
            filt_num[t] = sum;
        }

        /* denominador 1/Delta(B) (recursivo) */
        for (t = 1; t <= nobs_ext; t++) {
            real sum = filt_num[t];
            for (j = 1; j <= nd; j++)
                if (t - j >= 1) sum += Tm->Delta[i][j] * filt_out[t - j];
            filt_out[t] = sum;
        }

        for (t = 1; t <= nobs_ext; t++) det_out[t] += filt_out[t];
    }

    free_vector(filt_out, 1, nobs_ext);
    free_vector(filt_num, 1, nobs_ext);
    free_vector(v, 1, nobs_ext);
}

/* ─── read_fue_pre ────────────────────────────────────────────────────── */
int read_fue_pre(const char *filename,
                 struct Tusmodel *Tm, struct Tseries *Ts, real ***DataMat)
{
    FILE *f;
    char  line[FUEPRE_LINE];
    int   i, j;

    if (NULL == (f = fopen(filename, "r"))) {
        fprintf(stderr, "Error opening %s\n", filename);
        return 1;
    }

    /*  LA CABECERA ES LIBRE, y por eso aqui NO se cuentan lineas.  El parser
     *  autoritativo (fue/src/fue/inp.py [3.0], FILE_CONTRACT.md 2.0) descarta
     *  lo que venga hasta el separador cuyo texto dice "frequency", y ese es
     *  el unico sitio de todo el formato donde mira lo que un comentario DICE.
     *
     *  Contar cinco lineas no lee el FORMATO: lee un fichero concreto.  El
     *  .pre lo escribe fue con una linea en blanco tras el banner -- cinco
     *  renglones --, pero el .inp que escribe drvec no la trae y el de DRVUS
     *  trae ademas una linea de especificacion menos.  La familia de ficheros
     *  que este lector tiene delante NO es de cabecera uniforme, y un renglon
     *  de desplazamiento no da error: da un nobs leido de la linea equivocada.
     *  Medido desde drvec: un .inp de su bateria daba nobs = 1787128427 y el
     *  proceso moria por memoria.                                            */
    {
        int seen = 0;
        while (fgets(line, FUEPRE_LINE, f))
            if (strstr(line, "requency")) { seen = 1; break; }
        if (!seen) {
            fprintf(stderr, "ERROR: %s no trae el separador de frecuencia;"
                            " no es un fichero del formato fue\n", filename);
            fclose(f);
            return 1;
        }
    }

    Tm->residuals = (char *)malloc(FUEPRE_LINE);

    /* ── Frequency ── */
    fgets(line, FUEPRE_LINE, f);
    if (strstr(line, "number") || strstr(line, "Number"))
        { Ts->freq = 1; Ts->numbering = 1; }
    else
        { sscanf(line, "%u", &Ts->freq); Ts->numbering = 0; }

    /* ── nobs, dates, name ── */
    fgets(line, FUEPRE_LINE, f);  /* comment */
    fgets(line, FUEPRE_LINE, f);
    {
        char namef[80]; int outyear;
        Ts->nobs = 0;             /* si la linea no trae numero, se ve abajo */
        if (Ts->freq > 1)
            sscanf(line, "%d %d %d %s %s",
                   &Ts->nobs, &Ts->begtime, &Ts->begyear,
                   namef, Tm->residuals);
        else {
            sscanf(line, "%d %d %d %s %s",
                   &Ts->nobs, &outyear, &Ts->begyear,
                   namef, Tm->residuals);
            Ts->begtime = 1;
        }
        Ts->name = strdup(namef);
    }

    /*  Un nobs disparatado es la firma de una lectura desalineada, y pedir el
     *  vector antes de mirarlo convierte un fichero mal formado en una muerte
     *  por memoria, que no dice nada de lo que pasa.                          */
    if (Ts->nobs <= 0) {
        fprintf(stderr, "ERROR: %s declara %d observaciones\n",
                filename, Ts->nobs);
        free(Tm->residuals);
        fclose(f);
        return 1;
    }

    Ts->data = vector(1, Ts->nobs);

    /* ── NdetVar ──  (puerto fiel de fue.c [3.2]: fgets cabecera + fscanf) */
    fgets(line, FUEPRE_LINE, f);                 /* cabecera */
    Tm->NdetVar = 0;
    fscanf(f, "%d\n", &Tm->NdetVar);

    *DataMat = matrix(0, Tm->NdetVar, 1, Ts->nobs);

    if (Tm->NdetVar > 0) {

        /* ── [3.2.0] Nombres de las deterministas y generación de DataMat ──
           fue lee el tipo con fscanf("%s") y a continuación sus argumentos.
           Aquí se lee la línea completa y se delega en gen_detvar.          */
        fgets(line, FUEPRE_LINE, f);             /* "**" */

        Tm->detspec = (char **)malloc((size_t)Tm->NdetVar * sizeof(char *)) - 1;

        for (i = 1; i <= Tm->NdetVar; i++) {
            fgets(line, FUEPRE_LINE, f);
            line[strcspn(line, "\r\n")] = '\0';

            /* Se guarda la especificacion: hace falta para regenerar la
               determinista en fechas FUTURAS al prever.                     */
            Tm->detspec[i] = strdup(line);

            if (!gen_detvar(line, Ts, (*DataMat)[i])) {
                fprintf(stderr,
                        "Error: variable determinista %d (\"%s\") no reconocida.\n"
                        "  drtran admite: impulse, compimp, step, ramp, easter,\n"
                        "  trend, cos, sin, alter.\n"
                        "  Las variables NO ESTÁNDAR no se admiten por diseño: son\n"
                        "  una versión rudimentaria de un modelo de transferencia con\n"
                        "  input X. Especifica esa relación como transferencia (ω/δ, b),\n"
                        "  que es precisamente lo que drtran estima.\n",
                        i, line);
                fclose(f);
                return 2;
            }
        }

        /* ── [3.2.1] Reserva de omegas y deltas ── */
        Tm->Nomega = ivector(1, Tm->NdetVar);
        Tm->Omega  = (real **)malloc((size_t)Tm->NdetVar * sizeof(real *)) - 1;
        Tm->Imega  = (int  **)malloc((size_t)Tm->NdetVar * sizeof(int  *)) - 1;
        Tm->Ndelta = ivector(1, Tm->NdetVar);
        Tm->Delta  = (real **)malloc((size_t)Tm->NdetVar * sizeof(real *)) - 1;
        Tm->Ielta  = (int  **)malloc((size_t)Tm->NdetVar * sizeof(int  *)) - 1;

        /* ── [3.2.2] Omegas: ω(B) de cada determinista ── */
        fgets(line, FUEPRE_LINE, f);             /* cabecera "**" */
        for (i = 1; i <= Tm->NdetVar; i++)
            if (fscanf(f, "%d", &Tm->Nomega[i]) != 1) Tm->Nomega[i] = 0;
        fscanf(f, "\n");

        for (i = 1; i <= Tm->NdetVar; i++) {
            Tm->Omega[i] = vector(0, Tm->Nomega[i]);
            Tm->Imega[i] = ivector(0, Tm->Nomega[i]);
            fgets(line, FUEPRE_LINE, f);         /* "**" */
            for (j = 0; j <= Tm->Nomega[i]; j++) {
                if (fscanf(f, "%lf", &Tm->Omega[i][j]) != 1) Tm->Omega[i][j] = 0.0;
                if (fscanf(f, "%d\n", &Tm->Imega[i][j]) != 1) Tm->Imega[i][j] = 0;
            }
        }

        /* ── [3.2.3] Deltas: δ(B) de cada determinista (solo si Ndelta > 0) ── */
        fgets(line, FUEPRE_LINE, f);             /* cabecera "**" */
        for (i = 1; i <= Tm->NdetVar; i++)
            if (fscanf(f, "%d", &Tm->Ndelta[i]) != 1) Tm->Ndelta[i] = 0;
        fscanf(f, "\n");

        for (i = 1; i <= Tm->NdetVar; i++) {
            if (Tm->Ndelta[i] <= 0) continue;

            Tm->Delta[i] = vector(1, Tm->Ndelta[i]);
            Tm->Ielta[i] = ivector(1, Tm->Ndelta[i]);
            fgets(line, FUEPRE_LINE, f);         /* "**" */
            for (j = 1; j <= Tm->Ndelta[i]; j++) {
                if (fscanf(f, "%lf", &Tm->Delta[i][j]) != 1) Tm->Delta[i][j] = 0.0;
                if (fscanf(f, "%d\n", &Tm->Ielta[i][j]) != 1) Tm->Ielta[i][j] = 0;
            }
        }

    } else {
        /* NdetVar == 0: dummy allocation */
        Tm->Nomega = ivector(1, 1); Tm->Nomega[1] = 0;
        Tm->Omega  = (real **)malloc(sizeof(real *)) - 1;
        Tm->Imega  = (int  **)malloc(sizeof(int  *)) - 1;
        Tm->Ndelta = ivector(1, 1); Tm->Ndelta[1] = 0;
        Tm->Delta  = (real **)malloc(sizeof(real *)) - 1;
        Tm->Ielta  = (int  **)malloc(sizeof(int  *)) - 1;
    }

    /* ── Factores ARMA: AR regular, AR anual, MA regular, MA anual ──
       Antes solo se parseaba el AR regular; los demás se declaraban pero no se
       leían (puntero reservado, sin órdenes ni coeficientes), lo que provocaba
       un segfault en cuanto el modelo tenía MA o estacionalidad.            */
    read_arma_section(f, line, &Tm->NumAr1, &Tm->p1, &Tm->Ar1, &Tm->Ia1);
    read_arma_section(f, line, &Tm->NumAr2, &Tm->p2, &Tm->Ar2, &Tm->Ia2);
    read_arma_section(f, line, &Tm->NumMa1, &Tm->q1, &Tm->Ma1, &Tm->Im1);
    read_arma_section(f, line, &Tm->NumMa2, &Tm->q2, &Tm->Ma2, &Tm->Im2);

    /* ── Factores de frecuencia fija (AR(2)/MA(2) irreducibles) ── */
    read_fixfreq_section(f, line, &Tm->NumAr1f, &Tm->pfre1, &Tm->Ar1f, &Tm->Ia1f);
    read_fixfreq_section(f, line, &Tm->NumMa1f, &Tm->qfre1, &Tm->Ma1f, &Tm->Im1f);

    /* ── mu ──
       FUE escribe "valor flag" si la media se estima (p.ej. "0.154472  1"),
       y un único "0" si la media NO forma parte del modelo. El flag es lo que
       decide si mu es un parámetro libre, no el que su valor sea o no cero.  */
    fgets(line, FUEPRE_LINE, f); fgets(line, FUEPRE_LINE, f);
    if (sscanf(line, "%lf %d", &Tm->mu, &Tm->Imu) < 2) {
        Tm->Imu = 0;   /* media fijada en el valor leído (típicamente 0) */
    }

    /* ── Box-Cox + diffs ── */
    fgets(line, FUEPRE_LINE, f); fgets(line, FUEPRE_LINE, f);
    sscanf(line, "%lf %d %d", &Tm->boxlam, &Tm->nrdiff, &Tm->nadiff);

    /* ── ifadf ── */
    /*  BUG-11.  La seccion la escriben SIEMPRE los dos escritores de fue --
     *  fue-1.13.1/src/fue.c:3485 y fue/src/fue/report.py:1203 --: con freq > 1
     *  los freq/2+1 flags, y con datos ANUALES un " 0" literal.  Este lector
     *  leia las dos lineas SOLO dentro del if, asi que en un fichero anual
     *  todo lo que viene detras se desplazaba: cbands y refactor salian 0.0 y
     *  LA SERIE NO SE LEIA, quedando a ceros, con read_fue_pre devolviendo
     *  EXITO.  Silencioso y verosimil.
     *
     *  No se veia aqui porque los datos de drtran son mensuales.  Lo encontro
     *  drvec, cuyo banco es anual (mink-muskrat, 1850-1911), al reutilizar
     *  este lector en vez de escribir un segundo.  El defecto entro con la
     *  EXTRACCION: el lector propio de fue conserva la rama que la copia
     *  perdio.                                                                */
    fgets(line, FUEPRE_LINE, f); fgets(line, FUEPRE_LINE, f);
    if (Ts->freq > 1) {
        Tm->ifadf = ivector(0, Ts->freq / 2);
        /* Exactly freq/2+1 integers, or the pointer advanced by an
           UNINITIALISED offset (REGLAS-NO-ESCRITAS 30).                   */
        { char *p = line; for (i = 0; i <= Ts->freq / 2; i++) {
            int off = 0;
            if (sscanf(p, "%d%n", &Tm->ifadf[i], &off) != 1) {
                fprintf(stderr, "ERROR: %s: the annual-difference factors line "
                                "needs %d integers (freq %d); found %d\n",
                        filename, Ts->freq / 2 + 1, Ts->freq, i);
                fclose(f);
                return 1;
            }
            p += off; } }
    } else {
        Tm->ifadf = NULL;   /* antes quedaba sin inicializar */
    }

    /* ── cbands + refactor ── */
    fgets(line, FUEPRE_LINE, f); fgets(line, FUEPRE_LINE, f);
    sscanf(line, "%lf %lf", &Tm->cbands, &Ts->refactor);
    if (Ts->refactor == 0.0) Ts->refactor = 1.0;

    /* ── Data section ── */
/* lectura de la serie */
    fgets(line, FUEPRE_LINE, f);  /* "** Time series..." */
    /* A short data block is an ERROR. It used to stop reading and return
       success with the rest of the series left at zero (REGLAS-NO-ESCRITAS
       16): a model estimated on data that are not in the file.            */
    for (i = 1; i <= Ts->nobs; i++) {
        if (!fgets(line, FUEPRE_LINE, f) || sscanf(line, "%lf", &Ts->data[i]) != 1) {
            fprintf(stderr, "ERROR: %s: the data block ends at observation %d "
                            "of the %d declared\n", filename, i - 1, Ts->nobs);
            fclose(f);
            return 1;
        }
    }

    fclose(f);

    /* ── Finalize model ── */
    Tm->sper = Ts->freq;
    for (i = 1; i <= Tm->NumAr1; i++) Tm->Ar1[i][0] = -1.0;
    for (i = 1; i <= Tm->NumAr2; i++) Tm->Ar2[i][0] = -1.0;
    for (i = 1; i <= Tm->NumMa1; i++) Tm->Ma1[i][0] = -1.0;
    for (i = 1; i <= Tm->NumMa2; i++) Tm->Ma2[i][0] = -1.0;
    for (i = 1; i <= Tm->NumAr1f; i++) Tm->Ar1f[i][0] = -1.0;
    for (i = 1; i <= Tm->NumMa1f; i++) Tm->Ma1f[i][0] = -1.0;

    Tm->ornsop = Tm->nrdiff + Tm->sper * Tm->nadiff;
    if (Tm->sper == 12) {
        if (Tm->ifadf[0] == 1) Tm->ornsop += 1;
        if (Tm->ifadf[1] == 1) Tm->ornsop += 2;
        if (Tm->ifadf[2] == 1) Tm->ornsop += 2;
        if (Tm->ifadf[3] == 1) Tm->ornsop += 2;
        if (Tm->ifadf[4] == 1) Tm->ornsop += 2;
        if (Tm->ifadf[5] == 1) Tm->ornsop += 2;
        if (Tm->ifadf[6] == 1) Tm->ornsop += 1;
    } else if (Tm->sper == 4) {
        if (Tm->ifadf[0] == 1) Tm->ornsop += 1;
        if (Tm->ifadf[1] == 1) Tm->ornsop += 2;
        if (Tm->ifadf[2] == 1) Tm->ornsop += 1;
    }

    Tm->rnsop = vector(0, Tm->ornsop);
    CalcNonsOp(Tm->sper, Tm->nrdiff, Tm->nadiff, Tm->ifadf,
               Tm->ornsop, Tm->rnsop);

    return 0;
}

/*****************************************************************************/
/*  free_fue_pre -- BUG-12: read_fue_pre no traia desasignador                */
/*****************************************************************************/
/*  Cada lectura de un .pre reservaba una veintena de bloques -- la serie, los
 *  deterministas con sus omega y delta, los cuatro bloques de factores ARMA,
 *  los de frecuencia fija, ifadf, rnsop y los residuos -- y no habia forma de
 *  soltarlos.  Medido sobre un caso mensual de examples/: 20 bloques
 *  "definitely lost", varios de esta funcion.
 *
 *  En un programa que lee dos ficheros y termina no hace dano, y por eso duro.
 *  Lo que hace es TAPAR: una salida de valgrind con veinte fugas conocidas es
 *  una en la que la veintiuna no se ve.  Y en cuanto alguien llame al lector en
 *  un bucle -- un barrido de especificaciones, un bootstrap -- deja de ser
 *  inofensivo.
 *
 *  Escrito en drvec, que reutilizo este lector y necesitaba cerrarlo para que
 *  su bateria de memoria pudiera ser verde; devuelto aqui el 2026-08-21.
 *
 *  free_base1 deshace el idioma `malloc(n*size) - 1` con el que el lector
 *  reserva sus arrays base-1: el bloque real empieza en p+1.                 */
static void free_base1( void *base1 )
{
    void **p = (void **) base1;
    if ( p != NULL ) free( (void *) &p[1] );
}

void free_fue_pre( struct Tusmodel *Tm, struct Tseries *Ts, real **DataMat )
{
    int i;

    if ( Ts != NULL )
        {
        if ( Ts->name ) free( Ts->name );
        if ( Ts->data ) free_vector( Ts->data, 1, Ts->nobs );
        Ts->name = NULL; Ts->data = NULL;
        }
    if ( Tm == NULL ) return;

    if ( DataMat && Ts ) free_matrix( DataMat, 0, Tm->NdetVar, 1, Ts->nobs );

    /* ── deterministas ── */
    if ( Tm->NdetVar > 0 )
        {
        for ( i = 1; i <= Tm->NdetVar; i++ )
            {
            if ( Tm->detspec && Tm->detspec[i] ) free( Tm->detspec[i] );
            if ( Tm->Omega && Tm->Omega[i] )
                free_vector( Tm->Omega[i], 0, Tm->Nomega[i] );
            if ( Tm->Imega && Tm->Imega[i] )
                free_ivector( Tm->Imega[i], 0, Tm->Nomega[i] );
            if ( Tm->Ndelta && Tm->Ndelta[i] > 0 )
                {
                if ( Tm->Delta && Tm->Delta[i] )
                    free_vector( Tm->Delta[i], 1, Tm->Ndelta[i] );
                if ( Tm->Ielta && Tm->Ielta[i] )
                    free_ivector( Tm->Ielta[i], 1, Tm->Ndelta[i] );
                }
            }
        if ( Tm->detspec ) free_base1( Tm->detspec );
        if ( Tm->Nomega )  free_ivector( Tm->Nomega, 1, Tm->NdetVar );
        if ( Tm->Ndelta )  free_ivector( Tm->Ndelta, 1, Tm->NdetVar );
        }
    else
        {
        /* la reserva ficticia del caso sin deterministas */
        if ( Tm->Nomega ) free_ivector( Tm->Nomega, 1, 1 );
        if ( Tm->Ndelta ) free_ivector( Tm->Ndelta, 1, 1 );
        }
    if ( Tm->Omega ) free_base1( Tm->Omega );
    if ( Tm->Imega ) free_base1( Tm->Imega );
    if ( Tm->Delta ) free_base1( Tm->Delta );
    if ( Tm->Ielta ) free_base1( Tm->Ielta );
    Tm->detspec = NULL; Tm->Nomega = NULL; Tm->Ndelta = NULL;
    Tm->Omega = NULL; Tm->Imega = NULL; Tm->Delta = NULL; Tm->Ielta = NULL;

    /* ── factores ARMA: los cuatro bloques tienen la misma forma ── */
    {
    int   nums[4];
    int  *ords[4];
    real **cfs[4];
    int  **fls[4];
    int b;

    nums[0] = Tm->NumAr1; ords[0] = Tm->p1; cfs[0] = Tm->Ar1; fls[0] = Tm->Ia1;
    nums[1] = Tm->NumAr2; ords[1] = Tm->p2; cfs[1] = Tm->Ar2; fls[1] = Tm->Ia2;
    nums[2] = Tm->NumMa1; ords[2] = Tm->q1; cfs[2] = Tm->Ma1; fls[2] = Tm->Im1;
    nums[3] = Tm->NumMa2; ords[3] = Tm->q2; cfs[3] = Tm->Ma2; fls[3] = Tm->Im2;

    for ( b = 0; b < 4; b++ )
        {
        if ( nums[b] <= 0 ) continue;
        for ( i = 1; i <= nums[b]; i++ )
            {
            if ( cfs[b] && cfs[b][i] ) free_vector( cfs[b][i], 0, ords[b][i] );
            if ( fls[b] && fls[b][i] ) free_ivector( fls[b][i], 0, ords[b][i] );
            }
        if ( ords[b] ) free_ivector( ords[b], 1, nums[b] );
        if ( cfs[b] )  free_base1( cfs[b] );
        if ( fls[b] )  free_base1( fls[b] );
        }
    }
    Tm->p1 = Tm->p2 = Tm->q1 = Tm->q2 = NULL;
    Tm->Ar1 = Tm->Ar2 = Tm->Ma1 = Tm->Ma2 = NULL;
    Tm->Ia1 = Tm->Ia2 = Tm->Im1 = Tm->Im2 = NULL;
    Tm->NumAr1 = Tm->NumAr2 = Tm->NumMa1 = Tm->NumMa2 = 0;

    /* ── factores de frecuencia fija: coef es (0..2) por factor ── */
    if ( Tm->NumAr1f > 0 )
        {
        for ( i = 1; i <= Tm->NumAr1f; i++ )
            if ( Tm->Ar1f && Tm->Ar1f[i] ) free_vector( Tm->Ar1f[i], 0, 2 );
        if ( Tm->pfre1 ) free_ivector( Tm->pfre1, 1, Tm->NumAr1f );
        if ( Tm->Ia1f )  free_ivector( Tm->Ia1f,  1, Tm->NumAr1f );
        if ( Tm->Ar1f )  free_base1( Tm->Ar1f );
        }
    if ( Tm->NumMa1f > 0 )
        {
        for ( i = 1; i <= Tm->NumMa1f; i++ )
            if ( Tm->Ma1f && Tm->Ma1f[i] ) free_vector( Tm->Ma1f[i], 0, 2 );
        if ( Tm->qfre1 ) free_ivector( Tm->qfre1, 1, Tm->NumMa1f );
        if ( Tm->Im1f )  free_ivector( Tm->Im1f,  1, Tm->NumMa1f );
        if ( Tm->Ma1f )  free_base1( Tm->Ma1f );
        }
    Tm->Ar1f = Tm->Ma1f = NULL; Tm->pfre1 = Tm->qfre1 = NULL;
    Tm->Ia1f = Tm->Im1f = NULL;
    Tm->NumAr1f = Tm->NumMa1f = 0;

    /* ── resto ── */
    if ( Tm->ifadf && Tm->sper > 1 ) free_ivector( Tm->ifadf, 0, Tm->sper / 2 );
    if ( Tm->rnsop ) free_vector( Tm->rnsop, 0, Tm->ornsop );
    if ( Tm->residuals ) free( Tm->residuals );
    Tm->ifadf = NULL; Tm->rnsop = NULL; Tm->residuals = NULL;
}



/*****************************************************************************/
/*  operators_differ_tm -- los DOS operadores no estacionarios, comparados.   */
/*                                                                           */
/*  Compara el POLINOMIO, no el par (nrdiff, nadiff), y la diferencia         */
/*  importa: nabla nabla_4 escrito a la manera de la escuela --nrdiff=2,      */
/*  nadiff=0, ifadf=[0,1,1], como lo lleva EA de m6-- es EL MISMO OPERADOR    */
/*  que nrdiff=1, nadiff=1, y comparando los enteros esas dos codificaciones  */
/*  se leerian como desajuste. rnsop ya trae el polinomio armado.            */
/*                                                                           */
/*  Vive aqui, y no en drtran.c con el main(), para que el GUI pueda usar la  */
/*  misma comparacion que usa el motor en vez de escribir otra.              */
/*****************************************************************************/

int operators_differ_tm( const struct Tusmodel *a, const struct Tusmodel *b )
{
   int j;

   if ( a->ornsop != b->ornsop ) return 1;
   for ( j = 0; j <= a->ornsop; j++ )
       if ( fabs( a->rnsop[j] - b->rnsop[j] ) > 1e-9 ) return 1;
   return 0;
}


/* ========================================================================== */
/*  Los coeficientes LIBRES del modelo leido                                   */
/*                                                                            */
/*  Estaban en drtran.c y tran_shootx.c. fue lleva un flag por coeficiente    */
/*  (Ia1/Ia2/Ia1f, Im1/Im2/Im1f, Imega/Ielta): "0.0000  0" es un coeficiente  */
/*  FIJO, no un valor inicial. El orden de recorrido de pack_* y unpack_* es  */
/*  el contrato entre quien arma el vector y quien lo lee: por eso viven      */
/*  juntos y en un solo sitio.                                                */
/* ========================================================================== */

/* Un factor de frecuencia fija exige c₂ < 0 (su módulo es r = sqrt(−c₂)).
   fue devuelve ifault si no se cumple; drtran rechaza el punto igual.      */
int invalid_fixfreq(struct Tusmodel *Tm)
{
    int i;
    for (i = 1; i <= Tm->NumAr1f; i++)
        if (Tm->Ar1f[i][2] >= 0.0) return 1;
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Ma1f[i][2] >= 0.0) return 1;
    return 0;
}

/* Número de parámetros AR LIBRES (coeficientes de factores, no expandidos) */
int n_ar_free_params(struct Tusmodel *Tm)
{
    int n = 0, i, j;
    for (i = 1; i <= Tm->NumAr1;  i++)
        for (j = 1; j <= Tm->p1[i]; j++) if (Tm->Ia1[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumAr2;  i++)
        for (j = 1; j <= Tm->p2[i]; j++) if (Tm->Ia2[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumAr1f; i++)   /* un solo coef libre por factor: c₂ */
        if (Tm->Ia1f[i] == 1) n++;
    return n;
}

/* Número de parámetros MA LIBRES */
int n_ma_free_params(struct Tusmodel *Tm)
{
    int n = 0, i, j;
    for (i = 1; i <= Tm->NumMa1;  i++)
        for (j = 1; j <= Tm->q1[i]; j++) if (Tm->Im1[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumMa2;  i++)
        for (j = 1; j <= Tm->q2[i]; j++) if (Tm->Im2[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Im1f[i] == 1) n++;
    return n;
}

/* Empaqueta en x[] los coeficientes AR LIBRES de Tm (índice base idx). */
int pack_ar_factors(struct Tusmodel *Tm, real *x, int idx)
{
    int i, j, base = idx;
    for (i = 1; i <= Tm->NumAr1; i++)
        for (j = 1; j <= Tm->p1[i]; j++)
            if (Tm->Ia1[i][j] == 1) x[idx++] = Tm->Ar1[i][j];
    for (i = 1; i <= Tm->NumAr2; i++)
        for (j = 1; j <= Tm->p2[i]; j++)
            if (Tm->Ia2[i][j] == 1) x[idx++] = Tm->Ar2[i][j];
    for (i = 1; i <= Tm->NumAr1f; i++)
        if (Tm->Ia1f[i] == 1) x[idx++] = Tm->Ar1f[i][2];
    return idx - base;
}

/* Empaqueta en x[] los coeficientes MA LIBRES de Tm */
int pack_ma_factors(struct Tusmodel *Tm, real *x, int idx)
{
    int i, j, base = idx;
    for (i = 1; i <= Tm->NumMa1; i++)
        for (j = 1; j <= Tm->q1[i]; j++)
            if (Tm->Im1[i][j] == 1) x[idx++] = Tm->Ma1[i][j];
    for (i = 1; i <= Tm->NumMa2; i++)
        for (j = 1; j <= Tm->q2[i]; j++)
            if (Tm->Im2[i][j] == 1) x[idx++] = Tm->Ma2[i][j];
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Im1f[i] == 1) x[idx++] = Tm->Ma1f[i][2];
    return idx - base;
}

int n_det_free_params(struct Tusmodel *Tm)
{
    int n = 0, i, j;
    for (i = 1; i <= Tm->NdetVar; i++) {
        for (j = 0; j <= Tm->Nomega[i]; j++)
            if (Tm->Imega[i][j] == 1) n++;
        for (j = 1; j <= Tm->Ndelta[i]; j++)
            if (Tm->Ielta[i][j] == 1) n++;
    }
    return n;
}

/* Empaqueta en x[] los coeficientes deterministas libres */
int pack_det_params(struct Tusmodel *Tm, real *x, int idx)
{
    int i, j, base = idx;
    for (i = 1; i <= Tm->NdetVar; i++) {
        for (j = 0; j <= Tm->Nomega[i]; j++)
            if (Tm->Imega[i][j] == 1) x[idx++] = Tm->Omega[i][j];
        for (j = 1; j <= Tm->Ndelta[i]; j++)
            if (Tm->Ielta[i][j] == 1) x[idx++] = Tm->Delta[i][j];
    }
    return idx - base;
}

/* Desempaqueta desde x[] a Tm los coeficientes deterministas libres */
void unpack_det_params(struct Tusmodel *Tm, real *x, int *idx)
{
    int i, j;
    for (i = 1; i <= Tm->NdetVar; i++) {
        for (j = 0; j <= Tm->Nomega[i]; j++)
            if (Tm->Imega[i][j] == 1) Tm->Omega[i][j] = x[(*idx)++];
        for (j = 1; j <= Tm->Ndelta[i]; j++)
            if (Tm->Ielta[i][j] == 1) Tm->Delta[i][j] = x[(*idx)++];
    }
}

/* Desempaqueta coeficientes AR desde x[] a Tm (usado en shootx) */
void unpack_ar_factors(struct Tusmodel *Tm, real *x, int *idx)
{
    int i, j;
    for (i = 1; i <= Tm->NumAr1; i++)
        for (j = 1; j <= Tm->p1[i]; j++)
            if (Tm->Ia1[i][j] == 1) Tm->Ar1[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumAr2; i++)
        for (j = 1; j <= Tm->p2[i]; j++)
            if (Tm->Ia2[i][j] == 1) Tm->Ar2[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumAr1f; i++)
        if (Tm->Ia1f[i] == 1) Tm->Ar1f[i][2] = x[(*idx)++];
}

/* Desempaqueta coeficientes MA desde x[] a Tm */
void unpack_ma_factors(struct Tusmodel *Tm, real *x, int *idx)
{
    int i, j;
    for (i = 1; i <= Tm->NumMa1; i++)
        for (j = 1; j <= Tm->q1[i]; j++)
            if (Tm->Im1[i][j] == 1) Tm->Ma1[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumMa2; i++)
        for (j = 1; j <= Tm->q2[i]; j++)
            if (Tm->Im2[i][j] == 1) Tm->Ma2[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Im1f[i] == 1) Tm->Ma1f[i][2] = x[(*idx)++];
}


/* ========================================================================== */
/*  fuepre_check_alignment -- BUG-2: que se crucen FECHAS, no posiciones       */
/*                                                                            */
/*  El cast conjunto alinea las series por el FINAL y las recorta a la mas    */
/*  corta. Eso es correcto para lo que se escribio --distintos d/D sobre la   */
/*  MISMA ventana-- y mudo cuando las ventanas son trozos distintos del       */
/*  calendario: dos series con el mismo numero de observaciones y catorce     */
/*  anos de desfase se cruzaban sin una palabra, porque la fecha no entraba   */
/*  en ninguna cuenta.                                                        */
/*                                                                            */
/*  Es un RECHAZO, no un recorte, como en drtran-python                       */
/*  (cast.check_alignment): recortar a la ventana comun de calendario         */
/*  cambia sobre que observaciones se ajusta el modelo, y esa decision es de  */
/*  quien construye los .pre en art. Adivinarla aqui cambiaria una respuesta  */
/*  equivocada y muda por otra distinta y callada.                            */
/*                                                                            */
/*  Ts[1..m]. Devuelve 0 si se pueden cruzar; si no, != 0 con el motivo en    */
/*  why[size].                                                                */
/* ========================================================================== */
static void end_date(const struct Tseries *Ts, int *per, int *sub)
{
    ObsToDate(Ts->begyear, Ts->begtime, Ts->nobs, Ts->freq, per, sub);
}

int fuepre_check_alignment(const struct Tseries *Ts, int m, char *why, size_t size)
{
    int i, ref_per, ref_sub, per, sub;

    for (i = 2; i <= m; i++) {
        if (Ts[i].freq != Ts[1].freq) {
            snprintf(why, size,
                     "'%s' is %d-per-year and '%s' is %d: they cannot be "
                     "modelled jointly. Rebuild them at the same frequency "
                     "in art.",
                     Ts[i].name, Ts[i].freq, Ts[1].name, Ts[1].freq);
            return 1;
        }
        /* Sin fechas ("number" en la frecuencia) no hay calendario que
           comparar: el final comun es la unica lectura posible.          */
        if (Ts[i].numbering != Ts[1].numbering) {
            snprintf(why, size,
                     "'%s' is dated and '%s' is not: there is no calendar to "
                     "align them on.",
                     Ts[i].numbering ? Ts[1].name : Ts[i].name,
                     Ts[i].numbering ? Ts[i].name : Ts[1].name);
            return 2;
        }
    }
    if (Ts[1].numbering) return 0;

    end_date(&Ts[1], &ref_per, &ref_sub);
    for (i = 2; i <= m; i++) {
        end_date(&Ts[i], &per, &sub);
        if (per != ref_per || sub != ref_sub) {
            snprintf(why, size,
                     "the series do NOT end on the same date: %s ends "
                     "%02d/%d and %s ends %02d/%d. The joint cast aligns at "
                     "the END and trims to the shortest, which assumes a "
                     "common last observation; with different windows it "
                     "would pair observations that are years apart and say "
                     "nothing. Rebuild both .pre in art over the window you "
                     "mean to model.",
                     Ts[1].name, ref_sub, ref_per, Ts[i].name, sub, per);
            return 3;
        }
    }
    return 0;
}
