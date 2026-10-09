/*
 * pagina.c -- la pagina del modelo: el grafico de los residuos y, debajo, el
 * modelo estimado, como la que fue deja en su PDF.
 *
 * TRES ECUACIONES Y NO UNA, que es como lo pidio el analista:
 *
 *     ln Y_t = SUM_j [ w_j(B) / d_j(B) ] B^bj ln X_j,t  +  D_t  +  N_t
 *     phi(B) ( nabla N_t - mu ) = theta(B) a_t ;   sigma_a = x %
 *     D_t   = los deterministas
 *
 * La transferencia va EN NIVEL --en el de la serie transformada--, que es
 * como se piensa el efecto de una entrada; el ruido lleva toda la parte
 * estocastica, con sus diferencias. Escrita de una vez, como fue, la
 * transferencia quedaba enterrada dentro del corchete de las diferencias.
 *
 * LA D.T. VA DEBAJO DE SU COEFICIENTE, cada una centrada bajo el suyo, y en
 * una fraccion la raya llega hasta el mas ancho de numerador y denominador.
 * Los armonicos se escriben con un sumatorio: diez coeficientes no dicen nada
 * a simple vista y estan en la tabla del .out.
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
 *
 * Letra recta, como el fue original y como la pagina de fue desde que el
 * analista lo pidio.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "gui.h"
#include "fugdraw.h"

#define F_NUM   FD_HELV
#define F_VAR   FD_HELV
#define F_ROMAN FD_TIMES
#define F_GREEK FD_SYMBOL

#define SYM_PI    "p"
#define SYM_SIGMA "s"
#define SYM_SUM   "\345"       /* Symbol: el sumatorio                       */
#define SYM_ALPHA "a"
#define SYM_BETA  "b"
#define SYM_XI    "x"
#define SYM_RADICAL "\326"

#define SUP_RISE  0.42
#define SUB_RISE -0.22
#define SMALL     0.72
#define SE_DROP   1.05         /* de la linea del valor a la de su d.t.      */

#define POLY_MAX  64

/* ------------------------------------------------------------------------ */
/* La pluma: dibuja de izquierda a derecha, o solo mide si f es NULL         */
/* ------------------------------------------------------------------------ */

typedef struct {
    FDFig *f;
    double x, y, size;
    double left, right, indent, leading;
} Pen;

static void put( Pen *p, int font, double size, double rise, const char *s )
{
    if (p->f) fd_text( p->f, p->x, p->y + rise, font, size, FD_LEFT, s );
    p->x += fd_text_width( font, size, s );
}

static void space( Pen *p, double em ) { p->x += em * p->size; }

static void scripts( Pen *p, int font, const char *base, const char *sub,
                     const char *sup )
{
    double sz = p->size, sm = SMALL * sz, ws, wp, x;

    put( p, font, sz, 0.0, base );
    x  = p->x;
    ws = sub ? fd_text_width( F_VAR, sm, sub ) : 0.0;
    wp = sup ? fd_text_width( F_NUM, sm, sup ) : 0.0;
    if (p->f) {
        if (sub) fd_text( p->f, x, p->y + SUB_RISE * sz, F_VAR, sm, FD_LEFT, sub );
        if (sup) fd_text( p->f, x, p->y + SUP_RISE * sz, F_NUM, sm, FD_LEFT, sup );
    }
    p->x = x + (ws > wp ? ws : wp);
}

static void op( Pen *p, const char *s, double em )
{
    space( p, em );
    put( p, F_ROMAN, p->size, 0.0, s );
    space( p, em );
}

/* ------------------------------------------------------------------------ */
/* Los coeficientes                                                          */
/* ------------------------------------------------------------------------ */

typedef struct { double v, se; int has_se; } Co;

enum { K_OMEGA, K_DELTA, K_PHI };

/* Los decimales, por el tamano, como fue (eq_decimals): que un coeficiente
 * pequeno no salga 0.00.                                                  */
static int decimales( double v, int kind )
{
    v = fabs( v );
    if (kind == K_PHI) return 2;
    if (v >= .10) return 2;
    if (v >= .01) return 3;
    if (kind == K_DELTA || v >= .001) return 4;
    if (v >= .0001) return 5;
    return 6;
}

