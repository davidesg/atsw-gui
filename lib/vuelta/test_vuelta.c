/*
 * test_vuelta.c -- la vuelta de drtran al univariante, contra el motor.
 *
 * Lo que NO se comprueba aqui: que en la diagonal lo escrito sea el .pre. Lo
 * es cuando el .pre es el optimo de fue (el caso IPC <- WTI del manual: los
 * mismos valores, a los seis decimales del .out), pero los .pre del m6 son
 * los numeros redondeados del libro, y eso mediria el .pre, no la vuelta.
 *
 *   test_vuelta DIR fichero.out a.pre b.pre ...
 *
 * El .out es de una corrida de drtran sobre esos .pre, en ese orden. Por
 * cada serie:
 *   - lo escrito se relee y da, en el orden de lib/slots, los valores del .out;
 *   - las banderas son las del .pre, todas;
 *   - y con el .pre de OTRA serie, las cuentas no casan y no se escribe nada.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vuelta.h"
#include "inpdet.h"

static int fallos = 0;

static void ok( int c, const char *que, const char *mas )
{
    printf( "  %s %s%s%s\n", c ? "ok   " : "FALLO", que, mas ? ": " : "", mas ? mas : "" );
    if ( !c ) fallos++;
}

/* Los libres de un fichero en el orden de lib/slots; fl_todas las banderas
 * en el orden del fichero, para compararlas.                             */
static int libres( const char *f, double *v, int *fl, int *nfl )
{
    IdArma a;
    IdMaf  m;
    double d[8000], mu;
    int    dfl[8000], nd, fmu, n = 0, k, i, j, s;
    char   w[256];
    const int sec[2][2] = { { 0, 1 }, { 2, 3 } };

    *nfl = 0;
    if ( id_arma_lee( f, &a, w, sizeof w ) || id_maf_lee( f, &m, w, sizeof w ) ) return -1;
    for ( s = 0; s < 2; s++ )
        {
        for ( k = 0; k < 2; k++ )
            for ( i = 0; i < a.nf[sec[s][k]]; i++ )
                for ( j = 0; j < a.ord[sec[s][k]][i]; j++ )
                    {
                    fl[(*nfl)++] = a.fl[sec[s][k]][i][j];
                    if ( a.fl[sec[s][k]][i][j] == 1 ) v[n++] = a.v[sec[s][k]][i][j];
                    }
        if ( s == 1 )
            for ( i = 0; i < m.n; i++ )
                { fl[(*nfl)++] = m.fl[i]; if ( m.fl[i] == 1 ) v[n++] = m.v[i]; }
        }
    if ( ( nd = id_det_coefs( f, d, dfl, 8000, w, sizeof w ) ) < 0 ) return -1;
    for ( i = 0; i < nd; i++ )
        { fl[(*nfl)++] = dfl[i]; if ( dfl[i] == 1 ) v[n++] = d[i]; }
    if ( id_media_lee( f, &mu, &fmu, w, sizeof w ) ) return -1;
    fl[(*nfl)++] = fmu;
    if ( fmu == 1 ) v[n++] = mu;
    return n;
}

/* Los de la serie k en el .out, en su orden: AR, MA, deterministas, media. */
static int del_out( const OdParams *p, int k, double *v )
{
    const char *cl[4] = { "phi_", "theta_", "omega_d", "mu[" };
    char        pre[32], pre2[32];
    int         c, i, n = 0;

    for ( c = 0; c < 4; c++ )
        {
        if ( c == 3 ) snprintf( pre, sizeof pre, "mu[%d]", k );
        else          snprintf( pre, sizeof pre, "%s%d[", cl[c], k );
        snprintf( pre2, sizeof pre2, "delta_d%d[", k );
        for ( i = 0; i < p->n; i++ )
            if ( !strncmp( p->p[i].nombre, pre, strlen( pre ) ) ||
                 ( c == 2 && !strncmp( p->p[i].nombre, pre2, strlen( pre2 ) ) ) )
                v[n++] = p->p[i].valor;
        }
    return n;
}

static OdParams par;

int main( int argc, char **argv )
{
    int k;

    if ( argc < 4 ) { fprintf( stderr, "uso: test_vuelta DIR .out .pre...\n" ); return 2; }
    if ( od_params_file( argv[2], &par ) ) { printf( "no leo %s\n", argv[2] ); return 1; }
    printf( "la vuelta de drtran al univariante (%d series)\n", argc - 3 );

    for ( k = 1; k + 2 < argc; k++ )
        {
        const char *pre = argv[k + 2];
        char   dst[1024], que[256], why[512];
        double vp[8000], vw[8000], vo[8000];
        int    fp[8000], fw[8000], np, nw, no, nfp, nfw, i, mov, igual = 1;

        snprintf( dst, sizeof dst, "%s/vuelta_%d.inp", argv[1], k );
        remove( dst );
        snprintf( que, sizeof que, "serie %d (%s)", k, strrchr( pre, '/' ) ? strrchr( pre, '/' ) + 1 : pre );
        ok( vu_escribe( pre, &par, k, dst, &mov, why, sizeof why ) == 0, que, why[0] ? why : NULL );

        np = libres( pre, vp, fp, &nfp );
        nw = libres( dst, vw, fw, &nfw );
        no = del_out( &par, k, vo );
        ok( nw == np && nfw == nfp, "  los mismos coeficientes que el .pre", NULL );
        for ( i = 0; i < nfp && i < nfw; i++ ) if ( fp[i] != fw[i] ) igual = 0;
        ok( igual, "  y las mismas banderas", NULL );

        /* LO ESCRITO ES LO DEL .out: lo que el .out trae, en su orden. Lo
         * que no trae (la media fija del .pre, que no es slot) no cuenta. */
        igual = ( no <= nw );
        for ( i = 0; i < no && i < nw; i++ ) if ( vw[i] != vo[i] ) igual = 0;
        ok( igual, "  se relee lo que dice el .out, en su orden", NULL );

        }

    /* CON EL .pre DE OTRA SERIE no casa, y no se escribe nada. */
    if ( argc > 4 )
        {
        char dst[1024], why[512];
        FILE *f;

        snprintf( dst, sizeof dst, "%s/vuelta_mal.inp", argv[1] );
        remove( dst );
        ok( vu_escribe( argv[4], &par, 1, dst, NULL, why, sizeof why ) != 0,
            "el .pre de otra serie no casa", why );
        f = fopen( dst, "rb" );
        ok( f == NULL, "y no deja fichero", NULL );
        if ( f ) fclose( f );
        }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
