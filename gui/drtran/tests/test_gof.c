/*
 * test_gof.c -- el R² de Brajin, contra numeros que salen del motor.
 *
 * EL ORACULO NO ES UNA CONSTANTE INVENTADA: el fichero de residuos lo escribe
 * drtran -e en esta misma corrida, y las series salen de los .pre. Lo que se
 * fija aqui son las dos propiedades que hacen util el numero, y que fallaron
 * en drtran-python cuando el denominador se saco de la W del cast:
 *
 *   1. R² en (0, 1) y creciente donde HAY transferencia,
 *   2. el denominador IDENTICO en las dos corridas -- es propiedad de los
 *      DATOS, no del ajuste. Si se mueve, los dos R² no son comparables y el
 *      sintoma es que el R² baja mientras la d.t. residual tambien baja.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "fue_pre_reader.h"
#include "outdiag.h"
#include "gof.h"

FILE *outputv = NULL;
real  macheps = 2.22e-16;

static int fallos = 0, pasos = 0;

static void ok( int c, const char *fmt, ... )
{
    va_list ap;

    va_start( ap, fmt );
    printf( c ? "  ok   " : "  FALLO " );
    vprintf( fmt, ap );
    printf( "\n" );
    va_end( ap );
    if (c) pasos++; else fallos++;
}

static const char *NOM[6] = { "EP", "EI", "EU", "EC", "EA", "P" };

int main( int argc, char **argv )
{
    OdResiduos rt, rd;
    double     sw[6], sw_d[6];
    double     r2t[6], r2d[6], dtt[6], dtd[6];
    int        i;

    if (argc < 4) {
        printf( "uso: test_gof DIR_M6 res_transfer.txt res_diagonal.txt\n" );
        return 0;
    }
    if (od_residuos( argv[2], &rt ) || od_residuos( argv[3], &rd )) {
        printf( "  no puedo leer los residuos\n" );
        return 0;
    }

    printf( "el R² de Brajin (A.28), sobre la serie ESTACIONARIA\n" );

    for (i = 0; i < 6 && i < rt.m; i++) {
        struct Tseries  ts;
        struct Tusmodel tm;
        real          **dm = NULL;
        char            path[512];
        double         *w;
        int             nw;

        memset( &ts, 0, sizeof ts );
        memset( &tm, 0, sizeof tm );
        snprintf( path, sizeof path, "%s/M6_%s.pre", argv[1], NOM[i] );
        if (read_fue_pre( path, &tm, &ts, &dm )) {
            printf( "  no puedo leer %s\n", path );
            return 0;
        }

        w = gof_estacionaria( &ts, &tm, &nw );
        sw[i]   = gof_suma_cuad_cola( w, nw, rt.n );
        sw_d[i] = gof_suma_cuad_cola( w, nw, rd.n );
        free( w );

        gof_r2_brajin( rt.v[i], rt.n, sw[i],   &r2t[i], &dtt[i] );
        gof_r2_brajin( rd.v[i], rd.n, sw_d[i], &r2d[i], &dtd[i] );

        printf( "   %-3s  diagonal %.4f  ->  transferencia %.4f"
                "   (d.t. %.3f -> %.3f)\n",
                NOM[i], r2d[i], r2t[i], dtd[i], dtt[i] );
    }

    printf( "\nlo que tiene que cumplirse\n" );

    for (i = 0; i < 6 && i < rt.m; i++)
        ok( r2t[i] > 0.0 && r2t[i] < 1.0,
            "%s: el R² esta en (0,1) -- sobre el NIVEL saldria ~1", NOM[i] );

    /* LA PROPIEDAD QUE SE PERDIO EN drtran-python: el denominador es de los
     * DATOS. Si sale distinto entre las dos corridas, se ha colado un
     * parametro dentro y los dos R² dejan de ser comparables.            */
    for (i = 0; i < 6 && i < rt.m; i++)
        ok( fabs( sw[i] - sw_d[i] ) < 1e-6,
            "%s: el denominador NO se mueve entre las dos corridas", NOM[i] );

    /* EP y EI son las dos que reciben entradas en el m6. */
    ok( r2t[0] > r2d[0], "EP recibe entradas y su R² SUBE (%.4f -> %.4f)",
        r2d[0], r2t[0] );
    ok( r2t[1] > r2d[1], "EI recibe entradas y su R² SUBE (%.4f -> %.4f)",
        r2d[1], r2t[1] );

    /* Y LA COHERENCIA: si el R² sube, la d.t. residual baja. Dos cifras del
     * mismo ajuste apuntando en sentidos opuestos delataron el error.    */
    for (i = 0; i < 2; i++)
        ok( ( r2t[i] > r2d[i] ) == ( dtt[i] < dtd[i] ),
            "%s: el R² y la d.t. residual apuntan al MISMO lado", NOM[i] );

    ok( fabs( r2t[5] - r2d[5] ) < 1e-9,
        "P no tiene enlaces y su R² no se mueve" );

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
