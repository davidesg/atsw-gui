/* fake_drvarma.c -- un drvarma cuya puerta FALLA, para la prueba de sim_gui.
 *
 * Escribe en <-o NOMBRE>.out la cabecera de la escalera y una puerta que no
 * cuadra, y sale con 5, como drvarma (escalera.c) cuando el sistema diagonal
 * no reproduce los univariantes. Es un programa y no un guion: en Windows un
 * guion de shell no se lanza.
 */
#include <stdio.h>
#include <string.h>

int main( int argc, char **argv )
{
    const char *o = NULL;
    char r[4096];
    FILE *f;
    int i;

    for ( i = 1; i < argc - 1; i++ ) if ( !strcmp( argv[i], "-o" ) ) o = argv[i + 1];
    if ( !o ) return 1;
    snprintf( r, sizeof r, "%s.out", o );
    if ( !( f = fopen( r, "w" ) ) ) return 1;
    fprintf( f, "Program          : drvarma 5.0.0 (falso) (ladder mode: .pre input)\n"
                "Model            : VARMA with univariate diagonals; cross orders p=0 q=0\n"
                "Innovation cov.  : diagonal\n\n"
                "=============================================================\n"
                "  THE DIAGONAL GATE\n"
                "=============================================================\n"
                "  series                     logL         sigma2      max|move|\n"
                "  MUSKRAT             -292.791318        831.114       4.15e-05\n"
                "  MINK                -286.554344        688.124       4.15e-05\n"
                "  SUM                 -579.345662\n"
                "  joint diagonal      -575.000000\n"
                "  difference             4.35\n"
                "  GATE: FAILED\n\n" );
    fclose( f );
    printf( "ERROR: the diagonal gate failed (difference 4.35).\n" );
    return 5;
}
