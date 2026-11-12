/*
 * pagina.c -- «Ver modelo»: la pagina de cada serie del sistema (fase S4b
 * de docs/DISENO-sim.md, §3.4).
 *
 * ES LA PAGINA DE fue, con el mismo codigo (lib/equation/eqdraw.c): el
 * grafico de los residuos con su ACF y su PACF, el modelo estimado y la
 * tabla de los residuos grandes. La ecuacion de la serie A se escribe
 * exactamente como fue escribe su modelo univariante, y los terminos
 * CRUZADOS van en la linea del ruido, que es donde los pone el modelo que
 * estima drvarma, Phi(B)(w - mu) = Theta(B) a (escalera.c):
 *
 *     ln A_t = deterministas + N_t
 *     phi(B)[nabla N_t - mu] = (c1 B + c2 B^2) w_{B,t} + theta(B) A_t
 *                              + (-t1 B) A_{B,t} ;   sigma
 *
 * con w_{B,t} la serie estacionaria de B sin su media, y A_{B,t} su
 * innovacion (decidido por el analista el 2026-11-05: en la linea del
 * ruido, no en el hueco de la transferencia, que no seria la ecuacion
 * estimada). Una serie sin enlaces da la pagina de fue tal cual.
 *
 * DE DONDE SALE CADA COSA. La FORMA, del fichero de fue de la serie (el
 * mismo lector que el motor). Los VALORES, de la tabla de parametros del
 * .out (lib/outvarma), por el nombre que les da drvarma (escalera.c,
 * esc_names); lo que no esta en la tabla --fijo en el .pre, o con -fixarma
 * o sin -redet-- se escribe con el valor del fichero y sin desviacion.
 *
 * LAS UNIDADES. El motor trabaja con refactor * BoxCox(y): aqui se deshace
 * como hace fue. Los deterministas y la media se dividen por refactor(A);
 * un coeficiente cruzado se multiplica por refactor(B) / refactor(A); sigma
 * es sqrt(Sigma_AA) / refactor(A), en % si va en logaritmos.
 *
 * CON UN VEC (S10e), la ecuacion es la de drvec: alpha (ec_{t-1} - E) en la
 * linea del ruido y ec_t en una nota (ver «el VEC», mas abajo).
 */

/* Antes que GTK: main.h de drvarma define TRUE y FALSE. */
#include "fue_pre_reader.h"
#include "sim.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "fugdraw.h"
#include "fugplot.h"
#include "eqdraw.h"
#include "preview.h"

typedef struct { double v, se; int has_se; } Co;

/* El parametro que se llama asi si se estimo (flag 1 y esta en la tabla);
 * si no, el del fichero. escala pasa de las unidades del motor a las de la
 * ecuacion.                                                              */
static Co coef( const VarmaOut *v, int flag, double del_pre, double escala, const char *fmt, ... )
     G_GNUC_PRINTF(5, 6);
static Co coef( const VarmaOut *v, int flag, double del_pre, double escala, const char *fmt, ... )
{
    Co c = { del_pre * escala, 0.0, 0 };
    char nombre[OV_NOMBRE];
    const OvParam *q;
    va_list ap;

    if (flag != 1 || !v) return c;
    va_start( ap, fmt );
    vsnprintf( nombre, sizeof nombre, fmt, ap );
    va_end( ap );
    if (!( q = ov_param( v, nombre ) )) return c;
    c.v = q->est * escala;
    c.se = q->se * fabs( escala );
    c.has_se = q->tiene_se && q->se > 0;
    return c;
}

/* Un factor (1 - c1 B - c2 B^2 ...), con el convenio de Box-Jenkins. */
static void factor( Equation *eq, EqPart part, const Co *c, const int *lag, int n, int anual, int estilo )
{
    int j, alguno = 0;
    for (j = 0; j < n; j++) alguno |= c[j].v != 0.0 || c[j].has_se;
    if (!alguno) return;
    eq_add_open( eq, part, '(', 0 );
    eq_add_text( eq, part, "1" );
    for (j = 0; j < n; j++) {
        eq_add_coef( eq, part, estilo, c[j].v > 0.0 ? '-' : '+', c[j].v, c[j].has_se, c[j].se );
        eq_add_b( eq, part, lag[j], anual );
    }
    eq_add_close( eq, part, ')' );
}

/* Los factores AR (ar = 1) o MA de la serie, con sus nombres de drvarma:
 * phi_A[B^j] (phi2_A[B^j] si hay varios regulares), Phi_A[B^js] los
 * anuales, phif_A[f=k] los de frecuencia fija.                           */
