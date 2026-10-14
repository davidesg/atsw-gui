/*
 * formales.c -- los contrastes formales de art: ver formales.h.
 *
 * Las formulas son las de art/formal_tests.py, contrastadas numero a numero
 * con art-python en test_formales.c. Lo que NO se porta es lo que hay
 * encima de ellas: el veredicto encadenado.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gsl/gsl_cdf.h>

#include "formales.h"
#include "inpdet.h"
#include "outfile.h"
#include "raices.h"

/* Un LR algo negativo es el ruido del optimizador entre dos maximos iguales
 * y se lee como 0; por debajo de -TOL_LR el libre no llego a su maximo y el
 * LR no es un estadistico (art BUG-0236). */
#define TOL_LR 1e-3

/* Shin-Fuller (1998), Tabla II: n, 10 %, 5 %, 1 % */
static const double SF_CRIT[][4] = {
    {  25, 1.02, 1.68, 3.33 },
    {  50, 1.06, 1.75, 3.41 },
    { 100, 1.07, 1.75, 3.41 },
    { 250, 1.07, 1.76, 3.44 },
    { 500, 1.08, 1.77, 3.46 },
};

void fm_criticos_sf( int n, double *c10, double *c5, double *c1 )
{
    int k, m = (int) G_N_ELEMENTS( SF_CRIT );

    if ( n <= SF_CRIT[0][0] ) { *c10 = SF_CRIT[0][1]; *c5 = SF_CRIT[0][2]; *c1 = SF_CRIT[0][3]; return; }
    for ( k = 0; k + 1 < m; k++ )
        if ( n <= SF_CRIT[k + 1][0] )
            {
            double t = ( n - SF_CRIT[k][0] ) / ( SF_CRIT[k + 1][0] - SF_CRIT[k][0] );

            *c10 = SF_CRIT[k][1] + t * ( SF_CRIT[k + 1][1] - SF_CRIT[k][1] );
            *c5  = SF_CRIT[k][2] + t * ( SF_CRIT[k + 1][2] - SF_CRIT[k][2] );
            *c1  = SF_CRIT[k][3] + t * ( SF_CRIT[k + 1][3] - SF_CRIT[k][3] );
            return;
            }
    *c10 = SF_CRIT[m - 1][1]; *c5 = SF_CRIT[m - 1][2]; *c1 = SF_CRIT[m - 1][3];
}

/* Davis, Chen y Dunsmuir, Tabla 3.2: s = 4 y s = 12; entre medias y fuera,
 * interpolado en s y acotado, como art (_dcd_crit_s). */
void fm_criticos_dcd_s( int s, double *c10, double *c5, double *c1 )
{
    double w = ( s - 4 ) / 8.0;

    if ( w < 0.0 ) w = 0.0;
    if ( w > 1.0 ) w = 1.0;
    *c10 = ( 1 - w ) * 1.21 + w * 1.36;
    *c5  = ( 1 - w ) * 2.18 + w * 2.31;
    *c1  = ( 1 - w ) * 4.75 + w * 5.12;
}

static FmPrueba *nueva( FmPlan *pl, FmTipo tipo, int factor, const char *dir, const char *base )
{
    FmPrueba *t;

    if ( pl->n >= FM_MAX ) return NULL;
    t = &pl->t[pl->n++];
    memset( t, 0, sizeof *t );
    t->tipo = tipo;
    t->factor = factor;
    t->p = NAN;
    g_snprintf( t->base, sizeof t->base, "%s", base );
    g_snprintf( t->inp, sizeof t->inp, "%s%s%s.inp", dir, G_DIR_SEPARATOR_S, base );
    return t;
}

/* P(B) = 1 - c1 B - ... - cp B^p entre (1 - B/raiz): el factor que queda,
 * en la misma convencion. Es _deflacta de art, por division sintetica. */
static int deflacta( const double *c, int p, double raiz, double *resto )
{
    double r = 1.0 / raiz, q[ID_MAX_ORD];
    int    j;

    q[0] = 1.0;
    for ( j = 1; j < p; j++ ) q[j] = -c[j - 1] + r * q[j - 1];
    for ( j = 1; j < p; j++ ) resto[j - 1] = -q[j];
    return p - 1;
}

