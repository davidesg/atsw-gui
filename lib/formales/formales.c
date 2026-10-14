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
#include <glib/gstdio.h>
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

/* EL PAR COMPLEJO (frecuencias interiores, ley s = 2): por n, de la
 * simulacion del estudio SF_MEG, con las cinco interiores juntas; por
 * encima de 960, los asintoticos. Redondeados a tres decimales, como art
 * (_dcd_crit). */
static const double DCDF_CRIT[][4] = {
    { 120, 1.12, 2.06, 4.64 },
    { 240, 1.13, 2.07, 4.52 },
    { 480, 1.10, 2.04, 4.53 },
    { 960, 1.11, 2.03, 4.52 },
};

static double tres( double x ) { return floor( x * 1000.0 + 0.5 ) / 1000.0; }

void fm_criticos_dcd_f( int n, double *c10, double *c5, double *c1 )
{
    int    k, m = (int) G_N_ELEMENTS( DCDF_CRIT );
    double a = 1.11, b = 2.04, c = 4.52;

    if ( n <= DCDF_CRIT[0][0] ) { a = DCDF_CRIT[0][1]; b = DCDF_CRIT[0][2]; c = DCDF_CRIT[0][3]; }
    else if ( n < DCDF_CRIT[m - 1][0] )
        for ( k = 0; k + 1 < m; k++ )
            if ( n <= DCDF_CRIT[k + 1][0] )
                {
                double w = ( n - DCDF_CRIT[k][0] ) / ( DCDF_CRIT[k + 1][0] - DCDF_CRIT[k][0] );

                a = ( 1 - w ) * DCDF_CRIT[k][1] + w * DCDF_CRIT[k + 1][1];
                b = ( 1 - w ) * DCDF_CRIT[k][2] + w * DCDF_CRIT[k + 1][2];
                c = ( 1 - w ) * DCDF_CRIT[k][3] + w * DCDF_CRIT[k + 1][3];
                break;
                }
    *c10 = tres( a ); *c5 = tres( b ); *c1 = tres( c );
}

/* El orden de los factores individuales de la diferencia anual: 1 en f = 0
 * y en Nyquist, 2 en las interiores (fue.c, ornsop). */
