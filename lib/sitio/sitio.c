/*
 * sitio.c -- donde esta el programa que se esta ejecutando (ver sitio.h).
 */

#include "sitio.h"

#ifdef G_OS_WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
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
