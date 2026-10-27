/*
 * sitio.c -- donde esta el programa que se esta ejecutando (ver sitio.h).
 */

#include "sitio.h"

#include <stdlib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
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

/* --- los motores, sin consola en Windows: ver sitio.h ----------------- */

#ifndef G_OS_WIN32

gboolean sitio_lanza_con_tuberias( const gchar *dir, gchar **argv, gchar **envp,
                                   GSpawnFlags flags,
                                   GSpawnChildSetupFunc child_setup,
                                   gpointer user_data, GPid *child_pid,
                                   gint *standard_input, gint *standard_output,
                                   gint *standard_error, GError **error )
{
    return g_spawn_async_with_pipes( dir, argv, envp, flags, child_setup,
                                     user_data, child_pid, standard_input,
                                     standard_output, standard_error, error );
}

gboolean sitio_lanza_y_espera( const gchar *dir, gchar **argv, gchar **envp,
                               GSpawnFlags flags,
                               GSpawnChildSetupFunc child_setup,
                               gpointer user_data, gchar **standard_output,
                               gchar **standard_error, gint *wait_status,
                               GError **error )
{
    return g_spawn_sync( dir, argv, envp, flags, child_setup, user_data,
                         standard_output, standard_error, wait_status, error );
}

#else

gboolean sitio_abre( const char *ruta )
{
    gunichar2 *w = g_utf8_to_utf16( ruta, -1, NULL, NULL, NULL );
    HINSTANCE  r;

    if ( w == NULL ) return FALSE;
    r = ShellExecuteW( NULL, L"open", (wchar_t *) w, NULL, NULL, SW_SHOWNORMAL );
    g_free( w );
    return (INT_PTR) r > 32;
}

/* Un argumento para la linea de ordenes, con las reglas con que la parte el
 * C de Microsoft (CommandLineToArgvW): entre comillas si lleva espacios o
 * comillas, y las barras invertidas dobladas solo delante de una comilla o
 * del final.                                                           */
static void cita( GString *s, const gchar *a )
{
    const gchar *p;

    if ( *a && !strpbrk( a, " \t\n\v\"" ) ) { g_string_append( s, a ); return; }
    g_string_append_c( s, '"' );
    for ( p = a; ; p++ )
        {
        gsize barras = 0;

        while ( *p == '\\' ) { barras++; p++; }
        if ( *p == '\0' )
            { while ( barras-- ) g_string_append( s, "\\\\" ); break; }
        if ( *p == '"' )
            { while ( barras-- ) g_string_append( s, "\\\\" );
              g_string_append( s, "\\\"" ); }
        else
            { while ( barras-- ) g_string_append_c( s, '\\' );
              g_string_append_c( s, *p ); }
        }
    g_string_append_c( s, '"' );
}

static HANDLE nul( DWORD acceso, SECURITY_ATTRIBUTES *sa )
{
    return CreateFileW( L"NUL", acceso, FILE_SHARE_READ | FILE_SHARE_WRITE, sa,
                        OPEN_EXISTING, 0, NULL );
}

static void cierra( HANDLE *h )
{
    if ( *h && *h != INVALID_HANDLE_VALUE ) CloseHandle( *h );
    *h = NULL;
}

/* EL LANZAMIENTO EN WINDOWS. in_w, out_r y err_r, si no son NULL, reciben el
 * extremo del padre de cada tuberia; los que son NULL van a NUL. El hijo
 * hereda SOLO sus tres manejadores (PROC_THREAD_ATTRIBUTE_HANDLE_LIST): si
 * heredara cualquier otro heredable --la tuberia de otra corrida lanzada a
 * la vez-- esa no veria su fin de fichero hasta que este acabase.        */