static void lado( Equation *eq, const VarmaOut *v, const struct Tusmodel *t, const char *nom, int ar )
{
    const char *lo = ar ? "phi" : "theta", *up = ar ? "Phi" : "Theta";
    int n1 = ar ? t->NumAr1 : t->NumMa1, n2 = ar ? t->NumAr2 : t->NumMa2;
    int nf = ar ? t->NumAr1f : t->NumMa1f;
    int *o1 = ar ? t->p1 : t->q1, *o2 = ar ? t->p2 : t->q2;
    real **c1 = ar ? t->Ar1 : t->Ma1, **c2 = ar ? t->Ar2 : t->Ma2;
    int **f1 = ar ? t->Ia1 : t->Im1, **f2 = ar ? t->Ia2 : t->Im2;
    real **cf = ar ? t->Ar1f : t->Ma1f;
    int *ff = ar ? t->Ia1f : t->Im1f, *fr = ar ? t->pfre1 : t->qfre1;
    EqPart reg = ar ? EQ_ARR : EQ_MAR, anu = ar ? EQ_ARA : EQ_MAA, fij = ar ? EQ_ARF : EQ_MAF;
    int fijos = v->arma_fijos, f, j;
    Co c[64];
    int lag[64];

    for (f = 1; f <= n1; f++) {
        int n = 0;
        for (j = 1; j <= o1[f] && n < 64; j++, n++) {
            c[n] = n1 > 1 ? coef( v, fijos ? 0 : f1[f][j], c1[f][j], 1.0, "%s%d_%s[B^%d]", lo, f, nom, j )
                          : coef( v, fijos ? 0 : f1[f][j], c1[f][j], 1.0, "%s_%s[B^%d]", lo, nom, j );
            lag[n] = j;
        }
        factor( eq, reg, c, lag, n, 0, CS_PHI );
    }
    for (f = 1; f <= n2; f++) {
        int n = 0;
        for (j = 1; j <= o2[f] && n < 64; j++, n++) {
            c[n] = coef( v, fijos ? 0 : f2[f][j], c2[f][j], 1.0, "%s_%s[B^%d]", up, nom, j * t->sper );
            lag[n] = j * t->sper;
        }
        factor( eq, anu, c, lag, n, 1, CS_PHI );
    }
    for (f = 1; f <= nf; f++) {
        Co x = coef( v, fijos ? 0 : ff[f], cf[f][1], 1.0, "%sf_%s[f=%d]", lo, nom, fr[f] );
        eq_add_open( eq, fij, 'f', fr[f] );
        eq_add_text( eq, fij, "1" );
        if (fr[f] != 3) {
            eq_add_coef( eq, fij, CS_PHIF, x.v > 0.0 ? '-' : '+', x.v, x.has_se, x.se );
            eq_add_b( eq, fij, 1, 0 );
        }
        eq_add_close( eq, fij, ')' );
    }
}

/* La variable de un determinista, como la escribe fue (drtran_gui,
 * pagina.c: variable).                                                   */
static void variable( Equation *eq, const char *spec, int freq )
{
    char tipo[32] = "";
    int a = 0, b = 0, n = spec ? sscanf( spec, "%31s %d %d", tipo, &a, &b ) : 0;
    EqItem *it;

    if (n < 1) snprintf( tipo, sizeof tipo, "non-standard" );
    if (!strcmp( tipo, "impulse" ) || !strcmp( tipo, "compimp" ) ||
        !strcmp( tipo, "step" ) || !strcmp( tipo, "ramp" )) {
        int mes = n == 3 ? a : 0, anio = n == 3 ? b : n == 2 ? a : 0;
        it = eq_add( eq, EQ_DETER, EI_DUMMY );
        snprintf( it->text, sizeof it->text, "%s", !strcmp( tipo, "impulse" ) ? "I" :
                  !strcmp( tipo, "compimp" ) ? "CI" : !strcmp( tipo, "step" ) ? "S" : "R" );
        it->a = freq == 1 ? 0 : mes;
        it->b = freq == 1 && anio == 0 ? mes : anio;
    } else if (!strcmp( tipo, "time" ))
        eq_add_text( eq, EQ_DETER, "t" );
    else if (!strcmp( tipo, "season" )) {
        it = eq_add( eq, EQ_DETER, EI_DUMMY );
        snprintf( it->text, sizeof it->text, "season" );
        it->a = a;
    } else if (!strcmp( tipo, "cos" ) || !strcmp( tipo, "sin" )) {
        it = eq_add( eq, EQ_DETER, EI_HARMONIC );
        snprintf( it->text, sizeof it->text, "%s", tipo );
        it->value = a;
        eq_harmonic_angle( a, freq, &it->a, &it->b );
    } else if (!strcmp( tipo, "alter" ))
        eq_add( eq, EQ_DETER, EI_ALTER );
    else if (!strcmp( tipo, "non-standard" ))
        eq_add( eq, EQ_DETER, EI_DUMMY );
    else {
        it = eq_add( eq, EQ_DETER, EI_DUMMY );
        snprintf( it->text, sizeof it->text, "%s", tipo );
    }
}

/* Un determinista: omega(B)/delta(B) por su variable (con -redet, sus
 * valores son w%d_A[spec] y d%d_A[spec]).                                */
static void determinista( Equation *eq, const VarmaOut *v, const struct Tusmodel *t, int iv,
                          const char *nom, double rY, int freq )
{
    const char *sp = t->detspec && t->detspec[iv] ? t->detspec[iv] : "det";
    int re = v ? v->det_reestimados : 0, k;
    Co w0 = coef( v, re ? t->Imega[iv][0] : 0, t->Omega[iv][0], 1.0 / rY, "w0_%s[%s]", nom, sp );
    Co num[64], den[64];
    int nn = 0, nd = 0;

    for (k = 1; k <= t->Nomega[iv] && nn < 64; k++)
        num[nn++] = coef( v, re ? t->Imega[iv][k] : 0, t->Omega[iv][k], 1.0 / rY, "w%d_%s[%s]", k, nom, sp );
    for (k = 1; k <= t->Ndelta[iv] && nd < 64; k++)
        den[nd++] = coef( v, re ? t->Ielta[iv][k] : 0, t->Delta[iv][k], 1.0, "d%d_%s[%s]", k, nom, sp );

    eq_add_op( eq, EQ_DETER, w0.v < 0.0 ? '-' : '+' );
    eq->item[EQ_DETER][eq->n[EQ_DETER] - 1].a = ( w0.v == 0.0 );
    if (nn > 0) eq_add_open( eq, EQ_DETER, '(', 0 );
    eq_add_coef( eq, EQ_DETER, CS_OMEGA, ' ', w0.v, w0.has_se, w0.se );
    for (k = 0; k < nn; k++) {
        eq_add_coef( eq, EQ_DETER, CS_OMEGA, ( ( w0.v > 0.0 ) == ( num[k].v > 0.0 ) ) ? '-' : '+',
                     num[k].v, num[k].has_se, num[k].se );
        eq_add_b( eq, EQ_DETER, k + 1, 0 );
    }
    if (nn > 0) eq_add_close( eq, EQ_DETER, ')' );
    if (nd > 0) {
        int alguno = 0;
        for (k = 0; k < nd; k++) alguno |= den[k].v != 0.0 || den[k].has_se;
        if (alguno) {
            eq_add_op( eq, EQ_DETER, '/' );
            eq_add_open( eq, EQ_DETER, '(', 0 );
            eq_add_text( eq, EQ_DETER, "1" );
            for (k = 0; k < nd; k++) {
                eq_add_coef( eq, EQ_DETER, CS_DELTA, den[k].v > 0.0 ? '-' : '+', den[k].v,
                             den[k].has_se, den[k].se );
                eq_add_b( eq, EQ_DETER, k + 1, 0 );
            }
            eq_add_close( eq, EQ_DETER, ')' );
        }
    }
    variable( eq, sp, freq );
}

