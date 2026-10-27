/*
 * test_lanza.c -- sitio_lanza_y_espera y sitio_lanza_con_tuberias (sitio.h).
 *
 * El programa se lanza a si mismo con --hijo, que escribe en la salida y en
 * el error y sale con 3. En las tres plataformas se comprueba que las dos
 * funciones lanzan, separan salida y error, devuelven el estado y pasan los
 * argumentos tal cual (con espacios, comillas y barras: en Windows la linea
 * de ordenes la compone sitio.c, y ahi es donde se puede romper).
 *
 * EN WINDOWS, ADEMAS, LA CONSOLA (gui/atsw/BUGS.md BUG-0001). La prueba
 * suelta la suya (FreeConsole), para estar como un GUI enlazado con
 * -mwindows, y el hijo dice si tiene ventana de consola (GetConsoleWindow).
 * Con sitio_lanza_* no puede tenerla: se lanza con CREATE_NO_WINDOW. Con
 * g_spawn_sync se mira tambien y se dice, sin que cuente para el resultado:
 * es la causa del fallo, vista en la maquina donde corre la prueba. Y se
 * dice lo que tarda cada camino, que es la otra mitad del sintoma.
 *
 * Sale con 0 si todo pasa, con 1 si no.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "sitio.h"

#ifdef G_OS_WIN32
#  include <windows.h>
#  include <io.h>
#  define lee_fd _read
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  define lee_fd read
#endif

static int fallos = 0;

static void check( int ok, const char *que, const char *detalle )
{
    printf( "  %s  %s\n", ok ? "ok  " : "FALLA", que );
    if ( !ok ) { fallos++; if ( detalle ) printf( "        %s\n", detalle ); }
}

static int codigo( gint st )
{
#ifdef G_OS_WIN32
    return st;
#else
    return WIFEXITED( st ) ? WEXITSTATUS( st ) : -1;
#endif
}

/* Sin letras fuera del ASCII: el hijo las recibe en la pagina de codigos
 * del sistema, y esta prueba no lleva el manifiesto UTF-8 de los motores
 * (lib/windows/utf8.mk). Las comillas y las barras son lo que compone
 * sitio.c, y eso es lo que se mira.                                    */
static const char *const RARO[] = { "con espacio", "co\"milla", "barra\\",
                                    "\\\\server\\a b\\", "", "a\\\"b", NULL };

static int hijo( int argc, char **argv )
{
    int i;

#ifdef G_OS_WIN32
    printf( "consola=%d\n", GetConsoleWindow() != NULL );
#endif
    for ( i = 2; i < argc; i++ ) printf( "arg=[%s]\n", argv[i] );
    fprintf( stderr, "esto va al error\n" );
    return 3;
}

/* Los argumentos que llegan, tal como los escribe el hijo. */
static gchar *esperados( void )
{
    GString *s = g_string_new( NULL );
    int      i;

    for ( i = 0; RARO[i]; i++ ) g_string_append_printf( s, "arg=[%s]\n", RARO[i] );
    return g_string_free( s, FALSE );
}

static gchar **orden( const char *yo )
{
    gchar **av = g_new0( gchar *, G_N_ELEMENTS(RARO) + 2 );
    int     i;

    av[0] = g_strdup( yo );
    av[1] = g_strdup( "--hijo" );
    for ( i = 0; RARO[i]; i++ ) av[i + 2] = g_strdup( RARO[i] );
    return av;
}

static gchar *lee_todo( gint fd )
{
    GString *s = g_string_new( NULL );
    char     b[512];
    int      n;

    while ( ( n = (int) lee_fd( fd, b, sizeof b ) ) > 0 )
        g_string_append_len( s, b, n );
#ifdef G_OS_WIN32
    _close( fd );
#else
    close( fd );
#endif
    return g_string_free( s, FALSE );
}

/* Sin los \r: en Windows el hijo escribe en modo texto (\r\n), y las dos
 * funciones --como g_spawn-- entregan los bytes tal cual.              */
static gchar *sin_cr( gchar *t )
{
    gchar *a, *b;

    if ( t == NULL ) return NULL;
    for ( a = b = t; *a; a++ ) if ( *a != '\r' ) *b++ = *a;
    *b = '\0';
    return t;
}

typedef struct { GMainLoop *bucle; gint st; } Fin;

static void fin( GPid pid, gint st, gpointer d )
{
    Fin *f = d;

    f->st = st;
    g_spawn_close_pid( pid );
    g_main_loop_quit( f->bucle );
}

#ifdef G_OS_WIN32
/* «consola=1» en la salida del hijo: tenia ventana de consola. */
static int con_consola( const char *sal )
{
    return sal && strstr( sal, "consola=1" ) != NULL;
}
#endif

