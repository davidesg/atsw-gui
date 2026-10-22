/*
 * parte.c -- EL MODELO FACTORIZADO: cada factor AR regular partido en sus
 * factores irreducibles, como un modelo nuevo que fue pueda estimar.
 *
 * La ventana de la factorizacion ENSEÑA los factores; esto los ESCRIBE.
 * Un AR(p) libre pasa a ser el producto de sus factores reales (1 - a B) y
 * de sus pares complejos (1 - a1 B - a2 B^2), cada uno con su semilla y
 * libre. Es lo que hay que estimar para ver cada amortiguamiento con su ±
 * (en un AR(p) los pares no tienen covarianza propia) y lo que se contrasta
 * despues: un par en una frecuencia estacional se fija, uno que sobra se
 * quita. Sin el, el analista tenia que copiar los a1, a2 a mano al .inp.
 *
 * QUE SE CONSERVA. Partido entero con los coeficientes LIBRES, el operador
 * es el mismo: el producto de los factores es el polinomio de partida, y
 * fue arrancando de ahi tiene que volver a la misma verosimilitud (con la
 * del .pre, a 4 decimales, casi). Con coeficientes FIJOS no: un AR(12) con
 * el c2 fijo a cero no es un producto de factores libres, y partirlo es
 * soltar la restriccion. Se hace igual y se dice (*con_fijos).
 *
 * Lo demas del fichero va byte a byte (id_escribe_arma).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raices.h"
#include "inpdet.h"

/* seis decimales: es una semilla, y el .inp se lee a ojo */
static double semilla( double v )
{
    return round( v * 1e6 ) / 1e6;
}

int rz_deriva_factorizado( const char *origen, const char *destino, int s,
                           int *partidos, int *con_fijos, char *why, size_t nwhy )
{
    IdArma a;
    int    nf = 0, ord[ID_MAX_FAC], fl[ID_MAX_FAC][ID_MAX_ORD];
    double v[ID_MAX_FAC][ID_MAX_ORD];
    int    i, j, k, np = 0, fijos = 0;

    if ( partidos ) *partidos = 0;
    if ( con_fijos ) *con_fijos = 0;
    if ( id_arma_lee( origen, &a, why, nwhy ) != 0 ) return 1;
    if ( a.nf[0] == 0 )
        { snprintf( why, nwhy, "El modelo no tiene AR regular: no hay nada que partir." ); return 1; }

    for ( i = 0; i < a.nf[0]; i++ )
        {
        RzAr r;
        int  pasa, n0 = nf;

        /* un factor de orden 1 ya es irreducible */
        if ( a.ord[0][i] > 1 && a.ord[0][i] <= RZ_MAX_ORDEN
             && rz_factoriza( a.v[0][i], a.ord[0][i], s, NULL, &r ) == 0 && r.nf > 1 )
            {
            /* primero los reales y despues los pares, como en la ventana */
            for ( pasa = 0; pasa < 2; pasa++ )
                for ( k = 0; k < r.nf; k++ )
                    {
                    const RzFactor *f = &r.f[k];

                    if ( f->complejo != pasa ) continue;
                    if ( nf >= ID_MAX_FAC )
                        { snprintf( why, nwhy, "Partido salen más de %d factores AR: "
                                    "fue no admite tantos.", ID_MAX_FAC ); return 1; }
                    ord[nf] = f->complejo ? 2 : 1;
                    v[nf][0] = semilla( f->a1 );
                    v[nf][1] = f->complejo ? semilla( f->a2 ) : 0.0;
                    fl[nf][0] = fl[nf][1] = 1;
                    nf++;
                    }
            /* los ceros del final no restringen: bajan el orden */
            for ( j = r.orden - 1; j >= 0 && a.v[0][i][j] == 0.0; j-- ) ;
            for ( ; j >= 0; j-- ) if ( a.fl[0][i][j] == 0 ) fijos = 1;
            np++;
            }
        if ( nf > n0 ) continue;

        /* se queda como estaba: irreducible, o sin raices (todo cero) */
        if ( nf >= ID_MAX_FAC )
            { snprintf( why, nwhy, "Partido salen más de %d factores AR: "
                        "fue no admite tantos.", ID_MAX_FAC ); return 1; }
        ord[nf] = a.ord[0][i];
        for ( j = 0; j < ord[nf]; j++ ) { v[nf][j] = a.v[0][i][j]; fl[nf][j] = a.fl[0][i][j]; }
        nf++;
        }
    if ( np == 0 )
        { snprintf( why, nwhy, "Cada factor AR ya es irreducible: el modelo ya está "
                    "factorizado del todo." ); return 1; }

    a.nf[0] = nf;
    for ( i = 0; i < nf; i++ )
        {
        a.ord[0][i] = ord[i];
        for ( j = 0; j < ord[i]; j++ ) { a.v[0][i][j] = v[i][j]; a.fl[0][i][j] = fl[i][j]; }
        }
    if ( id_escribe_arma( origen, destino, &a, why, nwhy ) != 0 ) return 1;
    if ( partidos ) *partidos = np;
    if ( con_fijos ) *con_fijos = fijos;
    return 0;
}