static gboolean lanza_win( const gchar *dir, gchar **argv, GSpawnFlags flags,
                           HANDLE *proc, HANDLE *in_w, HANDLE *out_r,
                           HANDLE *err_r, GError **error )
{
    SECURITY_ATTRIBUTES  sa = { sizeof sa, NULL, TRUE };
    HANDLE               hijo[3] = { NULL, NULL, NULL };
    HANDLE               padre[3] = { NULL, NULL, NULL };
    STARTUPINFOEXW       si;
    PROCESS_INFORMATION  pi;
    SIZE_T               tam = 0;
    gchar               *app = NULL;
    GString             *cl = g_string_new( NULL );
    gunichar2           *wapp = NULL, *wcl = NULL, *wdir = NULL;
    DWORD                como, err = 0;
    gboolean             ok = FALSE;
    int                  i;

    if ( argv == NULL || argv[0] == NULL )
        { g_set_error( error, G_SPAWN_ERROR, G_SPAWN_ERROR_INVAL,
                       "no hay programa que lanzar" );
          g_string_free( cl, TRUE ); return FALSE; }

    /* El programa, con su ruta y su .exe: CreateProcessW no busca en el
     * PATH ni pone la extension cuando se le da el nombre aparte.      */
    if ( ( flags & G_SPAWN_SEARCH_PATH ) && !g_path_is_absolute( argv[0] ) )
        app = g_find_program_in_path( argv[0] );
    if ( app == NULL ) app = g_strdup( argv[0] );
    if ( !g_file_test( app, G_FILE_TEST_IS_REGULAR ) )
        {
        gchar *conexe = g_strconcat( app, ".exe", NULL );

        if ( g_file_test( conexe, G_FILE_TEST_IS_REGULAR ) )
            { g_free( app ); app = conexe; }
        else
            g_free( conexe );
        }

    for ( i = 0; argv[i]; i++ )
        {
        if ( i ) g_string_append_c( cl, ' ' );
        cita( cl, argv[i] );
        }

    /* Los tres manejadores del hijo: tuberia o NUL. */
    if ( in_w ? !CreatePipe( &hijo[0], &padre[0], &sa, 0 )
              : ( hijo[0] = nul( GENERIC_READ, &sa ) ) == INVALID_HANDLE_VALUE )
        goto fuera;
    if ( out_r ? !CreatePipe( &padre[1], &hijo[1], &sa, 0 )
               : ( hijo[1] = nul( GENERIC_WRITE, &sa ) ) == INVALID_HANDLE_VALUE )
        goto fuera;
    if ( err_r ? !CreatePipe( &padre[2], &hijo[2], &sa, 0 )
               : ( hijo[2] = nul( GENERIC_WRITE, &sa ) ) == INVALID_HANDLE_VALUE )
        goto fuera;
    for ( i = 0; i < 3; i++ )
        if ( padre[i] ) SetHandleInformation( padre[i], HANDLE_FLAG_INHERIT, 0 );

    memset( &si, 0, sizeof si );
    si.StartupInfo.cb         = sizeof si;
    si.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput  = hijo[0];
    si.StartupInfo.hStdOutput = hijo[1];
    si.StartupInfo.hStdError  = hijo[2];
    InitializeProcThreadAttributeList( NULL, 1, 0, &tam );
    si.lpAttributeList = g_malloc( tam );
    if ( !InitializeProcThreadAttributeList( si.lpAttributeList, 1, 0, &tam ) )
        { g_free( si.lpAttributeList ); si.lpAttributeList = NULL; goto fuera; }
    if ( !UpdateProcThreadAttribute( si.lpAttributeList, 0,
                                     PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                     hijo, sizeof hijo, NULL, NULL ) )
        goto fuera;

    wapp = g_utf8_to_utf16( app, -1, NULL, NULL, NULL );
    wcl  = g_utf8_to_utf16( cl->str, -1, NULL, NULL, NULL );
    wdir = dir ? g_utf8_to_utf16( dir, -1, NULL, NULL, NULL ) : NULL;
    if ( wapp == NULL || wcl == NULL || ( dir && wdir == NULL ) )
        { SetLastError( ERROR_NO_UNICODE_TRANSLATION ); goto fuera; }

    /* LA RAZON DE TODO ESTO: sin consola propia si el que lanza no la tiene. */
    como = EXTENDED_STARTUPINFO_PRESENT
         | ( GetConsoleWindow() == NULL ? CREATE_NO_WINDOW : 0 );
    memset( &pi, 0, sizeof pi );
    if ( !CreateProcessW( (wchar_t *) wapp, (wchar_t *) wcl, NULL, NULL, TRUE,
                          como, NULL, (wchar_t *) wdir, &si.StartupInfo, &pi ) )
        goto fuera;

    CloseHandle( pi.hThread );
    *proc = pi.hProcess;
    if ( in_w )  { *in_w  = padre[0]; padre[0] = NULL; }
    if ( out_r ) { *out_r = padre[1]; padre[1] = NULL; }
    if ( err_r ) { *err_r = padre[2]; padre[2] = NULL; }
    ok = TRUE;

fuera:
    if ( !ok ) err = GetLastError();
    if ( si.lpAttributeList )
        { DeleteProcThreadAttributeList( si.lpAttributeList );
          g_free( si.lpAttributeList ); }
    for ( i = 0; i < 3; i++ ) { cierra( &hijo[i] ); cierra( &padre[i] ); }
    if ( !ok )
        {
        gchar *m = g_win32_error_message( err );

        g_set_error( error, G_SPAWN_ERROR,
                     err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND
                         ? G_SPAWN_ERROR_NOENT : G_SPAWN_ERROR_FAILED,
                     "no se pudo lanzar %s: %s", app, m );
        g_free( m );
        }
    g_free( wapp ); g_free( wcl ); g_free( wdir );
    g_free( app );
    g_string_free( cl, TRUE );
    return ok;
}