/* Los items de un termino cruzado, (c1 B + c2 B^2 ...) w_{B,t} o A_{B,t},
 * con los coeficientes ya en el convenio de la ecuacion.                 */
static int terminos( EqItem *it, int max, int primero, int ma, const Co *c, int orden, const char *B )
{
    int k, n = 0, alguno = 0;

    for (k = 0; k < orden; k++) alguno |= c[k].v != 0.0 || c[k].has_se;
    if (!alguno || max < 3 * orden + 4) return 0;
    if (!primero || ma) { memset( &it[n], 0, sizeof it[n] ); it[n].kind = EI_OP; it[n].sign = '+'; n++; }
    memset( &it[n], 0, sizeof it[n] ); it[n].kind = EI_OPEN; it[n].bracket = '('; n++;
    for (k = 0; k < orden; k++) {
        memset( &it[n], 0, sizeof it[n] );
        it[n].kind = EI_COEF;
        it[n].style = CS_PHI;
        it[n].sign = k == 0 ? ( c[k].v < 0.0 ? '-' : ' ' ) : ( c[k].v < 0.0 ? '-' : '+' );
        it[n].value = fabs( c[k].v );
        it[n].has_se = c[k].has_se;
        it[n].se = c[k].se;
        n++;
        memset( &it[n], 0, sizeof it[n] );
        it[n].kind = EI_B;
        it[n].a = k + 1;
        n++;
    }
    memset( &it[n], 0, sizeof it[n] ); it[n].kind = EI_CLOSE; it[n].bracket = ')'; n++;
    memset( &it[n], 0, sizeof it[n] );
    it[n].kind = EI_INPUT;
    it[n].a = ma ? 4 : 3;
    snprintf( it[n].text, sizeof it[n].text, "%s", B );
    n++;
    return n;
}

/* Un termino cruzado del VARMA: (c1 B + c2 B^2 ...) w_{B,t} o A_{B,t}.
 * signo es el del convenio: +1 para el AR (pasa al lado derecho sumando),
 * -1 para el MA (Theta(B) = I - SUM theta_k B^k).                         */
static int cruzado( EqItem *it, int max, int primero, const VarmaOut *v, const char *A, const char *B,
                    int orden, int ma, double escala )
{
    int k;
    Co c[32];

    if (orden > 32) orden = 32;
    for (k = 1; k <= orden; k++) {
        char nombre[OV_NOMBRE];
        const OvParam *q;
        snprintf( nombre, sizeof nombre, "%s%d[%s<-%s]", ma ? "MA" : "AR", k, A, B );
        c[k - 1].v = 0; c[k - 1].se = 0; c[k - 1].has_se = 0;
        if (( q = ov_param( v, nombre ) )) {
            c[k - 1].v = ( ma ? -q->est : q->est ) * escala;
            c[k - 1].se = q->se * fabs( escala );
            c[k - 1].has_se = q->tiene_se && q->se > 0;
        }
    }
    return terminos( it, max, primero, ma, c, orden, B );
}

