/*
 * series.h -- las series cargadas, y sus papeles.
 *
 * El .pre se lee con EL LECTOR DEL MOTOR (lib/fuepre/fue_pre_reader.c),
 * enlazado tal cual. No hay un lector del GUI: el estudio del contrato conto
 * seis implementaciones del formato y cada divergencia que encontro salia de
 * que el mismo fichero viviera en dos sitios. Aqui no se abre esa puerta.
 *
 * Lo que el GUI anade encima es lo que el motor no hace y hace falta ANTES de
 * lanzarlo: el orden (cual es la salida), la ventana comun entre calendarios,
 * y si los operadores no estacionarios son compatibles.
 */

#ifndef DRTRAN_GUI_SERIES_H
#define DRTRAN_GUI_SERIES_H

#include <gtk/gtk.h>

#include "main.h"                 /* struct Tseries, struct Tusmodel, real  */

#define GUI_MAX_SER  8            /* MAX_SER del motor: 1 salida + 7 entradas */

/* Una serie cargada: lo que el .pre dice de ella, mas su ruta. */
typedef struct {
    gchar            *path;       /* ruta del .pre, tal como se abrio        */
    struct Tseries    ts;         /* lo que leyo el lector del motor         */
    struct Tusmodel   tm;         /* su modelo univariante                   */
    real            **datamat;    /* columnas de deterministas no estandar   */
    gchar            *operador;   /* el polinomio no estacionario, legible   */
} Serie;

/* El conjunto. El ORDEN es significativo: la primera es la salida (Y) y el
 * resto las entradas, y ese orden es el indice al que se refieren q[i,j],
 * phi_i, theta_i y mu[i] en el .cns. */
typedef struct {
    Serie *s[GUI_MAX_SER];
    int    n;
} Conjunto;

/* Carga un .pre. Devuelve NULL y pone el motivo en why[size]. */
Serie *serie_cargar(const char *path, char *why, size_t size);
void   serie_libre(Serie *s);

/* La fecha de la observacion i (1..nobs), como "03/1976". Nueva; liberar. */
gchar *serie_fecha(const Serie *s, int obs);

/* El polinomio no estacionario en forma legible: "(1-B)(1-B^12)". Nueva. */
gchar *serie_operador(const Serie *s);

/* --- la ventana comun ---------------------------------------------------- */

/* La interseccion de los calendarios, en observaciones de CADA serie.
 * Devuelve TRUE si hay tramo comun; si no, el motivo en why[size].
 * desde[i]/hasta[i] son indices 1..nobs de la serie i.                     */
gboolean conjunto_ventana_comun(const Conjunto *c, int *desde, int *hasta,
                                char *why, size_t size);

/* --- la compatibilidad de operadores ------------------------------------- */

typedef enum {
    OP_IGUALES,      /* mismo polinomio: cast empotrado, verosimilitud exacta */
    OP_ANIDADOS,     /* uno divide al otro: hay Delta(B), sigue siendo exacto  */
    OP_INCOMPATIBLES /* ni lo uno ni lo otro: el motor pasa al cast por resta  */
} Compat;

Compat conjunto_compat(const Conjunto *c, int i, int j);
const char *compat_texto(Compat k);

#endif /* DRTRAN_GUI_SERIES_H */