/* El manejador del extremo del padre, como descriptor del C: es lo que
 * devuelve g_spawn_async_with_pipes en Windows, y lo que espera
 * g_io_channel_win32_new_fd.                                           */
static gint a_fd( HANDLE h, int modo )
{
    gint fd = _open_osfhandle( (intptr_t) h, modo | _O_BINARY );

    if ( fd < 0 ) CloseHandle( h );
    return fd;
}

gboolean sitio_lanza_con_tuberias( const gchar *dir, gchar **argv, gchar **envp,
                                   GSpawnFlags flags,
                                   GSpawnChildSetupFunc child_setup,
                                   gpointer user_data, GPid *child_pid,
                                   gint *standard_input, gint *standard_output,
                                   gint *standard_error, GError **error )
{
    HANDLE proc = NULL, in = NULL, out = NULL, err = NULL;

    (void) child_setup; (void) user_data;
    if ( envp != NULL )                 /* no hace falta: que lo haga GLib */
        return g_spawn_async_with_pipes( dir, argv, envp, flags, child_setup,
                                         user_data, child_pid, standard_input,
                                         standard_output, standard_error, error );
    if ( !lanza_win( dir, argv, flags, &proc,
                     standard_input ? &in : NULL,
                     standard_output ? &out : NULL,
                     standard_error ? &err : NULL, error ) )
        return FALSE;
    if ( standard_input )  *standard_input  = a_fd( in, _O_WRONLY );
    if ( standard_output ) *standard_output = a_fd( out, _O_RDONLY );
    if ( standard_error )  *standard_error  = a_fd( err, _O_RDONLY );
    if ( child_pid ) *child_pid = proc;
    else             CloseHandle( proc );
    return TRUE;
}

/* Lee una tuberia hasta su fin. El error va en un hilo y la salida en el
 * que llama: leidas una detras de otra, un motor que llenase la tuberia del
 * error mientras se espera la de la salida se quedaria parado para siempre. */
typedef struct { HANDLE h; GString *s; } Lectura;

static gpointer lee( gpointer d )
{
    Lectura *l = d;
    char     buf[4096];
    DWORD    n;

    while ( ReadFile( l->h, buf, sizeof buf, &n, NULL ) && n > 0 )
        g_string_append_len( l->s, buf, (gssize) n );
    return NULL;
}

gboolean sitio_lanza_y_espera( const gchar *dir, gchar **argv, gchar **envp,
                               GSpawnFlags flags,
                               GSpawnChildSetupFunc child_setup,
                               gpointer user_data, gchar **standard_output,
                               gchar **standard_error, gint *wait_status,
                               GError **error )
{
    HANDLE   proc = NULL, out = NULL, err = NULL;
    Lectura  lo = { NULL, NULL }, le = { NULL, NULL };
    GThread *hilo = NULL;
    DWORD    codigo = 0;

    if ( envp != NULL )
        return g_spawn_sync( dir, argv, envp, flags, child_setup, user_data,
                             standard_output, standard_error, wait_status, error );
    if ( !lanza_win( dir, argv, flags, &proc, NULL,
                     standard_output ? &out : NULL,
                     standard_error ? &err : NULL, error ) )
        return FALSE;
    if ( err )
        {
        le.h = err; le.s = g_string_new( NULL );
        hilo = g_thread_new( "sitio-stderr", lee, &le );
        }
    if ( out )
        { lo.h = out; lo.s = g_string_new( NULL ); lee( &lo ); }
    if ( hilo ) g_thread_join( hilo );
    WaitForSingleObject( proc, INFINITE );
    GetExitCodeProcess( proc, &codigo );
    CloseHandle( proc );
    if ( out ) CloseHandle( out );
    if ( err ) CloseHandle( err );
    if ( standard_output ) *standard_output = g_string_free( lo.s, FALSE );
    if ( standard_error )  *standard_error  = g_string_free( le.s, FALSE );
    if ( wait_status ) *wait_status = (gint) codigo;
    return TRUE;
}

#endif