/* La ecuacion de la serie i. cruz_ar y cruz_ma son del que llama. */
static int ecuacion( Sim *m, int i, const VarmaOut *v, Equation *eq, EqItem *cruz_ar, EqItem *cruz_ma,
                     int max, char *why, size_t nw )
{
    struct Tusmodel tm;
    struct Tseries ts;
    real **dm = NULL;
    const char *nom = m->sis.s[i].nombre;
    double rY, lam;
    int k, b, freq, nar = 0, nma = 0;
    GString *nota = g_string_new( NULL );

    memset( &tm, 0, sizeof tm );
    memset( &ts, 0, sizeof ts );
    if (read_fue_pre( m->sis.s[i].ruta, &tm, &ts, &dm ) != 0) {
        snprintf( why, nw, "no pude leer %s", m->sis.s[i].ruta );
        g_string_free( nota, TRUE );
        return 1;
    }
    rY = ts.refactor > 0 ? ts.refactor : 1.0;
    lam = tm.boxlam;
    freq = tm.sper > 0 ? tm.sper : ts.freq;

    snprintf( eq->name, sizeof eq->name, "%s", nom );
    snprintf( eq->residuals, sizeof eq->residuals, "A" );
    eq->is_log = fabs( lam ) < 1e-8;
    eq->boxcox = !eq->is_log && fabs( lam - 1.0 ) > 1e-8;
    eq->lambda = lam;
    eq->freq = ts.freq;
    eq->nobs = v->nobs;
    eq->sigma_pct = eq->is_log;
    eq->sigma = v->tiene_sigma && v->sigma[i][i] > 0 ? sqrt( v->sigma[i][i] ) / rY * ( eq->is_log ? 100.0 : 1.0 ) : 0.0;

    /* 1 · los deterministas, y N_t */
    if (tm.NdetVar > 0) {
        eq_add_text( eq, EQ_DETER, eq->name );
        eq_add_op( eq, EQ_DETER, '=' );
        for (k = 1; k <= tm.NdetVar; k++) determinista( eq, v, &tm, k, nom, rY, freq );
        eq_add_op( eq, EQ_DETER, '+' );
        eq_add_text( eq, EQ_DETER, "N" );
    }

    /* 2 · el ruido de la serie, el de su fichero */
    lado( eq, v, &tm, nom, 1 );
    lado( eq, v, &tm, nom, 0 );
    if (tm.Imu == 1 || tm.mu != 0.0) {
        Co mu = coef( v, tm.Imu, tm.mu, 1.0 / rY, "mu_%s", nom );
        eq_add_coef( eq, EQ_MU, CS_MU, mu.v > 0.0 ? '-' : '+', mu.v, mu.has_se, mu.se );
    }
    if (tm.nrdiff > 0) eq_add( eq, EQ_NRDIFF, EI_NABLA )->a = tm.nrdiff;
    if (tm.nadiff > 0) eq_add( eq, EQ_NADIFF, EI_NABLA )->b = tm.nadiff * tm.sper;
    else if (tm.ifadf && ( tm.sper == 4 || tm.sper == 12 ))
        for (k = 0; k <= tm.sper / 2; k++) {
            EqItem *it;
            if (tm.ifadf[k] <= 0) continue;
            if (k == 0) { eq_add( eq, EQ_IFADF, EI_NABLA )->a = 1; continue; }
            it = eq_add( eq, EQ_IFADF, EI_OPEN );
            it->bracket = 'i';
            it->a = k;
            it->b = tm.sper;
        }

    /* 3 · los cruzados, en la linea del ruido */
    for (b = 0; b < m->sis.n; b++) {
        struct Tusmodel tb;
        struct Tseries sb;
        real **db = NULL;
        double rB = 1.0;
        int a1, a2;
        if (b == i) continue;
        memset( &tb, 0, sizeof tb );
        memset( &sb, 0, sizeof sb );
        if (read_fue_pre( m->sis.s[b].ruta, &tb, &sb, &db ) == 0) {
            rB = sb.refactor > 0 ? sb.refactor : 1.0;
            free_fue_pre( &tb, &sb, db );
        }
        a1 = cruzado( cruz_ar + nar, max - nar, nar == 0, v, nom, m->sis.s[b].nombre, v->p, 0, rB / rY );
        a2 = cruzado( cruz_ma + nma, max - nma, nma == 0, v, nom, m->sis.s[b].nombre, v->q, 1, rB / rY );
        nar += a1;
        nma += a2;
        if (a1)
            g_string_append_printf( nota, "%sw(%s,t): la serie estacionaria de %s sin su media",
                                    nota->len ? "; " : "", m->sis.s[b].nombre, m->sis.s[b].nombre );
        if (a2)
            g_string_append_printf( nota, "%sA(%s,t): la innovación de %s", nota->len ? "; " : "",
                                    m->sis.s[b].nombre, m->sis.s[b].nombre );
    }
    eq->cruz_ar = cruz_ar;
    eq->ncruz_ar = nar;
    eq->cruz_ma = cruz_ma;
    eq->ncruz_ma = nma;
    if (nota->len) {
        /* fugdraw escribe en Latin-1 */
        gchar *l1 = g_convert( nota->str, -1, "ISO-8859-1", "UTF-8", NULL, NULL, NULL );
        snprintf( eq->nota, sizeof eq->nota, "%s.", l1 ? l1 : nota->str );
        g_free( l1 );
    }
    g_string_free( nota, TRUE );
    free_fue_pre( &tm, &ts, dm );
    return 0;
}

/* --- el VEC (S10e) ---------------------------------------------------------
 *
 * drvec estima, sobre w = refactor * BoxCox(z) - deterministas (los del .pre,
 * FIJOS: drvec los resta y no los reestima), el VEC en niveles
 *
 *     nabla w_t = alpha (beta' w_{t-1} - E[ec]) + Gamma_1 nabla w_{t-1} + ...
 *                 + A_t - Theta_1 A_{t-1} - ...
 *
 * y la ecuacion de la serie A se escribe como la de fue: la primera linea,
 * sus deterministas (del .pre) y N_t; la segunda, el ruido, con su Gamma
 * propio como factor AR, su Theta propio como factor MA y, a la derecha,
 * alpha (ec_{t-1} - E) y los terminos cruzados:
 *
 *     LRM_t = deterministas + N_t
 *     (1 - g B)[nabla N_t - mu] = a (ec_{t-1} - E) + (c B) w_{B,t} + ... + A_t ; sigma
 *
 * con ec_t = N_{Y1,t} + SUM b_m N_{m,t} en una nota debajo: la relacion de
 * equilibrio, normalizada sobre su serie de Y1 (la de «4 · Modelo»).
 *
 * LAS UNIDADES, como en el VARMA: todo pasa a las de la serie (las de N_A,
 * su Box-Cox), dividiendo por los refactor. ec_j va en las unidades de su
 * serie de Y1, k: b_m * rf_m / rf_k, E / rf_k, y alpha_A * rf_k / rf_A. Un
 * Gamma cruzado se multiplica por rf_B / rf_A; sigma es sqrt(Sigma_AA) /
 * rf_A, en % si va en logaritmos.
 *
 * LA MEDIA mu: E[nabla w] = 0 salvo con el caso 3, en que la del bloque
 * nabla Y2 es libre (E[D.x]) y la de Y1 la impone beta: E[nabla Y1] =
 * -beta_2' E[nabla Y2].                                                    */