static void plan_estacional( const char *pre, const char *dir, FmPlan *pl )
{
    static char det[ID_MAX_DET][ID_LINEA];
    int         libre[ID_MAX_DET], nd, k, nn = 0;
    const char *nom[ID_MAX_DET];
    char        why[256];
    FmPrueba   *t;

    nd = id_det_lee( pre, det, libre, ID_MAX_DET, why, sizeof why );
    if ( nd < 0 ) { g_snprintf( pl->no_aplica[0], 256, "Simplificación estacional: %s.", why ); return; }
    for ( k = 0; k < nd; k++ )
        if ( libre[k] && ( g_str_has_prefix( det[k], "cos " ) || g_str_has_prefix( det[k], "sin " )
                           || !strcmp( det[k], "alter" ) ) )
            nom[nn++] = det[k];
    if ( nn == 0 )
        { g_snprintf( pl->no_aplica[0], 256, "Simplificación estacional: el modelo no tiene "
                      "armónicos libres." ); return; }
    if ( !( t = nueva( pl, FM_ESTACIONAL, -1, dir, "fm_estacional" ) ) ) return;
    t->gl = nn;
    g_snprintf( t->hipotesis, sizeof t->hipotesis, "los %d términos armónicos valen cero", nn );
    if ( id_fija_det( pre, t->inp, nom, nn, NULL, why, sizeof why ) != 0 )
        { pl->n--; g_snprintf( pl->no_aplica[0], 256, "Simplificación estacional: %s.", why ); }
}

static void plan_sf( const char *pre, const char *dir, const IdArma *a, int nobs, int s,
                     FmPlan *pl )
{
    int       i, j, libres = 0, hay_real = 0, dom = -1;
    double    phi_dom = 0.0, raiz_dom = 0.0, rho = 1.0 - 4.0 / nobs;
    char      why[256];
    FmPrueba *t;

    for ( i = 0; i < a->nf[0]; i++ )
        for ( j = 0; j < a->ord[0][i]; j++ ) libres += a->fl[0][i][j] != 0;
    if ( libres == 0 )
        { g_snprintf( pl->no_aplica[1], 256, "Shin-Fuller: el modelo no tiene AR regular libre." );
          return; }

    /* la raiz REAL POSITIVA mas cercana a +1, en cualquier factor */
    for ( i = 0; i < a->nf[0]; i++ )
        {
        RzAr r;

        if ( rz_factoriza( a->v[0][i], a->ord[0][i], s, NULL, &r ) != 0 ) continue;
        for ( j = 0; j < r.orden; j++ )
            {
            double re = r.zre[j], im = r.zim[j];

            if ( re == 0.0 && im == 0.0 ) continue;
            if ( fabs( im ) > 1e-8 * fmax( 1.0, fabs( re ) ) ) continue;
            hay_real = 1;
            if ( 1.0 / re > 0.0 && ( dom < 0 || 1.0 / re > phi_dom ) )
                { phi_dom = 1.0 / re; raiz_dom = re; dom = i; }
            }
        }
    if ( dom < 0 )
        {
        g_snprintf( pl->no_aplica[1], 256, hay_real
            ? "Shin-Fuller: las raíces reales del AR son negativas, y una raíz negativa no "
              "tiene nada que ver con d."
            : "Shin-Fuller: el AR no tiene ninguna raíz real que aislar; una raíz compleja "
              "cerca de la unidad es cosa del MEG, no de este contraste." );
        return;
        }

    if ( !( t = nueva( pl, FM_SF, dom, dir, "fm_sf" ) ) ) return;
    t->coef = phi_dom;
    t->nulo = rho;
    g_snprintf( t->hipotesis, sizeof t->hipotesis,
                "una raíz del AR en ρ = 1 − 4/n = %.4f (falta una diferencia regular)", rho );
    if ( a->ord[0][dom] == 1 )
        {
        if ( id_fija_coef( pre, t->inp, 0, dom, 0, rho, why, sizeof why ) != 0 ) goto mal;
        }
    else
        {
        double resto[ID_MAX_ORD];
        int    nr = deflacta( a->v[0][dom], a->ord[0][dom], raiz_dom, resto );

        if ( id_parte_ar( pre, t->inp, dom, rho, resto, nr, why, sizeof why ) != 0 ) goto mal;
        }
    return;
mal:
    pl->n--;
    g_snprintf( pl->no_aplica[1], 256, "Shin-Fuller: %s.", why );
}

