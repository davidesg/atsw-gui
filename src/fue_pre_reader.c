/*****************************************************************************/
/*  fue_pre_reader.c — Lector de archivos .pre (DRVUS/FUE)                   */
/*  Parsea el modelo completo: deterministas (Omega/Delta), factores ARMA,   */
/*  media (valor + flag de estimación), Box-Cox, diferencias y la serie.     */
/*****************************************************************************/

#include "main.h"
#include "drtran.h"
#include "fue_pre_reader.h"
#include <string.h>
#include <math.h>

#ifndef MAXSTR
#define MAXSTR 512
#endif

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
        pol4[0] = -1.0; pol4[1] = 1.0;
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

/* Nota: DateToObs, ObsToDate y Easter las aporta el motor (diagnose.c /
   nlatools.c) y estan declaradas en main.h: son las mismas rutinas que fue. */


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

    /* --- Semana Santa (solo mensual) --- */
    if (strncmp(line, "easter", 6) == 0 && Ts->freq == 12) {
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
    if (strncmp(line, "alter", 5) == 0) {
        for (j = 1; j <= Ts->nobs; j++) v[j] = pow(-1.0, (double)j);
        return 1;
    }
    if (strncmp(line, "trend", 5) == 0 || strncmp(line, "time", 4) == 0) {
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

    fgets(line, MAXSTR, f);                     /* cabecera */

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

        fgets(line, MAXSTR, f);                 /* "**" */
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

    fgets(line, MAXSTR, f);                     /* cabecera */

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
        fgets(line, MAXSTR, f);                 /* "**" */
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

        /* numerador Omega(B) */
        for (t = 1; t <= nobs_ext; t++) {
            real sum = 0.0;
            for (j = 0; j <= nw; j++)
                if (t - j >= 1) sum += Tm->Omega[i][j] * v[t - j];
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
    char  line[MAXSTR];
    int   i, j;

    if (NULL == (f = fopen(filename, "r"))) {
        fprintf(stderr, "Error opening %s\n", filename);
        return 1;
    }

    /* Skip 5 header lines */
    for (i = 0; i < 5; i++) fgets(line, MAXSTR, f);

    Tm->residuals = (char *)malloc(MAXSTR);

    /* ── Frequency ── */
    fgets(line, MAXSTR, f);  /* comment */
    fgets(line, MAXSTR, f);
    if (strstr(line, "number") || strstr(line, "Number"))
        { Ts->freq = 1; Ts->numbering = 1; }
    else
        { sscanf(line, "%u", &Ts->freq); Ts->numbering = 0; }

    /* ── nobs, dates, name ── */
    fgets(line, MAXSTR, f);  /* comment */
    fgets(line, MAXSTR, f);
    {
        char namef[80]; int outyear;
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

    Ts->data = vector(1, Ts->nobs);

    /* ── NdetVar ──  (puerto fiel de fue.c [3.2]: fgets cabecera + fscanf) */
    fgets(line, MAXSTR, f);                 /* cabecera */
    Tm->NdetVar = 0;
    fscanf(f, "%d\n", &Tm->NdetVar);

    *DataMat = matrix(0, Tm->NdetVar, 1, Ts->nobs);

    if (Tm->NdetVar > 0) {

        /* ── [3.2.0] Nombres de las deterministas y generación de DataMat ──
           fue lee el tipo con fscanf("%s") y a continuación sus argumentos.
           Aquí se lee la línea completa y se delega en gen_detvar.          */
        fgets(line, MAXSTR, f);             /* "**" */

        Tm->detspec = (char **)malloc((size_t)Tm->NdetVar * sizeof(char *)) - 1;

        for (i = 1; i <= Tm->NdetVar; i++) {
            fgets(line, MAXSTR, f);
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
        fgets(line, MAXSTR, f);             /* cabecera "**" */
        for (i = 1; i <= Tm->NdetVar; i++)
            if (fscanf(f, "%d", &Tm->Nomega[i]) != 1) Tm->Nomega[i] = 0;
        fscanf(f, "\n");

        for (i = 1; i <= Tm->NdetVar; i++) {
            Tm->Omega[i] = vector(0, Tm->Nomega[i]);
            Tm->Imega[i] = ivector(0, Tm->Nomega[i]);
            fgets(line, MAXSTR, f);         /* "**" */
            for (j = 0; j <= Tm->Nomega[i]; j++) {
                if (fscanf(f, "%lf", &Tm->Omega[i][j]) != 1) Tm->Omega[i][j] = 0.0;
                if (fscanf(f, "%d\n", &Tm->Imega[i][j]) != 1) Tm->Imega[i][j] = 0;
            }
        }

        /* ── [3.2.3] Deltas: δ(B) de cada determinista (solo si Ndelta > 0) ── */
        fgets(line, MAXSTR, f);             /* cabecera "**" */
        for (i = 1; i <= Tm->NdetVar; i++)
            if (fscanf(f, "%d", &Tm->Ndelta[i]) != 1) Tm->Ndelta[i] = 0;
        fscanf(f, "\n");

        for (i = 1; i <= Tm->NdetVar; i++) {
            if (Tm->Ndelta[i] <= 0) continue;

            Tm->Delta[i] = vector(1, Tm->Ndelta[i]);
            Tm->Ielta[i] = ivector(1, Tm->Ndelta[i]);
            fgets(line, MAXSTR, f);         /* "**" */
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
    fgets(line, MAXSTR, f); fgets(line, MAXSTR, f);
    if (sscanf(line, "%lf %d", &Tm->mu, &Tm->Imu) < 2) {
        Tm->Imu = 0;   /* media fijada en el valor leído (típicamente 0) */
    }

    /* ── Box-Cox + diffs ── */
    fgets(line, MAXSTR, f); fgets(line, MAXSTR, f);
    sscanf(line, "%lf %d %d", &Tm->boxlam, &Tm->nrdiff, &Tm->nadiff);

    /* ── ifadf ── */
    if (Ts->freq > 1) {
        Tm->ifadf = ivector(0, Ts->freq / 2);
        fgets(line, MAXSTR, f); fgets(line, MAXSTR, f);
        { char *p = line; for (i = 0; i <= Ts->freq / 2; i++)
            { int off; sscanf(p, "%d%n", &Tm->ifadf[i], &off); p += off; } }
    }

    /* ── cbands + refactor ── */
    fgets(line, MAXSTR, f); fgets(line, MAXSTR, f);
    sscanf(line, "%lf %lf", &Tm->cbands, &Ts->refactor);
    if (Ts->refactor == 0.0) Ts->refactor = 1.0;

    /* ── Data section ── */
/* lectura de la serie */
    fgets(line, MAXSTR, f);  /* "** Time series..." */
    for (i = 1; i <= Ts->nobs; i++) {
        if (!fgets(line, MAXSTR, f)) break;
        sscanf(line, "%lf", &Ts->data[i]);
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
