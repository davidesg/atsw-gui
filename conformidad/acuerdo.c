/* acuerdo.c -- ¿que dice el validador en C de este fichero?
 *
 *     acuerdo <fichero>
 *
 * Imprime una linea con el veredicto de las dos puertas del C y sale con 0.
 * No carga nada ni estima: sólo pregunta.
 *
 *     fue    lo acepta como .inp de fue
 *     fuf    lo acepta como .inp de fuf
 *     no     ninguna de las dos, y el motivo de la de fue
 *
 * Sirve para contrastarlo con lo que dice el lector de Python, que es el otro
 * lector del formato. Que los dos coincidan en QUE ES LEGIBLE es una promesa
 * distinta de que los escritores conserven el modelo, y hasta ahora no la
 * comprobaba nadie.
 */

#include <stdio.h>
#include <string.h>

#include "inpcheck.h"

int main( int argc, char **argv )
{
    char porque_fue[512], porque_fuf[512];

    if ( argc < 2 ) { fprintf( stderr, "uso: acuerdo <fichero>\n" ); return 2; }

    if ( inp_check_fue( argv[1], porque_fue, sizeof porque_fue ) == 0 ) {
        printf( "fue\n" );
        return 0;
    }
    if ( inp_check_fuf( argv[1], porque_fuf, sizeof porque_fuf ) == 0 ) {
        printf( "fuf\n" );
        return 0;
    }
    printf( "no %s\n", porque_fue );
    return 0;
}
