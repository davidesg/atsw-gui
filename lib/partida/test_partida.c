/*
 * test_partida.c -- la regla de la ruta U contra sima, y el modelo de partida
 * hecho de verdad, con art y fue (run_test.sh los pone en el PATH).
 *
 *   test_partida EJEMPLOS ART TMP
 *
 * EJEMPLOS es docs/ejemplos: los datos crudos de jenkins_alavi (anuales) y
 * johansen_juselius (trimestrales). Los numeros de referencia son los de
 * sima.raw.characterize_one (sima-python main, 2026-11-16): la misma regla
 * en Python. La F estacional no se coteja con la de sima: la madre la
 * contrasta como la ventana de art (d = 1, 100 log) y sima con su λ; para
 * estas series el veredicto coincide.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "partida.h"
#include "engine.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( "  %-66s %s\n", que, c ? "ok" : "FALLA" );
    if ( !c ) fallos++;
}

/* datos.csv de la madre: «# freq», «# start», cabecera y fecha,valor. */
static int lee_csv( const char *ruta, double *x, int max )
{
    FILE *f = fopen( ruta, "r" );
    char  l[256];
    int   n = 0;

    if ( !f ) return -1;
    while ( fgets( l, sizeof l, f ) && n < max )
        {
        char *c = strchr( l, ',' );
        if ( l[0] < '0' || l[0] > '9' || !c ) continue;
        x[n++] = g_ascii_strtod( c + 1, NULL );
        }
    fclose( f );
    return n;
}

typedef struct {
    const char *ej, *serie;
    int         freq;
    double      corr_raw, corr_log, lam;
    int         d, d_tests, d_por, armonicos;
} Ref;

static const Ref REF[] = {
    { "jenkins_alavi", "MINK",    1, 0.4119422688840916, -0.4899665125648938, 1, 0, 0, PT_D_TESTS, 0 },
    { "jenkins_alavi", "MUSKRAT", 1, 0.821950538972544, -0.014331269739161996, 0, 1, 2, PT_D_PASO, 0 },
    { "johansen_juselius", "IBO", 4, -0.31106927094667364, -0.6348821109675714, 1, 1, 1, PT_D_TESTS, 0 },
    { "johansen_juselius", "IDE", 4, -0.3225990127564411, -0.45711706662405927, 1, 1, 1, PT_D_TESTS, 0 },
    { "johansen_juselius", "LRM", 4, 0.11074711270000943, 0.10032375642837442, 0, 1, 1, PT_D_TESTS, 1 },
    { "johansen_juselius", "LRY", 4, -0.4711850033208692, -0.48627269854195365, 1, 1, 1, PT_D_TESTS, 0 },
};

static gchar *el_fue( const char *p ) { return g_find_program_in_path( p ); }