/* El valor y, debajo y centrada, su d.t. El valor va como se le pase:
 * con el signo delante si es el primero de su polinomio.                 */
static void apilado( Pen *p, double v, const Co *c, int kind )
{
    char   top[64], bot[64];
    double sz = p->size, wt, wb, w;
    int    d = decimales( c->v, kind );

    snprintf( top, sizeof top, "%.*f", d, v );
    snprintf( bot, sizeof bot, "(%.*f)", d, c->se );
    wt = fd_text_width( F_NUM, sz, top );
    wb = c->has_se ? fd_text_width( F_NUM, sz, bot ) : 0.0;
    w  = wt > wb ? wt : wb;
    if (p->f) {
        fd_text( p->f, p->x + (w - wt) / 2.0, p->y, F_NUM, sz, FD_LEFT, top );
        if (c->has_se)
            fd_text( p->f, p->x + (w - wb) / 2.0, p->y - SE_DROP * sz, F_NUM, sz,
                     FD_LEFT, bot );
    }
    p->x += w;
}

static void pon_b( Pen *p, int lag )
{
    char e[16];

    space( p, 0.08 );
    if (lag == 1) { put( p, F_VAR, p->size, 0.0, "B" ); return; }
    snprintf( e, sizeof e, "%d", lag );
    scripts( p, F_VAR, "B", NULL, e );
}

/* ------------------------------------------------------------------------ */
/* Los polinomios en B, con el convenio de Box-Jenkins:                      */
/*     c0 - c1 B - c2 B^2 ...     el primero suma, el resto restan           */
/* ------------------------------------------------------------------------ */

typedef struct {
    int    uno;                /* el primero es "1" (AR, MA, delta)         */
    Co     lead;               /* si no: el primer coeficiente, omega0      */
    Co     t[POLY_MAX];
    int    lag[POLY_MAX];
    int    n;
    int    kind;
} Poly;

static int poly_vacio( const Poly *q )
{
    int i;

    for (i = 0; i < q->n; i++)
        if (q->t[i].v != 0.0 || q->t[i].has_se) return 0;
    return 1;
}

static void pon_poly( Pen *p, const Poly *q, int parentesis )
{
    int i;

    if (parentesis) put( p, F_ROMAN, p->size, 0.0, "(" );
    if (q->uno) put( p, F_NUM, p->size, 0.0, "1" );
    else        apilado( p, q->lead.v, &q->lead, q->kind );
    for (i = 0; i < q->n; i++) {
        const Co *c = &q->t[i];

        if (c->v == 0.0 && !c->has_se) continue;     /* un cero fijo: nada */
        op( p, c->v > 0.0 ? "-" : "+", 0.22 );
        apilado( p, fabs( c->v ), c, q->kind );
        pon_b( p, q->lag[i] );
    }
    if (parentesis) put( p, F_ROMAN, p->size, 0.0, ")" );
}

static double ancho_poly( const Pen *p, const Poly *q, int parentesis )
{
    Pen m = *p;

    m.f = NULL;
    m.x = 0.0;
    pon_poly( &m, q, parentesis );
    return m.x;
}

/* num / den: cada uno con sus d.t. debajo, centrados, y la raya hasta el mas
 * ancho de los dos.                                                     */
#define FR_NUM  1.75           /* linea de valores del numerador, sobre y    */
#define FR_BAR  0.30           /* la raya                                    */
#define FR_DEN -0.80           /* linea de valores del denominador           */

static void fraccion( Pen *p, const Poly *num, const Poly *den )
{
    double sz = p->size, wn, wd, w, pad = 0.15 * sz, x0 = p->x;
    Pen    q = *p;

    wn = ancho_poly( p, num, 0 );
    wd = ancho_poly( p, den, 0 );
    w  = (wn > wd ? wn : wd) + 2.0 * pad;

    q.x = x0 + (w - wn) / 2.0;
    q.y = p->y + FR_NUM * sz;
    pon_poly( &q, num, 0 );

    q.x = x0 + (w - wd) / 2.0;
    q.y = p->y + FR_DEN * sz;
    pon_poly( &q, den, 0 );

    if (p->f) {
        fd_linewidth( p->f, 0.05 * sz );
        fd_line( p->f, x0, p->y + FR_BAR * sz, x0 + w, p->y + FR_BAR * sz );
    }
    p->x = x0 + w;
}

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
/* Una serie, transformada: ln X_t, X_t o X_t^(lambda)                       */
/* ------------------------------------------------------------------------ */

