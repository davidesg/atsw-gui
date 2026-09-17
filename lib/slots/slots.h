/*
 * slots.h -- la tabla de SLOTS y el .cns.
 *
 * UN SLOT ES UN PARAMETRO ESTRUCTURAL DEL SISTEMA, con nombre. La tabla la
 * genera el modelo: depende de la red (.dag), de los ordenes de cada enlace, y
 * de que coeficientes trae LIBRES cada .pre. No se inventa.
 *
 *     omega1[0]  omega1[1]  delta1[1]    por enlace: s+1 omegas y r deltas
 *     phi_2[B^1]  theta_2[B^12]          el ARMA de cada serie, EXPANDIDO
 *     phi_5[f=4]                         un factor de frecuencia fija
 *     omega_d3[1,0]                      los deterministas
 *     mu[3]                              la media
 *     log(var2/var1)                     las varianzas relativas
 *     q[3,2]                             las covarianzas de las innovaciones
 *
 * Y AQUI ESTA EL PUNTO. La tabla solo lleva los coeficientes que el .pre marca
 * como LIBRES: "0.0000 0" en un .pre es un coeficiente FIJO, o sea
 * ESPECIFICACION, y no es un parametro que se estime. Es el contrato, visto
 * desde el otro lado.
 *
 * Las covarianzas q[i,j] entran SIEMPRE, pero FIJAS EN CERO: la diagonal es el
 * caso por defecto y liberar una covarianza es una decision del analista. El
 * m6-1 no libera las 15 de su sistema: libera TRES.
 *
 * EL .cns dice, en un solo lenguaje, todo lo que se puede decir de un slot:
 *
 *     NOMBRE = free            liberar   (las q[i,j] nacen fijas en cero)
 *     NOMBRE = 0.5             fijar
 *     NOMBRE = OTRO            compartir: un grado de libertad en dos sitios
 *     NOMBRE = [-]a * b        producto  (numerador factorizado con MA compartida)
 *     NOMBRE = a + b - c       combinacion lineal (un factor FIJO (1-B) impone
 *                              nu(1)=0, o sea w0 = w1 + w2 + ...)
 *
 * Nada de esto es nuevo: build_slots, find_slot y read_constraints estaban
 * dentro de drtran.c sobre sus globales. El motor llama a estas mismas, asi que
 * el GUI no puede ofrecer un slot que el motor no tenga ni aceptar un .cns que
 * el motor rechace.
 */

#ifndef ATSW_SLOTS_H
#define ATSW_SLOTS_H

#include <stddef.h>

#include "main.h"                 /* struct Tusmodel, real */
#include "netfile.h"              /* NetLink               */

#define SLOT_MAX       400
#define SLOT_LC_TERMS    6        /* terminos por combinacion lineal */
#define SLOT_NAME       40

/* Que es un slot */
#define SLOT_FREE     0
#define SLOT_FIXED    1
#define SLOT_ALIAS    2
#define SLOT_PRODUCT  3
#define SLOT_LINCOMB  4

/* Estructura de arrays, y no array de estructuras, a proposito: es la forma
 * que ya tenia el motor, y asi sus 66 puntos de uso no cambian una letra. */
typedef struct {
   char name [SLOT_MAX + 1][SLOT_NAME];
   int  kind [SLOT_MAX + 1];
   int  alias[SLOT_MAX + 1];
   real value[SLOT_MAX + 1];            /* el valor si FIJO; el signo si PRODUCTO */
   int  pa   [SLOT_MAX + 1];
   int  pb   [SLOT_MAX + 1];
   int  nlc  [SLOT_MAX + 1];
   real lc_sign[SLOT_MAX + 1][SLOT_LC_TERMS];
   int  lc_a   [SLOT_MAX + 1][SLOT_LC_TERMS];
   int  lc_b   [SLOT_MAX + 1][SLOT_LC_TERMS];
   int  n;
} SlotTable;

