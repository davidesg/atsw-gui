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
   int    n;                /* cuantos periodos abarca                    */
   double z_max;            /* el mayor |z| del episodio, con su signo    */
   int    i_max;            /* donde esta                                 */
   double p;                /* P(chi2_L > suma z^2): lo improbable que es */
} AnEpisodio;

/* SE PUNTUA EL TRAMO, NO LOS PUNTOS.
 *
 * No es lo mismo un anomalo aislado de 3 sigma --raro-- que uno de 2, que
 * pasa el 5 % de las veces. Pero un 3 CON un 2 antes y otro despues es, en
 * un gaussiano, practicamente imposible. La probabilidad de un INCIDENTE no
 * es la del punto aislado, y una regla que mire punto a punto no puede
 * distinguir esas dos cosas por mucho que se le afine el umbral.
 *
 * Asi que se puntua la ventana entera. Con L periodos contiguos y los
 * residuos tipificados z_1..z_L:
 *
 *     S = suma z_i^2      y bajo la nula      S ~ chi2(L)
 *
 * ES UN EPISODIO si el tramo es AL MENOS TAN IMPROBABLE como un extremo
 * aislado en el umbral de declarar:
 *
 *     P( chi2_L > S )  <=  p1(u) / K        con u = an_umbral(n)
 *
 * Tres propiedades, y por eso es esta regla y no otra:
 *
 *   1. CON L = 1 ES LA REGLA DE SIEMPRE. P(chi2_1 > z^2) <= p1(u) es
 *      |z| >= u. No hay caso especial ni discontinuidad.
 *   2. NO TRAE NINGUNA CONSTANTE NUEVA salvo K. u ya estaba, y depende de n,
 *      que es lo que gobierna las comparaciones multiples.
 *   3. USA LAS MAGNITUDES. (3.5, 2.0) deja de ser el mismo caso que
 *      (2.0, 2.0), que es lo que un doble umbral no sabe distinguir.
 *
 * EL ESCALON POR PUNTO SALE DERIVADO, no elegido. Si todos valen lo mismo,
 * con n = 261 hacen falta 3.34 / 2.66 / 2.35 / 2.17 / 2.04 para L = 1..5. El
 * umbral fijo de 2.0 que hubo aqui era, sin saberlo, el valor correcto para
 * L ~ 4-5: demasiado laxo para parejas y demasiado estricto para tramos
 * largos.
 *
 * K, Y ESTA MEDIDO. Escanear varias longitudes infla los falsos positivos:
 * 4 000 series gaussianas de n = 261 dan 0,34 episodios falsos por serie con
 * K = 1 frente a los 0,21 de la regla anterior. Con K = 1,5 salen 0,226 --la
 * misma carga de falsas alarmas que habia-- al precio de que un extremo
 * aislado pase a pedir 3,45 en vez de 3,34. Decision del analista.
 *
 * UN PERIODO TRANQUILO ROMPE EL TRAMO, y esto hay que pedirlo aparte.
 *
 * La primera version confiaba en que el contraste lo rechazara solo --meter
 * un periodo callado cuesta un grado de libertad y no aporta suma-- y la
 * bateria enseño que no: dos picos de 5 sigma separados por dos ceros dan
 * una ventana de cuatro con p = 4e-10, mas improbable que cualquiera de los
 * dos solo. Y lo es, pero lo es PORQUE CONTIENE DOS SUCESOS, no porque sea
 * uno. Improbabilidad de la ventana no es unicidad del suceso.
 *
 * Asi que el tramo tiene que ser SOLIDO: todos sus periodos con |z| >=
 * AN_ACTIVO. Por debajo de una desviacion tipica no hay nada que explicar, y
 * un periodo asi separa dos sucesos en vez de unirlos.
 *
 * Eso sustituye al parametro de hueco que habia, y es mejor: el hueco era un
 * numero de periodos --una convencion-- y esto es una condicion sobre el
 * dato. Si el analista quiere tratar dos sucesos cercanos como uno, marca
 * las dos casillas: la ventana ya calibra varios episodios a la vez.
 *
 * LO QUE ESTA REGLA NO ARREGLA, dicho aqui: los z se tipifican con la
 * desviacion tipica MUESTRAL, que los propios anomalos inflan. El contraste
 * es por tanto CONSERVADOR, y tanto mas cuanto peor es el caso. Una escala
 * robusta lo corregiria y cambiaria todos los veredictos: es otra decision,
 * y el analista la dejo en muestral.
 *
 * Devuelve cuantos episodios encontro, los mas significativos primero y sin
 * solaparse.                                                            */
#define AN_K       1.5     /* calibrado: misma carga de falsas alarmas   */
#define AN_LMAX    8       /* la longitud maxima que se escanea          */
#define AN_ACTIVO  1.0     /* por debajo de 1 sigma no hay nada que explicar */

/* EL UMBRAL DEL VECINO YA NO AGRUPA: agrupa el escaner. Se queda porque es
 * el de TREADWAY --«¿la forma de abajo deja un anomalo al lado?»--, que es
 * una pregunta condicional y distinta. Y es el mismo con el que el motor
 * marca los residuos con «@» en el .out.                                */
#define AN_UMBRAL_VECINO   2.0

int an_episodios( const double *z, int n, double umbral,
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