static void pon_serie( Pen *p, const Serie *s )
{
    char   l[32];
    double lam = s->tm.boxlam;

    if (fabs( lam ) < 1e-8) {
        put( p, F_ROMAN, p->size, 0.0, "ln" );
        space( p, 0.2 );
        scripts( p, F_VAR, s->ts.name, "t", NULL );
    } else if (fabs( lam - 1.0 ) < 1e-8)
        scripts( p, F_VAR, s->ts.name, "t", NULL );
    else {
        snprintf( l, sizeof l, "(%.2f)", lam );
        scripts( p, F_VAR, s->ts.name, "t", l );
    }
}

/* ------------------------------------------------------------------------ */
/* Las lineas se parten entre terminos, no dentro de uno                     */
/* ------------------------------------------------------------------------ */

typedef void (*Dibuja)( Pen *, const void * );

static double ancho( const Pen *p, Dibuja d, const void *arg )
{
    Pen m = *p;

    m.f = NULL;
    m.x = 0.0;
    d( &m, arg );
    return m.x;
}

/* Un termino precedido de su operador; se parte la linea antes si no cabe */
static void termino( Pen *p, const char *sg, Dibuja d, const void *arg )
{
    double w = ancho( p, d, arg ) + 1.0 * p->size;

    if (p->x > p->left + p->indent && p->x + w > p->right) {
        p->x  = p->left + p->indent;
        p->y -= p->leading;
    }
    if (sg) op( p, sg, 0.3 );
    d( p, arg );
}

/* ------------------------------------------------------------------------ */
/* 1 · La transferencia                                                      */
/* ------------------------------------------------------------------------ */

typedef struct {
    Poly         num, den;
    int          b;
    const Serie *x;
} Enlace;

static void pon_enlace( Pen *p, const void *arg )
{
    const Enlace *e = arg;

    if (e->den.n > 0 && !poly_vacio( &e->den )) fraccion( p, &e->num, &e->den );
    else pon_poly( p, &e->num, e->num.n > 0 );
    if (e->b > 0) pon_b( p, e->b );
    space( p, 0.15 );
    pon_serie( p, e->x );
}

static void pon_letra( Pen *p, const void *arg )
{
    scripts( p, F_VAR, (const char *) arg, "t", NULL );
}

/* ------------------------------------------------------------------------ */
/* 3 · Los deterministas                                                     */
/* ------------------------------------------------------------------------ */

typedef struct {
    Poly        num, den;
    char        tipo[32];
    int         mes, anio, k;
    int         sumatorio;     /* los armonicos, de una vez                  */
    int         j0, j1, s;
} Det;

