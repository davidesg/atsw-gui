/*
 * test_coste.c -- lo que cuesta cada corrida, en costes.csv (lib/coste).
 *
 * LO QUE PIDE EL ESTUDIO (docs/ESTUDIO-terminal-mcp.md, fase 1a): cada
 * corrida de un motor deja una linea con todos sus campos; el tiempo de CPU
 * no es negativo y no pasa del de reloj por los nucleos; ninguna energia sin
 * su metodo; dos escritores a la vez no rompen el fichero. Y lo que lo hace
 * inofensivo: SIN CONTEXTO NO SE ESCRIBE NADA.
 *
 * EL MOTOR ES ESTE MISMO PROGRAMA. Con --quema=S gasta S segundos de CPU,
 * dice «TEST_COSTE 9.99» como fue dice «FUE 1.14» y deja <base>.pre, que es
 * lo que hace un motor de verdad. Un guion no vale: en Windows no se lanza.
 *
 *   test_coste DIRECTORIO_DE_TRABAJO
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#else
#  include <sys/wait.h>
#endif

#include "coste.h"
#include "engine.h"

static int fallos = 0, hechas = 0;

static void check( int ok, const char *que )
{
    printf( "  %-6s %s\n", ok ? "ok" : "FALLO", que );
    hechas++;
    if ( !ok ) fallos++;
}

/* ---- el motor falso ----------------------------------------------------- */

static int motor_falso( double seg, const char *base )
{
    clock_t      fin = clock() + (clock_t) ( seg * CLOCKS_PER_SEC );
    volatile double x = 0.0;
    long         i = 0;

    printf( "TEST_COSTE 9.99: un motor de mentira\n" );
    while ( clock() < fin ) x += sqrt( (double) ( ++i % 1000 ) );
    if ( base )
        {
        gchar *pre = g_strconcat( base, ".pre", NULL );
        FILE  *f = g_fopen( pre, "w" );

        if ( f ) { fprintf( f, "el optimo de %s, %.3f\n", base, x ); fclose( f ); }
        g_free( pre );
        }
    return 0;
}

/* ---- utilidades --------------------------------------------------------- */

static gchar *g_prog;

static gchar *nuevo_dir( const char *raiz, const char *nombre )
{
    gchar *d = g_build_filename( raiz, nombre, NULL );
    gchar *f = g_build_filename( d, COSTE_FICHERO, NULL );

    g_mkdir_with_parents( d, 0700 );
    g_unlink( f );
    g_free( f );
    return d;
}

static gboolean hay_fichero( const char *dir )
{
    gchar   *f = g_build_filename( dir, COSTE_FICHERO, NULL );
    gboolean r = g_file_test( f, G_FILE_TEST_EXISTS );

    g_free( f );
    return r;
}

/* Cuantas cabeceras y cuantas lineas tiene el fichero, a pelo. */
static void cuenta( const char *dir, int *cab, int *lin )
{
    gchar  *f = g_build_filename( dir, COSTE_FICHERO, NULL ), *t = NULL;
    gchar **v;
    int     i;

    *cab = *lin = 0;
    if ( g_file_get_contents( f, &t, NULL, NULL ) )
        {
        v = g_strsplit( t, "\n", -1 );
        for ( i = 0; v[i]; i++ )
            {
            if ( !*v[i] ) continue;
            if ( g_str_has_prefix( v[i], "fecha," ) ) ( *cab )++;
            else ( *lin )++;
            }
        g_strfreev( v );
        }
    g_free( t );
    g_free( f );
}

static gchar *sha_fichero( const char *ruta )
{
    gchar *t = NULL, *s;
    gsize  n = 0;

    if ( !g_file_get_contents( ruta, &t, &n, NULL ) ) return g_strdup( "" );
    s = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) t, n );
    g_free( t );
    return s;
}

