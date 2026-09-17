/*
 * verdict.h -- como acabo el optimizador, leido de lo que el motor escribe.
 *
 * TASTE distinguia tres desenlaces y les ponia nombre (MRQEST.PAS:372-376):
 * CONVERGENCIA, ALCANZADO EL Nº MAXIMO DE ITERACIONES, LONGITUD DE PASO MUY
 * PEQUEÑA. Era de lo mejor de su diseño: distingue converger de rendirse, y
 * distingue dos formas de rendirse.
 *
 * drtran tiene SEIS, y ademas el ifault del evaluador de la verosimilitud:
 *
 *     1  convergencia por el gradiente
 *     2  convergencia por el parametro
 *     3  parado en un punto sin mejora  -- lo normal si se arranca EN el optimo
 *     4  NO converge: limite de iteraciones
 *     5  NO converge: cinco pasos seguidos de longitud maxima
 *
 * Lo escribe asi, en el .out y en la pantalla:
 *
 *     **** CONVERGENCE OBTAINED AFTER 349 ITERATIONS (of 500)
 *     **** parameter stopping criterium satisfied
 *     **** ifault = 3 (estimates not reliable)
 *
 * Aqui NO se reimplementa ningun criterio de parada: se LEE lo que el motor
 * dice. El motor es quien sabe como acabo; el GUI solo tiene que no perderlo.
 *
 * Por que un modulo y no tres greps en el GUI: porque el caso 3 se lee mal si
 * no se sabe lo que es. "STOPPED AT A POINT WITH NO IMPROVEMENT" suena a
 * fracaso y es lo que sale cuando el .pre YA ERA el optimo -- que es
 * exactamente la invariante del contrato. Confundir eso con un fallo seria
 * confundir el exito con el fracaso.
 */

#ifndef ATSW_VERDICT_H
#define ATSW_VERDICT_H

#include <stddef.h>

typedef enum {
   VER_NADA = 0,       /* no se encontro veredicto: no llego a estimar      */
   VER_GRADIENTE,      /* 1: convergencia por el gradiente                  */
   VER_PARAMETRO,      /* 2: convergencia por el parametro                  */
   VER_SIN_MEJORA,     /* 3: parado sin mejora -- normal si se partia del optimo */
   VER_ITERACIONES,    /* 4: no converge, limite de iteraciones             */
   VER_PASOS,          /* 5: no converge, cinco pasos de longitud maxima    */
   VER_OTRO            /* algo que no estaba en la lista                    */
} Veredicto;

typedef struct {
   Veredicto ver;
   int       iters;        /* cuantas hizo, -1 si no lo dijo        */
   int       maxits;       /* de cuantas, -1 si no lo dijo          */
   int       ifault;       /* 0 si no hubo                          */
   double    logl;         /* la verosimilitud, si la escribio      */
   int       tiene_logl;
   char      frase[160];   /* la linea del motor, tal cual          */
} VerdictInfo;

/* Lee el texto que el motor escribio --su salida por pantalla o su .out-- y
 * saca el veredicto. Devuelve v->ver por comodidad.                      */
Veredicto verdict_parse( const char *texto, VerdictInfo *v );

/* TRUE si el optimizador llego a un optimo. El caso 3 CUENTA: pararse sin
 * mejora arrancando de un .pre es que ya se estaba en el optimo.         */
int verdict_ok( const VerdictInfo *v );

#endif /* ATSW_VERDICT_H */
