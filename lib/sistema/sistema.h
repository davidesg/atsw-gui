/*
 * sistema.h -- un sistema de SIM: la lista ORDENADA de ficheros de fue.
 *
 * docs/DISENO-sim.md §0.1: un sistema (VARMA o VEC) es una lista ordenada de
 * ficheros de fue, uno por serie, cada uno con su modelo univariante entero.
 * El orden importa: es el de Cholesky de la IRF y la FEVD.
 *
 * Esto es lo que comparten, SIN VENTANA, sim_gui, atsw-cli y el asistente
 * (DISENO-sim.md §2): leer cada fichero con el lector de drvarma (lib/fuepre,
 * el mismo que usa el motor: lo que acepta este modulo es lo que acepta
 * drvarma, por construccion), la ventana comun y la orden de la puerta.
 * No sabe nada de GTK.
 *
 * LA VENTANA COMUN la calcula drvarma igual (escalera.c, fuepre_check_
 * alignment): misma frecuencia y misma fecha FINAL (BUG-2), y empieza en el
 * inicio mas tardio. Aqui se calcula ANTES de lanzar, para decir en la
 * pagina 1 que se va a cruzar.
 *
 * ENLACE: sistema.c + lib/fuepre/fue_pre_reader.c + engines/drvarma/src/
 * nlatools.c + lib/dates + lib/fuedet, con GSL; y el programa define
 * «real macheps», como drtran_gui (series.c): el lector lo usa y el que lo
 * define en el motor es su main.
 */

#ifndef ATSW_SISTEMA_H
#define ATSW_SISTEMA_H

#include <stddef.h>

#define SIS_MAX_SER  16
#define SIS_RUTA   1024
#define SIS_NOMBRE   64

typedef struct {
   char   ruta[SIS_RUTA];      /* el fichero, como se dio                  */
   char   nombre[SIS_NOMBRE];  /* el de la serie, del propio fichero       */
   int    es_pre;              /* .pre (un optimo) o .inp (especificacion) */
   int    freq, nobs;
   int    ini_per, ini_anio;   /* la primera observacion                   */
   int    fin_per, fin_anio;   /* la ultima                                */
   double lambda;              /* Box-Cox                                  */
   int    d, D;                /* diferencias regulares y anuales          */
   char   ar[64], ma[64];      /* los ordenes, legibles: "6", "1·12", "-"  */
   int    ndet;                /* variables deterministas                  */
   int    media;               /* lleva media                              */
} SisSerie;

typedef struct {
   SisSerie s[SIS_MAX_SER];
   int      n;
} Sistema;

/* La ventana comun, EN OBSERVACIONES ESTACIONARIAS, como la escribe drvarma
 * («Common window : 1/1851 - 1/1911 (61 stationary observations)»): cada
 * serie empieza tras sus d + s*D primeras, y la ventana en la mas tardia.
 * desde/hasta en fecha absoluta (anio * freq + per - 1); recorta[i] =
 * cuantas observaciones estacionarias del principio pierde la serie i.    */
typedef struct {
   int  freq;
   long desde, hasta;
   int  nobs;
   int  recorta[SIS_MAX_SER];
} SisVentana;

/* Anade un fichero al final. 0 si se pudo; si no, el motivo en why.
 * Se niega a pasar de SIS_MAX_SER y a repetir una serie (mismo nombre). */
int  sis_anade( Sistema *s, const char *ruta, char *why, size_t n );
void sis_quita( Sistema *s, int i );
/* Mueve la serie i a la posicion j (el orden es el de Cholesky). */
void sis_mueve( Sistema *s, int i, int j );

/* La ventana comun. 0 si la hay; si no, el motivo en why (frecuencias
 * distintas, finales distintos: drvarma sale con 4).                   */
int  sis_ventana( const Sistema *s, SisVentana *v, char *why, size_t n );

/* Una fecha absoluta como la escribe drvarma: «per/anio». */
void sis_fecha( int freq, long abs, char *out, size_t n );

/* Cuantas series llevan diferencias (d + D > 0): con dos o mas, la pregunta
 * de la cointegracion es del rango (N1c; DISENO-sim.md §3.2).            */
int  sis_diferenciadas( const Sistema *s );

/* La orden de la puerta: «A.pre B.pre ... 0 0 -diagcov -o SALIDA».
 * argv recibe punteros a s y a salida (no se copian); devuelve cuantos,
 * o -1 si no caben en max. Sin el nombre del programa ni el NULL final. */
int  sis_orden_puerta( const Sistema *s, const char *salida,
                       const char **argv, int max );

#endif /* ATSW_SISTEMA_H */
