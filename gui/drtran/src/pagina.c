/*
 * pagina.c -- la pagina del modelo: el grafico de los residuos y, debajo, el
 * modelo estimado y la tabla de los residuos de mas de tres desviaciones.
 *
 * ES LA PAGINA DE fue, con el mismo codigo (lib/equation/eqdraw.c): cada
 * ecuacion del caso se escribe exactamente como fue escribe un modelo
 * univariante, y lo unico que se anade es la TRANSFERENCIA, delante de los
 * deterministas:
 *
 *     ln Y_t = w(B)/d(B) B^b ln X_t + ... + deterministas + N_t
 *     phi(B) [ nabla N_t - mu ] = theta(B) A_t ;   sigma
 *
 * Cada termino de transferencia se escribe como fue escribe una variable
 * determinista con su omega(B)/delta(B), con la entrada en el sitio de xi, y
 * con el convenio de signo de Box-Jenkins (lib/equation/eqtran.h): el primer
 * omega suma y los demas restan. Lo pidio el analista: un formato distinto
 * para el modelo de transferencia --las tres ecuaciones de antes-- no tenia
 * sentido al lado del de fue.
 *
 * DE DONDE SALE CADA COSA. La FORMA, de los .pre cargados y de la red: los
 * mismos que se mandaron al motor. Los VALORES, de la tabla de parametros del
 * .out (lib/outdiag), buscandolos por el NOMBRE que les da lib/slots y en el
 * mismo orden en que los construye; un coeficiente que no esta en la tabla
 * --fijo en el .pre, o clavado desde la linea de ordenes-- se escribe con el
 * valor del .pre y sin d.t.
 *
 * LAS UNIDADES. El motor trabaja con refactor * BoxCox(y) (lib/prewhiten), y
 * aqui se deshace como hace fue en su ecuacion: un omega de la transferencia
 * se multiplica por refactor(X) / refactor(Y), los deterministas y la media se
 * dividen por refactor(Y). Las deltas y el ARMA no tienen unidades.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "gui.h"
#include "fugdraw.h"
#include "eqdraw.h"

#define POLY_MAX  64

typedef struct { double v, se; int has_se; } Co;

/* ------------------------------------------------------------------------ */
/* Los valores: la tabla del .out, por nombre                                */
/* ------------------------------------------------------------------------ */

typedef struct {
    const OdParams *par;
    char  visto[OD_MAX_PAR][64];   /* los nombres ya pedidos, con repeticion */
    int   nvisto;
} Valores;

/* La k-esima fila que se llama asi, con k las veces que ya se pidio: hay
 * nombres repetidos (dos factores regulares dan dos theta_5[B^1]) y la
 * tabla va en el orden de lib/slots, que es el que se sigue aqui.        */
static const OdPar *busca( Valores *V, const char *nombre )
{
    int i, ya = 0, k = 0;

    for (i = 0; i < V->nvisto; i++)
        if (!strcmp( V->visto[i], nombre )) ya++;
    if (V->nvisto < OD_MAX_PAR)
        snprintf( V->visto[V->nvisto++], sizeof V->visto[0], "%s", nombre );

    for (i = 0; V->par && i < V->par->n; i++)
        if (!strcmp( V->par->p[i].nombre, nombre ) && k++ == ya)
            return &V->par->p[i];
    return NULL;
}

/* El coeficiente que se llama nombre si se estimo (flag 1); si no, el del
 * .pre. escala pasa de las unidades del motor a las de la ecuacion.     */
static Co coef( Valores *V, int flag, double del_pre, double escala,
                const char *fmt, ... ) __attribute__((format(printf, 5, 6)));

static Co coef( Valores *V, int flag, double del_pre, double escala,
                const char *fmt, ... )
{
    Co           c = { del_pre * escala, 0.0, 0 };
    char         nombre[64];
    const OdPar *q;
    va_list      ap;

    if (flag != 1) return c;
    va_start( ap, fmt );
    vsnprintf( nombre, sizeof nombre, fmt, ap );
    va_end( ap );
    if (!(q = busca( V, nombre ))) return c;
    c.v      = q->valor * escala;
    c.se     = q->dt * fabs( escala );
    c.has_se = q->libre && q->dt > 0.0;
    return c;
}