int main( int argc, char **argv )
{
    const char *ej = argc > 1 ? argv[1] : "docs/ejemplos";
    const char *art = argc > 2 ? argv[2] : "art";
    const char *tmp = argc > 3 ? argv[3] : ".";
    double      x[512];
    size_t      i;

    printf( "LA REGLA, contra sima (characterize_one)\n" );
    for ( i = 0; i < G_N_ELEMENTS( REF ); i++ )
        {
        const Ref *r = &REF[i];
        gchar     *csv = g_build_filename( ej, r->ej, r->serie, "datos.csv", NULL );
        gchar     *txt = g_strdup_printf( "%s/%s.txt", tmp, r->serie );
        gchar     *q;
        PtPartida  p;
        ArtCand   *c = g_new0( ArtCand, 1 );
        char       why[512];
        int        n = lee_csv( csv, x, 512 ), rc;

        rc = n > 0 ? pt_identifica( art, txt, x, n, r->freq, 1, r->freq == 1 ? 1850 : 1974,
                                    &p, c, why, sizeof why ) : 1;
        q = g_strdup_printf( "%s: art identifica (%d obs)", r->serie, n );
        ok( rc == 0, q ); g_free( q );
        if ( rc != 0 ) { printf( "    %s\n", why ); g_free( csv ); g_free( txt ); g_free( c ); continue; }
        q = g_strdup_printf( "%s: media–d.t. %+.6f / %+.6f, la de sima", r->serie, p.corr_raw, p.corr_log );
        ok( fabs( p.corr_raw - r->corr_raw ) < 1e-9 && fabs( p.corr_log - r->corr_log ) < 1e-9, q ); g_free( q );
        q = g_strdup_printf( "%s: λ = %g", r->serie, p.lam );
        ok( p.lam == r->lam, q ); g_free( q );
        q = g_strdup_printf( "%s: ADF y KPSS dicen %d, la partida es d = %d", r->serie, p.d_tests, p.d );
        ok( p.d_tests == r->d_tests && p.d == r->d && p.d_por == r->d_por, q ); g_free( q );
        q = g_strdup_printf( "%s: %s", r->serie, p.armonicos ? "con armónicos (la F los ve)" : "sin armónicos" );
        ok( p.armonicos == r->armonicos, q ); g_free( q );
        {
            char razon[PR_RAZON], tipo[64];
            pt_razon( &p, &c->cand[0], "C9", razon, sizeof razon );
            printf( "    %s: (%d,%d)(%d,%d) %s; %s\n", r->serie, c->cand[0].p, c->cand[0].q,
                    c->cand[0].P, c->cand[0].Q, ac_kind( &c->cand[0], tipo, sizeof tipo ), razon );
            ok( g_str_has_prefix( razon, "Modelo de partida (ruta U), sin revisar, para el sistema C9. " ),
                "la razón dice qué es y para qué sistema" );
        }
        g_free( csv ); g_free( txt ); g_free( c );
        }

    printf( "\nUNO DE VERDAD: johansen_juselius, LRM, en una copia del proyecto\n" );
    {
        static Proyecto pr;
        PrError    e;
        PtPartida  p;
        char       id[PR_ID], why[1024], pre[PR_RUTA], inp[PR_RUTA];
        gchar     *yaml = g_build_filename( tmp, "jj", "proyecto.yaml", NULL );
        gchar     *cache = g_build_filename( tmp, "cache", NULL );
        int        rc, k;

        engine_set_finder( el_fue );
        ok( pr_leer( yaml, &pr, &e ) == 0, "el proyecto se lee" );
        rc = pt_construye( &pr, "LRM", "", art, cache, "C2", 1, id, sizeof id, &p, why, sizeof why );
        if ( rc ) printf( "    %s\n", why );
        ok( rc == 0, "pt_construye: hecho" );
        k = pr_modelo_idx( &pr, "LRM", "", id );
        ok( k >= 0 && !strcmp( pr.m[k].padre, pr_datos_de( &pr, "LRM", "" ) ),
            "un modelo nuevo, hijo del nodo de datos" );
        ok( k >= 0 && pr.m[k].sin_revisar, "marcado sin revisar" );
        ok( k >= 0 && !pr.m[k].elegido, "y no elegido: lo elige el analista" );
        ok( k >= 0 && g_str_has_prefix( pr.m[k].razon, "Modelo de partida (ruta U), sin revisar, para el sistema C2." ),
            "con su razón" );
        pr_ruta( &pr, "LRM", "", id, ".pre", pre, sizeof pre );
        pr_ruta( &pr, "LRM", "", id, ".inp", inp, sizeof inp );
        ok( g_file_test( pre, G_FILE_TEST_EXISTS ), "estimado con fue: su .pre está" );
        {
            gchar *t = NULL;
            g_file_get_contents( inp, &t, NULL, NULL );
            ok( t && strstr( t, "cos 1" ) && strstr( t, "alter" ), "el .inp lleva los armónicos" );
            g_free( t );
        }
        ok( pr_escribir( &pr, yaml, &e ) == 0 && pr_leer( yaml, &pr, &e ) == 0 &&
            pr.m[pr_modelo_idx( &pr, "LRM", "", id )].sin_revisar,
            "y la marca sobrevive al manifiesto" );
        rc = pt_construye( &pr, "NADA", "", art, cache, NULL, 1, id, sizeof id, &p, why, sizeof why );
        ok( rc == 1, "una serie que no existe: no, y lo dice" );
        g_free( yaml ); g_free( cache );
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
