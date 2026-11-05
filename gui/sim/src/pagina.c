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

    if (flag != 1) return c;
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
    int re = v->det_reestimados, k;
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

/* Un termino cruzado: (c1 B + c2 B^2 ...) w_{B,t} o A_{B,t}. signo es el
 * del convenio: +1 para el AR (pasa al lado derecho sumando), -1 para el MA
 * (Theta(B) = I - SUM theta_k B^k).                                       */
static int cruzado( EqItem *it, int max, int primero, const VarmaOut *v, const char *A, const char *B,
                    int orden, int ma, double escala )
{
    int k, n = 0, alguno = 0;
    Co c[32];

    for (k = 1; k <= orden && k <= 32; k++) {
        char nombre[OV_NOMBRE];
        const OvParam *q;
        snprintf( nombre, sizeof nombre, "%s%d[%s<-%s]", ma ? "MA" : "AR", k, A, B );
        c[k - 1].v = 0; c[k - 1].se = 0; c[k - 1].has_se = 0;
        if (( q = ov_param( v, nombre ) )) {
            c[k - 1].v = ( ma ? -q->est : q->est ) * escala;
            c[k - 1].se = q->se * fabs( escala );
            c[k - 1].has_se = q->tiene_se && q->se > 0;
            alguno = 1;
        }
    }
    if (!alguno || max < 3 * orden + 4) return 0;
    if (!primero || ma) { memset( &it[n], 0, sizeof it[n] ); it[n].kind = EI_OP; it[n].sign = '+'; n++; }
    memset( &it[n], 0, sizeof it[n] ); it[n].kind = EI_OPEN; it[n].bracket = '('; n++;
    for (k = 0; k < orden && k < 32; k++) {
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

    if (!v || v->fallo) { snprintf( why, nw, "no hay sistema estimado" ); return 1; }
    if (i < 0 || i >= m->sis.n) { snprintf( why, nw, "no hay serie %d", i + 1 ); return 1; }
    if (eq_new( &eq )) { snprintf( why, nw, "sin memoria" ); return 1; }
    cruz_ar = g_new0( EqItem, 2048 );
    cruz_ma = g_new0( EqItem, 2048 );
    if (ecuacion( m, i, v, &eq, cruz_ar, cruz_ma, 2048, why, nw ) != 0) {
        eq_release( &eq ); g_free( cruz_ar ); g_free( cruz_ma );
        return 1;
    }

    /* EL GRAFICO Y LOS RESIDUOS GRANDES, con los residuos del .out. */
    res = g_new0( OvResiduos, 1 );
    if (m->pe.out && ov_residuos_file( m->pe.out, res ) == 0 && i < res->m && res->n > 1) {
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
        for (k = 0; k < N; k++) { ts.data[k + 1] = res->v[i][k]; s += res->v[i][k]; }
        med = s / N;
        for (k = 0; k < N; k++) s2 += ( res->v[i][k] - med ) * ( res->v[i][k] - med );
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
        nat = eq_outliers( res->v[i], N, med, dt, fechas ? fecha_de : NULL, NULL, at, N );
        free_vector( ts.data, 1, N );
    }

    rc = eq_page_pdf( pdf, graf, &eq, at, nat, fechas );
    if (graf) fd_fig_free( graf );
    g_res = NULL;
    g_free( at );
    g_free( res );
    eq_release( &eq );
    g_free( cruz_ar );
    g_free( cruz_ma );
    if (rc) snprintf( why, nw, "no pude escribir %s", pdf );
    return rc;
}

int pagina_cruzados( Sim *m, int i )
{
    Equation eq;
    EqItem *ca, *cm;
    char why[256];
    int n = -1;

    if (!m->pe.v || i < 0 || i >= m->sis.n || eq_new( &eq )) return -1;
    ca = g_new0( EqItem, 2048 );
    cm = g_new0( EqItem, 2048 );
    if (ecuacion( m, i, m->pe.v, &eq, ca, cm, 2048, why, sizeof why ) == 0) n = eq.ncruz_ar + eq.ncruz_ma;
    eq_release( &eq );
    g_free( ca ); g_free( cm );
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