typedef struct {
    int    m, s, r, k;          /* series, nabla Y2, rango; A en el orden de drvec */
    double rf[OVEC_MAX_SER];    /* el refactor de cada una, en el orden de drvec    */
    double lam[OVEC_MAX_SER];
} VecUd;

static int vec_ind( const VecOut *x, const char *nom )
{
    int k;
    for (k = 0; k < x->m; k++) if (!strcmp( x->serie[k], nom )) return k;
    return -1;
}

static int vec_ud( Sim *m, const VecOut *x, int i, VecUd *u )
{
    int k, b;

    memset( u, 0, sizeof *u );
    u->m = x->m;
    u->r = x->r > 0 ? x->r : x->nr;
    u->s = x->m - u->r;
    if (u->r < 1 || u->s < 0 || x->m > OVEC_MAX_SER) return 1;
    if (( u->k = vec_ind( x, m->sis.s[i].nombre ) ) < 0) return 1;
    for (k = 0; k < x->m; k++) {
        u->rf[k] = 1.0;
        u->lam[k] = 1.0;
        for (b = 0; b < m->sis.n; b++) {
            struct Tusmodel tb;
            struct Tseries sb;
            real **db = NULL;
            if (strcmp( m->sis.s[b].nombre, x->serie[k] )) continue;
            memset( &tb, 0, sizeof tb );
            memset( &sb, 0, sizeof sb );
            if (read_fue_pre( m->sis.s[b].ruta, &tb, &sb, &db ) == 0) {
                u->rf[k] = sb.refactor > 0 ? sb.refactor : 1.0;
                u->lam[k] = tb.boxlam;
                free_fue_pre( &tb, &sb, db );
            }
        }
    }
    return 0;
}

/* Un parametro de la tabla de drvec, con la escala; si no esta (fijo o
 * restringido), por_defecto sin desviacion.                              */
static Co vpar( const VecOut *x, double por_defecto, double escala, const char *fmt, ... )
     G_GNUC_PRINTF(4, 5);
static Co vpar( const VecOut *x, double por_defecto, double escala, const char *fmt, ... )
{
    Co c = { por_defecto * escala, 0.0, 0 };
    char nombre[OVEC_NOMBRE];
    const OvecPar *q;
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( nombre, sizeof nombre, fmt, ap );
    va_end( ap );
    if (!( q = ovec_param( x, nombre ) )) return c;
    c.v = q->est * escala;
    c.se = q->se * fabs( escala );
    c.has_se = q->se > 0;
    return c;
}

/* E[nabla w] de la serie k del VEC, en las unidades de w (caso 3). */
static Co vec_media( const VecOut *x, const VecUd *u, int k )
{
    Co c = { 0.0, 0.0, 0 }, e;
    int mm;

    if (strcmp( x->caso, "3" )) return c;
    if (k < u->s) return vpar( x, 0.0, 1.0, "E[D.%s]", x->serie[k] );
    for (mm = 0; mm < u->s; mm++) {
        e = vpar( x, 0.0, 1.0, "E[D.%s]", x->serie[mm] );
        c.v -= x->beta[mm][k - u->s] * e.v;
    }
    return c;
}

/* La nota del VEC, en UTF-8: que es ec_t, y que son w_{B,t} y A_{B,t}. */
static void vec_nota( const VecOut *x, const VecUd *u, int det, const char *cruz, GString *g )
{
    int j, mm;

    for (j = 0; j < u->r; j++) {
        int y1 = u->s + j;
        Co t;
        if (j) g_string_append_c( g, '\n' );
        if (u->r > 1) g_string_append_printf( g, "ec%d(t) = N(%s,t)", j + 1, x->serie[y1] );
        else g_string_append_printf( g, "ec(t) = N(%s,t)", x->serie[y1] );
        for (mm = 0; mm < u->s; mm++) {
            Co b = vpar( x, x->beta[mm][j], u->rf[mm] / u->rf[y1], "%s in ec%d", x->serie[mm], j + 1 );
            if (b.v == 0.0) continue;
            g_string_append_printf( g, " %s %.4f N(%s,t)", b.v < 0 ? "-" : "+", fabs( b.v ), x->serie[mm] );
        }
        g_string_append_printf( g, ": la relación de equilibrio, normalizada sobre %s", x->serie[y1] );
        t = vpar( x, 0.0, 1.0 / u->rf[y1], "trend[ec%d]", j + 1 );
        if (t.v != 0.0)
        {
            char n[8] = "";
            if (u->r > 1) snprintf( n, sizeof n, "%d", j + 1 );
            g_string_append_printf( g, "\nE[ec%s] lleva una tendencia de %.4g por periodo (t centrado en la "
                                    "mitad de la muestra)", n, t.v );
        }
    }
    g_string_append_printf( g, "\nN(X,t): la serie X sin sus deterministas%s", det ? " (los de su .pre, fijos)" : "" );
    if (cruz && *cruz) g_string_append_printf( g, "\n%s", cruz );
}