int main( int argc, char **argv )
{
    gchar  *yo, *quiere, **av, *sal = NULL, *err = NULL;
    GError *e = NULL;
    gint    st = -1;
    gboolean ok;

    if ( argc > 1 && !strcmp( argv[1], "--hijo" ) ) return hijo( argc, argv );

    yo = g_find_program_in_path( argv[0] );
    if ( yo == NULL ) yo = g_strdup( argv[0] );
    quiere = esperados();
    av = orden( yo );

#ifdef G_OS_WIN32
    /* Como un GUI: sin consola. La salida de la prueba es una tuberia (la
       de la bateria), y sigue valiendo.                               */
    FreeConsole();
    printf( "(sin consola, como un GUI enlazado con -mwindows)\n" );
#endif

    /* --- sitio_lanza_y_espera ---------------------------------------- */
    ok = sitio_lanza_y_espera( NULL, av, NULL, 0, NULL, NULL, &sal, &err, &st, &e );
    check( ok, "sitio_lanza_y_espera lanza", e ? e->message : NULL );
    if ( ok )
        {
        sin_cr( sal ); sin_cr( err );
        check( codigo( st ) == 3, "y devuelve el estado del hijo (3)", NULL );
        check( sal && strstr( sal, quiere ) != NULL,
               "los argumentos llegan tal cual (espacios, comillas, barras)", sal );
        check( err && !strcmp( err, "esto va al error\n" )
                   && !strstr( sal, "esto va al error" ),
               "la salida y el error, separados", err );
#ifdef G_OS_WIN32
        check( !con_consola( sal ), "el hijo NO tiene ventana de consola", sal );
#endif
        }
    g_free( sal ); g_free( err ); sal = err = NULL;
    if ( e ) { g_error_free( e ); e = NULL; }

    /* --- sitio_lanza_con_tuberias ------------------------------------ */
    {
    GPid pid;
    gint fo = -1, fe = -1;
    Fin  f = { NULL, -1 };

    ok = sitio_lanza_con_tuberias( NULL, av, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                   NULL, NULL, &pid, NULL, &fo, &fe, &e );
    check( ok, "sitio_lanza_con_tuberias lanza", e ? e->message : NULL );
    if ( ok )
        {
        sal = sin_cr( lee_todo( fo ) );
        err = sin_cr( lee_todo( fe ) );
        f.bucle = g_main_loop_new( NULL, FALSE );
        g_child_watch_add( pid, fin, &f );
        g_main_loop_run( f.bucle );
        g_main_loop_unref( f.bucle );
        check( codigo( f.st ) == 3, "y g_child_watch recoge su estado (3)", NULL );
        check( sal && strstr( sal, quiere ) != NULL, "con los argumentos tal cual", sal );
        check( err && !strcmp( err, "esto va al error\n" ), "y el error aparte", err );
#ifdef G_OS_WIN32
        check( !con_consola( sal ), "el hijo NO tiene ventana de consola", sal );
#endif
        g_free( sal ); g_free( err ); sal = err = NULL;
        }
    if ( e ) { g_error_free( e ); e = NULL; }
    }

    /* --- un programa que no esta ------------------------------------- */
    {
    gchar *no[] = { "/no/existe/programa", NULL };

    ok = sitio_lanza_y_espera( NULL, no, NULL, 0, NULL, NULL, &sal, &err, &st, &e );
    check( !ok && e != NULL, "un programa que no existe da error, no revienta", NULL );
    if ( e ) { g_error_free( e ); e = NULL; }
    if ( ok ) { g_free( sal ); g_free( err ); }
    sal = err = NULL;
    }

#ifdef G_OS_WIN32
    /* LA CAUSA, VISTA AQUI: lo mismo con g_spawn_sync. No cuenta para el
       resultado (es GLib, no esto), pero dice si la consola asomaba.  */
    {
    gint64 t0;
    int    i, n = 5;
    double ms_g = 0, ms_s = 0;

    ok = g_spawn_sync( NULL, av, NULL, 0, NULL, NULL, &sal, &err, &st, &e );
    if ( ok )
        printf( "  info  con g_spawn_sync el hijo %s ventana de consola\n",
                con_consola( sal ) ? "SI tiene" : "no tiene" );
    else
        printf( "  info  g_spawn_sync no lanza: %s\n", e ? e->message : "?" );
    g_free( sal ); g_free( err ); sal = err = NULL;
    if ( e ) { g_error_free( e ); e = NULL; }

    for ( i = 0; i < n; i++ )
        {
        t0 = g_get_monotonic_time();
        if ( g_spawn_sync( NULL, av, NULL, 0, NULL, NULL, &sal, &err, &st, NULL ) )
            { g_free( sal ); g_free( err ); }
        ms_g += ( g_get_monotonic_time() - t0 ) / 1000.0;
        t0 = g_get_monotonic_time();
        if ( sitio_lanza_y_espera( NULL, av, NULL, 0, NULL, NULL, &sal, &err, &st, NULL ) )
            { g_free( sal ); g_free( err ); }
        ms_s += ( g_get_monotonic_time() - t0 ) / 1000.0;
        }
    printf( "  info  una corrida, de media en %d: g_spawn_sync %.1f ms, "
            "sitio_lanza_y_espera %.1f ms\n", n, ms_g / n, ms_s / n );
    sal = err = NULL;
    }
#endif

    g_strfreev( av );
    g_free( quiere ); g_free( yo );
    printf( fallos ? "test_lanza: %d FALLOS\n" : "test_lanza: todo pasa\n", fallos );
    fflush( stdout );
    return fallos ? 1 : 0;
}