static int orden_ifadf( const int ifadf[7], int s )
{
    int k, o = 0;

    for ( k = 0; k <= s / 2 && k < 7; k++ )
        if ( ifadf[k] ) o += ( k == 0 || 2 * k == s ) ? 1 : 2;
    return o;
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
    /* EL TESTIGO DE NYQUIST de un modelo reformulado tras el MEG: un MA(1)
       regular de raiz negativa junto a ifadf[s/2] = 1. Su frontera es
       theta = -1, la que cancela (1 + B), no theta = +1: contrastarlo en +1
       mide la frecuencia cero con un factor que apunta a pi (lo que hace
       art, que lo trata como un MA mas). */
    int  nyq = k == 2 && pl->s >= 2 && pl->s / 2 < 7 && pl->ifadf[pl->s / 2], testigo = -1;

    /* el testigo es UNO: el MA(1) libre mas cercano a -1. Un MA(1) negativo
       del modelo (theta = -0.43) no es un testigo, y sigue en +1. */
    for ( i = 0; nyq && i < a->nf[2]; i++ )
        if ( a->ord[2][i] == 1 && a->fl[2][i][0] && a->v[2][i][0] < 0.0 &&
             ( testigo < 0 || a->v[2][i][0] < a->v[2][testigo][0] ) ) testigo = i;

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
        if ( i == testigo )
            {
            t->nulo = -1.0;
            t->freq = pl->s / 2;
            g_snprintf( t->hipotesis, sizeof t->hipotesis, "θ = −1 en el MA regular %d, el "
                        "testigo de Nyquist: cancela el (1 + B) y la estacionalidad en f = %d "
                        "es determinista", i + 1, pl->s / 2 );
            }
        if ( id_fija_coef( pre, t->inp, k, i, 0, t->nulo, why, sizeof why ) != 0 )
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

/* ---- los de varias etapas -------------------------------------------- */

static void pon_base( const FmPlan *pl, FmPrueba *t, const char *base )
{
    g_snprintf( t->base, sizeof t->base, "%s", base );
    g_snprintf( t->inp, sizeof t->inp, "%s%s%s.inp", pl->dir, G_DIR_SEPARATOR_S, base );
}

/* la ruta de base con otra extension */
static gchar *con_ext( const FmPlan *pl, const char *base, const char *ext )
{
    return g_strdup_printf( "%s%s%s%s", pl->dir, G_DIR_SEPARATOR_S, base, ext );
}

static double logelf( const char *out )
{
    gchar *t = NULL, *p;
    double v = NAN;

    if ( g_file_get_contents( out, &t, NULL, NULL ) && ( p = strstr( t, "logelf:" ) ) )
        v = g_ascii_strtod( p + 7, NULL );
    g_free( t );
    return v;
}

/* El modelo que fue dejo en base: el .pre, con los coeficientes del .out */
static int lee_modelo( const FmPlan *pl, const char *base, IdArma *a )
{
    gchar *pre = con_ext( pl, base, ".pre" ), *out = con_ext( pl, base, ".out" );
    char   why[256];
    int    r = id_arma_lee( pre, a, why, sizeof why );

    if ( r == 0 ) coefs_del_out( out, a );
    g_free( pre ); g_free( out );
    return r;
}

static double coef_ma( const FmPlan *pl, const char *base, int idx )
{
    IdArma *a = g_new0( IdArma, 1 );
    double  v = NAN;

    if ( lee_modelo( pl, base, a ) == 0 && idx < a->nf[2] && a->ord[2][idx] > 0 )
        v = a->v[2][idx][0];
    g_free( a );
    return v;
}

/* Reescribe el .pre de desde con el MA regular idx en (valor, bandera) y lo
 * deja como el .inp de base. */
static double coef_maf_out( const char *out, int idx );

/* EL .pre CON LOS COEFICIENTES DEL .out, en destino. El .pre los guarda con
 * cuatro decimales, y eso no es solo precision: un AR(2) en 1.375800 y
 * -0.375801 queda en 1.3758 y -0.3758, que suman 1, y fue no arranca de una
 * raiz unitaria (RIPC.2 en el MEG). art parte de los valores exactos. Lo
 * ARMA y los MA(2) de frecuencia fija salen del .out (seis decimales); lo
 * demas, del .pre. Sin .out, el .pre tal cual. 0, o 1.                   */
static int preciso( const char *pre, const char *out, const char *destino )
{
    IdArma *a = g_new0( IdArma, 1 );
    IdMaf   m;
    gchar  *t = g_strdup_printf( "%s.t", destino );
    char    why[256];
    int     r = 1, i;

    if ( id_arma_lee( pre, a, why, sizeof why ) != 0 ) goto fin;
    if ( out && g_file_test( out, G_FILE_TEST_EXISTS ) ) coefs_del_out( out, a );
    if ( id_escribe_arma( pre, t, a, why, sizeof why ) != 0 ) goto fin;
    if ( id_maf_lee( t, &m, why, sizeof why ) != 0 ) goto fin;
    for ( i = 0; out && i < m.n; i++ )
        {
        double v = coef_maf_out( out, i );

        if ( isfinite( v ) ) m.v[i] = v;
        }
    r = id_escribe_maf( t, destino, &m, why, sizeof why );
fin:
    g_unlink( t );
    g_free( t ); g_free( a );
    return r;
}

/* el .pre de base, con sus coeficientes exactos, en destino */
static int preciso_de( const FmPlan *pl, const char *base, const char *destino )
{
    gchar *pre = con_ext( pl, base, ".pre" ), *out = con_ext( pl, base, ".out" );
    int    r = preciso( pre, out, destino );

    g_free( pre ); g_free( out );
    return r;
}

/* Reescribe el .pre de desde --con los coeficientes del .out-- con el MA
 * regular idx en (valor, bandera) y lo deja como el .inp de base. */
static int ma_en( const FmPlan *pl, const char *desde, const char *base, int idx,
                  double valor, int flag )
{
    IdArma *a = g_new0( IdArma, 1 );
    gchar  *inp = con_ext( pl, base, ".inp" ), *t = g_strdup_printf( "%s.t0", inp );
    char    why[256];
    int     r = 1;

    if ( preciso_de( pl, desde, t ) == 0 && id_arma_lee( t, a, why, sizeof why ) == 0 &&
         idx < a->nf[2] )
        {
        a->ord[2][idx] = 1;
        a->v[2][idx][0] = valor;
        a->fl[2][idx][0] = flag;
        r = id_escribe_arma( t, inp, a, why, sizeof why );
        }
    g_unlink( t );
    g_free( a ); g_free( inp ); g_free( t );
    return r;
}

/* La raiz AR real POSITIVA mas cercana a +1, en los factores regulares con
 * algun coeficiente libre: phi, y el factor y la raiz en B. 0 si no hay. */
static int ar_dominante( const IdArma *a, int s, double *phi, int *fac, double *raiz )
{
    int i, j, k, hay = 0;

    for ( i = 0; i < a->nf[0]; i++ )
        {
        RzAr r;
        int  libre = 0;

        for ( k = 0; k < a->ord[0][i]; k++ ) libre |= a->fl[0][i][k] != 0;
        if ( !libre || rz_factoriza( a->v[0][i], a->ord[0][i], s, NULL, &r ) != 0 ) continue;
        for ( j = 0; j < r.orden; j++ )
            {
            double re = r.zre[j], im = r.zim[j];

            if ( ( re == 0.0 && im == 0.0 ) || fabs( im ) > 1e-8 * fmax( 1.0, fabs( re ) ) ||
                 re <= 0.0 ) continue;
            if ( !hay || 1.0 / re > *phi ) { *phi = 1.0 / re; *fac = i; *raiz = re; hay = 1; }
            }
        }
    return hay;
}

/* Parte el AR regular fac en (1 - rho B) FIJO por el resto libre. */
static void parte_ar( IdArma *a, int fac, double rho, double raiz )
{
    double resto[ID_MAX_ORD];
    int    p = a->ord[0][fac], nr = p > 1 ? deflacta( a->v[0][fac], p, raiz, resto ) : 0;
    int    i, j;

    if ( nr > 0 && a->nf[0] < ID_MAX_FAC )
        {
        for ( i = a->nf[0]; i > fac + 1; i-- )
            {
            a->ord[0][i] = a->ord[0][i - 1];
            memcpy( a->v[0][i], a->v[0][i - 1], sizeof a->v[0][i] );
            memcpy( a->fl[0][i], a->fl[0][i - 1], sizeof a->fl[0][i] );
            }
        a->nf[0]++;
        a->ord[0][fac + 1] = nr;
        for ( j = 0; j < nr; j++ ) { a->v[0][fac + 1][j] = resto[j]; a->fl[0][fac + 1][j] = 1; }
        }
    a->ord[0][fac] = 1;
    a->v[0][fac][0] = rho;
    a->fl[0][fac][0] = 0;
}

/* ∇ con su potencia en superindice: «∇²» y no «∇^2» */
static const char *nabla( int d )
{
    static const char *const v[] = { "∇⁰", "∇", "∇²", "∇³" };

    return d >= 0 && d <= 3 ? v[d] : "∇ⁿ";
}

static void plan_sobre( FmPlan *pl, const IdArma *base )
{
    IdArma   *a;
    FmPrueba *t;
    int       ifadf[7], i, k, nyq = 0, p_ar = 0, n_ma1 = 0;
    char      why[256];
    gchar    *t1, *t2;
    GString  *q;

    /* desde d = 2 no se sube (art BUG-0235): ∇³ no es una hipotesis */
    if ( pl->d >= 2 )
        { g_snprintf( pl->no_aplica[4], 256, "Sobrediferenciación: desde d = %d no se "
                      "contrasta; una diferencia más no es una hipótesis.", pl->d ); return; }
    if ( id_ifadf( pl->pre, ifadf, why, sizeof why ) == 0 && pl->s >= 2 && pl->s / 2 < 7 )
        nyq = ifadf[pl->s / 2] == 1;

    /* EL CANDIDATO: d + 1, sin media (la ∇ extra la anula), el AR del
       modelo dentro, y el testigo como MA regular: sustituye a los MA(1);
       un MA(q >= 2) se conserva (BUG-0243). Con Nyquist estocastico, su
       testigo se queda y este va en su propio factor (BUG-0009). */
    a = g_new0( IdArma, 1 );
    *a = *base;
    for ( i = 0; i < a->nf[0]; i++ )
        for ( k = 0; k < a->ord[0][i]; k++ ) p_ar += a->fl[0][i][k] != 0;
    pl->p_ar = p_ar;
    q = g_string_new( "" );
    if ( !nyq )
        {
        int n = 0;

        for ( i = 0; i < a->nf[2]; i++ )
            if ( a->ord[2][i] >= 2 )
                {
                g_string_append_printf( q, ", su MA(%d) conservado (reestimado)", a->ord[2][i] );
                if ( n != i )
                    {
                    a->ord[2][n] = a->ord[2][i];
                    memcpy( a->v[2][n], a->v[2][i], sizeof a->v[2][n] );
                    memcpy( a->fl[2][n], a->fl[2][i], sizeof a->fl[2][n] );
                    }
                n++;
                }
            else n_ma1 += a->fl[2][i][0] != 0;
        a->nf[2] = n;
        }
    if ( a->nf[2] >= ID_MAX_FAC ) { g_free( a ); g_string_free( q, TRUE ); return; }
    i = a->nf[2]++;
    a->ord[2][i] = 1; a->v[2][i][0] = 0.85; a->fl[2][i][0] = 1;

    if ( !( t = nueva( pl, FM_SOBRE, i, pl->dir, "fm_sobre_a" ) ) )
        { g_free( a ); g_string_free( q, TRUE ); return; }
    t->idx = i;
    t->nulo = 1.0;
    t->c10 = 1.00; t->c5 = 1.94; t->c1 = 4.41;
    t->phi_r = NAN; t->rho_m = NAN;         /* el AR del candidato no se acota */
    g_snprintf( t->hipotesis, sizeof t->hipotesis, "θ = 1 en un testigo MA(1) sobre %s "
                "(la diferencia extra sobra: con d = %d basta)", nabla( pl->d + 1 ), pl->d );
    g_snprintf( t->testigo, sizeof t->testigo, "un MA(1) añadido sobre %s, desde +0.85",
                nabla( pl->d + 1 ) );
    g_snprintf( t->candidato, sizeof t->candidato, "%s%s%s%s, sin media", nabla( pl->d + 1 ),
                p_ar ? ", con el AR regular del modelo (reestimado) dentro" : ", sin AR regular",
                n_ma1 ? ", su MA(1) regular sustituido por el testigo" : "", q->str );
    g_string_free( q, TRUE );

    t1 = con_ext( pl, "fm_sobre_t1", ".inp" );
    t2 = con_ext( pl, "fm_sobre_t2", ".inp" );
    if ( id_pon_transformacion( pl->pre, t1, pl->lam, pl->d + 1, pl->D, why, sizeof why ) ||
         id_fija_media( t1, t2, 0.0, 0, why, sizeof why ) ||
         id_escribe_arma( t2, t->inp, a, why, sizeof why ) )
        { pl->n--; g_snprintf( pl->no_aplica[4], 256, "Sobrediferenciación: %s.", why ); }
    g_unlink( t1 ); g_unlink( t2 );
    g_free( t1 ); g_free( t2 ); g_free( a );
}


/* ---- el MEG y el DCD_f ----------------------------------------------- */

/* Los armonicos deterministas de la frecuencia f, con los nombres del .inp:
 * «cos f» y «sin f», o «alter» en Nyquist. Cuantos hay. */
static int armonicos_en( const char *pre, int s, int f, char nom[2][ID_LINEA] )
{
    static char det[ID_MAX_DET][ID_LINEA];
    int         libre[ID_MAX_DET], nd, k, nn = 0;
    char        why[256], c[32], si[32];

    nd = id_det_lee( pre, det, libre, ID_MAX_DET, why, sizeof why );
    g_snprintf( c, sizeof c, "cos %d", f );
    g_snprintf( si, sizeof si, "sin %d", f );
    for ( k = 0; k < nd && nn < 2; k++ )
        if ( 2 * f == s ? !strcmp( det[k], "alter" ) : !strcmp( det[k], c ) || !strcmp( det[k], si ) )
            g_snprintf( nom[nn++], ID_LINEA, "%s", det[k] );
    return nn;
}

int fm_reformula( const char *pre, const char *out, const char *destino, int s, int f,
                  int *testigo, char *why, size_t nwhy )
{
    char         nom[2][ID_LINEA];
    const char  *pn[2] = { nom[0], nom[1] };
    int          ifadf[7], nn, r = 1;
    gchar       *t0 = g_strdup_printf( "%s.t0", destino );
    gchar       *t1 = g_strdup_printf( "%s.t1", destino ), *t2 = g_strdup_printf( "%s.t2", destino );

    if ( s < 2 || f < 1 || 2 * f > s || s / 2 >= 7 )
        { g_snprintf( why, nwhy, "f = %d no es una frecuencia estacional de s = %d", f, s ); goto fin; }
    if ( id_ifadf( pre, ifadf, why, nwhy ) != 0 ) goto fin;
    if ( ifadf[f] )
        { g_snprintf( why, nwhy, "f = %d ya es estocástica (ifadf[%d] = 1)", f, f ); goto fin; }
    nn = armonicos_en( pre, s, f, nom );
    if ( nn == 0 )
        { g_snprintf( why, nwhy, 2 * f == s ? "no hay término alter (−1)ᵗ que reformular"
                                            : "no hay armónicos cos/sin en f = %d que reformular", f );
          goto fin; }
    ifadf[f] = 1;
    if ( preciso( pre, out, t0 ) != 0 )
        { g_snprintf( why, nwhy, "no pude leer los coeficientes del modelo" ); goto fin; }
    if ( id_quita_det( t0, t1, pn, nn, NULL, why, nwhy ) != 0 ||
         id_pon_ifadf( t1, t2, ifadf, why, nwhy ) != 0 ) goto fin;
    if ( 2 * f == s )
        {
        /* Nyquist: el factor es (1 + B), de orden 1, y su testigo un MA(1)
           regular (1 + 0.9 B): -0.9 en la convencion de fue */
        IdArma *a = g_new0( IdArma, 1 );

        if ( id_arma_lee( t2, a, why, nwhy ) == 0 )
            {
            if ( a->nf[2] >= ID_MAX_FAC ) g_snprintf( why, nwhy, "no cabe otro MA regular" );
            else
                {
                int i = a->nf[2]++;

                a->ord[2][i] = 1; a->v[2][i][0] = -0.9; a->fl[2][i][0] = 1;
                if ( testigo ) *testigo = i;
                r = id_escribe_arma( t2, destino, a, why, nwhy );
                }
            }
        g_free( a );
        }
    else
        {
        /* interior: (1 - 2cos(w) B + B^2), y su testigo un MA(2) de frecuencia
           fija en f con lambda^2 = 0.9 */
        IdMaf m;

        if ( id_maf_lee( t2, &m, why, nwhy ) == 0 )
            {
            if ( m.n >= ID_MAX_FAC ) g_snprintf( why, nwhy, "no cabe otro MA de frecuencia fija" );
            else
                {
                m.freq[m.n] = f; m.v[m.n] = -0.9; m.fl[m.n] = 1;
                if ( testigo ) *testigo = m.n;
                m.n++;
                r = id_escribe_maf( t2, destino, &m, why, nwhy );
                }
            }
        }
fin:
    g_unlink( t0 ); g_unlink( t1 ); g_unlink( t2 );
    g_free( t0 ); g_free( t1 ); g_free( t2 );
    return r;
}

static void plan_meg( FmPlan *pl )
{
    int f, alguna = 0;

    if ( pl->s < 2 ) return;
    if ( pl->D != 0 )
        { g_snprintf( pl->no_aplica[6], 256, "MEG: contrasta armónicos deterministas, así que "
                      "pide D = 0 con armónicos; con la diferencia estacional completa, su lado "
                      "del par es el DCD estacional." ); return; }
    if ( pl->n_armon == 0 )
        { g_snprintf( pl->no_aplica[6], 256, "MEG: el modelo no tiene armónicos deterministas "
                      "(cos/sin, alter) que contrastar contra su forma estocástica." ); return; }
    if ( pl->s / 2 >= 7 )
        { g_snprintf( pl->no_aplica[6], 256, "MEG: s = %d es más de lo que lleva la línea de "
                      "factores de la diferencia anual.", pl->s ); return; }

    for ( f = 1; 2 * f <= pl->s; f++ )
        {
        FmPrueba *t;
        char      base[32], why[256], nom[2][ID_LINEA];
        int       ifadf[7], nyq = 2 * f == pl->s;

        g_snprintf( base, sizeof base, "fm_meg%d_a", f );
        if ( !( t = nueva( pl, FM_MEG, -1, pl->dir, base ) ) ) return;
        t->freq = f;
        t->nulo = -1.0;
        t->phi_r = NAN; t->rho_m = NAN; t->l_testigo = NAN;
        memcpy( ifadf, pl->ifadf, sizeof ifadf );
        ifadf[f] = 1;
        t->n_crit = pl->nobs - pl->d - pl->D * pl->s - orden_ifadf( ifadf, pl->s );
        if ( nyq ) { t->c10 = 1.00; t->c5 = 1.94; t->c1 = 4.41; }
        else       fm_criticos_dcd_f( t->n_crit, &t->c10, &t->c5, &t->c1 );
        g_snprintf( t->hipotesis, sizeof t->hipotesis, nyq
            ? "el testigo (1 + θB) en θ = −1: cancela el (1 + B) y la estacionalidad en f = %d "
              "es determinista"
            : "el testigo de frecuencia fija en θ₂ = −1: cancela la raíz unitaria en f = %d y la "
              "estacionalidad ahí es determinista", f );
        g_snprintf( t->testigo, sizeof t->testigo, nyq
            ? "un MA(1) regular (1 + θB), desde θ = −0.9"
            : "un MA(2) de frecuencia fija en f = %d, desde θ₂ = −0.9", f );
        if ( pl->ifadf[f] )
            { t->omitida = 1; g_snprintf( why, sizeof why, "ya es estocástica (ifadf[%d] = 1): "
                                          "se contrasta sobre el modelo determinista", f ); }
        else if ( armonicos_en( pl->pre, pl->s, f, nom ) == 0 )
            { t->omitida = 1; g_snprintf( why, sizeof why, nyq
                ? "el modelo no tiene el término alter (−1)ᵗ: no hay forma determinista que contrastar"
                : "el modelo no tiene el cos/sin de f = %d: no hay forma determinista que contrastar", f ); }
        else if ( fm_reformula( pl->pre, pl->out[0] ? pl->out : NULL, t->inp, pl->s, f, &t->idx,
                                why, sizeof why ) != 0 )
            t->omitida = 1;
        if ( t->omitida )
            {
            t->hecho = 1;
            t->calculable = 0;
            g_snprintf( t->motivo, sizeof t->motivo, "sin contrastar: %s", why );
            }
        else alguna++;
        }
    (void) alguna;
}


static void plan_dcd_f( FmPlan *pl )
{
    IdMaf m;
    char  why[256];
    int   i, alguno = 0;

    if ( id_maf_lee( pl->pre, &m, why, sizeof why ) != 0 || m.n == 0 ) return;
    for ( i = 0; i < m.n; i++ )
        {
        FmPrueba *t;
        char      base[32];
        IdMaf     r = m;

        if ( !m.fl[i] ) continue;
        g_snprintf( base, sizeof base, "fm_dcdf%d", i + 1 );
        if ( !( t = nueva( pl, FM_DCD_F, i, pl->dir, base ) ) ) return;
        t->idx = i;
        t->freq = (int) lround( m.freq[i] );
        t->coef = pl->out[0] ? coef_maf_out( pl->out, i ) : NAN;
        if ( !isfinite( t->coef ) ) t->coef = m.v[i];
        t->nulo = -1.0;
        t->n_crit = pl->nobs - pl->d - pl->D * pl->s - orden_ifadf( pl->ifadf, pl->s );
        fm_criticos_dcd_f( t->n_crit, &t->c10, &t->c5, &t->c1 );
        g_snprintf( t->hipotesis, sizeof t->hipotesis, "θ₂ = −1 en el MA de frecuencia fija %d "
                    "(f = %d): cancela la raíz unitaria de ahí y la estacionalidad es determinista",
                    i + 1, t->freq );
        r.v[i] = -1.0; r.fl[i] = 0;
        if ( id_escribe_maf( pl->pre, t->inp, &r, why, sizeof why ) != 0 )
            { pl->n--; g_snprintf( pl->no_aplica[7], 256, "DCD_f: %s.", why ); return; }
        alguno++;
        }
    (void) alguno;
}

/* El theta_2 del MA de frecuencia fija idx, del .out: seis decimales. NAN
 * si no esta. */
static double coef_maf_out( const char *out, int idx )
{
    gchar  *txt = NULL, **lin, *cab = g_strdup_printf( "Coefficients for regular f-fixed MA factor %d ",
                                                        idx + 1 );
    double  v = NAN;
    int     i;

    if ( g_file_get_contents( out, &txt, NULL, NULL ) )
        {
        lin = g_strsplit( txt, "\n", -1 );
        for ( i = 0; lin[i]; i++ )
            if ( g_str_has_prefix( lin[i], cab ) && lin[i + 1] && lin[i + 2] )
                { v = g_ascii_strtod( lin[i + 2], NULL ); break; }
        g_strfreev( lin );
        }
    g_free( txt ); g_free( cab );
    return v;
}

/* Reescribe el .pre de desde con el MA de frecuencia fija idx en (valor,
 * bandera) y lo deja como el .inp de base. */
static int maf_en( const FmPlan *pl, const char *desde, const char *base, int idx,
                   double valor, int flag )
{
    IdMaf  m;
    gchar *inp = con_ext( pl, base, ".inp" ), *t = g_strdup_printf( "%s.t0", inp );
    char   why[256];
    int    r = 1;

    if ( preciso_de( pl, desde, t ) == 0 && id_maf_lee( t, &m, why, sizeof why ) == 0 &&
         idx < m.n )
        {
        m.v[idx] = valor;
        m.fl[idx] = flag;
        r = id_escribe_maf( t, inp, &m, why, sizeof why );
        }
    g_unlink( t );
    g_free( inp ); g_free( t );
    return r;
}

/* El final del MEG: el LR del testigo, sin reintento (como art) */
static void meg_fin( FmPrueba *t )
{
    double lr;

    t->hecho = 1;
    t->l_libre = t->l_testigo;
    t->calculable = 0;
    if ( !isfinite( t->l_restr ) )
        { g_snprintf( t->motivo, sizeof t->motivo, "la estimación con el testigo en la frontera "
                      "falló" ); return; }
    lr = 2.0 * ( t->l_testigo - t->l_restr );
    if ( lr < -TOL_LR )
        { g_snprintf( t->motivo, sizeof t->motivo, "el modelo estocástico libre queda POR DEBAJO "
                      "del que tiene el testigo en la frontera (LR = %.3f): el máximo libre no se "
                      "alcanzó, y eso no es un veredicto", lr ); return; }
    if ( lr < 0.0 ) lr = 0.0;
    t->est = lr;
    t->calculable = 1;
    t->rechaza = lr > t->c5;
}

int fm_plan_sub( FmPlan *pl, char *why, size_t nwhy )
{
    IdArma   *a;
    FmPrueba *t;
    int       i, j, cand = -1, comp = -1;
    double    th = 0.0, raiz = 0.0, modc = 0.0;

    if ( pl->d < 1 )
        { g_snprintf( why, nwhy, "con d = 0 no hay diferencia que cuestionar" ); return 1; }
    for ( i = 0; i < pl->n; i++ )
        if ( pl->t[i].tipo == FM_SUB ) { g_snprintf( why, nwhy, "ya está planteado" ); return 1; }
    a = g_new0( IdArma, 1 );
    if ( id_arma_lee( pl->pre, a, why, nwhy ) != 0 ) { g_free( a ); return 1; }
    if ( pl->out[0] ) coefs_del_out( pl->out, a );

    /* EL TESTIGO ES UNA RAIZ, NO UN FACTOR DE ORDEN 1 (BUG-0243): la raiz
       REAL del MA regular mas cercana a +1, en un factor libre entero. */
    for ( i = 0; i < a->nf[2]; i++ )
        {
        RzAr r;
        int  todo = a->ord[2][i] > 0;

        for ( j = 0; j < a->ord[2][i]; j++ ) todo &= a->fl[2][i][j] != 0;
        if ( !todo || rz_factoriza( a->v[2][i], a->ord[2][i], pl->s, NULL, &r ) != 0 ) continue;
        for ( j = 0; j < r.orden; j++ )
            {
            double re = r.zre[j], im = r.zim[j];

            if ( re == 0.0 && im == 0.0 ) continue;
            if ( fabs( im ) <= 1e-8 * fmax( 1.0, fabs( re ) ) )
                { if ( cand < 0 || 1.0 / re > th ) { th = 1.0 / re; cand = i; raiz = re; } }
            else if ( comp < 0 ) { comp = i; modc = 1.0 / hypot( re, im ); }
            }
        }

    if ( !( t = nueva( pl, FM_SUB, -1, pl->dir, "fm_sub_a" ) ) ) { g_free( a ); return 1; }
    t->nulo = 1.0;
    t->c10 = 1.00; t->c5 = 1.94; t->c1 = 4.41;
    t->phi_r = NAN; t->rho_m = NAN;
    g_snprintf( t->hipotesis, sizeof t->hipotesis, "θ = 1 en el MA regular (la última "
                "diferencia sobra: con d = %d bastaba)", pl->d - 1 );
    if ( cand >= 0 || comp >= 0 )
        {
        int    f = cand >= 0 ? cand : comp, p = a->ord[2][f], nr = p - 1, k;
        double resto[ID_MAX_ORD];

        if ( cand >= 0 && p > 1 ) deflacta( a->v[2][f], p, raiz, resto );
        else for ( k = 0; k < nr; k++ ) resto[k] = 0.0;
        if ( nr > 0 && a->nf[2] < ID_MAX_FAC )
            {
            for ( k = a->nf[2]; k > f + 1; k-- )
                {
                a->ord[2][k] = a->ord[2][k - 1];
                memcpy( a->v[2][k], a->v[2][k - 1], sizeof a->v[2][k] );
                memcpy( a->fl[2][k], a->fl[2][k - 1], sizeof a->fl[2][k] );
                }
            a->nf[2]++;
            a->ord[2][f + 1] = nr;
            for ( k = 0; k < nr; k++ ) { a->v[2][f + 1][k] = resto[k]; a->fl[2][f + 1][k] = 1; }
            }
        a->ord[2][f] = 1; a->v[2][f][0] = 1.0; a->fl[2][f][0] = 0;
        t->idx = f;
        t->coef = cand >= 0 ? th : modc;
        t->raices_complejas = cand < 0;
        g_snprintf( t->testigo, sizeof t->testigo, cand < 0
                    ? "el MA(%d) del modelo, de raíces complejas" : p == 1
                    ? "el MA(1) del modelo" : "la raíz real del MA(%d) más cercana a +1", p );
        t->etapa = 1;                       /* el libre es el modelo: al restringido */
        t->l_testigo = NAN;                 /* fm_avanza pone el del modelo        */
        pon_base( pl, t, "fm_sub_b" );
        }
    else
        {
        /* sin MA regular no hay nada que mirar: se AÑADE un testigo */
        if ( a->nf[2] >= ID_MAX_FAC ) { pl->n--; g_free( a ); g_snprintf( why, nwhy, "sin sitio" ); return 1; }
        i = a->nf[2]++;
        a->ord[2][i] = 1; a->v[2][i][0] = 0.85; a->fl[2][i][0] = 1;
        t->idx = i;
        g_snprintf( t->testigo, sizeof t->testigo, "un MA(1) añadido, desde +0.85" );
        t->etapa = 0;
        }
    if ( id_escribe_arma( pl->pre, t->inp, a, why, nwhy ) != 0 ) { pl->n--; g_free( a ); return 1; }
    g_free( a );
    return 0;
}

/* El final de los de frontera: las comprobaciones de _frontera_theta_uno */
static void frontera_fin( FmPrueba *t )
{
    double lr;

    t->hecho = 1;
    t->l_libre = t->l_testigo;
    t->calculable = 0;
    if ( !isfinite( t->l_restr ) )
        { g_snprintf( t->motivo, sizeof t->motivo, "la verosimilitud del modelo restringido "
                      "(θ = 1) no es finita: el ajuste falló" ); return; }
    if ( !isfinite( t->l_testigo ) )
        { g_snprintf( t->motivo, sizeof t->motivo, "la verosimilitud del modelo libre no es "
                      "finita: el ajuste falló" ); return; }
    lr = 2.0 * ( t->l_testigo - t->l_restr );
    if ( lr < -TOL_LR )
        { g_snprintf( t->motivo, sizeof t->motivo, "el modelo libre queda POR DEBAJO del "
                      "restringido (LR = %.3f) aun reintentando desde él: el máximo libre no "
                      "se alcanzó", lr ); return; }
    if ( lr < 0.0 ) lr = 0.0;
    t->est = lr;
    t->calculable = 1;
    t->rechaza = lr > t->c5;
}

/* SUB: la cota del AR del restringido, o el reintento, o el final */
static int sub_cota( FmPlan *pl, FmPrueba *t, const char *ultimo )
{
    IdArma *a = g_new0( IdArma, 1 );
    double  phi = 0.0, raiz = 0.0;
    int     fac = -1;

    if ( t->iter < t->maxiter && lee_modelo( pl, ultimo, a ) == 0 &&
         ar_dominante( a, pl->s, &phi, &fac, &raiz ) && phi >= t->rho_m )
        {
        gchar *pre = con_ext( pl, ultimo, ".pre" ), *base = g_strdup_printf( "fm_sub_c%d", t->iter + 1 );
        char   why[256];

        parte_ar( a, fac, t->rho_m, raiz );
        pon_base( pl, t, base );
        if ( id_escribe_arma( pre, t->inp, a, why, sizeof why ) == 0 )
            { t->etapa = 2; g_free( pre ); g_free( base ); g_free( a ); return 1; }
        g_free( pre ); g_free( base );
        t->l_restr = NAN;
        }
    g_free( a );

    /* BUG-0236: el reintento del libre desde el restringido SIN acotar */
    {
    double lr = 2.0 * ( t->l_testigo - t->l_restr );

    if ( isfinite( t->l_restr ) && isfinite( t->l_b ) && ( lr < 0.0 || t->coef < 0.0 ) &&
         ma_en( pl, "fm_sub_b", "fm_sub_r", t->idx, 0.95, 1 ) == 0 )
        { pon_base( pl, t, "fm_sub_r" ); t->etapa = 3; return 1; }
    }
    frontera_fin( t );
    return 0;
}

/* ¿Paro fue por el gradiente? Es lo que fue-python llama convergido. Un
 * .out que no lo dice no se reinicia. */
static int convergio( const char *out )
{
    Convergence c;
    int         r = 1;

    memset( &c, 0, sizeof c );
    if ( convergence_of( out, &c ) ) r = c.kind == CONV_GRADTOL || c.kind == CONV_NONE;
    convergence_clear( &c );
    return r;
}

static int copia( const char *de, const char *a )
{
    gchar *txt = NULL;
    gsize  n = 0;
    int    r = g_file_get_contents( de, &txt, &n, NULL ) && g_file_set_contents( a, txt, (gssize) n, NULL );

    g_free( txt );
    return r;
}

#define MAX_REINICIOS 3

/* Lanza el reinicio de la estimacion que acaba de correr en t->base: su .pre
 * (donde paro) es el .inp de base_r. 1 si lo dejo listo. */
static int reinicia( const FmPlan *pl, FmPrueba *t )
{
    gchar *rb = g_strdup_printf( "%s_r", t->base_real ), *inp = con_ext( pl, rb, ".inp" );
    int    r = preciso_de( pl, t->base_real, inp ) == 0;

    if ( r ) { pon_base( pl, t, rb ); t->en_reinicio = 1; }
    g_free( rb ); g_free( inp );
    return r;
}

/* Antes de leer una estimacion: el reinicio, si hace falta. 1 si hay que
 * correr fue otra vez; 0 con t->base otra vez la suya, y su .out/.pre los
 * del mejor intento. */
static int reinicio( const FmPlan *pl, FmPrueba *t, int *corrio )
{
    gchar *out;

    if ( t->en_reinicio )
        {
        gchar *rout = con_ext( pl, t->base, ".out" ), *rpre = con_ext( pl, t->base, ".pre" );
        double lr = *corrio ? logelf( rout ) : NAN;
        int    sigue = 0;

        t->en_reinicio = 0;
        out = con_ext( pl, t->base_real, ".out" );
        if ( isfinite( lr ) && lr > t->l_mejor + 1e-9 )
            {
            gchar *pre = con_ext( pl, t->base_real, ".pre" );

            if ( copia( rout, out ) && copia( rpre, pre ) )
                { t->l_mejor = lr; t->reinicios++; sigue = !convergio( out ); }
            g_free( pre );
            }
        g_free( rout ); g_free( rpre );
        pon_base( pl, t, t->base_real );
        *corrio = 1;                       /* el de base_real si corrio */
        if ( sigue && t->reinicios < MAX_REINICIOS && reinicia( pl, t ) ) { g_free( out ); return 1; }
        if ( !convergio( out ) ) t->dudoso = 1;
        g_free( out );
        return 0;
        }
    if ( !*corrio ) return 0;
    g_snprintf( t->base_real, sizeof t->base_real, "%s", t->base );
    out = con_ext( pl, t->base, ".out" );
    t->reinicios = 0;
    t->l_mejor = logelf( out );
    if ( !isfinite( t->l_mejor ) || convergio( out ) ) { g_free( out ); return 0; }
    g_free( out );
    return reinicia( pl, t );
}

int fm_avanza( FmPlan *pl, FmPrueba *t, double l_libre, int corrio )
{
    gchar *out = NULL;
    double l;

    if ( !t->omitida && reinicio( pl, t, &corrio ) ) return 1;

    if ( t->omitida ) return 0;
    if ( t->tipo != FM_SOBRE && t->tipo != FM_SUB && t->tipo != FM_MEG )
        {
        fm_resultado( t, l_libre, pl->nobs, pl->s, corrio );
        return 0;
        }

    out = con_ext( pl, t->base, ".out" );
    l = corrio ? logelf( out ) : NAN;

    if ( t->tipo == FM_MEG )
        {
        int nyq = 2 * t->freq == pl->s;

        if ( t->etapa == 0 )                             /* S, libre */
            {
            char a[32], b[32];

            t->l_testigo = l;
            t->coef = nyq ? coef_ma( pl, t->base, t->idx ) : coef_maf_out( out, t->idx );
            g_free( out );
            if ( !isfinite( l ) )
                { t->hecho = 1; t->calculable = 0; t->l_libre = NAN;
                  g_snprintf( t->motivo, sizeof t->motivo, "la estimación del modelo estocástico "
                              "con su testigo falló: ambiguo" ); return 0; }
            g_snprintf( a, sizeof a, "fm_meg%d_a", t->freq );
            g_snprintf( b, sizeof b, "fm_meg%d_b", t->freq );
            if ( ( nyq ? ma_en( pl, a, b, t->idx, -1.0, 0 ) : maf_en( pl, a, b, t->idx, -1.0, 0 ) ) != 0 )
                { t->l_restr = NAN; meg_fin( t ); return 0; }
            pon_base( pl, t, b ); t->etapa = 1;
            return 1;
            }
        g_free( out );
        t->l_restr = l;                                  /* S con el testigo en -1 */
        meg_fin( t );
        return 0;
        }
    g_free( out );

    if ( t->tipo == FM_SOBRE )
        switch ( t->etapa )
            {
            case 0:                                      /* el candidato libre */
                t->l_testigo = l;
                t->coef = coef_ma( pl, t->base, t->idx );
                if ( !isfinite( l ) )
                    { t->l_restr = NAN; frontera_fin( t );
                      g_snprintf( t->motivo, sizeof t->motivo, "la estimación del candidato "
                                  "con d + 1 falló" ); return 0; }
                if ( ma_en( pl, "fm_sobre_a", "fm_sobre_b", t->idx, 1.0, 0 ) != 0 )
                    { t->l_restr = NAN; frontera_fin( t ); return 0; }
                pon_base( pl, t, "fm_sobre_b" ); t->etapa = 1;
                return 1;
            case 1:                                      /* el restringido */
                t->l_restr = l;
                if ( isfinite( l ) && ( 2.0 * ( t->l_testigo - l ) < 0.0 || t->coef < 0.0 ) &&
                     ma_en( pl, "fm_sobre_b", "fm_sobre_c", t->idx, 0.95, 1 ) == 0 )
                    { pon_base( pl, t, "fm_sobre_c" ); t->etapa = 2; return 1; }
                frontera_fin( t );
                return 0;
            default:                                     /* el reintento */
                if ( isfinite( l ) && l > t->l_testigo )
                    { t->l_testigo = l; t->coef = coef_ma( pl, t->base, t->idx ); }
                frontera_fin( t );
                return 0;
            }

    /* FM_SUB */
    switch ( t->etapa )
        {
        case 0:                                          /* el testigo añadido */
            t->l_testigo = l;
            t->coef = coef_ma( pl, t->base, t->idx );
            if ( !isfinite( l ) || ma_en( pl, "fm_sub_a", "fm_sub_b", t->idx, 1.0, 0 ) != 0 )
                { t->l_restr = NAN; frontera_fin( t ); return 0; }
            pon_base( pl, t, "fm_sub_b" ); t->etapa = 1;
            return 1;
        case 1:                                          /* el restringido */
            if ( !isfinite( t->l_testigo ) ) t->l_testigo = l_libre;   /* el libre es el modelo */
            t->l_restr = t->l_b = l;
            if ( isfinite( l ) && pl->nobs > 4 )
                {
                IdArma *a = g_new0( IdArma, 1 );
                double  phi = 0.0, raiz;
                int     fac, n = 0, i, k;

                t->rho_m = 1.0 - 4.0 / pl->nobs;
                if ( lee_modelo( pl, "fm_sub_b", a ) == 0 )
                    {
                    if ( ar_dominante( a, pl->s, &phi, &fac, &raiz ) ) t->phi_r = phi;
                    for ( i = 0; i < a->nf[0]; i++ )
                        for ( k = 0; k < a->ord[0][i]; k++ ) n += a->fl[0][i][k] != 0;
                    }
                g_free( a );
                t->maxiter = n > 1 ? n : 1;
                return sub_cota( pl, t, "fm_sub_b" );
                }
            return sub_cota( pl, t, "fm_sub_b" );
        case 2:                                          /* una cota del AR */
            t->iter++;
            t->acotado = 1;
            t->l_restr = l;
            if ( !isfinite( l ) ) { frontera_fin( t ); return 0; }
            {
            char ultimo[32];

            g_snprintf( ultimo, sizeof ultimo, "%s", t->base );
            return sub_cota( pl, t, ultimo );
            }
        default:                                         /* el reintento */
            if ( isfinite( l ) && l > t->l_testigo )
                { t->l_testigo = l; t->coef = coef_ma( pl, t->base, t->idx ); }
            frontera_fin( t );
            return 0;
        }
}

int fm_plan( const char *pre, const char *out, const char *dir, int nobs, int s, int d,
             FmPlan *pl, char *why, size_t nwhy )
{
    IdArma *a;

    memset( pl, 0, sizeof *pl );
    pl->nobs = nobs; pl->s = s; pl->d = d;
    if ( nobs < 5 ) { g_snprintf( why, nwhy, "el .out no dice cuántas observaciones hay" ); return 1; }
    g_snprintf( pl->dir, sizeof pl->dir, "%s", dir );
    g_snprintf( pl->pre, sizeof pl->pre, "%s", pre );
    g_snprintf( pl->out, sizeof pl->out, "%s", out ? out : "" );
    pl->lam = 1.0;
    if ( out )
        {
        FueOut *o = g_new0( FueOut, 1 );

        if ( fueout_read( out, o ) ) { pl->lam = o->lambda; pl->D = o->D; }
        g_free( o );
        }
    {
    static char det[ID_MAX_DET][ID_LINEA];
    int         libre[ID_MAX_DET], k, nd = id_det_lee( pre, det, libre, ID_MAX_DET, why, nwhy );

    for ( k = 0; k < nd; k++ )
        {
        pl->n_det++;
        if ( g_str_has_prefix( det[k], "cos " ) || g_str_has_prefix( det[k], "sin " ) ||
             !strcmp( det[k], "alter" ) ) pl->n_armon++;
        }
    }
    if ( s >= 2 ) id_ifadf( pre, pl->ifadf, why, nwhy );
    a = g_new0( IdArma, 1 );
    if ( id_arma_lee( pre, a, why, nwhy ) != 0 ) { g_free( a ); return 1; }
    if ( out ) coefs_del_out( out, a );
    plan_estacional( pre, dir, pl );
    plan_sf( pre, dir, a, nobs, s, pl );
    plan_dcd( pre, dir, a, 2, pl );
    if ( s > 1 ) plan_dcd( pre, dir, a, 3, pl );
    plan_sobre( pl, a );
    if ( s > 1 ) plan_dcd_f( pl );
    plan_meg( pl );
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
        default:       break;
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
        case FM_DCD_F:                     /* sus criticos, puestos por n al plantear */
            t->est = 2.0 * dl;
            t->rechaza = t->est > t->c5;
            break;
        default:
            break;
        }
}