static void pon_variable( Pen *p, const Det *d )
{
    char sup[64];

    if (!strcmp( d->tipo, "alter" )) {
        put( p, F_ROMAN, p->size, 0.0, "(-1)" );
        scripts( p, F_ROMAN, "", NULL, "t" );
    } else if (!strcmp( d->tipo, "time" ) || !strcmp( d->tipo, "trend" )) {
        space( p, 0.1 );
        put( p, F_VAR, p->size, 0.0, "t" );
    } else if (!strcmp( d->tipo, "cos" ) || !strcmp( d->tipo, "sin" )) {
        char a[48];

        space( p, 0.1 );
        put( p, F_ROMAN, p->size, 0.0, d->tipo );
        put( p, F_ROMAN, p->size, 0.0, "(2" );
        put( p, F_GREEK, p->size, 0.0, SYM_PI );
        if (d->k == 1) snprintf( a, sizeof a, "t/%d)", d->s );
        else           snprintf( a, sizeof a, "%dt/%d)", d->k, d->s );
        put( p, F_ROMAN, p->size, 0.0, a );
    } else if (!strcmp( d->tipo, "season" )) {
        space( p, 0.1 );
        snprintf( sup, sizeof sup, "%d", d->k );
        scripts( p, F_GREEK, SYM_XI, "t", sup );
    } else if (d->anio > 0) {
        const char *L = !strcmp( d->tipo, "impulse" ) ? "I" :
                        !strcmp( d->tipo, "compimp" ) ? "C" :
                        !strcmp( d->tipo, "step" )    ? "S" :
                        !strcmp( d->tipo, "ramp" )    ? "R" : d->tipo;

        space( p, 0.1 );
        if (d->mes > 0) snprintf( sup, sizeof sup, "%s,%d/%d", L, d->mes, d->anio );
        else            snprintf( sup, sizeof sup, "%s,%d", L, d->anio );
        scripts( p, F_GREEK, SYM_XI, "t", sup );
    } else if (!strcmp( d->tipo, "non-standard" )) {
        space( p, 0.1 );
        scripts( p, F_GREEK, SYM_XI, "t", NULL );
    } else {
        space( p, 0.1 );
        put( p, F_ROMAN, p->size, 0.0, d->tipo );
    }
}

/* Sigma_{j=j0}^{j1} [ alpha_j cos(2 pi j t/s) + beta_j sin(2 pi j t/s) ] */
static void pon_sumatorio( Pen *p, const Det *d )
{
    double sz = p->size, x0;
    char   lo[16], hi[16], a[16];

    snprintf( lo, sizeof lo, "j=%d", d->j0 );
    snprintf( hi, sizeof hi, "%d", d->j1 );
    x0 = p->x;
    put( p, F_GREEK, 1.25 * sz, -0.12 * sz, SYM_SUM );
    if (p->f) {
        double c = (x0 + p->x) / 2.0;

        fd_text( p->f, c, p->y - 1.05 * sz, F_NUM, SMALL * sz, FD_CENTER, lo );
        fd_text( p->f, c, p->y + 1.00 * sz, F_NUM, SMALL * sz, FD_CENTER, hi );
    }
    space( p, 0.2 );
    put( p, F_ROMAN, sz, 0.0, "[" );
    snprintf( a, sizeof a, "jt/%d)", d->s );

    scripts( p, F_GREEK, SYM_ALPHA, "j", NULL );
    space( p, 0.15 );
    put( p, F_ROMAN, sz, 0.0, "cos(2" );
    put( p, F_GREEK, sz, 0.0, SYM_PI );
    put( p, F_ROMAN, sz, 0.0, a );
    op( p, "+", 0.25 );
    scripts( p, F_GREEK, SYM_BETA, "j", NULL );
    space( p, 0.15 );
    put( p, F_ROMAN, sz, 0.0, "sin(2" );
    put( p, F_GREEK, sz, 0.0, SYM_PI );
    put( p, F_ROMAN, sz, 0.0, a );
    put( p, F_ROMAN, sz, 0.0, "]" );
}

static void pon_det( Pen *p, const void *arg )
{
    const Det *d = arg;

    if (d->sumatorio) { pon_sumatorio( p, d ); return; }
    if (d->den.n > 0 && !poly_vacio( &d->den )) fraccion( p, &d->num, &d->den );
    else pon_poly( p, &d->num, d->num.n > 0 );
    pon_variable( p, d );
}

/* ------------------------------------------------------------------------ */
/* 2 · El ruido                                                              */
/* ------------------------------------------------------------------------ */

/* Un factor de la diferencia anual, (1 - sqrt3 B + B^2), con su f debajo:
 * el mismo dibujo que la pagina de fue (report.c, draw_ifadf_factor).   */