/* Una linea leida tiene todos sus campos y sus numeros cuadran. */
static void comprueba_linea( const CosteLinea *l, const char *objeto,
                             const char *paso, const char *pre, const char *que )
{
    char   m[256];
    double nuc = (double) g_get_num_processors();

    g_snprintf( m, sizeof m, "%s: la fecha es AAAA-MM-DDTHH:MM:SS (%s)", que, l->fecha );
    check( strlen( l->fecha ) == 19 && l->fecha[10] == 'T' && l->fecha[4] == '-', m );
    g_snprintf( m, sizeof m, "%s: quien «madre», por «analista», via «C»", que );
    check( !strcmp( l->quien, "madre" ) && !strcmp( l->por, "analista" ) &&
           !strcmp( l->via, "C" ), m );
    g_snprintf( m, sizeof m, "%s: objeto «%s» y paso «%s»", que, l->objeto, l->paso );
    check( !strcmp( l->objeto, objeto ) && !strcmp( l->paso, paso ), m );
    g_snprintf( m, sizeof m, "%s: el motor con su version, de su saludo (%s)", que, l->motor );
    check( !strcmp( l->motor, "test_coste 9.99" ), m );
    g_snprintf( m, sizeof m, "%s: CPU %.3f s >= 0,1 (se quemaron 0,3)", que, l->cpu_s );
    check( l->cpu_s >= 0.1, m );
    g_snprintf( m, sizeof m, "%s: CPU %.3f s <= reloj %.3f s × %g nucleos", que,
                l->cpu_s, l->pared_s, nuc );
    check( l->cpu_s <= l->pared_s * nuc + 0.02, m );
    g_snprintf( m, sizeof m, "%s: energia %.3f J con su metodo «%s»", que,
                l->energia_j, l->metodo );
    check( l->energia_j >= 0.0 && strstr( l->metodo, "TDP" ) != NULL, m );
    if ( pre )
        {
        gchar *s = sha_fichero( pre );

        g_snprintf( m, sizeof m, "%s: el sha256 del .pre que dejo", que );
        check( !strcmp( l->sha_pre, s ), m );
        g_free( s );
        }
    else
        {
        g_snprintf( m, sizeof m, "%s: sin .pre, sha_pre vacio", que );
        check( !*l->sha_pre, m );
        }
}

/* ---- el camino asincrono ------------------------------------------------ */

static void hecho( const EngineResult *r, gpointer data )
{
    (void) r;
    g_main_loop_quit( (GMainLoop *) data );
}

/* ---- los escritores a la vez -------------------------------------------- */

#define HILOS   4
#define POR_HILO 250

typedef struct { const char *dir; int n; int malas; } Escritor;

static gpointer escribe_muchas( gpointer data )
{
    Escritor  *e = data;
    CosteLinea l;
    char       why[256], obj[64];
    int        i;

    memset( &l, 0, sizeof l );
    l.quien = "mtm"; l.por = "asistente"; l.paso = "estimar";
    l.motor = "art-python 0.9"; l.via = "python";
    l.energia_j = -1.0;
    for ( i = 0; i < e->n; i++ )
        {
        /* con coma y comillas: la linea tiene que seguir siendo una */
        g_snprintf( obj, sizeof obj, "S%d,\"x\"/m%03d", e->n, i );
        l.objeto = obj;
        l.cpu_s = 0.01 * ( i % 7 );
        l.pared_s = 0.02 * ( i % 7 ) + 0.001;
        if ( coste_apunta( e->dir, &l, why, sizeof why ) != 0 ) e->malas++;
        }
    return NULL;
}

/* Espera a un hijo y devuelve su estado de salida (-1 si no se sabe). */
static int espera_hijo( GPid pid )
{
#ifdef G_OS_WIN32
    DWORD c = (DWORD) -1;

    WaitForSingleObject( (HANDLE) pid, INFINITE );
    GetExitCodeProcess( (HANDLE) pid, &c );
    g_spawn_close_pid( pid );
    return (int) c;
#else
    int st = 0;

    if ( waitpid( pid, &st, 0 ) != pid || !WIFEXITED( st ) ) return -1;
    return WEXITSTATUS( st );
#endif
}