/* ------------------------------------------------------------------------ */
/* El modelo de la ecuacion i, leido                                         */
/* ------------------------------------------------------------------------ */

/* Un polinomio en B con el convenio de Box-Jenkins: c0 - c1 B - c2 B^2 ... */
typedef struct {
    Co     lead;               /* omega0 (el primero de un numerador)        */
    Co     t[POLY_MAX];
    int    lag[POLY_MAX];
    int    n;
    int    anual;              /* un factor anual: B^s con s escrita         */
} Poly;

static int poly_vacio( const Poly *q )
{
    int i;

    for (i = 0; i < q->n; i++)
        if (q->t[i].v != 0.0 || q->t[i].has_se) return 0;
    return 1;
}

typedef struct {
    Poly         num, den;
    int          b;
    const Serie *x;
} Enlace;

typedef struct {
    Poly        num, den;
    char        tipo[32];
    int         mes, anio, k;
} Det;

typedef struct { Poly q; int f; } Fijo;

typedef struct {
    Poly q[3 * 16];
    Fijo f[16];
    int  nq, nf;
} Factores;

typedef struct {
    Enlace   e[NET_MAX_LINK];
    int      ne;
    Det      d[64];
    int      nd;
    Factores ar, ma;
    Co       mu;
    int      hay_mu;
    double   sigma;            /* -1 si no se sabe                           */
    int      es_log;
} ModeloEq;

/* Los factores de un lado del ruido: regulares, anuales y de frecuencia
 * fija, con los valores de phi_i[...] o theta_i[...].                    */
static void factores( Factores *F, Valores *V, const struct Tusmodel *t, int i,
                      int ar )
{
    const char *sym = ar ? "phi" : "theta";
    int   n1 = ar ? t->NumAr1 : t->NumMa1, n2 = ar ? t->NumAr2 : t->NumMa2;
    int   nf = ar ? t->NumAr1f : t->NumMa1f;
    int  *o1 = ar ? t->p1 : t->q1, *o2 = ar ? t->p2 : t->q2;
    real **c1 = ar ? t->Ar1 : t->Ma1, **c2 = ar ? t->Ar2 : t->Ma2;
    int  **f1 = ar ? t->Ia1 : t->Im1, **f2 = ar ? t->Ia2 : t->Im2;
    real **cf = ar ? t->Ar1f : t->Ma1f;
    int   *ff = ar ? t->Ia1f : t->Im1f, *fr = ar ? t->pfre1 : t->qfre1;
    int    k, j;

    memset( F, 0, sizeof *F );
    /* EL ORDEN ES EL DE lib/slots (add_arma_slots): regulares, anuales y
     * de frecuencia fija. Es el de la tabla, y busca() cuenta con el.   */
    for (k = 1; k <= n1 && F->nq < 48; k++) {
        Poly *q = &F->q[F->nq++];

        for (j = 1; j <= o1[k] && q->n < POLY_MAX; j++) {
            q->t[q->n]   = coef( V, f1[k][j], c1[k][j], 1.0, "%s_%d[B^%d]", sym, i, j );
            q->lag[q->n++] = j;
        }
    }
    for (k = 1; k <= n2 && F->nq < 48; k++) {
        Poly *q = &F->q[F->nq++];

        q->anual = 1;
        for (j = 1; j <= o2[k] && q->n < POLY_MAX; j++) {
            q->t[q->n]   = coef( V, f2[k][j], c2[k][j], 1.0, "%s_%d[B^%d]", sym, i,
                                 j * t->sper );
            q->lag[q->n++] = j * t->sper;
        }
    }
    for (k = 1; k <= nf && F->nf < 16; k++) {
        Fijo *x = &F->f[F->nf++];

        x->f = fr[k];
        if (fr[k] != 3) {               /* con f = 3 no hay termino en B   */
            x->q.t[x->q.n] = (Co){ cf[k][1], 0.0, 0 };
            x->q.lag[x->q.n++] = 1;
        }
        x->q.t[x->q.n] = coef( V, ff[k], cf[k][2], 1.0, "%s_%d[f=%d]", sym, i, fr[k] );
        x->q.lag[x->q.n++] = 2;
    }
}