static int ecuacion_vec( Sim *m, int i, const VecOut *x, Equation *eq, EqItem *cruz_ar, EqItem *cruz_ma,
                         int max, GString *nota_utf8, char *why, size_t nw )
{
    struct Tusmodel tm;
    struct Tseries ts;
    real **dm = NULL;
    const char *nom = m->sis.s[i].nombre;
    VecUd u;
    double rA, lam;
    int k, j, mm, freq, nar = 0, nma = 0, ng;
    Co c[64];
    int lag[64];
    GString *cruz = g_string_new( NULL ), *cw = g_string_new( NULL ), *ca = g_string_new( NULL );

    if (vec_ud( m, x, i, &u ) != 0) {
        g_string_free( cw, TRUE );
        g_string_free( ca, TRUE );
        snprintf( why, nw, "%s no está en el VEC", nom );
        g_string_free( cruz, TRUE );
        return 1;
    }
    memset( &tm, 0, sizeof tm );
    memset( &ts, 0, sizeof ts );
    if (read_fue_pre( m->sis.s[i].ruta, &tm, &ts, &dm ) != 0) {
        snprintf( why, nw, "no pude leer %s", m->sis.s[i].ruta );
        g_string_free( cruz, TRUE );
        g_string_free( cw, TRUE );
        g_string_free( ca, TRUE );
        return 1;
    }
    k = u.k;
    rA = u.rf[k];
    lam = tm.boxlam;
    freq = tm.sper > 0 ? tm.sper : ts.freq;

    snprintf( eq->name, sizeof eq->name, "%s", nom );
    snprintf( eq->residuals, sizeof eq->residuals, "A" );
    eq->is_log = fabs( lam ) < 1e-8;
    eq->boxcox = !eq->is_log && fabs( lam - 1.0 ) > 1e-8;
    eq->lambda = lam;
    eq->freq = ts.freq;
    eq->nobs = x->nobs;
    eq->sigma_pct = eq->is_log;
    eq->sigma = x->sigma[k][k] > 0 ? sqrt( x->sigma[k][k] ) / rA * ( eq->is_log ? 100.0 : 1.0 ) : 0.0;

    /* 1 · los deterministas del .pre, fijos, y N_t */
    if (tm.NdetVar > 0) {
        eq_add_text( eq, EQ_DETER, eq->name );
        eq_add_op( eq, EQ_DETER, '=' );
        for (j = 1; j <= tm.NdetVar; j++) determinista( eq, NULL, &tm, j, nom, rA, freq );
        eq_add_op( eq, EQ_DETER, '+' );
        eq_add_text( eq, EQ_DETER, "N" );
    }

    /* 2 · el ruido: Gamma propio (AR), nabla N_t, la media, Theta propio (MA) */
    ng = x->p - 1;
    for (j = 1; j <= ng && j <= 64; j++) {
        c[j - 1] = vpar( x, 0.0, 1.0, "D.%s <- D.%s(-%d)", nom, nom, j );
        lag[j - 1] = j;
    }
    if (ng > 0) factor( eq, EQ_ARR, c, lag, ng > 64 ? 64 : ng, 0, CS_PHI );
    for (j = 1; j <= x->q && j <= 64; j++) {
        c[j - 1] = vpar( x, 0.0, 1.0, "D.%s <- A.%s(-%d)", nom, nom, j );
        lag[j - 1] = j;
    }
    if (x->q > 0) factor( eq, EQ_MAR, c, lag, x->q > 64 ? 64 : x->q, 0, CS_PHI );
    {
        Co mu = vec_media( x, &u, k );
        mu.v /= rA;
        mu.se /= rA;
        if (mu.v != 0.0 || mu.has_se)
            eq_add_coef( eq, EQ_MU, CS_MU, mu.v > 0.0 ? '-' : '+', mu.v, mu.has_se, mu.se );
    }
    eq_add( eq, EQ_NRDIFF, EI_NABLA )->a = 1;

    /* 3 · la correccion del error: alpha (ec_{t-1} - E), uno por relacion */
    for (j = 0; j < u.r; j++) {
        int y1 = u.s + j;
        Co a = vpar( x, x->alpha[k][j], u.rf[y1] / rA, "D.%s <- ec%d", nom, j + 1 );
        Co e = vpar( x, 0.0, 1.0 / u.rf[y1], "E[ec%d]", j + 1 );
        int conE = e.v != 0.0 || e.has_se;
        EqItem *it;
        if (a.v == 0.0 && !a.has_se) continue;
        if (max - nar < 8) break;
        if (nar > 0) { it = &cruz_ar[nar++]; memset( it, 0, sizeof *it ); it->kind = EI_OP; it->sign = a.v < 0 ? '-' : '+'; }
        it = &cruz_ar[nar++];
        memset( it, 0, sizeof *it );
        it->kind = EI_COEF;
        it->style = CS_OMEGA;     /* una ganancia: los decimales que pida (0.00048) */
        it->sign = nar == 1 && a.v < 0 ? '-' : ' ';
        it->value = fabs( a.v );
        it->has_se = a.has_se;
        it->se = a.se;
        if (conE) { it = &cruz_ar[nar++]; memset( it, 0, sizeof *it ); it->kind = EI_OPEN; it->bracket = '('; }
        it = &cruz_ar[nar++];
        memset( it, 0, sizeof *it );
        it->kind = EI_INPUT;
        it->a = 5;
        if (u.r > 1) snprintf( it->text, sizeof it->text, "%d,t-1", j + 1 );
        else snprintf( it->text, sizeof it->text, "t-1" );
        if (conE) {
            it = &cruz_ar[nar++];
            memset( it, 0, sizeof *it );
            it->kind = EI_COEF;
            it->style = CS_MU;
            it->sign = e.v > 0 ? '-' : '+';
            it->value = fabs( e.v );
            it->has_se = e.has_se;
            it->se = e.se;
            it = &cruz_ar[nar++]; memset( it, 0, sizeof *it ); it->kind = EI_CLOSE; it->bracket = ')';
        }
    }

    /* 4 · los cruzados de Gamma y Theta, en la linea del ruido */
    for (mm = 0; mm < x->m; mm++) {
        const char *B = x->serie[mm];
        double esc = u.rf[mm] / rA;
        int a1 = 0, a2 = 0;
        if (mm == k) continue;
        for (j = 1; j <= ng && j <= 32; j++) c[j - 1] = vpar( x, 0.0, esc, "D.%s <- D.%s(-%d)", nom, B, j );
        if (ng > 0) a1 = terminos( cruz_ar + nar, max - nar, nar == 0, 0, c, ng > 32 ? 32 : ng, B );
        for (j = 1; j <= x->q && j <= 32; j++) {
            c[j - 1] = vpar( x, 0.0, esc, "D.%s <- A.%s(-%d)", nom, B, j );
            c[j - 1].v = -c[j - 1].v;
        }
        if (x->q > 0) a2 = terminos( cruz_ma + nma, max - nma, nma == 0, 1, c, x->q > 32 ? 32 : x->q, B );
        nar += a1;
        nma += a2;
        if (a1) g_string_append_printf( cw, "%s%s", cw->len ? ", " : "", B );
        if (a2) g_string_append_printf( ca, "%s%s", ca->len ? ", " : "", B );
    }
    if (cw->len)
        g_string_append_printf( cruz, "w(B,t): la diferencia de N(B,t), sin su media (B = %s)", cw->str );
    if (ca->len)
        g_string_append_printf( cruz, "%sA(B,t): la innovación de B (B = %s)", cruz->len ? "; " : "", ca->str );
    g_string_free( cw, TRUE );
    g_string_free( ca, TRUE );
    eq->cruz_ar = cruz_ar;
    eq->ncruz_ar = nar;
    eq->cruz_ma = cruz_ma;
    eq->ncruz_ma = nma;

    vec_nota( x, &u, tm.NdetVar > 0, cruz->str, nota_utf8 );
    {
        /* fugdraw escribe en Latin-1 */
        gchar *l1 = g_convert( nota_utf8->str, -1, "ISO-8859-1", "UTF-8", NULL, NULL, NULL );
        snprintf( eq->nota, sizeof eq->nota, "%s", l1 ? l1 : nota_utf8->str );
        g_free( l1 );
    }
    g_string_free( cruz, TRUE );
    free_fue_pre( &tm, &ts, dm );
    return 0;
}

