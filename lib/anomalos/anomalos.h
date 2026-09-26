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

/* DOS UMBRALES, PORQUE SON DOS PREGUNTAS.
 *
 * DECLARAR un suceso donde no se sabía que hubiera uno es mirar las n
 * observaciones a la vez: es un problema de comparaciones múltiples, y por
 * eso el umbral crece con n (an_umbral).
 *
 * EXTENDERLO a su vecino no lo es. Una vez declarado el suceso en T,
 * preguntar por T+1 es UNA pregunta, no n. Pedirle ahí el mismo umbral alto
 * es contestar una pregunta con el listón de otra, y cuesta caro: art lo
 * midió --la mitad de la potencia, 36 % frente a 75 % (BUG-0087)--.
 *
 *     |z| > 2.0  ->  p = 0.046, uno de cada 22
 *
 * Y 2.0 no sale de un libro: es el umbral con el que EL PROPIO MOTOR marca
 * los residuos con «@» en el .out (diagnose.c). Con un umbral más alto la
 * ventana agrupaba menos de lo que el informe que el analista tiene delante
 * señala -- se veían tres arroba seguidos y sólo uno se recogía.
 *
 * El caso que lo destapó, en IPC_ES m04:
 *
 *     1/2021  z = +3.01   @   el motor lo marca; el umbral alto, no
 *     2/2021  z = -3.50   @   el unico que pasaba
 *     3/2021  z = +2.36   @   el motor lo marca; el umbral alto, no
 *
 * Tres períodos contiguos con firma +,-,+ que se leían como un suceso de
 * uno. La forma que se le propone a eso no es la misma.
 *
 * LA REGLA: se encadenan los ACTIVOS (|z| >= umbral_vecino) con el hueco de
 * ventana, y una cadena es un episodio sólo si contiene algún EXTREMO
 * (|z| >= umbral). Así el número de falsos episodios lo sigue gobernando el
 * umbral alto, y la extensión no se deja fuera lo que es del mismo suceso.
 *
 * ventana es un PARÁMETRO DECLARADO, no un número mágico enterrado. Por
 * defecto 2, que admite un período tranquilo dentro del suceso.
 *
 * Devuelve cuántos episodios encontró.                                    */
#define AN_VENTANA         2
#define AN_UMBRAL_VECINO   2.0

int an_episodios( const double *z, int n, double umbral, double umbral_vecino,
                  int ventana, AnEpisodio *out, int max );

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

/* EL LJUNG-BOX, CON Y SIN.
 *
 * Q = n(n+2) suma_{k=1..m} r(k)^2 / (n-k)
 *
 * Los dos con LA MISMA formula y cada uno con SU n --el «sin» tiene menos
 * observaciones-- que es la unica comparacion que significa algo. No se
 * compara contra el Q que imprime el motor: ese sale de su propio estimador
 * y mezclarlos seria restar peras de manzanas.
 *
 * El p NO se calcula aqui: hace falta una chi-cuadrado y este modulo es
 * aritmetica elemental a proposito. Lo pone quien llame, que ya la tiene.  */
void an_q( const AnCalibra *c, int m, double *q_con, double *q_sin );

/* --- la normalidad, con y sin ------------------------------------------- */

/* EL MISMO CONJUNTO OMITIDO, OTRO ESTIMADOR, Y NO ES UN DESCUIDO.
 *
 * Para la ACF se omite por DESVIACION A CERO --mu sobre las retenidas, 0 en
 * los huecos-- porque eso deja los denominadores de todos los retardos
 * comparables entre si.
 *
 * Para la normalidad no vale: meter k valores en el centro de la
 * distribucion añade observaciones que no se observaron, justo donde mas
 * pesan para la curtosis. El efecto es pequeño con n grande --medido: 0,01
 * de curtosis con n = 300 y un hueco-- pero es del lado malo, y sobre todo
 * es responder otra pregunta. Una pregunta sobre la FORMA se contesta sobre
 * las observaciones que QUEDAN, sin rellenar los huecos.
 *
 * Son dos preguntas distintas y por eso son dos estimadores distintos.
 *
 * El JB de aqui y el del .out SI son comparables desde que al motor se le
 * corrigio la division entera de n/6 --imprimia un 1,5 % bajo con 262
 * observaciones--. El Q sigue sin serlo, que ese sale de otro estimador. */
typedef struct {
   int    n;
   double media, sd;
   double skew, kurt;     /* la curtosis en EXCESO, como el motor          */
   double jb;             /* n/6 (S^2 + K^2/4), con 2 g.l.                 */
} AnNormal;

/* Con y sin los indices de omitir[]. 0 si pudo. */
int an_normalidad( const double *z, int n, const int *omitir, int nomitir,
                   AnNormal *con, AnNormal *sinellos );

const char *an_veredicto_es( AnVeredicto v );

#endif /* ATSW_ANOMALOS_H */