static void pon_ifadf( Pen *p, int i, int freq )
{
    double sz = p->size, x0 = p->x;
    char   f[32];

    if (i == 0) { put( p, F_GREEK, sz, 0.0, FD_SYM_NABLA ); return; }
    put( p, F_ROMAN, sz, 0.0, "(" );
    put( p, F_NUM, sz, 0.0, "1" );
    if (freq == 4) {
        op( p, "+", 0.2 );
        if (i == 1) scripts( p, F_VAR, "B", NULL, "2" );
        else put( p, F_VAR, sz, 0.0, "B" );
    } else if (i == 6) {
        op( p, "+", 0.2 );
        put( p, F_VAR, sz, 0.0, "B" );
    } else {
        if (i != 3) {
            op( p, i < 3 ? "-" : "+", 0.2 );
            if (i == 1 || i == 5) {
                double w = fd_text_width( F_NUM, sz, "3" );

                put( p, F_GREEK, sz, 0.0, SYM_RADICAL );
                if (p->f) {
                    fd_linewidth( p->f, 0.045 * sz );
                    fd_line( p->f, p->x, p->y + 0.72 * sz, p->x + w, p->y + 0.72 * sz );
                }
                put( p, F_NUM, sz, 0.0, "3" );
            }
            put( p, F_VAR, sz, 0.0, "B" );
        }
        op( p, "+", 0.2 );
        scripts( p, F_VAR, "B", NULL, "2" );
    }
    put( p, F_ROMAN, sz, 0.0, ")" );
    snprintf( f, sizeof f, "f = %d", i );
    if (p->f)
        fd_text( p->f, (x0 + p->x) / 2.0, p->y - 2.05 * sz, F_NUM, SMALL * sz,
                 FD_CENTER, f );
}

typedef struct { Poly q; int f; } Fijo;

/* Un factor de frecuencia fija, (1 - c1 B - c2 B^2) con "f = k" debajo */
static void pon_fijo( Pen *p, const void *arg )
{
    const Fijo *F = arg;
    double      x0 = p->x;
    char        f[32];

    pon_poly( p, &F->q, 1 );
    snprintf( f, sizeof f, "f = %d", F->f );
    if (p->f)
        fd_text( p->f, (x0 + p->x) / 2.0, p->y - 2.05 * p->size, F_NUM,
                 SMALL * p->size, FD_CENTER, f );
}

static void pon_factor( Pen *p, const void *arg ) { pon_poly( p, arg, 1 ); }

/* Los factores de un lado del ruido: regulares, anuales y de frecuencia
 * fija, con los valores de phi_i[...] o theta_i[...].                    */
typedef struct {
    Poly q[3 * 16];
    Fijo f[16];
    int  nq, nf;
} Factores;

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

        q->uno = 1; q->kind = K_PHI;
        for (j = 1; j <= o1[k] && q->n < POLY_MAX; j++) {
            q->t[q->n]   = coef( V, f1[k][j], c1[k][j], 1.0, "%s_%d[B^%d]", sym, i, j );
            q->lag[q->n++] = j;
        }
    }
    for (k = 1; k <= n2 && F->nq < 48; k++) {
        Poly *q = &F->q[F->nq++];

        q->uno = 1; q->kind = K_PHI;
        for (j = 1; j <= o2[k] && q->n < POLY_MAX; j++) {
            q->t[q->n]   = coef( V, f2[k][j], c2[k][j], 1.0, "%s_%d[B^%d]", sym, i,
                                 j * t->sper );
            q->lag[q->n++] = j * t->sper;
        }
    }
    for (k = 1; k <= nf && F->nf < 16; k++) {
        Fijo *x = &F->f[F->nf++];

        x->f = fr[k];
        x->q.uno = 1; x->q.kind = K_PHI;
        if (fr[k] != 3) {               /* con f = 3 no hay termino en B   */
            x->q.t[x->q.n] = (Co){ cf[k][1], 0.0, 0 };
            x->q.lag[x->q.n++] = 1;
        }
        x->q.t[x->q.n] = coef( V, ff[k], cf[k][2], 1.0, "%s_%d[f=%d]", sym, i, fr[k] );
        x->q.lag[x->q.n++] = 2;
    }
}

static void pon_factores( Pen *p, const Factores *F )
{
    int k;

    for (k = 0; k < F->nq; k++)
        if (!poly_vacio( &F->q[k] )) termino( p, NULL, pon_factor, &F->q[k] );
    for (k = 0; k < F->nf; k++)
        termino( p, NULL, pon_fijo, &F->f[k] );
}

