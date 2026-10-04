/*
 * hijo_falso.c -- un hijo que solo dice con que argumentos lo lanzaron.
 *
 * La madre lanza fue_gui, gtk_fmg y drtran_gui con «--proyecto P [opcion]
 * [fichero]», y lo que hay que comprobar es justo eso: QUE LES LLEGA. Un
 * GUI de verdad abre su ventana y no lo cuenta; este lo escribe en el
 * fichero que diga ATSW_HIJO_LOG y se va.
 *
 * EN C Y NO EN sh, a proposito: en Windows GLib no puede lanzar un guion
 * de shell -- g_spawn necesita un .exe de verdad--, asi que un falso en sh
 * solo probaria la madre en dos de las tres plataformas.
 *
 * Se escribe a un temporal y se renombra, y la ultima linea es «FIN»: la
 * prueba sondea el fichero mientras el hijo corre, y no puede leerlo a
 * medias creyendo que esta entero.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main( int argc, char **argv )
{
    const char *log = getenv( "ATSW_HIJO_LOG" );
    char        tmp[4096];
    FILE       *f;
    int         i;

    if ( log == NULL || strlen( log ) + 8 > sizeof tmp ) return 1;
    snprintf( tmp, sizeof tmp, "%s.tmp", log );

    /* En binario: en modo texto, Windows escribe "FIN\r\n" y la prueba,
       que busca "FIN\n", esperaba en balde.                             */
    f = fopen( tmp, "wb" );
    if ( f == NULL ) return 1;
    for ( i = 1; i < argc; i++ ) fprintf( f, "%s\n", argv[i] );
    fprintf( f, "FIN\n" );
    fclose( f );

    remove( log );                 /* en Windows, rename no pisa */
    return rename( tmp, log ) == 0 ? 0 : 1;
}
