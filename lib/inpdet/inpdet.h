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

/* TODOS los deterministas, EN SU ORDEN, tal como el motor los lee:
 * det[0] es la variable 1, det[1] la 2, y asi.
 *
 * Existe porque el .out NO los nombra: dice «Omegas for deterministic
 * variable 12» y nada mas. Para poder decir «step 3 2022» en vez de «la
 * intervencion 12» hay que casar por POSICION con el .inp -- que es una
 * busqueda, no un parseo de nombre: el orden es parte del formato.
 *
 * Devuelve cuantos, o -1 con el motivo en porque[n].                   */
int id_nombres( const char *origen, char det[][ID_LINEA], int max,
                char *porque, size_t n );

/* Copia origen en destino añadiendo los deterministas de nuevo[0..nn-1] al
 * final del bloque, con semilla 0.0, estimados y sin denominador.
 *
 * nomega[k] es LA CUENTA DEL FICHERO, no el número de coeficientes: el
 * bloque lleva nomega[k]+1 valores. Así lo lee el motor, y así hay que
 * decirlo para que no se confunda con el otro:
 *
 *     nomega = 0   UN coeficiente   la lectura escalar, el peldaño 1
 *     nomega = L   L+1 coeficientes la forma general de un episodio de L
 *                                   períodos, el peldaño 2
 *
 * El peldaño 2 es UNA intervención con L+1 omegas, NO L+1 intervenciones:
 * es la familia anidada ω(B) aplicada a un escalón de nivel, y con ganancia
 * ω(1)=0 equivale a L impulsos.
 *
 * nomega puede ser NULL: entonces todos a 0.
 *
 * Con nn == 0 copia el fichero tal cual, y eso es la prueba del módulo.
 *
 * Devuelve 0 si pudo; si no, 1 y el motivo en porque[n], y destino no se
 * toca.                                                                   */
int id_anade( const char *origen, const char *destino,
              const char *const *nuevo, const int *nomega, int nn,
              char *porque, size_t n );

#endif /* ATSW_INPDET_H */