static void arma( Mtram *m, int i, ModeloEq *M )
{
    Serie                 *Y = m->c.s[i - 1];
    const struct Tusmodel *t = &Y->tm;
    double                 rY = Y->ts.refactor > 0 ? Y->ts.refactor : 1.0;
    Valores               *V = g_new0( Valores, 1 );
    int                    j, k, iv;

    memset( M, 0, sizeof *M );
    V->par = m->dia.hay_par ? &m->dia.par : NULL;

    /* Los enlaces se numeran 1..n en el orden de la red, como en el .out;
     * hay que recorrerlos TODOS para que busca() cuente bien.           */
    for (j = 0; j < m->red.n; j++) {
        const NetLink *l = &m->red.lnk[j];
        Enlace        *e;
        Serie         *X;
        double         esc;

        if (l->out != i || M->ne >= NET_MAX_LINK) continue;
        if (l->inp < 1 || l->inp > m->c.n) continue;
        X   = m->c.s[l->inp - 1];
        esc = (X->ts.refactor > 0 ? X->ts.refactor : 1.0) / rY;
        e   = &M->e[M->ne++];
        e->x = X;
        e->b = l->b;
        e->num.lead = coef( V, 1, 0.0, esc, "omega%d[%d]", j + 1, 0 );
        for (k = 1; k <= l->s && k < POLY_MAX; k++) {
            e->num.t[e->num.n]   = coef( V, 1, 0.0, esc, "omega%d[%d]", j + 1, k );
            e->num.lag[e->num.n++] = k;
        }
        for (k = 1; k <= l->r && k < POLY_MAX; k++) {
            e->den.t[e->den.n]   = coef( V, 1, 0.0, 1.0, "delta%d[%d]", j + 1, k );
            e->den.lag[e->den.n++] = k;
        }
    }

    factores( &M->ar, V, t, i, 1 );
    factores( &M->ma, V, t, i, 0 );

    for (iv = 1; iv <= t->NdetVar && M->nd < 64; iv++) {
        Det  *d = &M->d[M->nd];
        char  tipo[32] = "";
        int   a = 0, b = 0, n;

        n = t->detspec && t->detspec[iv]
            ? sscanf( t->detspec[iv], "%31s %d %d", tipo, &a, &b ) : 0;
        if (n < 1) snprintf( tipo, sizeof tipo, "non-standard" );
        snprintf( d->tipo, sizeof d->tipo, "%s", tipo );
        if (!strcmp( tipo, "cos" ) || !strcmp( tipo, "sin" )) d->k = a;
        else if (!strcmp( tipo, "season" )) d->k = a;
        else if (n == 3) { d->mes = a; d->anio = b; }
        else if (n == 2) d->anio = a;

        d->num.lead = coef( V, t->Imega[iv][0], t->Omega[iv][0], 1.0 / rY,
                            "omega_d%d[%d,%d]", i, iv, 0 );
        for (k = 1; k <= t->Nomega[iv] && k < POLY_MAX; k++) {
            d->num.t[d->num.n]   = coef( V, t->Imega[iv][k], t->Omega[iv][k],
                                         1.0 / rY, "omega_d%d[%d,%d]", i, iv, k );
            d->num.lag[d->num.n++] = k;
        }
        for (k = 1; k <= t->Ndelta[iv] && k < POLY_MAX; k++) {
            d->den.t[d->den.n]   = coef( V, t->Ielta[iv][k], t->Delta[iv][k], 1.0,
                                         "delta_d%d[%d,%d]", i, iv, k );
            d->den.lag[d->den.n++] = k;
        }
        M->nd++;
    }

    /* La media va en el orden de lib/slots: despues de los deterministas */
    M->mu     = coef( V, t->Imu, t->mu, 1.0 / rY, "mu[%d]", i );
    M->hay_mu = t->Imu == 1 || t->mu != 0.0;

    /* sigma: la de los residuos de esta ecuacion, en las unidades de la
     * serie; en % si va en logaritmos, como fue.                       */
    M->es_log = fabs( t->boxlam ) < 1e-8;
    M->sigma  = -1.0;
    if (m->dia.hay_res && i - 1 < m->dia.res.m && m->dia.res.n > 1) {
        const double *r = m->dia.res.v[i - 1];
        double        s = 0.0, s2 = 0.0;
        int           n = m->dia.res.n;

        for (k = 0; k < n; k++) { s += r[k]; s2 += r[k] * r[k]; }
        s2 = (s2 - s * s / n) / n;
        if (s2 > 0.0) M->sigma = sqrt( s2 ) / rY * (M->es_log ? 100.0 : 1.0);
    }
    g_free( V );
}

