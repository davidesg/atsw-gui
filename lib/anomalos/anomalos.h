/*
 * anomalos.h -- cuánto de lo que ves en el correlograma es el anómalo.
 *
 * LA PREGUNTA
 *
 * Antes de elegir órdenes hay que saber si la estructura que se ve en el
 * correlograma es del proceso o del atípico. Y hay que saberlo EN LAS DOS
 * FUNCIONES, porque cada una decide una cosa:
 *
 *     la PACF decide el orden AR
 *     la ACF  decide el orden MA
 *
 * Calibrar sólo la ACF deja media identificación a ciegas, y no porque la ACF
 * prediga a la PACF: PORQUE NO LA PREDICE. La PACF es una transformación NO
 * LINEAL de la ACF (Durbin-Levinson), así que las dos pueden moverse en
 * direcciones opuestas y cambiar de veredicto en sentidos contrarios en el
 * mismo retardo.
 *
 * El caso medido que trae art -- nabla ln PGAS, n = 83, banda +-0,2151,
 * omitiendo |z| > 2,5 -- en el retardo 2:
 *
 *     ACF(2)   +0,1321 -> +0,3143   ENMASCARADA (el anómalo la tapaba)
 *     PACF(2)  -0,2964 -> -0,1967   FABRICADA   (no existe sin él)
 *
 * El mismo anómalo escondía una señal MA y fabricaba una AR A LA VEZ. Quien
 * calibrase sólo la ACF concluiría «hay más MA de la que creía» y no se
 * enteraría de que el AR(2) que estaba a punto de estimar ERA el anómalo.
 *
 * Y SIRVE EN LOS DOS SENTIDOS, que es lo que evita SOBRE-INTERVENIR: si al
 * quitar el anómalo ningún retardo cambia de veredicto dentro/fuera de banda,
 * intervenirlo no compra nada para la identificación -- y añadir una
 * intervención que no hace falta es gastar un parámetro y tocar la serie sin
 * motivo. Un módulo que sólo sabe decir «hay un atípico» empuja a intervenir
 * siempre; éste sabe decir «no hace falta».
 *
 * POR QUÉ NO HACE FALTA EL MOTOR
 *
 * El .out de fue trae una calibración propia --qué tramos de fechas
 * distorsionan cada r(k)-- pero SÓLO sobre los residuos de un modelo YA
 * ESTIMADO: fug no la trae. Justo donde esto decide, eligiendo los órdenes,
 * no hay nada que leer. Así que se calcula, y es barato: la PACF sale de la
 * ACF, de modo que UNA omisión da LAS DOS funciones.
 */

#ifndef ATSW_ANOMALOS_H
#define ATSW_ANOMALOS_H

#include <stddef.h>

#define AN_MAX_LAG  64
#define AN_MAX_EP   64

/* --- episodios ---------------------------------------------------------- */

/* UN SUCESO NO SON TRES ATÍPICOS.
 *
 * Un suceso que dura tres períodos eran tres extremos sueltos, cada uno con
 * su forma decidida por una comprobación de adyacencia -- una ETIQUETA en vez
 * de un contraste. En la réplica de Bolivia costaba 16,24 de AIC, y 1 de 8
 * corridas encontraba la segunda intervención del episodio de 2008-09.     */
typedef struct {
   int    desde, hasta;     /* indices 0..n-1, ambos inclusive            */
   int    n;                /* cuantos extremos lleva dentro              */
   double z_max;            /* el mayor |z| del episodio, con su signo    */
   int    i_max;            /* donde esta                                 */
} AnEpisodio;

/* Agrupa los |z| >= umbral en episodios: dos extremos separados por un hueco
 * <= ventana son el mismo suceso.
 *
 * ventana es un PARÁMETRO DECLARADO, no un número mágico enterrado. Por
 * defecto 2, que admite un período tranquilo dentro del suceso.
 *
 * Devuelve cuántos episodios encontró.                                    */
#define AN_VENTANA  2

int an_episodios( const double *z, int n, double umbral, int ventana,
                  AnEpisodio *out, int max );

/* EL UMBRAL DEPENDE DE n, y por eso no es una constante.
 *
 * Bajo especificación correcta el máximo de n normales crece con n: con 80
 * observaciones un |z| de 2,5 es noticia y con 600 no lo es. Devolver 3 fijo
 * haría que las series largas no tuvieran nunca atípicos y las cortas los
 * tuvieran siempre.                                                       */
double an_umbral( int n );

/* --- calibración -------------------------------------------------------- */

typedef enum {
   AN_IGUAL = 0,      /* el retardo no cambia de lado                     */
   AN_ENMASCARADA,    /* fuera de banda SOLO al quitar el anómalo: FALTA  */
   AN_FABRICADA       /* fuera de banda SOLO con él: SOBRA                */
} AnVeredicto;

typedef struct {
   int         lag;
   double      acf_con,  acf_sin;
   double      pacf_con, pacf_sin;
   AnVeredicto acf, pacf;
} AnLag;

typedef struct {
   int    nlags;
   AnLag  l[AN_MAX_LAG];
   double banda_con, banda_sin;   /* +-1.96/raiz(n)                       */
   int    n_con, n_sin;           /* observaciones retenidas              */
   int    cambia;                 /* cuantos retardos cambian de veredicto */
} AnCalibra;

/* La ACF y la PACF con y sin los índices de omitir[], y el veredicto por
 * retardo. 0 si pudo.
 *
 * El estimador, declarado:
 *
 *     mu se calcula sobre las observaciones RETENIDAS
 *     z~[t] = (z[t] - mu) si t no esta en I, y 0 si lo esta
 *     r(k)  = suma z~[t] z~[t+k] / suma z~[t]^2
 *     phi(k) por Durbin-Levinson sobre ese r(k)
 *
 * La desviación a cero y no la eliminación por pares: eliminar por pares
 * cambia el número de sumandos de cada retardo y las r(k) dejan de ser
 * comparables entre sí.                                                   */
int an_calibra( const double *z, int n, const int *omitir, int nomitir,
                int lags, AnCalibra *out );

const char *an_veredicto_es( AnVeredicto v );

#endif /* ATSW_ANOMALOS_H */
