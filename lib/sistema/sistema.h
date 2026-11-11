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

/* ------------------------------------------------------------------------ */
/* El rango de cointegracion (S10b, sima prepare_vec y rank_test)           */
/* ------------------------------------------------------------------------ */

/* La prueba del rango de drvec es para series I(1): cada una con d = 1 y sin
 * diferencia estacional (D = 0), y al menos dos. 1 si el sistema la admite;
 * 0 con el motivo en why (el caso mixto, con alguna d = 0, lo hace sima:
 * prepare_vec mixed=True; no es el de por omision).                     */
int  sis_rango_admite( const Sistema *s, char *why, size_t n );

/* El caso determinista de drvec: "1", "2", "3" o "pre" (el de cada .pre). */
typedef struct {
   int  p, q;                 /* el VARMA sobre las series en niveles      */
   char caso[8];
   int  B;                    /* replicas del bootstrap; 0: sin bootstrap  */
} SisRango;

void sis_rango_defecto( SisRango *r );

/* La orden de la prueba:
 *   A.pre B.pre ... p q 0 -case C -lrtest [-bootstrap B] -name NOMBRE
 * NOMBRE es un nombre a secas: drvec escribe NOMBRE.out y NOMBRE.boot en el
 * directorio en que corre. argv apunta a s, a nombre y a buf (que guarda p,
 * q y B y tiene que vivir lo que argv); devuelve cuantos, -1 si no caben. */
int  sis_orden_rango( const Sistema *s, const SisRango *r, const char *nombre,
                      char *buf, size_t nbuf, const char **argv, int max );

/* ------------------------------------------------------------------------ */
/* El modelo: lo que la pagina 4 manda a drvarma (N3, sima estimate)        */
/* ------------------------------------------------------------------------ */

/* enlace[a][b] != 0: la serie b entra en la ecuacion de la serie a, en AR y
 * en MA, en todos los retardos hasta p y q (-links "A<-B"). Si estan todos
 * los pares, la orden no lleva -links: es el valor por omision de drvarma. */
typedef struct {
   int           p, q;                        /* la dinamica cruzada       */
   int           diagcov;                     /* Sigma diagonal            */
   unsigned char enlace[SIS_MAX_SER][SIS_MAX_SER];
   int           redet;                       /* reestimar deterministas   */
   int           fixarma;                     /* el ARMA univariante fijo  */
   int           metodo;                      /* -m: 1 (defecto) o 2       */
   int           hessian;                     /* 0 fd (defecto), 1 bfgs    */
   int           lik;                         /* 0 elf (defecto), 1 shea, 2 both */
   char          zero[2048];                  /* -zero: «AR1[A<-B],MA2[B<-A]», o "" */

   /* LA CLASE (S10c): VARMA en diferencias con drvarma, o VEC con drvec. En
    * el VEC p y q son los del VARMA sobre las series EN NIVELES (p >= 1), y
    * lo de arriba de enlaces, Sigma y opciones no se usa.               */
   int           clase;                       /* SIS_VARMA o SIS_VEC       */
   int           r;                           /* el rango del VEC          */
   char          caso[8];                     /* -case: 1, 2, 3 o pre      */
   /* LA NORMALIZACION: las series del bloque Y1, sobre las que se normaliza
    * beta (r de ellas). La logL EXACTA del VEC cambia con ella (decision del
    * analista, 2026-11-11): dos VEC solo se comparan si la comparten.    */
   unsigned char y1[SIS_MAX_SER];
} SisModelo;

#define SIS_VARMA 0
#define SIS_VEC   1

/* El VEC: cuantas series hay en Y1 (tienen que ser r), las de por omision
 * (las r ultimas del sistema), y la normalizacion por nombre: «LRM» o
 * «LRM, IDE». sis_normalizacion devuelve la longitud.                   */
int  sis_vec_ny1( const SisModelo *mo, int m );
void sis_vec_y1_defecto( SisModelo *mo, int m );
int  sis_normalizacion( const Sistema *s, const SisModelo *mo, char *out, size_t n );

