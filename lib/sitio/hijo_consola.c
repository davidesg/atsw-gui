/*
 * hijo_consola.c -- un programa DE CONSOLA, como los motores, que dice si
 * tiene ventana de consola. Lo lanza test_consola.c (ver alli).
 *
 * Escribe «consola=1» si la tiene y «consola=0» si no, y sale con 0.
 */

#include <stdio.h>

#ifdef _WIN32
#  include <windows.h>
#endif

int main( void )
{
#ifdef _WIN32
    printf( "consola=%d\n", GetConsoleWindow() != NULL );
#else
    printf( "consola=0\n" );
#endif
    return 0;
}
