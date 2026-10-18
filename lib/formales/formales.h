/*
 * formales.h -- los contrastes formales de art (su formal_tests), en el GUI.
 *
 * Contrastan hipotesis sobre el MODELO ESTIMADO, y cada uno lo hace igual:
 * el modelo restringido es el estimado con un parametro FIJO en su valor
 * nulo, fue lo reestima desde el optimo, y se comparan las dos
 * log-verosimilitudes exactas (logelf). Es lo que hace art con
 * copy.deepcopy(model) y .fit().
 *
 * EL NOMBRE DEL MEG (lo fijo el analista, 18-oct-2026): en español «modelos
 * MEG»; en inglés, cuando haya interfaz en inglés, «HSM». Los modelos de
 * estacionalidad hibrida por frecuencias son de Abraham y Box (1978), pero
 * los contrastes de aqui reunen a varios autores (HEGY, Gallego y Treadway,
 * entre otros). Para no ser imprecisos, la interfaz no atribuye: las citas
 * y las atribuciones van al manual del investigador.
 *
 *   simplificacion estacional  H0: cos_k = sin_k = 0 en todos los armonicos
 *                              libres. LR = 2(L - L0) ~ chi2(gl).
 *   Shin-Fuller (1998)         H0: una raiz del AR en rho_m = 1 - 4/n (una
 *                              diferencia regular de menos). Phi = L - L0, NO
 *                              2(L - L0); criticos de su Tabla II por n. La
 *                              raiz que se aisla es la real POSITIVA mas
 *                              cercana a +1 (art BUG-0215); un factor de orden
 *                              p > 1 se parte en (1 - rho_m B) por uno libre
 *                              de orden p - 1 (art BUG-0065).
 *   DCD                        H0: theta = 1 en un MA(1) regular (una
 *                              diferencia regular de mas). LR = 2(L - L0);
 *                              criticos de Davis-Dunsmuir 1.00/1.94/4.41.
 *   DCD estacional             H0: Theta = 1 en un MA(1) anual: la diferencia
 *                              estacional sobra. Criticos de Davis, Chen y
 *                              Dunsmuir, Tabla 3.2, por s.
 *   sobrediferenciacion        el lado MA del par en f = 0: se impone d + 1,
 *                              sin media, con un testigo MA(1) en +0.85, y
 *                              H0: theta = 1 (la diferencia extra sobra).
 *                              Tres estimaciones como mucho: el candidato, el
 *                              restringido y, si el LR sale negativo o el
 *                              testigo se va al eje negativo, el libre otra
 *                              vez desde el restringido (art BUG-0236).
 *   MEG                        ¿la estacionalidad es estocastica o
 *                              determinista? Frecuencia a frecuencia, sobre
 *                              un modelo con D = 0 y armonicos: el modelo
 *                              estocastico S quita el cos/sin de f (o el
 *                              alter en Nyquist), pone la raiz unitaria
 *                              (ifadf[f] = 1) y un TESTIGO de
 *                              sobrediferenciacion libre del mismo orden --un
 *                              MA(2) de frecuencia fija, o un MA(1) regular
 *                              en Nyquist--, y contrasta H0: el testigo en la
 *                              frontera (-1), que cancela la raiz. Invertible
 *                              -> estocastica; en la frontera -> determinista.
 *                              Criticos: la ley s = 2 del par complejo, por
 *                              n, en las interiores; la s = 1 en Nyquist.
 *                              Salen de simulacion (Monte Carlo del estudio
 *                              SF_MEG, sin publicar). Dos estimaciones por
 *                              frecuencia: S libre y S restringido.
 *   DCD_f                      el mismo contraste sobre los MA(2) de
 *                              frecuencia fija que el modelo YA tiene: el
 *                              testigo de un modelo reformulado tras el MEG.
 *   subdiferenciacion          ¿sobraba la ultima diferencia? H0: theta = 1
 *                              en el MA del modelo (o en un testigo añadido
 *                              si no tiene). El AR del restringido se acota en
 *                              rho_m si quiere reconstruir la diferencia
 *                              (BUG-0224). SE PIDE, NO SE OFRECE (BUG-0167):
 *                              fm_plan no lo plantea; lo hace fm_plan_sub.
 *
 * AQUI NO SE DECIDE NADA y NO SE ENCADENA NADA. Cada contraste da su numero,
 * su critico y lo que dice; el veredicto encadenado de art
 * (describe_formal_tests) es donde viven sus fallos abiertos y no se porta.
 * Un LR que no es un estadistico --no finito, o negativo-- sale como «no
 * calculable», nunca como veredicto (art BUG-0232, BUG-0236).
 *
 * La biblioteca no corre el motor: fm_plan escribe los .inp restringidos y
 * fm_resultado lee el .out que fue deja al lado. Quien llama decide como
 * correrlo (la ventana, sin bloquear; la prueba, de una vez).
 */

#ifndef ATSW_FORMALES_H
#define ATSW_FORMALES_H

#include <stddef.h>

#define FM_MAX      32
#define FM_RUTA     1024

typedef enum { FM_ESTACIONAL, FM_SF, FM_DCD, FM_DCD_S, FM_SOBRE, FM_SUB, FM_MEG,
               FM_DCD_F } FmTipo;

/* las lineas de no_aplica, una por familia */
#define FM_NA 8

/* El testigo «en la frontera»: a esta distancia de +1 o menos. La banda de
 * cuasi-cancelacion del articulo, r = 0.90-0.95, por el lado generoso. */
#define FM_BANDA 0.10