/* Lo que la LINEA DE ORDENES clava, ademas de lo que ya diga el .pre: -X/-N
 * el ARMA, -D/-E los deterministas, -M la media. NULL = la linea de ordenes no
 * clava nada. Indexados 1..nser.
 *
 * Ojo con la direccion: esto solo puede clavar DE MAS. Las banderas del .pre
 * se respetan siempre --un coeficiente FIJO es especificacion-- y nadie las
 * libera desde fuera.                                                    */
typedef struct {
   const int *arma;
   const int *det;
   const int *mu;
} SlotFix;

/* Construye la tabla EN EL MISMO ORDEN que el vector de parametros. Tm[1..nser]
 * y lnk[0..nlink-1]. fix puede ser NULL.                                  */
void slots_build( SlotTable *st, struct Tusmodel *Tm, int nser,
                  const NetLink *lnk, int nlink, const SlotFix *fix );

/* El indice del slot que se llama name, o 0. */
int  slots_find( const SlotTable *st, const char *name );

/* Cuantos quedan LIBRES: es lo que ve el optimizador. */
int  slots_nfree( const SlotTable *st );

/* Lo que el slot i dice de si mismo, en una linea de .cns ("q[3,2] = free").
 * Un slot LIBRE que nacio libre no necesita linea: devuelve 0 y no escribe. */
int  slots_line( const SlotTable *st, int i, char *out, size_t size );

/* Lleva a la tabla NUEVA lo que decia la VIEJA, emparejando POR NOMBRE.
 *
 * Hace falta porque la tabla se reconstruye entera cada vez que cambia algo
 * --cargar una serie, tocar un enlace-- y sin esto lo que el analista haya
 * dicho se pierde en silencio, que es la peor forma de perderse.
 *
 * Devuelve cuantas restricciones se llevo; en *perdidas, cuantas nombraban un
 * slot que ya no existe.
 *
 * OJO CON REORDENAR LAS SERIES. Los nombres llevan la POSICION dentro --
 * q[3,2], phi_2[B^1], mu[4]-- asi que despues de mover una serie el mismo
 * nombre significa otra cosa y emparejar por nombre seria EXACTAMENTE lo
 * contrario de conservar. Para eso no se usa esto: se tira y se dice.    */
int slots_carry( SlotTable *nuevo, const SlotTable *viejo, int *perdidas );

/* ------------------------------------------------------------------------ */
/* El .cns                                                                   */
/* ------------------------------------------------------------------------ */

/* Como netfile: el HECHO, no la frase. El motor lo cuenta en ingles y el GUI
 * en espanol, y los dos leen el mismo fichero.                            */
typedef enum {
   CNS_OK = 0,
   CNS_ENOFILE,
   CNS_EUNKNOWN,     /* no hay ningun parametro que se llame asi (token)     */
   CNS_EOPERAND,     /* un operando del producto no existe (token)           */
   CNS_ESELF,        /* un slot en su propia definicion (token)              */
   CNS_ELC,          /* no entiendo la combinacion lineal (token = todo)     */
   CNS_EPARSE        /* no entiendo la linea (token = todo)                  */
} CnsErr;

typedef struct {
   CnsErr err;
   int    line;
   char   lhs[SLOT_NAME];
   char   token[128];
} CnsError;

const char *cns_error_en( const CnsError *e, char *out, size_t size );

/* Aplica un .cns sobre una tabla ya construida. Devuelve cuantas
 * restricciones aplico, o -1 con el hecho en *e (que puede ser NULL).     */
int cns_read( const char *path, SlotTable *st, CnsError *e );

/* Escribe el .cns que reproduce el estado de la tabla: solo los slots que
 * dicen algo, o sea los que no nacieron libres. Devuelve cuantas lineas.  */
int cns_write( const char *path, const SlotTable *st, const char *cabecera );

#endif /* ATSW_SLOTS_H */
