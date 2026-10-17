/*
 * vuelta.h -- lo que drtran devuelve al univariante (DISENO-casos §3).
 *
 * drtran parte de un .pre por serie y reestima TODO junto: la transferencia y,
 * a su lado, el bloque univariante de cada serie --su ARMA, sus deterministas
 * y su media--, que se MUEVE al estimarlo con la transferencia. Esto escribe
 * ese bloque de vuelta, con el formato de fue, para que el analista lo lleve
 * a una especificacion modificada o a fue y fuf.
 *
 * ES UN .inp, NO UN .pre, y la extension es la afirmacion. Un .pre dice «este
 * es el optimo de esta especificacion»: corre fue sobre el y los numeros no se
 * mueven. Estos bloques son optimos CON la transferencia, no solos; en la
 * diagonal evaluan PEOR que el .pre (mtram lo midio: -772.84 contra -767.42 en
 * el caso canonico), y tiene que ser asi. Un buen punto de partida y un .pre
 * falso: el fichero no lleva marca de quien lo escribio, asi que el nombre es
 * lo unico que lo dice.
 *
 * LA TRANSFERENCIA NO VA: es un fichero univariante. La red esta en el .dag.
 *
 * POR ORDEN, NO POR NOMBRE. El .out nombra phi_1[B^1] a los dos primeros
 * coeficientes de dos factores AR regulares: el nombre no es unico. Lo que
 * casa es el ORDEN de lib/slots --por serie, los AR (regulares, anuales, de
 * frecuencia fija), los MA igual, los deterministas por variable y la media--
 * con las banderas del .pre, que dicen cuales son libres. Si las cuentas no
 * cuadran, no se escribe nada.
 */

#ifndef ATSW_VUELTA_H
#define ATSW_VUELTA_H

#include <stddef.h>
#include "outdiag.h"
#include "proyecto.h"

/* Escribe en destino el .pre (o .inp) de la serie k (1..n, el orden de la
 * linea de ordenes de drtran) con su bloque univariante reestimado, leido de
 * par (la tabla del .out). Las banderas, tal cual. Lo que el .out no trae
 * --un bloque que la orden clavo (-N, -D, -M)-- se queda como estaba.
 * *movidos, si no es NULL, dice cuantos coeficientes cambiaron de valor.
 * 0, o 1 con el motivo; destino no se toca si falla.                     */
int vu_escribe( const char *pre, const OdParams *par, int k,
                const char *destino, int *movidos, char *porque, size_t n );

/* 1 si el modelo del .out lleva alguna transferencia. Sin ella --el
 * diagonal-- los bloques son los de fue y no hay nada que devolver.     */
int vu_con_transferencia( const OdParams *par );

/* LA VUELTA DE UN MODELO DE UN CASO, para drtran_gui y para la madre: por
 * cada entrada del caso, un MODELO DERIVADO de la serie, hijo del que entro,
 * con su .inp y la razon de donde viene. Cambia el proyecto EN MEMORIA:
 * guardarlo es de quien llama. hechos recibe «SERIE/id · SERIE/id». 0 si
 * todas; 1 con el motivo (las que salieron, salieron, y lo dice hechos). */
int vu_caso( Proyecto *p, const char *caso, const char *corrida,
             char *hechos, size_t nh, char *porque, size_t n );

#endif /* ATSW_VUELTA_H */