/* Los residuos de la serie i, en las unidades de w: los de su fila del .out
 * y, con un VEC, los de Y1 reconstruidos. drvec diagnostica W = (nabla Y2,
 * beta'Y): la fila de una serie de Y1 es la de su ec_j, cuya innovacion es
 * beta_j' A_t = a_{Y1,j} + SUM b_mj a_m. Asi que a_{Y1,j} = a_{ec_j} -
 * SUM b_mj a_m (sobre JJ: sd 2.11, la sigma del modelo 2.13).            */
static int residuos_de( Sim *m, int i, const OvResiduos *res, double *out )
{
    const VecOut *x = m->pe.vec;
    int t, mm, k;

    if (!x || !x->hay_vec) {
        if (i >= res->m) return 1;
        memcpy( out, res->v[i], sizeof( double ) * res->n );
        return 0;
    }
    if (( k = vec_ind( x, m->sis.s[i].nombre ) ) < 0 || x->m > res->m) return 1;
    memcpy( out, res->v[k], sizeof( double ) * res->n );
    if (k >= x->m - x->r)
        for (mm = 0; mm < x->m - x->r; mm++)
            for (t = 0; t < res->n; t++) out[t] -= x->beta[mm][k - ( x->m - x->r )] * res->v[mm][t];
    return 0;
}

/* --- la pagina ------------------------------------------------------------ */

static const OvResiduos *g_res = NULL;
static void fecha_de( int k, char *buf, size_t size, void *data )
{
    (void) data;
    snprintf( buf, size, "%s", g_res ? g_res->fecha[k] : "" );
}

gchar *pagina_ruta( Sim *m, int i )
{
    gchar *nom = g_strdup_printf( "%s_%s.pdf", m->pe.corrida[0] ? m->pe.corrida : "modelo",
                                  m->sis.s[i].nombre );
    gchar *r = sim_artefacto( m, nom );
    g_free( nom );
    return r;
}