/* ------------------------------------------------------------------------ */
/* El modelo, como la ecuacion de fue (lib/equation/equation.h)              */
/* ------------------------------------------------------------------------ */

/* omega(B) / delta(B) de un termino, como lo arma eq_build en fue: el signo
 * del termino es el de omega0, y cada omega siguiente lleva el suyo respecto
 * de ese (Box-Jenkins: restan).                                          */
static void termino( Equation *eq, EqPart part, const Poly *num, const Poly *den )
{
    double w0 = num->lead.v;
    int    k;

    eq_add_op( eq, part, w0 < 0.0 ? '-' : '+' );
    eq->item[part][eq->n[part] - 1].a = (w0 == 0.0);
    if (num->n > 0) eq_add_open( eq, part, '(', 0 );
    eq_add_coef( eq, part, CS_OMEGA, ' ', w0, num->lead.has_se, num->lead.se );
    for (k = 0; k < num->n; k++) {
        const Co *c = &num->t[k];

        eq_add_coef( eq, part, CS_OMEGA, ((w0 > 0.0) == (c->v > 0.0)) ? '-' : '+',
                     c->v, c->has_se, c->se );
        eq_add_b( eq, part, num->lag[k], 0 );
    }
    if (num->n > 0) eq_add_close( eq, part, ')' );
    if (den->n > 0 && !poly_vacio( den )) {
        eq_add_op( eq, part, '/' );
        eq_add_open( eq, part, '(', 0 );
        eq_add_text( eq, part, "1" );
        for (k = 0; k < den->n; k++) {
            const Co *c = &den->t[k];

            eq_add_coef( eq, part, CS_DELTA, c->v > 0.0 ? '-' : '+', c->v,
                         c->has_se, c->se );
            eq_add_b( eq, part, den->lag[k], 0 );
        }
        eq_add_close( eq, part, ')' );
    }
}