/* La orden del VEC:
 *   Y2... Y1... p q r -case C -fdhess -name NOMBRE
 * primero las series de nabla Y2 y luego las de Y1, cada bloque en el orden
 * del sistema (drvec normaliza beta sobre las ultimas r). NOMBRE es un nombre
 * a secas: drvec escribe NOMBRE.out en el directorio en que corre. -1 si
 * no caben o si Y1 no tiene r series.                                  */
int  sis_orden_vec( const Sistema *s, const SisModelo *mo, const char *nombre,
                    char *buf, size_t nbuf, const char **argv, int max );

/* Un modelo de partida: p = 1, q = 0, Sigma completa, todos los enlaces. */
void sis_modelo_defecto( SisModelo *mo, int m );

/* Cuantos enlaces hay marcados, y cuantos pares posibles (m(m-1)).       */
int  sis_enlaces( const SisModelo *mo, int m );

/* Los enlaces como los lee drvarma: «A<-B,C<-A». Cadena vacia si estan
 * todos (no hace falta -links). Devuelve la longitud, -1 si no cabe.   */
int  sis_enlaces_texto( const Sistema *s, const SisModelo *mo, char *out, size_t n );

/* Al reves: «A<-B,C<-A» (o "" = todos) a la matriz de enlaces, por los
 * nombres de las series. 0 si se pudo; 1 si un nombre no es del sistema
 * (y la matriz queda como estaba).                                      */
int  sis_enlaces_de_texto( const Sistema *s, SisModelo *mo, const char *txt );

/* Hay retroalimentacion: algun par con enlace de ida y vuelta (A<-B y B<-A).
 * Es lo que trae a SIM y no a MT.                                       */
int  sis_retroalimenta( const SisModelo *mo, int m );

/* Los parametros CRUZADOS que se van a estimar: (p + q) por enlace, y los
 * de Sigma fuera de la diagonal si es completa.                         */
int  sis_parametros_cruzados( const SisModelo *mo, int m );

/* La orden del modelo:
 *   A.pre B.pre ... p q [-diagcov] [-links L] [-redet] [-fixarma] [-m 2]
 *   [-hessian bfgs] [-lik shea|both] -o SALIDA
 * argv apunta a cadenas de s, de salida y de buf (que guarda p, q y los
 * enlaces y tiene que vivir lo que argv); devuelve cuantos, -1 si no
 * caben.                                                                */
int  sis_orden_modelo( const Sistema *s, const SisModelo *mo, const char *salida,
                       char *buf, size_t nbuf, const char **argv, int max );

/* -zero solo lleva los coeficientes que el modelo tiene: su retardo dentro de
 * p o q y su par enlazado (drvarma rechaza los demas). sis_orden_modelo lo
 * filtra asi; esta es la misma cuenta, para quien quiera la lista. Devuelve
 * cuantos quedan.                                                       */
int  sis_ceros_validos( const Sistema *s, const SisModelo *mo, char *out, size_t n );

/* ------------------------------------------------------------------------ */
/* S7: simplificar y «¿es una red?» (sima simplify y structure)             */
/* ------------------------------------------------------------------------ */

/* La probabilidad de que una chi2 con df grados supere x. */
double sis_chi2_p( double x, int df );

/* El modelo restringido de un par: b deja de entrar en la ecuacion de a. */
void sis_modelo_sin_par( const SisModelo *mo, SisModelo *r, int a, int b );

/* El modelo triangular de un orden (perm[0..m-1], indices de serie): cada
 * serie solo recibe de las que van antes que ella. Lo que el modelo no
 * tenia enlazado sigue sin enlazar. Devuelve cuantos enlaces quita.     */
int  sis_modelo_triangular( const SisModelo *mo, SisModelo *r, const int *perm, int m );

#endif /* ATSW_SISTEMA_H */