static void plan_dcd( const char *pre, const char *dir, const IdArma *a, int k, FmPlan *pl )
{
    int  i, alguno = 0, largo = 0;
    char why[256], base[32];
    int  linea = ( k == 2 ) ? 2 : 3;

    for ( i = 0; i < a->nf[k]; i++ )
        {
        FmPrueba *t;

        if ( a->ord[k][i] != 1 ) { largo++; continue; }
        if ( !a->fl[k][i][0] ) continue;
        g_snprintf( base, sizeof base, "fm_dcd%s%d", k == 2 ? "" : "s", i + 1 );
        if ( !( t = nueva( pl, k == 2 ? FM_DCD : FM_DCD_S, i, dir, base ) ) ) return;
        t->coef = a->v[k][i][0];
        t->nulo = 1.0;
        g_snprintf( t->hipotesis, sizeof t->hipotesis, k == 2
            ? "θ = 1 en el MA regular %d (sobra una diferencia regular)"
            : "Θ = 1 en el MA anual %d (sobra la diferencia estacional)", i + 1 );
        if ( id_fija_coef( pre, t->inp, k, i, 0, 1.0, why, sizeof why ) != 0 )
            { pl->n--; g_snprintf( pl->no_aplica[linea], 256, "%s: %s.",
                                   k == 2 ? "DCD" : "DCD estacional", why ); return; }
        alguno++;
        }
    if ( !alguno )
        g_snprintf( pl->no_aplica[linea], 256, largo
            ? "%s: solo se contrasta un MA de orden 1, y el modelo los tiene de orden mayor."
            : "%s: el modelo no tiene MA %s libre de orden 1.",
            k == 2 ? "DCD" : "DCD estacional", k == 2 ? "regular" : "anual" );
}

/* LOS COEFICIENTES, DEL .out. El .pre los guarda con cuatro decimales y el
 * .out con seis: el coeficiente que se enseña, y la raiz que Shin-Fuller
 * aisla, salen de aqui. Las banderas siguen siendo las del .pre. Si el .out
 * no casa factor a factor con el .pre, se quedan los del .pre.          */
static void coefs_del_out( const char *out, IdArma *a )
{
    static const char *const sec[4] = {
        "Coefficients for regular AR factor", "Coefficients for annual AR factor",
        "Coefficients for regular MA factor", "Coefficients for annual MA factor" };
    IdArma  *b = g_new0( IdArma, 1 );
    gchar   *txt = NULL, **lin;
    int      i, k = -1, f = -1;

    if ( !g_file_get_contents( out, &txt, NULL, NULL ) ) { g_free( b ); return; }
    lin = g_strsplit( txt, "\n", -1 );
    g_free( txt );
    for ( i = 0; lin[i]; i++ )
        {
        int    m, usados = 0;
        double v;
        char   c;

        for ( m = 0; m < 4; m++ )
            if ( g_str_has_prefix( lin[i], sec[m] ) ) break;
        if ( m < 4 )
            {
            k = m;
            f = b->nf[k] < ID_MAX_FAC ? b->nf[k]++ : -1;
            continue;
            }
        if ( k >= 0 && f >= 0 && sscanf( lin[i], " %lf%n", &v, &usados ) == 1 &&
             ( sscanf( lin[i] + usados, " %c", &c ) != 1 || c == '(' ) )
            {
            if ( b->ord[k][f] < ID_MAX_ORD ) b->v[k][f][b->ord[k][f]++] = v;
            continue;
            }
        k = -1;
        }
    g_strfreev( lin );
    for ( k = 0; k < 4; k++ )
        {
        int casa = b->nf[k] == a->nf[k];

        for ( f = 0; casa && f < a->nf[k]; f++ ) casa = b->ord[k][f] == a->ord[k][f];
        if ( casa ) memcpy( a->v[k], b->v[k], sizeof a->v[k] );
        }
    g_free( b );
}