static void variable( Equation *eq, const Det *d, int freq )
{
    EqItem *it;

    if (!strcmp( d->tipo, "impulse" ) || !strcmp( d->tipo, "compimp" ) ||
        !strcmp( d->tipo, "step" ) || !strcmp( d->tipo, "ramp" )) {
        it = eq_add( eq, EQ_DETER, EI_DUMMY );
        snprintf( it->text, sizeof it->text, "%s",
                  !strcmp( d->tipo, "impulse" ) ? "I" :
                  !strcmp( d->tipo, "compimp" ) ? "CI" :
                  !strcmp( d->tipo, "step" )    ? "S" : "R" );
        it->a = freq == 1 ? 0 : d->mes;
        it->b = freq == 1 && d->anio == 0 ? d->mes : d->anio;
    } else if (!strcmp( d->tipo, "time" ))
        eq_add_text( eq, EQ_DETER, "t" );
    else if (!strcmp( d->tipo, "season" )) {
        it = eq_add( eq, EQ_DETER, EI_DUMMY );
        snprintf( it->text, sizeof it->text, "season" );
        it->a = d->k;
    } else if (!strcmp( d->tipo, "cos" ) || !strcmp( d->tipo, "sin" )) {
        it = eq_add( eq, EQ_DETER, EI_HARMONIC );
        snprintf( it->text, sizeof it->text, "%s", d->tipo );
        it->value = d->k;
        eq_harmonic_angle( d->k, freq, &it->a, &it->b );
    } else if (!strcmp( d->tipo, "alter" ))
        eq_add( eq, EQ_DETER, EI_ALTER );
    else if (!strcmp( d->tipo, "non-standard" ))
        eq_add( eq, EQ_DETER, EI_DUMMY );
    else {                                   /* easter, trend: su nombre */
        it = eq_add( eq, EQ_DETER, EI_DUMMY );
        snprintf( it->text, sizeof it->text, "%s", d->tipo );
    }
}

static void lado( Equation *eq, const Factores *F, EqPart reg, EqPart anu, EqPart fij )
{
    int k, j;

    for (k = 0; k < F->nq; k++) {
        const Poly *q = &F->q[k];
        EqPart      part = q->anual ? anu : reg;

        if (poly_vacio( q )) continue;
        eq_add_open( eq, part, '(', 0 );
        eq_add_text( eq, part, "1" );
        for (j = 0; j < q->n; j++) {
            eq_add_coef( eq, part, CS_PHI, q->t[j].v > 0.0 ? '-' : '+', q->t[j].v,
                         q->t[j].has_se, q->t[j].se );
            eq_add_b( eq, part, q->lag[j], q->anual );
        }
        eq_add_close( eq, part, ')' );
    }
    for (k = 0; k < F->nf; k++) {
        const Fijo *x = &F->f[k];

        eq_add_open( eq, fij, 'f', x->f );
        eq_add_text( eq, fij, "1" );
        for (j = 0; j < x->q.n; j++) {
            eq_add_coef( eq, fij, CS_PHIF, x->q.t[j].v > 0.0 ? '-' : '+', x->q.t[j].v,
                         x->q.t[j].has_se, x->q.t[j].se );
            eq_add_b( eq, fij, x->q.lag[j], 0 );
        }
        eq_add_close( eq, fij, ')' );
    }
}

