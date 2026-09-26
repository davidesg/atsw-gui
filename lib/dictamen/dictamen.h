/*
 * dictamen.h -- la diagnosis de un modelo univariante, en veredictos.
 *
 * LEER NO ES JUZGAR, y por eso esto no está en lib/outfile.
 *
 * lib/outfile lee el .out y devuelve HECHOS: la media y su error típico, la
 * escalera del Ljung-Box, el Jarque-Bera, el histograma, la tabla de
 * parámetros, los pares correlacionados. Aquí se decide qué dicen.
 *
 * Los umbrales --alfa, |t| > 2, |r| >= 0.7-- son MÉTODO, no formato. Pegados
 * al parser están en el sitio donde nadie los busca y no se pueden probar sin
 * un fichero delante; aquí tienen nombre, se prueban con números inventados,
 * y el día que el analista quiera mover uno hay UN sitio donde moverlo.
 *
 * Y al revés: si el motor cambia una línea de su informe, se arregla el
 * lector y los veredictos ni se enteran.
 *
 * LO QUE ESTE MÓDULO NO HACE: decir qué hacer. La escalera de alternativas
 * --qué quitar, qué intervenir, qué reformular-- es una DECISIÓN, y las
 * decisiones son del analista. Esto llega hasta «los residuos no son blancos,
 * y el peor es Q(12)» y se para ahí. Es la misma línea que separa el
 * manifiesto, que guarda decisiones, de los .out, que guardan resultados.
 */

#ifndef ATSW_DICTAMEN_H
#define ATSW_DICTAMEN_H

#include <stddef.h>

#include "outfile.h"

/* CUATRO ESTADOS Y NO DOS, y el cuarto es el que importa.
 *
 * DX_NO_CONSTA no es un aprobado: es que el .out no trae ese bloque. Es la
 * regla de la huella vacía del guion -- «no consta» nunca significa «cuadra».
 *
 * Y DX_MIRAR existe porque un p de .04 y uno de .000 no son la misma noticia
 * y no pueden salir iguales.                                            */
typedef enum {
   DX_NO_CONSTA = 0,
   DX_CUADRA,
   DX_MIRAR,
   DX_NO,

   /* «NO APLICA» NO ES «NO CONSTA», y meterlos en el mismo cajón seria
    * justo el error que la regla de arriba previene al reves.
    *
    * NO CONSTA es informacion que FALTA: el .out no trajo ese bloque, no se
    * sabe, y por eso no puede contar como aprobado.
    *
    * NO APLICA es un HECHO del modelo: no hay ninguna intervencion de varios
    * omegas, asi que la pregunta sobre su ganancia no tiene sujeto. Nada
    * falta. Un bloque asi NO arrastra el resumen -- si lo hiciera, ningun
    * modelo sin intervenciones podria cuadrar nunca.                    */
   DX_NO_APLICA
} DxEstado;

#define DX_TITULO  32
#define DX_DATO   128
#define DX_DICE   192
#define DX_MAX_LINEA 8

/* Una línea del dictamen. EL DATO ES DEL MOTOR, LA FRASE ES DEL MÓDULO, y el
   front end pone el idioma y el color: aquí no se sabe de ventanas.    */
typedef struct {
   DxEstado estado;
   char     titulo[DX_TITULO];
   char     dato[DX_DATO];
   char     dice[DX_DICE];
} DxLinea;

typedef struct {
   DxLinea  l[DX_MAX_LINEA];
   int      n;
   DxEstado peor;          /* el peor de todos: el resumen de una ojeada */
} Dictamen;

/* Los umbrales, con nombre y en un sitio. */
#define DX_ALFA      0.05    /* rechaza                                   */
#define DX_ALFA_OJO  0.10    /* está en el filo                           */
#define DX_T_FLOJO   2.0     /* |t| por debajo: el parámetro no se gana su sitio */
#define DX_R_ALTO    0.7     /* dos parámetros que se pisan               */

/* El dictamen de un modelo. conv puede ser NULL: entonces la línea de la
   estimación sale como «no consta», que es la verdad.                  */
void dx_dictamen( const FueOut *o, const Convergence *conv, Dictamen *d );

/* El nombre de un estado, para quien tenga que pintarlo. */
const char *dx_estado_es( DxEstado e );

#endif /* ATSW_DICTAMEN_H */