/* Algun factor de la diferencia anual que no sea nabla: lleva su f debajo */
static int hay_ifadf( const struct Tusmodel *t )
{
    int i, nf = t->sper == 12 ? 7 : t->sper == 4 ? 3 : 0;

    if (t->nadiff > 0 || !t->ifadf) return 0;
    for (i = 1; i < nf; i++)
        if (t->ifadf[i] == 1) return 1;
    return 0;
}

typedef struct {
    const struct Tusmodel *t;
    Co   mu;
    int  hay_mu;
} Dif;

/* ( nabla^d nabla_s^D N_t - mu ) */
static void pon_dif( Pen *p, const void *arg )
{
    const Dif             *d = arg;
    const struct Tusmodel *t = d->t;
    double                 sz = p->size;
    char                   a[16], b[16];
    int                    i, nf = t->sper == 12 ? 7 : t->sper == 4 ? 3 : 0;

    if (d->hay_mu) put( p, F_ROMAN, sz, 0.0, "(" );
    if (t->nrdiff > 0) {
        snprintf( a, sizeof a, "%d", t->nrdiff );
        scripts( p, F_GREEK, FD_SYM_NABLA, NULL, t->nrdiff > 1 ? a : NULL );
    }
    if (t->nadiff > 0) {
        snprintf( a, sizeof a, "%d", t->sper );
        snprintf( b, sizeof b, "%d", t->nadiff );
        scripts( p, F_GREEK, FD_SYM_NABLA, a, t->nadiff > 1 ? b : NULL );
    } else if (t->ifadf)
        for (i = 0; i < nf; i++)
            if (t->ifadf[i] == 1) pon_ifadf( p, i, t->sper );
    space( p, 0.1 );
    scripts( p, F_VAR, "N", "t", NULL );
    if (d->hay_mu) {
        op( p, d->mu.v < 0.0 ? "+" : "-", 0.22 );
        apilado( p, fabs( d->mu.v ), &d->mu, K_OMEGA );
        put( p, F_ROMAN, sz, 0.0, ")" );
    }
}

/* ------------------------------------------------------------------------ */
/* La ecuacion entera de la serie i, desde (x, y) hacia abajo                */
/* ------------------------------------------------------------------------ */

typedef struct {
    Enlace  e[NET_MAX_LINK];
    int     ne;
    Det     d[64];
    int     nd;
    Factores ar, ma;
    Dif     dif;
    double  sigma;             /* -1 si no se sabe                           */
    int     es_log;
} Modelo3;

