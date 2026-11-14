/*
 * test_engine.c -- el plazo de las esperas sincronas (engine_run, engine_run_v).
 *
 * EL MOTOR ES ESTE MISMO PROGRAMA: con --duerme=S espera S segundos y sale
 * con 0. Un motor colgado es uno que duerme mas que el plazo.
 * AUDITORIA-procesos B5: la ventana que espera no se mueve, y sin plazo un
 * motor colgado la congelaba para siempre.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "engine.h"
#ifndef G_OS_WIN32
#  include <signal.h>
#  include <unistd.h>
#endif

static int fallos;

static void check( int ok, const char *que, const char *visto )
{
    printf( "  %s %s%s%s\n", ok ? "ok   " : "FALLA", que,
            ok || !visto ? "" : "; se vio: ", ok || !visto ? "" : visto );
    if ( !ok ) fallos++;
}

int main( int argc, char **argv )
{
    gchar       *prog;
    EngineResult r;
    gint64       t0;
    double       s;

    if ( argc >= 2 && g_str_has_prefix( argv[1], "--duerme=" ) )
        {
#ifndef G_OS_WIN32
        if ( argc >= 3 )                 /* el motor dice quien es */
            {
            gchar *n = g_strdup_printf( "%ld", (long) getpid() );
            g_file_set_contents( argv[2], n, -1, NULL );
            g_free( n );
            }
#endif
        g_usleep( (gulong) ( g_ascii_strtod( argv[1] + 9, NULL ) * G_USEC_PER_SEC ) );
        return 0;
        }
#ifndef G_OS_WIN32
    /* UN GUI QUE SE CAE: lanza un motor que duerme y muere con SIGABRT. */
    if ( argc >= 4 && !strcmp( argv[1], "--cae" ) )
        {
        const char *const av[] = { "--duerme=30", argv[3], NULL };
        EngineJob *j = engine_start( NULL, argv[2], av, NULL, NULL, NULL, NULL );
        int        t;

        for ( t = 0; t < 50 && !g_file_test( argv[3], G_FILE_TEST_EXISTS ); t++ )
            g_usleep( 100000 );
        (void) j;
        abort();
        }
#endif

    prog = g_path_is_absolute( argv[0] ) ? g_strdup( argv[0] )
                                         : g_find_program_in_path( argv[0] );
    if ( !prog )
        {
        gchar *cwd = g_get_current_dir();
        prog = g_build_filename( cwd, argv[0], NULL );
        g_free( cwd );
        }
    printf( "lib/engine: el plazo de las esperas sincronas\n" );

    g_unsetenv( "ATSW_PLAZO_MOTOR_S" );
    check( engine_plazo_s() == 300, "sin la variable, el plazo es de 300 s", NULL );

    /* Uno que acaba a tiempo: como siempre. */
    g_setenv( "ATSW_PLAZO_MOTOR_S", "5", TRUE );
    r = engine_run( NULL, prog, "--duerme=0.1", NULL );
    check( r.status == 0, "un motor que acaba antes del plazo sale con 0", r.message );
    engine_result_clear( &r );

    /* Uno colgado: se para al plazo y se dice. */
    g_setenv( "ATSW_PLAZO_MOTOR_S", "1", TRUE );
    t0 = g_get_monotonic_time();
    {
    const char *const av[] = { "--duerme=60", NULL };
    r = engine_run_v( NULL, prog, av );
    }
    s = (double) ( g_get_monotonic_time() - t0 ) / G_USEC_PER_SEC;
    check( r.status == ENGINE_PLAZO, "uno colgado acaba en ENGINE_PLAZO", r.message );
    {
    gchar *m = g_strdup_printf( "%.1f s", s );
    check( s < 10, "y la espera dura el plazo, no lo que dormiria (60 s)", m );
    g_free( m );
    }
    check( r.message && strstr( r.message, "no terminó en 1 s" ) != NULL,
           "el mensaje dice que no termino en el plazo", r.message );
    check( !engine_wrote_results( &r ), "y no cuenta como resultados escritos", NULL );
    engine_result_clear( &r );

    /* 0 es sin plazo. */
    g_setenv( "ATSW_PLAZO_MOTOR_S", "0", TRUE );
    r = engine_run( NULL, prog, "--duerme=1.2", NULL );
    check( r.status == 0, "con ATSW_PLAZO_MOTOR_S=0 no hay plazo", r.message );
    engine_result_clear( &r );

#ifndef G_OS_WIN32
    /* B6: el GUI muere por una senal y su motor no se queda huerfano. En
       Linux lo cubre ademas PR_SET_PDEATHSIG; en macOS, solo el manejador. */
    {
    gchar *f = g_build_filename( g_get_tmp_dir(), "test_engine_motor.pid", NULL );
    gchar *av[] = { prog, (gchar *) "--cae", prog, f, NULL };
    gchar *s2 = NULL;
    long   pm = 0;
    int    vivo = 1, t;

    g_unlink( f );
    g_spawn_sync( NULL, av, NULL, G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
                  NULL, NULL, NULL, NULL );
    if ( g_file_get_contents( f, &s2, NULL, NULL ) ) pm = atol( s2 );
    check( pm > 0, "el GUI de la prueba lanzo su motor", NULL );
    for ( t = 0; pm > 0 && t < 30; t++ )
        {
        if ( kill( (pid_t) pm, 0 ) != 0 ) { vivo = 0; break; }
        g_usleep( 100000 );
        }
    check( pm > 0 && !vivo, "el GUI muere por SIGABRT y su motor no se queda huerfano", NULL );
    if ( pm > 0 && vivo ) kill( (pid_t) pm, SIGKILL );
    g_unlink( f ); g_free( f ); g_free( s2 );
    }
#endif

    g_free( prog );
    printf( "lib/engine: %d fallos\n", fallos );
    return fallos ? 1 : 0;
}
