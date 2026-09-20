/*
 * rutas.c -- ver rutas.h.
 *
 * Se aceptan las dos barras: la suite se compila tambien con MXE para Windows
 * (engines/fue/Makefile), y alli una ruta puede traer '\'.
 */

#include <string.h>
#include <stdio.h>

#include "rutas.h"

/* El ultimo separador, sea cual sea. NULL si no hay. */
static const char *sep( const char *path )
{
    const char *a = strrchr( path, '/' );
    const char *b = strrchr( path, '\\' );

    return ( a > b ) ? a : b;
}

/* Donde empieza la extension DEL NOMBRE. NULL si no hay.
 * Un punto en el directorio --"../caso/X"-- no es una extension.      */
static const char *ext( const char *path, const char *nombre )
{
    const char *p = strrchr( nombre, '.' );

    (void) path;
    /* ".oculto" no es extension: el punto inicial es parte del nombre. */
    return ( p != NULL && p != nombre ) ? p : NULL;
}

int ruta_componer( const char *path, const char *prefijo, const char *sufijo,
                   char *out, size_t n )
{
    const char *s, *nombre;
    int         dir, esc;

    if ( out == NULL || n == 0 ) return 1;
    out[0] = '\0';
    if ( path == NULL ) return 1;

    s      = sep( path );
    nombre = ( s == NULL ) ? path : s + 1;
    dir    = ( s == NULL ) ? 0 : (int) ( s + 1 - path );

    esc = snprintf( out, n, "%.*s%s%s%s", dir, path,
                    prefijo ? prefijo : "", nombre, sufijo ? sufijo : "" );
    return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}

int ruta_sin_ext( const char *path, char *out, size_t n )
{
    const char *s, *nombre, *e;
    int         largo, esc;

    if ( out == NULL || n == 0 ) return 1;
    out[0] = '\0';
    if ( path == NULL ) return 1;

    s      = sep( path );
    nombre = ( s == NULL ) ? path : s + 1;
    e      = ext( path, nombre );
    largo  = ( e == NULL ) ? (int) strlen( path ) : (int) ( e - path );

    esc = snprintf( out, n, "%.*s", largo, path );
    return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}

int ruta_dir( const char *path, char *out, size_t n )
{
    const char *s;
    int         esc;

    if ( out == NULL || n == 0 ) return 1;
    out[0] = '\0';
    if ( path == NULL ) return 1;

    s = sep( path );
    if ( s == NULL )
        esc = snprintf( out, n, "." );
    else if ( s == path )                    /* "/X": el directorio es "/" */
        esc = snprintf( out, n, "%c", *s );
    else
        esc = snprintf( out, n, "%.*s", (int) ( s - path ), path );

    return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}

int ruta_base( const char *path, char *out, size_t n )
{
    const char *s;
    int         esc;

    if ( out == NULL || n == 0 ) return 1;
    out[0] = '\0';
    if ( path == NULL ) return 1;

    s   = sep( path );
    esc = snprintf( out, n, "%s", ( s == NULL ) ? path : s + 1 );
    return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}

int ruta_nombre( const char *path, char *out, size_t n )
{
    const char *s, *nombre, *e;
    int         largo, esc;

    if ( out == NULL || n == 0 ) return 1;
    out[0] = '\0';
    if ( path == NULL ) return 1;

    s      = sep( path );
    nombre = ( s == NULL ) ? path : s + 1;
    e      = ext( path, nombre );
    largo  = ( e == NULL ) ? (int) strlen( nombre ) : (int) ( e - nombre );

    esc = snprintf( out, n, "%.*s", largo, nombre );
    return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}