static void arma( Mtram *m, int i, Modelo3 *M )
{
    Serie                 *Y = m->c.s[i - 1];
    const struct Tusmodel *t = &Y->tm;
    double                 rY = Y->ts.refactor > 0 ? Y->ts.refactor : 1.0;
    Valores               *V = g_new0( Valores, 1 );
    int                    j, k, iv, harm = 0;

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
        e->num.kind = K_OMEGA;
        e->num.lead = coef( V, 1, 0.0, esc, "omega%d[%d]", j + 1, 0 );
        for (k = 1; k <= l->s && k < POLY_MAX; k++) {
            e->num.t[e->num.n]   = coef( V, 1, 0.0, esc, "omega%d[%d]", j + 1, k );
            e->num.lag[e->num.n++] = k;
        }
        e->den.uno = 1; e->den.kind = K_DELTA;
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
        d->s = t->sper;
        if (!strcmp( tipo, "cos" ) || !strcmp( tipo, "sin" )) d->k = a;
        else if (!strcmp( tipo, "season" )) d->k = a;
        else if (n == 3) { d->mes = a; d->anio = b; }
        else if (n == 2) d->anio = a;

        d->num.kind = K_OMEGA;
        d->num.lead = coef( V, t->Imega[iv][0], t->Omega[iv][0], 1.0 / rY,
                            "omega_d%d[%d,%d]", i, iv, 0 );
        for (k = 1; k <= t->Nomega[iv] && k < POLY_MAX; k++) {
            d->num.t[d->num.n]   = coef( V, t->Imega[iv][k], t->Omega[iv][k],
                                         1.0 / rY, "omega_d%d[%d,%d]", i, iv, k );
            d->num.lag[d->num.n++] = k;
        }
        d->den.uno = 1; d->den.kind = K_DELTA;
        for (k = 1; k <= t->Ndelta[iv] && k < POLY_MAX; k++) {
            d->den.t[d->den.n]   = coef( V, t->Ielta[iv][k], t->Delta[iv][k], 1.0,
                                         "delta_d%d[%d,%d]", i, iv, k );
            d->den.lag[d->den.n++] = k;
        }
        if (d->k > 0 && (!strcmp( tipo, "cos" ) || !strcmp( tipo, "sin" ))) harm++;
        M->nd++;
    }

    /* MAS DE DOS ARMONICOS, CON UN SUMATORIO en el sitio del primero. Dos
     * --un cos y un sin-- se leen bien escritos.                        */
    if (harm > 2) {
        int primero = -1, j0 = 0, j1 = 0, w = 0;

        for (k = 0; k < M->nd; k++) {
            Det *d = &M->d[k];

            if (strcmp( d->tipo, "cos" ) && strcmp( d->tipo, "sin" )) {
                M->d[w++] = *d;
                continue;
            }
            if (primero < 0) { primero = w; M->d[w++] = *d; j0 = j1 = d->k; }
            if (d->k < j0) j0 = d->k;
            if (d->k > j1) j1 = d->k;
        }
        M->nd = w;
        M->d[primero].sumatorio = 1;
        M->d[primero].j0 = j0;
        M->d[primero].j1 = j1;
    }

    /* La media va en el orden de lib/slots: despues de los deterministas */
    M->dif.t      = t;
    M->dif.mu     = coef( V, t->Imu, t->mu, 1.0 / rY, "mu[%d]", i );
    M->dif.hay_mu = t->Imu == 1 || t->mu != 0.0;

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

static void pon_sigma( Pen *p, const Modelo3 *M )
{
    double sz = p->size;
    char   t[64];
    FDRun  r[2];

    put( p, F_ROMAN, sz, 0.0, ";" );
    space( p, 0.9 );
    r[0] = (FDRun){ F_GREEK, sz, 0.0, SYM_SIGMA, FD_ACC_HAT };
    r[1] = (FDRun){ F_VAR, SMALL * sz, SUB_RISE * sz, "a", FD_ACC_NONE };
    if (p->f) fd_runs( p->f, p->x, p->y, FD_LEFT, r, 2 );
    p->x += fd_runs_width( r, 2 );
    op( p, "=", 0.3 );
    snprintf( t, sizeof t, M->es_log ? "%.2f %%" : "%.4g", M->sigma );
    put( p, F_NUM, sz, 0.0, t );
}

/* El signo de un termino. Con UN solo coeficiente el signo sale fuera, para
 * que se lea "- 0.12 xi" y no "+ -0.12 xi"; con un polinomio el primero lo
 * lleva dentro del parentesis. Cambia num: se llama sobre una copia.    */
static const char *signo( Poly *num, int sumatorio, int primero )
{
    if (!sumatorio && num->n == 0 && num->lead.v < 0.0) {
        num->lead.v = -num->lead.v;
        return "-";
    }
    return primero ? NULL : "+";
}

