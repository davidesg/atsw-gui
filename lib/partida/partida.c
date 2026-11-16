/*
 * partida.c -- el modelo de partida de una serie sin modelo (ruta U de SIM).
 * Ver partida.h: la regla, y de donde sale cada parte.
 */

#include <math.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "partida.h"
#include "artrun.h"
#include "inpdet.h"

/* --- λ: la correlacion media-d.t. (art, describe_boxcox) ----------------- */

/* La de las medias y las d.t. de ng grupos de nog, cada vector tipificado
 * (art, mean_stdev_data): con una de las dos sin variacion, 0.           */
static double corr_mdt( const double *z, int n, int nog )
{
    int     ng = nog > 0 ? n / nog : 0, g, i;
    double *m, *s, mm = 0, ms = 0, vm = 0, vs = 0, c = 0;

    if ( ng < 2 ) return 0.0;
    m = g_new( double, ng );
    s = g_new( double, ng );
    for ( g = 0; g < ng; g++ )
        {
        double a = 0, b = 0;
        for ( i = 0; i < nog; i++ ) a += z[g * nog + i];
        a /= nog;
        for ( i = 0; i < nog; i++ ) b += ( z[g * nog + i] - a ) * ( z[g * nog + i] - a );
        m[g] = a;
        s[g] = sqrt( b / nog );
        mm += m[g];
        ms += s[g];
        }
    mm /= ng;
    ms /= ng;
    for ( g = 0; g < ng; g++ )
        {
        vm += ( m[g] - mm ) * ( m[g] - mm );
        vs += ( s[g] - ms ) * ( s[g] - ms );
        c  += ( m[g] - mm ) * ( s[g] - ms );
        }
    g_free( m );
    g_free( s );
    /* np.corrcoef de los tipificados: la de los originales. Con una d.t.
       por debajo de 1e-10 (art), 0.                                      */
    if ( sqrt( vm / ng ) < 1e-10 || sqrt( vs / ng ) < 1e-10 ) return 0.0;
    return c / sqrt( vm * vs );
}

double pt_lambda( const double *x, int n, int freq, PtPartida *p )
{
    double *z;
    int     i;

    p->nog = freq > 1 ? freq : 8;
    p->corr_raw = p->corr_log = 0.0;
    for ( i = 0; i < n; i++ )
        if ( !( x[i] > 0.0 ) )
            {
            p->lam = 1.0;
            p->lam_por = PT_LAM_NOPOS;
            return p->lam;
            }
    z = g_new( double, n );
    for ( i = 0; i < n; i++ ) z[i] = log( x[i] );
    p->corr_raw = corr_mdt( x, n, p->nog );
    p->corr_log = corr_mdt( z, n, p->nog );
    g_free( z );
    p->lam = fabs( p->corr_log ) < fabs( p->corr_raw ) ? 0.0 : 1.0;
    p->lam_por = PT_LAM_MDT;
    return p->lam;
}

/* --- d: ADF y KPSS, y la politica de art desde d = 0 --------------------- */

double pt_tendencia_r2( const double *x, int n, double lam )
{
    double st = 0, sz = 0, stt = 0, stz = 0, szz = 0, b, a, sse = 0, sst = 0, zm;
    int    i;

    if ( n < 3 ) return 0.0;
    for ( i = 0; i < n; i++ )
        {
        double z = lam == 0.0 ? log( x[i] ) : x[i];
        if ( !isfinite( z ) ) return 0.0;
        st += i; sz += z; stt += (double) i * i; stz += i * z; szz += z * z;
        }
    b  = ( n * stz - st * sz ) / ( n * stt - st * st );
    a  = ( sz - b * st ) / n;
    zm = sz / n;
    for ( i = 0; i < n; i++ )
        {
        double z = lam == 0.0 ? log( x[i] ) : x[i], f = a + b * i;
        sse += ( z - f ) * ( z - f );
        sst += ( z - zm ) * ( z - zm );
        }
    return sst > 0 ? 1.0 - sse / sst : 0.0;
}

