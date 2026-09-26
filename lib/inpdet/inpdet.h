/*
 * inpdet.h -- meter una intervención en el .inp de fue.
 *
 * POR QUE UN MODULO Y NO UNAS LINEAS EN LA VENTANA
 *
 * El .inp es POSICIONAL aunque no lo parezca. Las líneas «**» son etiquetas
 * para humanos, pero el número de ellas no: fue reabre el fichero y se
 * posiciona contando fgets a pelo, así que una línea de más desincroniza los
 * dos lectores y el .pre sale con nombres de determinista basura, SIN ERROR.
 * Y fue hace 115 fscanf sin comprobar el retorno ni una vez: un campo que
 * falta no da error, deja la variable con su valor anterior.
 *
 * Añadir un determinista no es añadir una línea. Es:
 *
 *     subir el número de deterministas
 *     añadir su línea al bloque de nombres
 *     añadir su cuenta de omegas a la fila de omegas
 *     añadir su bloque de omegas (semilla 0.0, estimado)
 *     añadir su cuenta de deltas a la fila de deltas
 *
 * Cinco sitios. Hacerlo a mano en un diálogo es la forma de que un día salga
 * un .pre con basura y nadie sepa por qué.
 *
 * LA REGLA DE ESTE MODULO
 *
 * Lo de antes y lo de después del bloque de deterministas sale BYTE A BYTE
 * IGUAL. Sólo el bloque se reescribe, y se reescribe en la forma que ya usan
 * todos los ficheros del corpus -- lo cual se comprueba: añadir CERO
 * variables a cualquier .inp tiene que devolver el fichero idéntico. Si
 * algún fichero no lo cumple, la batería lo dice antes que el motor.
 *
 * Y NO SE FIA DE SI MISMO: quien llame debe pasar el resultado por
 * inp_check_fue --la puerta del propio motor-- antes de darlo por bueno.
 * Escribir un .inp que el motor no acepta es peor que no escribirlo.
 */

#ifndef ATSW_INPDET_H
#define ATSW_INPDET_H

#include <stddef.h>

#define ID_MAX_DET   64        /* el MAX_COUNT de inpcheck es mayor; esto es
                                  lo que cabe en una ventana sin mentir     */
#define ID_LINEA     128

/* Las intervenciones (impulse, compimp, step, ramp) que el fichero YA trae,
 * en det[0..max-1] como el motor las lee ("step 10 2008"). Devuelve cuántas,
 * o -1 con el motivo en porque[n].
 *
 * Existe para no poner DOS intervenciones sobre el mismo suceso: eso no da
 * error, da un omega no significativo por síntoma, que se lee como «no hacía
 * falta» cuando lo que pasa es que está contada dos veces.                */
int id_intervenciones( const char *origen, char det[][ID_LINEA], int max,
                       char *porque, size_t n );

/* Copia origen en destino añadiendo los deterministas de nuevo[0..nn-1] al
 * final del bloque. Cada uno entra con UN omega, semilla 0.0 y estimado, y
 * sin denominador -- que es el peldaño 1 de la escalera.
 *
 * Con nn == 0 copia el fichero tal cual, y eso es la prueba del módulo.
 *
 * Devuelve 0 si pudo; si no, 1 y el motivo en porque[n], y destino no se
 * toca.                                                                   */
int id_anade( const char *origen, const char *destino,
              const char *const *nuevo, int nn, char *porque, size_t n );

#endif /* ATSW_INPDET_H */