/* Las tres ecuaciones. Devuelve la y de debajo de la ultima.              */
static double pon_modelo( Pen *p, Mtram *m, int i, const Modelo3 *M )
{
    double sz = p->size, alto_tr;
    int    k, hay_frac = 0;

    for (k = 0; k < M->ne; k++)
        if (M->e[k].den.n > 0) hay_frac = 1;
    for (k = 0; k < M->nd; k++)
        if (M->d[k].den.n > 0) hay_frac = 1;
    alto_tr = hay_frac ? 5.6 : 2.9;

    /* 1 · Y_t = transferencias + D_t + N_t */
    if (hay_frac) p->y -= 2.6 * sz;
    p->x = p->left;
    pon_serie( p, m->c.s[i - 1] );
    op( p, "=", 0.35 );
    p->indent  = p->x - p->left;
    p->leading = alto_tr * sz;
    for (k = 0; k < M->ne; k++) {
        Enlace c = M->e[k];

        termino( p, signo( &c.num, 0, k == 0 ), pon_enlace, &c );
    }
    if (M->nd > 0) termino( p, M->ne ? "+" : NULL, pon_letra, "D" );
    termino( p, M->ne || M->nd ? "+" : NULL, pon_letra, "N" );

    /* 2 · el ruido */
    p->y -= (hay_frac ? 2.4 : 0.0) * sz + 3.6 * sz;
    p->x = p->left;
    p->indent  = 0.0;
    p->leading = 2.9 * sz;
    pon_factores( p, &M->ar );
    termino( p, NULL, pon_dif, &M->dif );
    if (p->x + 2.0 * sz > p->right) { p->x = p->left; p->y -= p->leading; }
    op( p, "=", 0.35 );
    pon_factores( p, &M->ma );
    space( p, 0.1 );
    scripts( p, F_VAR, "a", "t", NULL );
    if (M->sigma > 0.0) {
        space( p, 0.4 );
        pon_sigma( p, M );
    }

    /* 3 · D_t */
    if (M->nd > 0) {
        /* las "f = k" de los factores cuelgan bajo la linea del ruido    */
        int etiquetas = M->ar.nf + M->ma.nf > 0 || hay_ifadf( M->dif.t );

        p->y -= (etiquetas ? 4.4 : 3.6) * sz + (hay_frac ? 2.4 * sz : 0.0);
        p->x = p->left;
        scripts( p, F_VAR, "D", "t", NULL );
        op( p, "=", 0.35 );
        p->indent  = p->x - p->left;
        p->leading = alto_tr * sz;
        for (k = 0; k < M->nd; k++) {
            Det c = M->d[k];

            termino( p, signo( &c.num, c.sumatorio, k == 0 ), pon_det, &c );
        }
    }
    return p->y - 2.0 * sz;
}

/* ------------------------------------------------------------------------ */

int pagina_modelo( Mtram *m, int i, FDFig *grafico, const char *pdf,
                   char *why, size_t n )
{
    const double A4C = 595.276, A4L = 841.89, margen = 36.0;
    double       PW, PH, size = 9.0, gw, gh, s, x[2], y[2], esc[2];
    FDFig       *pag, *figs[2];
    FDPdf       *doc;
    Modelo3     *M;
    Pen          p;
    int          nf = 0, apaisada, rc;

    if (i < 1 || i > m->c.n) {
        snprintf( why, n, "no hay ecuación %d", i );
        return 1;
    }
    if (!m->dia.hay_par) {
        snprintf( why, n, "el .out no trae la tabla de parámetros" );
        return 1;
    }

    M = g_new0( Modelo3, 1 );
    arma( m, i, M );

    gw = grafico ? grafico->w : 0.0;
    gh = grafico ? grafico->h : 0.0;
    apaisada = gw > A4C - 2 * margen;
    PW = apaisada ? A4L : A4C;
    PH = apaisada ? A4C : A4L;
    s  = 1.0;
    if (gw > 0.0 && gw > PW - 2 * margen) s = (PW - 2 * margen) / gw;

    pag = fd_fig_new( PW, PH );
    memset( &p, 0, sizeof p );
    p.f     = pag;
    p.size  = size;
    p.left  = margen;
    p.right = PW - margen;
    p.x     = margen;
    p.y     = PH - margen - s * gh - 2.6 * size;
    pon_modelo( &p, m, i, M );
    g_free( M );

    if (!(doc = fd_pdf_open( pdf ))) {
        fd_fig_free( pag );
        snprintf( why, n, "no pude escribir %s", pdf );
        return 1;
    }
    if (grafico) {
        figs[nf] = grafico; x[nf] = margen; y[nf] = PH - margen - s * gh;
        esc[nf] = s; nf++;
    }
    figs[nf] = pag; x[nf] = 0.0; y[nf] = 0.0; esc[nf] = 1.0; nf++;
    fd_pdf_page( doc, PW, PH, figs, x, y, esc, nf );
    rc = fd_pdf_close( doc );
    fd_fig_free( pag );
    if (rc) snprintf( why, n, "no pude escribir %s", pdf );
    return rc;
}
