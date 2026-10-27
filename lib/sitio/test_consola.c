/*
 * test_consola.c -- ¿asoma una consola cuando un GUI lanza un motor?
 * (gui/atsw/BUGS.md BUG-0001)
 *
 * En Windows se compila con -mwindows, como los GUIs: no tiene consola. Por
 * cada camino por el que un GUI lanza un programa de consola, lanza
 * hijo_consola.exe (un programa de consola, como los motores), y el hijo dice
 * si tiene ventana de consola (GetConsoleWindow). Ninguno puede tenerla.
 *
 *   engine_run          lib/engine, esperando: las corridas de la madre al
 *                       derivar, identificar, art, fue -d...
 *   engine_start        lib/engine, sin esperar: estimar en fue_gui,
 *                       drtran_gui y el editor de la madre
 *   vistazo             gui/atsw/src/vistazo.c: los graficos rapidos
 *   fug_gui             gui/fug/src/fug_run.c (run_fug): cada dibujo de fug_gui
 *
 * Los dos ultimos se lanzan COMO LO HACE EL CODIGO DEL ARBOL: si sitio.h
 * trae SITIO_SIN_CONSOLA, con lo que usan ahora (engine_start y
 * sitio_lanza_y_espera); si no --main antes del arreglo--, con g_spawn_sync,
 * que es lo que usaban. Asi la misma prueba falla antes del arreglo y pasa
 * despues.
 *
 * Y mide lo que tarda el camino del vistazo con fug de verdad, que es la
 * otra mitad del sintoma (la ventana congelada en cada clic).
 *
 *   test_consola RESULTADO HIJO [FUG BASE]
 *
 * Escribe el informe en RESULTADO (un GUI no tiene donde escribir) y sale
 * con 0 si ningun hijo tuvo consola, con 1 si alguno la tuvo.
 */

#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "engine.h"
#include "sitio.h"

#ifdef G_OS_WIN32
#  include <windows.h>
#endif

static FILE *inf;
static int   fallos;

typedef struct { GMainLoop *bucle; gchar *sal; int st; } Espera;

static void hecho( const EngineResult *r, gpointer d )
{
    Espera *e = d;

    e->sal = g_strdup( r->output ? r->output : "" );
    e->st  = r->status;
    g_main_loop_quit( e->bucle );
}

/* engine_start y a esperar en un bucle, como en un GUI. */
static gchar *por_engine_start( const char *dir, const char *prog,
                                const char *const *argv, int *st )
{
    Espera e = { NULL, NULL, -1 };

    e.bucle = g_main_loop_new( NULL, FALSE );
    if ( engine_start( dir, prog, argv, NULL, NULL, hecho, &e ) == NULL )
        { g_main_loop_unref( e.bucle ); *st = -1; return NULL; }
    g_main_loop_run( e.bucle );
    g_main_loop_unref( e.bucle );
    *st = e.st;
    return e.sal;
}

/* Como lanzaba (o lanza) el vistazo de la madre. */
static gchar *como_vistazo( const char *dir, const char *prog,
                            const char *const *args, int *st )
{
#ifdef SITIO_SIN_CONSOLA
    return por_engine_start( dir, prog, args, st );
#else
    gchar *argv[8], *sal = NULL, *err = NULL;
    gint   w = -1;
    int    i;

    argv[0] = (gchar *) prog;
    for ( i = 0; args[i] && i < 6; i++ ) argv[i + 1] = (gchar *) args[i];
    argv[i + 1] = NULL;
    if ( !g_spawn_sync( dir, argv, NULL, 0, NULL, NULL, &sal, &err, &w, NULL ) )
        { *st = -1; return NULL; }
    g_free( err );
    *st = w;
    return sal;
#endif
}

/* Como lanza fug_gui a fug (run_fug). */
static gchar *como_fug_gui( const char *dir, const char *prog, int *st )
{
    gchar *argv[] = { (gchar *) prog, NULL };
    gchar *sal = NULL, *err = NULL;
    gint   w = -1;
    gboolean ok;

#ifdef SITIO_SIN_CONSOLA
    ok = sitio_lanza_y_espera( dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                               &sal, &err, &w, NULL );
#else
    ok = g_spawn_sync( dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                       &sal, &err, &w, NULL );
#endif
    g_free( err );
    *st = ok ? w : -1;
    return ok ? sal : NULL;
}

static void mira( const char *camino, gchar *sal )
{
    const char *que;

    if ( sal == NULL )                       que = "NO SE PUDO LANZAR";
    else if ( strstr( sal, "consola=1" ) )   que = "SI ASOMA UNA CONSOLA";
    else if ( strstr( sal, "consola=0" ) )   que = "sin consola";
    else                                     que = "EL HIJO NO CONTESTO";
    if ( !sal || !strstr( sal, "consola=0" ) ) fallos++;
    fprintf( inf, "  %-14s %s\n", camino, que );
    g_free( sal );
}

int main( int argc, char **argv )
{
    const char *const nada[] = { NULL };
    gchar *dir;
    int    st;

    if ( argc < 3 ) return 2;
    inf = fopen( argv[1], "w" );
    if ( inf == NULL ) return 2;
    dir = g_path_get_dirname( argv[2] );

    fprintf( inf, "test_consola (%s)\n",
#ifdef SITIO_SIN_CONSOLA
             "con sitio_lanza: el arreglo"
#else
             "sin sitio_lanza: antes del arreglo"
#endif
           );
#ifdef G_OS_WIN32
    fprintf( inf, "  el que lanza %s consola propia\n",
             GetConsoleWindow() ? "TIENE (mal: no se compilo con -mwindows)" : "no tiene" );
#endif

    {
    EngineResult r = engine_run( dir, argv[2], NULL );

    mira( "engine_run", r.output ? g_strdup( r.output ) : NULL );
    engine_result_clear( &r );
    }
    mira( "engine_start", por_engine_start( dir, argv[2], nada, &st ) );
    mira( "vistazo", como_vistazo( dir, argv[2], nada, &st ) );
    mira( "fug_gui", como_fug_gui( dir, argv[2], &st ) );

    /* EL TIEMPO DEL VISTAZO, con fug y un .inp de verdad. */
    if ( argc >= 5 )
        {
        const char *const args[] = { argv[4], "-c", NULL };
        gchar *bdir = g_path_get_dirname( argv[4] );
        int    i, n = 10, bien = 0;
        gint64 t0 = g_get_monotonic_time();

        for ( i = 0; i < n; i++ )
            {
            gchar *s = como_vistazo( bdir, argv[3], args, &st );

            if ( s && st == 0 ) bien++;
            g_free( s );
            }
        fprintf( inf, "  el vistazo con fug: %.1f ms por dibujo (media de %d; "
                 "%d acabaron bien)\n",
                 ( g_get_monotonic_time() - t0 ) / 1000.0 / n, n, bien );
        g_free( bdir );
        }

    fprintf( inf, fallos ? "test_consola: %d caminos con consola o sin lanzar\n"
                         : "test_consola: ningun camino abre consola\n", fallos );
    fclose( inf );
    g_free( dir );
    return fallos ? 1 : 0;
}
