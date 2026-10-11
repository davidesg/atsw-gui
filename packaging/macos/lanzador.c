/*
 * lanzador.c -- el ejecutable de ATSW.app (Contents/MacOS/ATSW).
 *
 * La .app lleva dentro un prefijo entero, en Contents/Resources: bin/ con
 * los motores y los GUIs, lib/ con las bibliotecas de Homebrew (reescritas
 * a @executable_path/../lib), share/ con los esquemas y los iconos.
 *
 * GTK en macOS no sabe encontrar ese prefijo solo, como hace en Windows, asi
 * que hay que decirselo con variables de entorno antes de arrancar la madre.
 * Y el PATH: drtran_gui y fue_gui buscan los motores por el PATH, y la madre
 * no va a encontrar otro sitio dentro de la .app.
 *
 * Es un programa y no un guion de shell porque la .app se firma (ad hoc, sin
 * cuenta de desarrollador), y un ejecutable principal que es un guion se
 * firma mal y Gatekeeper dice que la aplicacion «esta danada».
 *
 * La cache de los cargadores de gdk-pixbuf lleva rutas absolutas, y la .app
 * se puede mover (o macOS la ejecuta desde una copia en otro sitio, la
 * «translocacion»). Por eso la cache va como plantilla, con @PREFIJO@ en
 * vez de la ruta, y se escribe la de verdad al arrancar.
 */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void entorno( const char *var, const char *res, const char *sub )
{
    char v[PATH_MAX * 2];

    snprintf( v, sizeof v, "%s/%s", res, sub );
    setenv( var, v, 1 );
}

/* La plantilla con @PREFIJO@ -> la cache de verdad en $TMPDIR. */
static void cache_pixbuf( const char *res )
{
    char  plantilla[PATH_MAX], cache[PATH_MAX], linea[PATH_MAX * 2];
    const char *tmp = getenv( "TMPDIR" );
    FILE *in, *out;

    snprintf( plantilla, sizeof plantilla,
              "%s/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache.in", res );
    snprintf( cache, sizeof cache, "%s/atsw-%d-loaders.cache",
              tmp && *tmp ? tmp : "/tmp", (int) getuid() );
    if ( (in = fopen( plantilla, "r" )) == NULL )
        return;
    if ( (out = fopen( cache, "w" )) == NULL )
        { fclose( in ); return; }
    while ( fgets( linea, sizeof linea, in ) )
        {
        char *p = linea, *q;

        while ( (q = strstr( p, "@PREFIJO@" )) != NULL )
            {
            fwrite( p, 1, (size_t) (q - p), out );
            fputs( res, out );
            p = q + strlen( "@PREFIJO@" );
            }
        fputs( p, out );
        }
    fclose( in );
    fclose( out );
    setenv( "GDK_PIXBUF_MODULE_FILE", cache, 1 );
}

int main( int argc, char **argv )
{
    char     yo[PATH_MAX], real[PATH_MAX], res[PATH_MAX], madre[PATH_MAX];
    char     path[PATH_MAX * 4];
    uint32_t n = sizeof yo;
    const char *viejo;
    char    *barra;
    int      i, j;

    if ( _NSGetExecutablePath( yo, &n ) != 0 || realpath( yo, real ) == NULL )
        { fprintf( stderr, "ATSW: no sé dónde estoy\n" ); return 1; }

    /* .../ATSW.app/Contents/MacOS/ATSW -> .../ATSW.app/Contents/Resources */
    if ( (barra = strrchr( real, '/' )) != NULL ) *barra = '\0';
    if ( (barra = strrchr( real, '/' )) != NULL ) *barra = '\0';
    snprintf( res, sizeof res, "%s/Resources", real );

    viejo = getenv( "PATH" );
    snprintf( path, sizeof path, "%s/bin:%s", res,
              viejo && *viejo ? viejo : "/usr/bin:/bin:/usr/sbin:/sbin" );
    setenv( "PATH", path, 1 );

    entorno( "XDG_DATA_DIRS",        res, "share" );
    entorno( "GSETTINGS_SCHEMA_DIR", res, "share/glib-2.0/schemas" );
    entorno( "GTK_DATA_PREFIX",      res, "" );
    entorno( "GTK_EXE_PREFIX",       res, "" );
    entorno( "GTK_PATH",             res, "lib/gtk-3.0" );
    cache_pixbuf( res );

    /* Desde el Finder el directorio de trabajo es /, donde no se puede
       escribir. Que los dialogos abran en la carpeta del usuario. */
    if ( getenv( "HOME" ) )
        (void) chdir( getenv( "HOME" ) );

    /* El Finder de los macOS viejos pasa un -psn_0_12345 que la madre no
       conoce. Fuera. */
    for ( i = j = 1; i < argc; i++ )
        if ( strncmp( argv[i], "-psn_", 5 ) != 0 )
            argv[j++] = argv[i];
    argv[j] = NULL;

    snprintf( madre, sizeof madre, "%s/bin/atsw_gui", res );
    argv[0] = madre;
    execv( madre, argv );
    fprintf( stderr, "ATSW: no puedo lanzar %s: %s\n", madre, strerror( errno ) );

    return 1;
}
