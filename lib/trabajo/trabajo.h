/*
 * trabajo.h -- un TRABAJO APARTE: lo que corre suelto, mas que una llamada,
 * y que la madre tiene que poder ENSEÑAR mientras corre.
 *
 * El primero es el bootstrap del rango (drvec -bootstrap B): de 10 a 25
 * minutos. Lo lanza atsw-cli (caso-rango --aparte, para el asistente) o
 * sim_gui (su pagina del rango), y la madre lo enseña en su barra de estado,
 * con su avance, sin saber nada de drvec (lo pidio el analista, 15-nov-2026:
 * «asi el analista sabe lo que esta pasando»).
 *
 * EL CONTRATO es un fichero de texto, <algo>.trabajo, junto a lo que el
 * trabajo escribe, con una clave y su valor por linea:
 *
 *     que        bootstrap del rango          lo que se enseña
 *     objeto     C1/rango                     de que es (caso o modelo)
 *     pid        12345                        el proceso que lo lleva
 *     inicio     1763224321                   cuando empezo (segundos)
 *     por        asistente                    quien lo lanzo
 *     total      57                           los pasos que tiene
 *     progreso   C1_rango.boot                el fichero que lo cuenta
 *     resultado  C1_rango.out                 lo que deja al terminar
 *
 * progreso y resultado van RELATIVOS a la carpeta del .trabajo. Lo hecho es
 * el numero de lineas del fichero de progreso que EMPIEZAN POR UN DIGITO (una
 * por paso; las cabeceras y los comentarios no): asi lo escribe drvec en su
 * .boot, y asi lo cuenta lib/outvec.
 *
 * Quien lanza el trabajo escribe el .trabajo; quien recoge el resultado lo
 * borra. Un .trabajo cuyo proceso ya no vive es un trabajo que TERMINO (con
 * su resultado escrito despues de empezar) o que SE CAYO (sin el).
 *
 * Sin GTK: lo usan atsw-cli, sim_gui y la madre.
 */
#ifndef ATSW_TRABAJO_H
#define ATSW_TRABAJO_H

#include <stddef.h>

#define TR_TEXTO 160
#define TR_RUTA  1024

typedef struct {
   char  que[TR_TEXTO];
   char  objeto[TR_TEXTO];
   long  pid;
   long  inicio;
   char  por[32];
   int   total;
   char  progreso[TR_RUTA];     /* relativo a la carpeta del .trabajo */
   char  resultado[TR_RUTA];
} Trabajo;

typedef enum {
   TR_EN_MARCHA = 0,            /* su proceso vive                         */
   TR_TERMINADO,                /* no vive, y dejo su resultado            */
   TR_CAIDO                     /* no vive, y no lo dejo                   */
} TrEstado;

/* Escribe el .trabajo (de una vez: se escribe aparte y se renombra). 0 bien. */
int  trabajo_escribe( const char *ruta, const Trabajo *t );

/* Lo lee. 0 si es un .trabajo; 1 si no se puede o le falta algo. */
int  trabajo_lee( const char *ruta, Trabajo *t );

/* Cuantos pasos van (las lineas del progreso que empiezan por un digito), o
 * -1 si el fichero aun no esta. ruta es la del .trabajo.                  */
int  trabajo_hechos( const char *ruta, const Trabajo *t );

/* En que esta: en marcha, terminado o caido. */
TrEstado trabajo_estado( const char *ruta, const Trabajo *t );

/* Lo que se enseña en una linea: «C1/rango · bootstrap del rango · 24/57 ·
 * quedan ~27 s · asistente», «... terminado» o «... se cayo». Devuelve out.
 * fraccion, si no es NULL, el avance de 0 a 1 (-1 si no se sabe).        */
char *trabajo_linea( const char *ruta, const Trabajo *t, long ahora,
                     char *out, size_t n, double *fraccion );

/* Los .trabajo del proyecto: en la carpeta work de cada caso (_casos) y de
 * cada serie.
 * Deja en rutas[0..max-1] (memoria de quien llama) y devuelve cuantos.    */
int  trabajo_lista( const char *raiz, char rutas[][TR_RUTA], int max );

#endif