int pagina_serie( Sim *m, int i, const char *pdf, char *why, size_t nw )
{
    const VarmaOut *v = m->pe.v;
    Equation eq;
    EqItem *cruz_ar, *cruz_ma;
    OvResiduos *res;
    FDFig *graf = NULL;
    EqOutlier *at = NULL;
    int nat = 0, rc, fechas = 0;

    const VecOut *x = m->pe.vec && m->pe.vec->hay_vec ? m->pe.vec : NULL;
    GString *nota = g_string_new( NULL );
    double *serie = NULL;

    if (!x && ( !v || v->fallo )) {
        snprintf( why, nw, "no hay sistema estimado" );
        g_string_free( nota, TRUE );
        return 1;
    }
    if (i < 0 || i >= m->sis.n) {
        snprintf( why, nw, "no hay serie %d", i + 1 );
        g_string_free( nota, TRUE );
        return 1;
    }
    if (eq_new( &eq )) { snprintf( why, nw, "sin memoria" ); g_string_free( nota, TRUE ); return 1; }
    cruz_ar = g_new0( EqItem, 2048 );
    cruz_ma = g_new0( EqItem, 2048 );
    if (( x ? ecuacion_vec( m, i, x, &eq, cruz_ar, cruz_ma, 2048, nota, why, nw )
            : ecuacion( m, i, v, &eq, cruz_ar, cruz_ma, 2048, why, nw ) ) != 0) {
        eq_release( &eq ); g_free( cruz_ar ); g_free( cruz_ma );
        g_string_free( nota, TRUE );
        return 1;
    }
    g_string_free( nota, TRUE );

    /* EL GRAFICO Y LOS RESIDUOS GRANDES, con los residuos del .out. */
    res = g_new0( OvResiduos, 1 );
    serie = g_new0( double, OV_MAX_OBS );
    if (m->pe.out && ov_residuos_file( m->pe.out, res ) == 0 && res->n > 1 &&
        residuos_de( m, i, res, serie ) == 0) {
        struct Tseries ts;
        struct Tusmodel tm;
        struct Tseries t0;
        real **dm = NULL;
        FPUnits u = { 1.0, 0 };
        double s = 0, s2 = 0, med, dt;
        int k, N = res->n;
        gchar *base;

        memset( &tm, 0, sizeof tm );
        memset( &t0, 0, sizeof t0 );
        if (read_fue_pre( m->sis.s[i].ruta, &tm, &t0, &dm ) == 0) {
            u.refactor = t0.refactor > 0 ? t0.refactor : 1.0;
            u.pct = fabs( tm.boxlam ) < 1e-8;
            free_fue_pre( &tm, &t0, dm );
        }
        memset( &ts, 0, sizeof ts );
        ts.name = m->sis.s[i].nombre;
        ts.nobs = N;
        ts.freq = m->sis.s[i].freq > 0 ? m->sis.s[i].freq : 1;
        ts.data = vector( 1, N );
        for (k = 0; k < N; k++) { ts.data[k + 1] = serie[k]; s += serie[k]; }
        med = s / N;
        for (k = 0; k < N; k++) s2 += ( serie[k] - med ) * ( serie[k] - med );
        ts.mean = med;
        ts.var = s2 / N;
        if (sscanf( res->fecha[0], "%d/%d", &ts.begtime, &ts.begyear ) != 2) {
            ts.begtime = 1;
            ts.begyear = atoi( res->fecha[0] );
        }
        base = sim_artefacto( m, "res" );
        graf = fp_PlotSer_CorrSer( &ts, 0, ts.nobs, 0, ts.begyear, 1.0, 0, 0, 0, 0.0, base,
                                   ts.name, u );
        g_free( base );
        dt = sqrt( ts.var );
        g_res = res;
        fechas = res->fecha[0][0] != 0;
        at = g_new0( EqOutlier, N );
        nat = eq_outliers( serie, N, med, dt, fechas ? fecha_de : NULL, NULL, at, N );
        free_vector( ts.data, 1, N );
    }

    rc = eq_page_pdf( pdf, graf, &eq, at, nat, fechas );
    if (graf) fd_fig_free( graf );
    g_res = NULL;
    g_free( at );
    g_free( res );
    g_free( serie );
    eq_release( &eq );
    g_free( cruz_ar );
    g_free( cruz_ma );
    if (rc) snprintf( why, nw, "no pude escribir %s", pdf );
    return rc;
}

int pagina_cruzados( Sim *m, int i )
{
    const VecOut *x = m->pe.vec && m->pe.vec->hay_vec ? m->pe.vec : NULL;
    Equation eq;
    EqItem *ca, *cm;
    GString *nota;
    char why[256];
    int n = -1;

    if (( !x && !m->pe.v ) || i < 0 || i >= m->sis.n || eq_new( &eq )) return -1;
    ca = g_new0( EqItem, 2048 );
    cm = g_new0( EqItem, 2048 );
    nota = g_string_new( NULL );
    if (( x ? ecuacion_vec( m, i, x, &eq, ca, cm, 2048, nota, why, sizeof why )
            : ecuacion( m, i, m->pe.v, &eq, ca, cm, 2048, why, sizeof why ) ) == 0)
        n = eq.ncruz_ar + eq.ncruz_ma;
    eq_release( &eq );
    g_free( ca ); g_free( cm );
    g_string_free( nota, TRUE );
    return n;
}

gchar *pagina_nota( Sim *m, int i )
{
    const VecOut *x = m->pe.vec && m->pe.vec->hay_vec ? m->pe.vec : NULL;
    Equation eq;
    EqItem *ca, *cm;
    GString *nota;
    char why[256];
    gchar *r = NULL;

    if (i < 0 || i >= m->sis.n || eq_new( &eq )) return NULL;
    ca = g_new0( EqItem, 2048 );
    cm = g_new0( EqItem, 2048 );
    nota = g_string_new( NULL );
    if (x) {
        if (ecuacion_vec( m, i, x, &eq, ca, cm, 2048, nota, why, sizeof why ) == 0) r = g_strdup( nota->str );
    } else if (m->pe.v && ecuacion( m, i, m->pe.v, &eq, ca, cm, 2048, why, sizeof why ) == 0)
        r = g_convert( eq.nota, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL );
    eq_release( &eq );
    g_free( ca ); g_free( cm );
    g_string_free( nota, TRUE );
    return r;
}

int pagina_residuos( Sim *m, int i, double *v, int max )
{
    OvResiduos *res;
    double *t;
    int n = -1;

    if (!m->pe.out || i < 0 || i >= m->sis.n) return -1;
    res = g_new0( OvResiduos, 1 );
    t = g_new0( double, OV_MAX_OBS );
    if (ov_residuos_file( m->pe.out, res ) == 0 && residuos_de( m, i, res, t ) == 0) {
        n = res->n < max ? res->n : max;
        memcpy( v, t, sizeof( double ) * n );
    }
    g_free( t );
    g_free( res );
    return n;
}

/* «Ver modelo»: la pagina de la serie i, escrita y en la ventana de
 * graficos. La usa la barra (la serie que se mira) y su menu.            */
gboolean pagina_ver( Sim *m, int i )
{
    gchar *pdf = pagina_ruta( m, i );
    char why[512];
    gboolean ok = pagina_serie( m, i, pdf, why, sizeof why ) == 0;

    if (!ok) sim_estado( m, "No pude escribir la página de %s: %s.", m->sis.s[i].nombre, why );
    else if (!preview_show( m, pdf )) { sim_estado( m, "No pude abrir %s", pdf ); ok = FALSE; }
    else sim_estado( m, "La página de %s: %s", m->sis.s[i].nombre, pdf );
    g_free( pdf );
    return ok;
}