int pt_d( const double *x, int n, PtPartida *p )
{
    int d;

    p->nraiz = rz_raiz_unitaria( x, n, p->lam, RZ_MAX_D, p->raiz );
    p->d_tests = p->nraiz > 0 ? rz_d_inicial( p->raiz, p->nraiz ) : 0;
    p->trend_r2 = pt_tendencia_r2( x, n, p->lam );
    d = p->d_tests;
    p->d_por = PT_D_TESTS;
    if ( d == 0 && p->trend_r2 > PT_TENDENCIA ) { d = 1; p->d_por = PT_D_TENDENCIA; }
    if ( d > 1 ) { d = 1; p->d_por = PT_D_PASO; }   /* un paso desde d = 0 */
    p->d = d;
    return d;
}

/* --- la media (la regla de la ventana de identificacion) ---------------- */

int pt_media_libre( const double *x, int n, double lam, int d, int D, int freq )
{
    double *w, m = 0.0, v = 0.0, c = 0.0, t;
    int     i, k, j;

    if ( d + D == 0 ) return 1;
    if ( d + D > 1 || x == NULL ) return 0;
    w = g_new( double, n );
    for ( i = 0; i < n; i++ ) w[i] = lam == 0.0 ? log( x[i] ) : x[i];
    for ( k = 0; k < d + D; k++ )
        {
        int s = k < d ? 1 : freq;

        for ( j = n - 1; j >= s; j-- ) w[j] -= w[j - s];
        memmove( w, w + s, (size_t) ( n - s ) * sizeof *w );
        n -= s;
        }
    if ( n < 10 ) { g_free( w ); return 0; }
    for ( i = 0; i < n; i++ ) m += w[i];
    m /= n;
    for ( i = 0; i < n; i++ ) v += ( w[i] - m ) * ( w[i] - m );
    for ( i = 1; i < n; i++ ) c += ( w[i] - m ) * ( w[i - 1] - m );
    g_free( w );
    if ( v <= 0.0 ) return 0;
    c /= v;
    if ( c > 0.9 ) c = 0.9;
    if ( c < -0.9 ) c = -0.9;
    t = m / sqrt( v / n / n * ( 1.0 + c ) / ( 1.0 - c ) );
    return fabs( t ) >= 2.0;
}

/* --- el .inp del hijo ----------------------------------------------------- */

int pt_armonicos_de( int s, char det[][128], int max )
{
    int k, n = 0;

    for ( k = 1; 2 * k < s && n + 2 <= max; k++ )
        {
        g_snprintf( det[n++], 128, "cos %d", k );
        g_snprintf( det[n++], 128, "sin %d", k );
        }
    if ( s % 2 == 0 && n < max ) g_snprintf( det[n++], 128, "alter" );
    return n;
}

/* Al .inp ya escrito le pone los armonicos que no tenga. */
static int pon_armonicos( const char *inp, int s, int *puestos, char *porque, size_t n )
{
    char        arm[ID_MAX_DET][ID_LINEA], ya[ID_MAX_DET][ID_LINEA];
    char        a2[ID_MAX_DET][128];
    const char *nuevo[ID_MAX_DET];
    int         na = pt_armonicos_de( s, a2, ID_MAX_DET ), nya, nn = 0, i, j, rc;
    gchar      *tmp;

    for ( i = 0; i < na; i++ ) g_snprintf( arm[i], ID_LINEA, "%s", a2[i] );
    *puestos = 0;
    nya = id_nombres( inp, ya, ID_MAX_DET, porque, n );
    if ( nya < 0 ) return 1;
    for ( i = 0; i < na; i++ )
        {
        gboolean esta = FALSE;

        for ( j = 0; j < nya; j++ ) if ( !g_ascii_strcasecmp( ya[j], arm[i] ) ) esta = TRUE;
        if ( !esta ) nuevo[nn++] = arm[i];
        }
    if ( nn == 0 ) return 0;
    tmp = g_strdup_printf( "%s.arm", inp );
    rc = id_anade( inp, tmp, nuevo, NULL, nn, porque, n );
    if ( rc == 0 && g_rename( tmp, inp ) != 0 )
        { g_snprintf( porque, n, "no pude escribir %s", inp ); rc = 1; }
    g_unlink( tmp );
    g_free( tmp );
    if ( rc == 0 ) *puestos = nn;
    return rc;
}