static void ecuacion( Mtram *m, int i, const ModeloEq *M, Equation *eq )
{
    const Serie           *Y = m->c.s[i - 1];
    const struct Tusmodel *t = &Y->tm;
    double                 lam = t->boxlam;
    int                    k, freq = t->sper > 0 ? t->sper : Y->ts.freq;

    snprintf( eq->name, sizeof eq->name, "%s", Y->ts.name );
    snprintf( eq->residuals, sizeof eq->residuals, "A%s", Y->ts.name );
    eq->is_log = fabs( lam ) < 1e-8;
    eq->boxcox = !eq->is_log && fabs( lam - 1.0 ) > 1e-8;
    eq->lambda = lam;
    eq->freq   = m->dia.res.freq > 0 ? m->dia.res.freq : Y->ts.freq;
    eq->nobs   = m->dia.res.n;
    eq->sigma_pct = M->es_log;
    eq->sigma  = M->sigma > 0.0 ? M->sigma : 0.0;

    /* 1 · la transferencia, delante de los deterministas */
    for (k = 0; k < M->ne; k++) {
        const Enlace *e = &M->e[k];
        double        lx = e->x->tm.boxlam;
        EqItem       *it;

        termino( eq, EQ_TRANS, &e->num, &e->den );
        if (e->b > 0) eq_add_b( eq, EQ_TRANS, e->b, 0 );
        it = eq_add( eq, EQ_TRANS, EI_INPUT );
        snprintf( it->text, sizeof it->text, "%s", e->x->ts.name );
        it->a = fabs( lx ) < 1e-8 ? 1 : fabs( lx - 1.0 ) > 1e-8 ? 2 : 0;
        it->value = lx;
    }

    /* 2 · los deterministas, y N_t */
    if (M->nd > 0) {
        eq_add_text( eq, EQ_DETER, eq->name );
        eq_add_op( eq, EQ_DETER, '=' );
        for (k = 0; k < M->nd; k++) {
            termino( eq, EQ_DETER, &M->d[k].num, &M->d[k].den );
            variable( eq, &M->d[k], freq );
        }
        eq_add_op( eq, EQ_DETER, '+' );
        eq_add_text( eq, EQ_DETER, "N" );
    }

    /* 3 · el ruido */
    lado( eq, &M->ar, EQ_ARR, EQ_ARA, EQ_ARF );
    lado( eq, &M->ma, EQ_MAR, EQ_MAA, EQ_MAF );
    if (M->hay_mu)
        eq_add_coef( eq, EQ_MU, CS_MU, M->mu.v > 0.0 ? '-' : '+', M->mu.v,
                     M->mu.has_se, M->mu.se );
    if (t->nrdiff > 0) eq_add( eq, EQ_NRDIFF, EI_NABLA )->a = t->nrdiff;
    if (t->nadiff > 0) eq_add( eq, EQ_NADIFF, EI_NABLA )->b = t->nadiff * t->sper;
    else if (t->ifadf && (t->sper == 4 || t->sper == 12)) {
        for (k = 0; k <= t->sper / 2; k++) {
            EqItem *it;

            if (t->ifadf[k] <= 0) continue;
            if (k == 0) { eq_add( eq, EQ_IFADF, EI_NABLA )->a = 1; continue; }
            it = eq_add( eq, EQ_IFADF, EI_OPEN );
            it->bracket = 'i';
            it->a = k;
            it->b = t->sper;
        }
    }
}

/* ------------------------------------------------------------------------ */

typedef struct { const OdResiduos *res; } Fechas;

static void fecha_de( int k, char *buf, size_t size, void *data )
{
    const Fechas *F = data;

    snprintf( buf, size, "%s", F->res->fecha[k] );
}

int pagina_modelo( Mtram *m, int i, FDFig *grafico, const char *pdf,
                   char *why, size_t n )
{
    ModeloEq     *M;
    Equation    eq;
    EqOutlier  *at = NULL;
    Fechas      F;
    int         nat = 0, rc, fechas = 0;

    if (i < 1 || i > m->c.n) {
        snprintf( why, n, "no hay ecuación %d", i );
        return 1;
    }
    if (!m->dia.hay_par) {
        snprintf( why, n, "el .out no trae la tabla de parámetros" );
        return 1;
    }
    if (eq_new( &eq )) {
        snprintf( why, n, "sin memoria" );
        return 1;
    }

    M = g_new0( ModeloEq, 1 );
    arma( m, i, M );
    ecuacion( m, i, M, &eq );
    g_free( M );

    /* los residuos de mas de tres desviaciones, con su fecha */
    if (m->dia.hay_res && i - 1 < m->dia.res.m && m->dia.res.n > 1) {
        const double *r = m->dia.res.v[i - 1];
        int           k, N = m->dia.res.n;
        double        s = 0.0, s2 = 0.0, media, dt;

        for (k = 0; k < N; k++) s += r[k];
        media = s / N;
        for (k = 0; k < N; k++) s2 += (r[k] - media) * (r[k] - media);
        dt = sqrt( s2 / N );
        F.res  = &m->dia.res;
        fechas = m->dia.res.fecha[0][0] != '\0';
        at     = g_new0( EqOutlier, N );
        nat    = eq_outliers( r, N, media, dt, fechas ? fecha_de : NULL, &F, at, N );
    }

    rc = eq_page_pdf( pdf, grafico, &eq, at, nat, fechas );
    g_free( at );
    eq_release( &eq );
    if (rc) snprintf( why, n, "no pude escribir %s", pdf );
    return rc;
}