int main( int argc, char **argv )
{
    gchar       *raiz, *d, *pre;
    EngineResult r;
    CosteLinea  *l = NULL;
    int          n = 0, malas = 0, cab, lin, i;
    char         m[256], why[512];

    /* --quema=S y no «--quema S»: como en fue, el primer argumento que no
       es una opcion es la base del modelo, y de ahi sale el .pre.       */
    if ( argc >= 2 && g_str_has_prefix( argv[1], "--quema=" ) )
        return motor_falso( g_ascii_strtod( argv[1] + 8, NULL ), argc >= 3 ? argv[2] : NULL );
    if ( argc >= 4 && !strcmp( argv[1], "--escribe" ) )
        {
        Escritor e = { argv[2], atoi( argv[3] ), 0 };

        escribe_muchas( &e );
        return e.malas ? 1 : 0;
        }
    if ( argc < 2 ) { fprintf( stderr, "uso: %s DIRECTORIO\n", argv[0] ); return 2; }

    g_prog = g_path_is_absolute( argv[0] ) ? g_strdup( argv[0] )
                                           : g_find_program_in_path( argv[0] );
    if ( !g_prog )
        {
        gchar *cwd = g_get_current_dir();

        g_prog = g_build_filename( cwd, argv[0], NULL );
        g_free( cwd );
        }
    raiz = g_strdup( argv[1] );
    g_mkdir_with_parents( raiz, 0700 );
    g_unsetenv( "ATSW_TDP_W" );

    printf( "lib/coste: lo que cuesta cada corrida (motor: %s)\n", g_prog );

    /* --- sin contexto, nada ------------------------------------------- */
    d = nuevo_dir( raiz, "sin" );
    coste_contexto_limpia();
    r = engine_run( d, g_prog, "--quema=0.05", NULL );
    check( r.status == 0, "sin contexto: el motor corre" );
    engine_result_clear( &r );
    check( !hay_fichero( d ), "sin contexto: no hay costes.csv" );
    /* un contexto sin raiz es no tener contexto */
    coste_contexto( NULL, "madre", "analista", "IPC/m01", "estimar" );
    r = engine_run( d, g_prog, "--quema=0.05", NULL );
    engine_result_clear( &r );
    check( !hay_fichero( d ), "contexto sin raiz: tampoco" );
    g_free( d );

    /* --- una corrida sincrona: una linea con todo ---------------------- */
    d = nuevo_dir( raiz, "proyecto" );
    coste_contexto_modelo( d, "IPC", "", "m04", "estimar" );
    r = engine_run( d, g_prog, "--quema=0.3", "IPC_m04", NULL );
    coste_contexto_limpia();
    check( r.status == 0, "sincrona: el motor corre" );
    engine_result_clear( &r );
    check( coste_lee( d, &l, &n, &malas, why, sizeof why ) == 0 && n == 1 && malas == 0,
           "sincrona: una corrida, una linea" );
    pre = g_build_filename( d, "IPC_m04.pre", NULL );
    if ( n == 1 ) comprueba_linea( &l[0], "IPC/m04", "estimar", pre, "sincrona" );
    coste_lineas_libera( l, n );

    /* --- asincrona, con el contexto quitado nada mas lanzar ------------- */
    {
    GMainLoop        *loop = g_main_loop_new( NULL, FALSE );
    const char *const args[] = { "--quema=0.3", NULL };

    coste_contexto_modelo( d, "IPC", "pre-covid", "m05", "meg" );
    if ( engine_run_async( d, g_prog, args, NULL, hecho, loop ) )
        {
        coste_contexto_limpia();   /* lib/engine se quedo con su copia */
        g_main_loop_run( loop );
        }
    else
        coste_contexto_limpia();
    g_main_loop_unref( loop );
    }
    check( coste_lee( d, &l, &n, &malas, why, sizeof why ) == 0 && n == 2,
           "asincrona: la segunda linea, con el contexto ya quitado" );
    if ( n == 2 ) comprueba_linea( &l[1], "IPC/pre-covid/m05", "meg", NULL, "asincrona" );
    coste_lineas_libera( l, n );

    /* --- un motor que no se puede lanzar no cuesta nada ----------------- */
    coste_contexto_modelo( d, "IPC", "", "m06", "estimar" );
    r = engine_run( d, "no_hay_tal_motor_en_ninguna_parte", NULL );
    coste_contexto_limpia();
    check( r.status == ENGINE_NORUN, "un motor que no existe no se lanza" );
    engine_result_clear( &r );
    cuenta( d, &cab, &lin );
    check( cab == 1 && lin == 2, "ni linea por lo que no corrio, y la cabecera una vez" );

    /* --- el TDP de fuera ------------------------------------------------ */
    g_setenv( "ATSW_TDP_W", "30", TRUE );
    coste_contexto_modelo( d, "WTI", "", "m01", "prever" );
    r = engine_run( d, g_prog, "--quema=0.3", NULL );
    coste_contexto_limpia();
    engine_result_clear( &r );
    g_unsetenv( "ATSW_TDP_W" );
    if ( coste_lee( d, &l, &n, NULL, why, sizeof why ) == 0 && n == 3 )
        {
        double e = l[2].cpu_s * 30.0 / g_get_num_processors();

        g_snprintf( m, sizeof m, "ATSW_TDP_W=30: «%s», %.3f J (cpu×30/nucleos = %.3f)",
                    l[2].metodo, l[2].energia_j, e );
        check( strstr( l[2].metodo, ": 30 W, " ) != NULL &&
               fabs( l[2].energia_j - e ) < 0.01, m );
        }
    else check( 0, "ATSW_TDP_W=30: la tercera linea" );
    coste_lineas_libera( l, n );
    g_free( pre );

    /* --- los que apuntan desde fuera ------------------------------------ */
    {
    CosteLinea x;

    memset( &x, 0, sizeof x );
    x.quien = "mtm"; x.por = "asistente"; x.objeto = "C1/c02"; x.paso = "estimar";
    x.motor = "drtran-python"; x.via = "python"; x.cpu_s = 1.5; x.pared_s = 2.0;
    x.energia_j = 3.0;
    check( coste_apunta( d, &x, why, sizeof why ) != 0,
           "una energia sin metodo se rechaza" );
    x.metodo = "medida: RAPL del paquete, con otros procesos";
    check( coste_apunta( d, &x, why, sizeof why ) == 0,
           "con su metodo, se apunta" );
    x.energia_j = -1.0; x.metodo = NULL;
    x.objeto = "Raro, \"con comillas\"/m1";
    check( coste_apunta( d, &x, why, sizeof why ) == 0,
           "sin energia, se estima con el metodo de siempre" );
    }
    if ( coste_lee( d, &l, &n, &malas, why, sizeof why ) == 0 && n == 5 && malas == 0 )
        {
        check( !strcmp( l[3].metodo, "medida: RAPL del paquete, con otros procesos" ) &&
               fabs( l[3].energia_j - 3.0 ) < 1e-9, "la energia medida va con su metodo" );
        check( !strcmp( l[4].objeto, "Raro, \"con comillas\"/m1" ),
               "las comas y las comillas vuelven tal cual" );
        check( strstr( l[4].metodo, "TDP" ) && l[4].energia_j > 0.0,
               "la estimada lleva el metodo de siempre" );
        }
    else check( 0, "las cinco lineas se leen" );
    coste_lineas_libera( l, n );

    /* --- el resumen ----------------------------------------------------- */
    {
    CosteTotal *t = NULL;
    int         nt = 0;
    double      cpu = 0.0;

    coste_lee( d, &l, &n, NULL, why, sizeof why );
    for ( i = 0; i < n; i++ ) cpu += l[i].cpu_s;
    coste_lineas_libera( l, n );

    check( coste_resumen( d, "serie", &t, &nt, why, sizeof why ) == 0 && nt == 4 &&
           !strcmp( t[0].clave, "IPC" ) && t[0].corridas == 2 &&
           !strcmp( t[1].clave, "WTI" ) && !strcmp( t[2].clave, "C1" ) &&
           t[2].corridas == 1, "por serie: IPC (2), WTI, C1, Raro" );
    coste_totales_libera( t, nt );
    check( coste_resumen( d, "via", &t, &nt, why, sizeof why ) == 0 && nt == 2 &&
           !strcmp( t[0].clave, "C" ) && t[0].corridas == 3 &&
           !strcmp( t[1].clave, "python" ) && t[1].corridas == 2, "por via: C 3, python 2" );
    coste_totales_libera( t, nt );
    check( coste_resumen( d, "motor", &t, &nt, why, sizeof why ) == 0 && nt == 2 &&
           !strcmp( t[0].clave, "test_coste" ), "por motor: sin la version" );
    coste_totales_libera( t, nt );
    check( coste_resumen( d, NULL, &t, &nt, why, sizeof why ) == 0 && nt == 1 &&
           t[0].corridas == 5 && fabs( t[0].cpu_s - cpu ) < 1e-9, "el total suma todo" );
    coste_totales_libera( t, nt );
    check( coste_resumen( d, "color", &t, &nt, why, sizeof why ) != 0,
           "un agrupamiento que no existe se dice" );
    }
    g_free( d );

    /* --- sin fichero no es un error ------------------------------------- */
    d = nuevo_dir( raiz, "vacio" );
    check( coste_lee( d, &l, &n, NULL, why, sizeof why ) == 0 && n == 0,
           "sin costes.csv: cero corridas, sin error" );
    g_free( d );

    /* --- dos escritores a la vez (y mas) -------------------------------- */
    d = nuevo_dir( raiz, "a_la_vez" );
    {
    GThread  *h[HILOS];
    Escritor  e[HILOS];
    int       total = HILOS * POR_HILO, mal = 0;

    GPid      p[2];
    gboolean  vivo[2];
    char      cuantas[16];

    /* Y DOS PROCESOS, que es el caso de verdad: la madre y el MTM. Este
       mismo programa con --escribe, lanzado por g_spawn, que tambien vale
       en Windows (fork no). Primero los procesos y luego los hilos, para
       que se pisen.                                                     */
    g_snprintf( cuantas, sizeof cuantas, "%d", POR_HILO );
    for ( i = 0; i < 2; i++ )
        {
        gchar *av[] = { g_prog, "--escribe", d, cuantas, NULL };

        vivo[i] = g_spawn_async( NULL, av, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &p[i], NULL );
        }
    for ( i = 0; i < HILOS; i++ )
        {
        e[i].dir = d; e[i].n = POR_HILO; e[i].malas = 0;
        h[i] = g_thread_new( "escritor", escribe_muchas, &e[i] );
        }
    for ( i = 0; i < HILOS; i++ ) { g_thread_join( h[i] ); mal += e[i].malas; }
    for ( i = 0; i < 2; i++ )
        if ( !vivo[i] ) mal++;
        else if ( espera_hijo( p[i] ) == 0 ) total += POR_HILO;
        else mal++;

    check( mal == 0, "a la vez: ninguna escritura fallo" );
    cuenta( d, &cab, &lin );
    g_snprintf( m, sizeof m, "a la vez: %d lineas de %d, y una cabecera (%d)", lin, total, cab );
    check( lin == total && cab == 1, m );
    check( coste_lee( d, &l, &n, &malas, why, sizeof why ) == 0 && n == total && malas == 0,
           "a la vez: todas se leen enteras, ninguna partida" );
    coste_lineas_libera( l, n );
    }
    g_free( d );

    printf( "test_coste: %d comprobaciones, %d fallos\n", hechas, fallos );
    g_free( raiz );
    g_free( g_prog );
    return fallos ? 1 : 0;
}