typedef struct {
    FmTipo tipo;
    int    factor;            /* factor (0-based) del AR o del MA; -1 si no */
    char   inp[FM_RUTA];      /* el modelo restringido, escrito por fm_plan */
    char   base[64];          /* su nombre sin .inp: lo que se le pasa a fue */
    char   hipotesis[256];    /* H0, dicha                                  */
    double coef;              /* lo estimado: theta, Theta, phi dominante   */
    double nulo;              /* el valor de H0: 1, rho_m                   */
    int    gl;                /* grados de libertad (simplificacion)        */

    /* lo que da fm_resultado */
    int    hecho;
    int    calculable;
    char   motivo[256];       /* por que no es calculable                   */
    double l_libre, l_restr;
    double est;               /* LR, o Phi en Shin-Fuller                   */
    double c10, c5, c1;       /* criticos                                   */
    double p;                 /* p-valor chi2 (simplificacion); NAN si no   */
    int    rechaza;           /* rechaza H0 al 5 %                          */
    int    al_otro_lado;      /* SF: phi dominante >= rho_m, Phi = 0        */

    /* los de varias etapas (sobre y subdiferenciacion) */
    int    etapa, idx, iter, maxiter;
    double l_testigo;         /* L del libre con el testigo                 */
    double l_b;               /* L del primer restringido, sin acotar       */
    double phi_r, rho_m;      /* SUB: raiz AR dominante del restringido     */
    int    acotado;           /* SUB: se acoto en rho_m (BUG-0224)          */
    int    raices_complejas;  /* SUB: el MA del modelo no tiene raiz real   */
    char   testigo[160];      /* de donde sale el testigo                   */
    char   candidato[320];    /* SOBRE: que lleva el candidato              */

    /* MEG, DCD_f y el DCD del testigo de Nyquist */
    int    freq;              /* la frecuencia estacional; 0 si no va de eso */
    int    n_crit;            /* la n con que se leen los criticos del par   */
    int    omitida;           /* MEG: la frecuencia no se pudo contrastar;
                                 motivo dice por que (hecho y no calculable) */

    /* EL REINICIO de fue-python (su BUG-0005): una estimacion que para sin
       anular el gradiente se vuelve a correr desde donde paro, hasta tres
       veces, mientras la verosimilitud mejore. El binario no lo hace. */
    int    reinicios, en_reinicio;
    int    dudoso;            /* alguna de sus estimaciones acabo sin anular
                                 el gradiente, aun reiniciando: el LR hereda
                                 esa duda (art lo avisa, y sigue)           */
    double l_mejor;
    char   base_real[64];
} FmPrueba;

typedef struct {
    int      n;
    FmPrueba t[FM_MAX];
    int      nobs, s, d, D;
    double   lam;
    char     dir[FM_RUTA], pre[FM_RUTA], out[FM_RUTA];
    int      n_det, n_armon;  /* deterministas, y de ellos armonicos       */
    int      p_ar;            /* coeficientes AR regulares libres          */
    int      ifadf[7];        /* los factores de la diferencia anual       */
    /* lo que no se pudo plantear, y por que: una linea por contraste */
    char     no_aplica[FM_NA][256];
} FmPlan;

/* Plantea los contrastes que el modelo admite y escribe sus .inp
 * restringidos en dir. pre es el .pre del modelo estimado (su optimo) y out
 * su .out: los coeficientes se leen de este, que los trae con seis
 * decimales (el .pre, con cuatro). nobs, s y d, los del .out. Devuelve 0
 * (aunque no haya ninguno: pl->n = 0, y no_aplica dice por que) o 1 con el
 * motivo en why.                                                          */
int fm_plan( const char *pre, const char *out, const char *dir, int nobs, int s, int d,
             FmPlan *pl, char *why, size_t nwhy );

/* El lado d-1, cuando el analista lo PIDE (BUG-0167): lo añade al plan. pl
 * es uno que ya planteo fm_plan. 0 si lo planteo; 1 con el motivo en why. */
int fm_plan_sub( FmPlan *pl, char *why, size_t nwhy );

/* Despues de correr fue sobre t->base: lee lo que dejo y, si al contraste
 * le falta otra estimacion, escribe su .inp en t->inp / t->base y devuelve
 * 1; si ya esta, lo calcula (t->hecho) y devuelve 0. corrio: fue acabo con
 * 0. l_libre, la log-verosimilitud del modelo estimado.                  */
int fm_avanza( FmPlan *pl, FmPrueba *t, double l_libre, int corrio );

/* Lee el .out del restringido (al lado de t->inp) y calcula el contraste
 * contra la log-verosimilitud del libre. Para los de una sola estimacion;
 * fm_avanza lo llama.                                                     */
void fm_resultado( FmPrueba *t, double l_libre, int nobs, int s, int corrio );

/* Los criticos, por si alguien los quiere enseñar sin contrastar */
void fm_criticos_sf( int n, double *c10, double *c5, double *c1 );
void fm_criticos_dcd_s( int s, double *c10, double *c5, double *c1 );
void fm_criticos_dcd_f( int n, double *c10, double *c5, double *c1 );

/* LA REFORMULACION DESPUES DEL MEG (meg_reformulate de art): el modelo
 * estocastico S en la frecuencia f, escrito desde el .pre del modelo en
 * destino, con los coeficientes de su .out (out; puede ser NULL). Es el MISMO que el MEG estima: sin los armonicos de f, con
 * ifadf[f] = 1 y con el testigo libre desde -0.9 --sin el, el AR_f solo
 * sobrediferencia la estacionalidad--. Una frecuencia cada vez: la mas
 * fuerte primero, y vuelta a contrastar sobre lo que salga. 0, o 1 con el
 * motivo. *testigo, si no es NULL, el indice del testigo en su seccion. */
int fm_reformula( const char *pre, const char *out, const char *destino, int s, int f,
                  int *testigo, char *why, size_t nwhy );

#endif /* ATSW_FORMALES_H */
