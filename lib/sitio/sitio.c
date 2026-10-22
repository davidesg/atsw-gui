/*
 * sitio.c -- donde esta el programa que se esta ejecutando (ver sitio.h).
 */

#include "sitio.h"

#include <stdlib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

gchar *sitio_mi_dir( void )
{
    gchar *yo  = NULL;
    gchar *dir;

#ifdef G_OS_WIN32
    wchar_t w[MAX_PATH * 4];
    DWORD   n = GetModuleFileNameW( NULL, w, G_N_ELEMENTS(w) );

    if ( n > 0 && n < G_N_ELEMENTS(w) )
        yo = g_utf16_to_utf8( (const gunichar2 *) w, n, NULL, NULL, NULL );
#elif defined(__APPLE__)
    char     buf[4096];
    uint32_t n = sizeof buf;

    if ( _NSGetExecutablePath( buf, &n ) == 0 )
        {
        char *real = realpath( buf, NULL );

        yo = g_strdup( real ? real : buf );
        free( real );
        }
#else
    yo = g_file_read_link( "/proc/self/exe", NULL );
#endif

    if ( yo == NULL )
        return NULL;
    dir = g_path_get_dirname( yo );
    g_free( yo );

    return dir;
}

gchar *sitio_exe( const char *programa )
{
#ifdef G_OS_WIN32
    if ( !g_str_has_suffix( programa, ".exe" ) )
        return g_strconcat( programa, ".exe", NULL );
#endif
    return g_strdup( programa );
}

gchar *sitio_busca( const char *programa, const char *const *sitios )
{
    gchar *dir = sitio_mi_dir();
    gchar *exe = sitio_exe( programa );
    gchar *p;
    int    i;

    for ( i = 0; dir && sitios && sitios[i]; i++ )
        {
        gchar *rel = g_strdup_printf( sitios[i], exe );

        p = g_build_filename( dir, rel, NULL );
        g_free( rel );
        if ( g_file_test( p, G_FILE_TEST_IS_EXECUTABLE ) )
            { g_free( dir ); g_free( exe ); return p; }
        g_free( p );
        }
    g_free( dir );

    p = g_find_program_in_path( exe );
    g_free( exe );

    return p;
}

/* --- los hijos se van con la madre: ver sitio.h ----------------------- */

long sitio_mi_pid( void )
{
#ifdef G_OS_WIN32
    return (long) GetCurrentProcessId();
#else
    return (long) getpid();
#endif
}

typedef struct {
    long        pid;
#ifdef G_OS_WIN32
    HANDLE      h;
#endif
    GSourceFunc al_irse;
    gpointer    datos;
} Vigia;

/* EN UNIX, getppid Y NO kill(pid, 0): la madre lanza a sus hijos
 * directamente (G_SPAWN_DO_NOT_REAP_CHILD), así que es su padre; cuando se
 * va, el hijo pasa a otro padre. Con kill, un pid reutilizado por otro
 * proceso pasaría por la madre. En Windows, el HANDLE abierto al arrancar:
 * mientras esté abierto, el pid no se reutiliza.                       */
static int viva( const Vigia *v )
{
#ifdef G_OS_WIN32
    return v->h != NULL && WaitForSingleObject( v->h, 0 ) == WAIT_TIMEOUT;
#else
    return (long) getppid() == v->pid;
#endif
}

static gboolean mira( gpointer d )
{
    Vigia *v = d;

    if ( viva( v ) ) return G_SOURCE_CONTINUE;
#ifdef G_OS_WIN32
    if ( v->h ) CloseHandle( v->h );
#endif
    v->al_irse( v->datos );
    g_free( v );
    return G_SOURCE_REMOVE;
}

int sitio_vigila_madre( const char *pid, GSourceFunc al_irse, gpointer datos )
{
    Vigia *v;
    char  *fin = NULL;
    long   n;

    if ( !pid || !*pid || !al_irse ) return 0;
    n = strtol( pid, &fin, 10 );
    if ( n <= 0 || ( fin && *fin ) ) return 0;
    v = g_new0( Vigia, 1 );
    v->pid = n;
    v->al_irse = al_irse;
    v->datos = datos;
#ifdef G_OS_WIN32
    v->h = OpenProcess( SYNCHRONIZE, FALSE, (DWORD) n );
#endif
    /* si ya no está al arrancar, se va en el primer segundo: la madre se
       cerró mientras el hijo se abría                                  */
    g_timeout_add_seconds( 1, mira, v );
    return 1;
}