int pt_deriva_inp( const char *origen, const char *destino, double lam, int d, int D,
                   int freq, const AcCand *k, int armonicos, int media, int *puestos,
                   char *porque, size_t n )
{
    gchar *tmp = g_strdup_printf( "%s.trans", destino );
    int    rc, nada = 0;

    if ( !puestos ) puestos = &nada;
    *puestos = 0;
    rc = id_pon_transformacion( origen, tmp, lam, d, D, porque, n );
    if ( rc == 0 )
        rc = id_pon_arma( tmp, destino, k->p, k->phi, k->q, k->theta,
                          k->P, k->Phi, k->Q, k->Theta, porque, n );
    g_unlink( tmp );
    g_free( tmp );
    /* Y LO QUE ART QUITO ANTES DE IDENTIFICAR: los armonicos. Sin ellos el
       hijo llevaba el AR(1) pero no la estacionalidad que se habia retirado
       para encontrarlo. Paso con IPC_ES.                                */
    if ( rc == 0 && armonicos )
        rc = pon_armonicos( destino, freq, puestos, porque, n );
    if ( rc == 0 && media )
        {
        tmp = g_strdup_printf( "%s.mu", destino );
        rc = id_fija_media( destino, tmp, 0.0, 1, porque, n );
        if ( rc == 0 && g_rename( tmp, destino ) != 0 )
            { g_snprintf( porque, n, "no pude escribir %s", destino ); rc = 1; }
        g_unlink( tmp );
        g_free( tmp );
        }
    return rc;
}

/* --- art ------------------------------------------------------------------ */

int pt_identifica( const char *art, const char *datos_txt, const double *x, int n,
                   int freq, int per, int anio, PtPartida *p, ArtCand *c,
                   char *why, size_t nw )
{
    int sin = 0;

    memset( p, 0, sizeof *p );
    if ( n < 20 ) { g_snprintf( why, nw, "muy pocas observaciones (%d)", n ); return 1; }
    pt_lambda( x, n, freq, p );
    pt_d( x, n, p );
    if ( ac_escribe_datos( datos_txt, x, n, freq, per, anio ) != 0 )
        { g_snprintf( why, nw, "no pude escribir %s", datos_txt ); return 1; }
    if ( ac_corre( art, datos_txt, freq, p->lam, p->d, 0, 0, c, &sin, why, nw ) != 0 ) return 1;
    if ( c->ncand < 1 ) { g_snprintf( why, nw, "art no propone ningún modelo" ); return 1; }
    p->hay_F = c->has_seasonal;
    p->F = c->seasonal_F;
    p->F_p = c->seasonal_p;
    /* La regla de la ventana (quita_armonicos): con D = 0 y freq > 1, art
       retira los armonicos salvo que su F no detecte nada.             */
    p->armonicos = freq > 1 && !sin;
    p->media_libre = pt_media_libre( x, n, p->lam, p->d, 0, freq );
    return 0;
}

void pt_razon( const PtPartida *p, const AcCand *k, const char *sistema, char *buf, size_t n )
{
    char    tipo[64];
    GString *g = g_string_new( "Modelo de partida (ruta U), sin revisar" );

    if ( sistema && *sistema ) g_string_append_printf( g, ", para el sistema %s", sistema );
    g_string_append( g, ". " );
    if ( p->lam_por == PT_LAM_NOPOS )
        g_string_append( g, "λ = 1: hay valores no positivos" );
    else
        g_string_append_printf( g, "λ = %g: correlación media–d.t. %+.3f en niveles, %+.3f en logaritmos",
                                p->lam, p->corr_raw, p->corr_log );
    g_string_append_printf( g, "; d = %d", p->d );
    if ( p->d_por == PT_D_TENDENCIA )
        g_string_append_printf( g, " (ADF y KPSS dicen 0, pero una recta explica el %.0f %% del nivel)",
                                100.0 * p->trend_r2 );
    else if ( p->d_por == PT_D_PASO )
        g_string_append_printf( g, " (ADF y KPSS piden %d: un paso cada vez)", p->d_tests );
    else
        g_string_append( g, " (ADF y KPSS)" );
    if ( p->hay_F )
        g_string_append_printf( g, "; %s (F = %.2f, p = %.4f)",
                                p->armonicos ? "estacionalidad determinista, con armónicos"
                                             : "sin estacionalidad",
                                p->F, p->F_p );
    g_string_append_printf( g, "; (%d,%d)(%d,%d) %s, el que propone art%s.",
                            k->p, k->q, k->P, k->Q, ac_kind( k, tipo, sizeof tipo ),
                            p->media_libre ? ", con la media libre" : "" );
    g_snprintf( buf, n, "%s", g->str );
    g_string_free( g, TRUE );
}