int fm_plan( const char *pre, const char *out, const char *dir, int nobs, int s, int d,
             FmPlan *pl, char *why, size_t nwhy )
{
    IdArma *a;

    memset( pl, 0, sizeof *pl );
    pl->nobs = nobs; pl->s = s; pl->d = d;
    if ( nobs < 5 ) { g_snprintf( why, nwhy, "el .out no dice cuántas observaciones hay" ); return 1; }
    a = g_new0( IdArma, 1 );
    if ( id_arma_lee( pre, a, why, nwhy ) != 0 ) { g_free( a ); return 1; }
    if ( out ) coefs_del_out( out, a );
    plan_estacional( pre, dir, pl );
    plan_sf( pre, dir, a, nobs, s, pl );
    plan_dcd( pre, dir, a, 2, pl );
    if ( s > 1 ) plan_dcd( pre, dir, a, 3, pl );
    g_free( a );
    return 0;
}

void fm_resultado( FmPrueba *t, double l_libre, int nobs, int s, int corrio )
{
    FueOut *o;
    gchar  *out;
    double  dl;

    t->hecho = 1;
    t->l_libre = l_libre;
    t->l_restr = NAN;
    t->calculable = 0;
    t->rechaza = 0;
    switch ( t->tipo )
        {
        case FM_ESTACIONAL:
            t->c10 = gsl_cdf_chisq_Pinv( 0.90, t->gl );
            t->c5  = gsl_cdf_chisq_Pinv( 0.95, t->gl );
            t->c1  = gsl_cdf_chisq_Pinv( 0.99, t->gl );
            break;
        case FM_SF:    fm_criticos_sf( nobs, &t->c10, &t->c5, &t->c1 ); break;
        case FM_DCD:   t->c10 = 1.00; t->c5 = 1.94; t->c1 = 4.41; break;
        case FM_DCD_S: fm_criticos_dcd_s( s, &t->c10, &t->c5, &t->c1 ); break;
        }

    out = g_strdup( t->inp );
    if ( g_str_has_suffix( out, ".inp" ) ) strcpy( out + strlen( out ) - 4, ".out" );
    o = g_new0( FueOut, 1 );
    if ( corrio && fueout_read( out, o ) && o->tiene_logelf ) t->l_restr = o->logelf;
    g_free( o );
    g_free( out );

    if ( !isfinite( t->l_restr ) )
        { g_snprintf( t->motivo, sizeof t->motivo, "la estimación del modelo restringido "
                      "falló: no hay log-verosimilitud con que comparar" ); return; }
    if ( !isfinite( l_libre ) )
        { g_snprintf( t->motivo, sizeof t->motivo, "el modelo estimado no trae su "
                      "log-verosimilitud (logelf) en el .out" ); return; }
    dl = l_libre - t->l_restr;
    if ( dl < -TOL_LR / 2 )
        { g_snprintf( t->motivo, sizeof t->motivo, "el restringido queda POR ENCIMA del "
                      "estimado (ΔL = %.4f): el estimado no está en su máximo", dl ); return; }
    if ( dl < 0.0 ) dl = 0.0;
    t->calculable = 1;

    switch ( t->tipo )
        {
        case FM_ESTACIONAL:
            t->est = 2.0 * dl;
            t->p = chisq_cola( t->est, t->gl );
            t->rechaza = t->p < 0.05;
            break;
        case FM_SF:
            /* ec. (3.5): CERO si la raiz esta por encima de rho_m. En ese
               lado los datos son al menos tan integrados como la nula, y la
               distancia no es evidencia de estacionariedad (BUG-0065). */
            t->al_otro_lado = t->coef >= t->nulo;
            t->est = t->coef > t->nulo ? 0.0 : dl;
            t->rechaza = !t->al_otro_lado && t->est > t->c5;
            break;
        case FM_DCD:
        case FM_DCD_S:
            t->est = 2.0 * dl;
            t->rechaza = t->est > t->c5;
            break;
        }
}
