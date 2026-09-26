/*
 * intervencion.h -- qué FORMA tiene el suceso, leída en los residuos.
 *
 * LA PREGUNTA, Y POR QUE NO LA DECIDE EL AJUSTE
 *
 * Marcado un episodio anómalo, queda elegir qué se le pone: ¿un escalón, que
 * no revierte, o un impulso, que sí? Las dos cuestan UN parámetro y NO están
 * anidadas, así que el AIC no puede arbitrar entre ellas: elegir por ajuste
 * es leer ruido (art, BUG-0086).
 *
 * Lo decide LA FIRMA que el suceso deja en los residuos, por el diccionario
 * de la función de transferencia -- el mismo que usa art.ltf:
 *
 *     en el NIVEL           en ∇                        suma en ∇
 *     escalón ω en T   →    UN impulso ω en T           ω
 *     impulso ω en T   →    DOS impulsos +ω, −ω         0
 *
 * Y el diccionario vale aquí porque el motor resta el efecto determinista EN
 * EL NIVEL y diferencia después (fue.c: vtmp1 = z − Σν·x, y el operador no
 * estacionario se aplica a vtmp1). Es decir: «step 10 2008» es un escalón en
 * la serie, no en la serie diferenciada. Comprobado en el motor, no supuesto.
 *
 * De ahí sale la regla, y sale SIN UMBRAL NUEVO: se lee sobre los EXTREMOS
 * del episodio, que son los que la intervención tiene que explicar.
 *
 * LO QUE ESTE MODULO NO HACE
 *
 * No estima, no escribe el .inp y no decide. Llega hasta «el episodio de
 * 10/2008 tiene firma de ESCALON, y ésta es la razón», y se para ahí -- la
 * misma línea que lib/dictamen y lib/anomalos. Leer no es juzgar.
 *
 * Y NO ES TODA LA ESCALERA. Esto es el PELDAÑO 1, la lectura escalar. La
 * escalera de Ockham de art sigue con el peldaño 2 (el episodio entero, L+1
 * escalones) y el 3 (la FLT con denominador, cuando la respuesta decae), y
 * subir de peldaño NO lo justifica el AIC: lo justifica que la forma de abajo
 * deje un vecino anómalo o no deje ruido blanco. Eso se ve REESTIMANDO, que
 * es otra cosa y otro sitio. Aquí se dice en qué peldaño estás.
 */

#ifndef ATSW_INTERVENCION_H
#define ATSW_INTERVENCION_H

#include <stddef.h>

/* CUANTO DEL PICO PUEDE QUEDAR SIN CANCELAR y seguir leyéndose como impulso.
 *
 * El diccionario dice que en ∇ un impulso de nivel deja +ω, −ω, cuya suma es
 * exactamente cero; lo que sobra de esa suma es lo que NO revierte. Es una
 * convención declarada, no un contraste: el contraste de ganancia nula se
 * hace después, sobre los ω estimados.                                    */
#define IV_TOL_CANCELA  0.35

typedef enum {
   IV_ESCALON = 0,    /* step     -- efecto PERMANENTE                     */
   IV_IMPULSO,        /* impulse  -- efecto TRANSITORIO, un período        */
   IV_RAMPA,          /* ramp     -- cambio de PENDIENTE desde la fecha    */
   IV_COMPIMP         /* compimp  -- impulso compensado al período siguiente */
} IvForma;

/* Un extremo del episodio: su observación (0..n-1, como en lib/anomalos) y
 * su residuo tipificado con su signo.                                     */
typedef struct {
   int    obs;
   double z;
} IvExtremo;

typedef struct {
   IvForma forma;
   int     peldano;          /* 1: la lectura escalar basta; 2: hay más    */
   char    razon[512];       /* POR QUE, en una frase que va al informe    */
   char    aviso[256];       /* "" si no hay nada que avisar               */
} IvLectura;

/* La forma que dice el DATO, a partir de los extremos del episodio y de la
 * diferenciación regular d del modelo.
 *
 * ext[] no hace falta que venga ordenado. Devuelve 0 si pudo.
 *
 * Un vecino por DEBAJO del umbral de extremo no cuenta, y es a propósito:
 * ésa es la cola de un episodio más largo, no la mitad compensadora de un
 * impulso. Lo que se hace con ella es subir de peldaño, no cambiar la
 * lectura escalar.                                                        */
int iv_lectura( const IvExtremo *ext, int next, int d, IvLectura *out );

/* LA FECHA ES LA DEL PRIMER EXTREMO del episodio, no la del mayor: la
 * intervención tiene que empezar donde empieza el suceso. Devuelve el índice
 * en ext[] (o -1 si no hay).                                              */
int iv_primer_extremo( const IvExtremo *ext, int next );

/* La palabra del .inp: "step", "impulse", "ramp", "compimp". */
const char *iv_palabra( IvForma f );

/* El nombre en castellano, para la ventana. */
const char *iv_nombre_es( IvForma f );

/* La línea del .inp: "step 10 2008", o "step 2008" si freq == 1 (el motor
 * lee el año solo para los anuales). Devuelve 0 si pudo.                  */
int iv_linea( IvForma f, int freq, int per, int anno, char *out, size_t n );

/* --- la superposición: cómo capta la forma el suceso -------------------- */

/* LA HUELLA -- lo que la forma dejaría EN LOS RESIDUOS.
 *
 * La intervención se especifica SIEMPRE en el nivel, sea cual sea la d, y el
 * motor diferencia después. Así que su huella en los residuos es
 * (1−B)^d (1−B^s)^D aplicado al regresor de nivel. Por eso un escalón deja un
 * pico y un impulso deja dos que suman cero: no es una regla aparte, es esta
 * cuenta.
 *
 * h[0..n−1] son los índices de residuo base..base+n−1, y t0 es el índice
 * donde arranca el suceso. Devuelve 0 si pudo.                           */
int iv_huella( IvForma f, int t0, int d, int D, int s, int base,
               double *h, int n );

/* LOS TRES NUMEROS DE LA SUPERPOSICION, que separan tres preguntas y se leen
 * SIN mirar la figura:
 *
 *   escala  cuánto hay que multiplicar la forma para que encaje;
 *   r2      qué fracción de la ventana explica la forma YA escalada. Bajo con
 *           una escala razonable ⇒ el problema no es la amplitud, es el
 *           PERFIL: esa forma no es la del suceso;
 *   resto   el mayor |z| que SOBREVIVE a quitarla. Si tras ajustar queda un
 *           4, la hipótesis no cubre lo que hay -- y eso es exactamente el
 *           criterio de Treadway para subir de peldaño.
 *
 * DONDE NO LLEGA, dicho aquí para que no se le pida lo que no da: esto NO
 * distingue una forma correcta de otra que deja una cola permanente pequeña.
 * El r2 apenas se mueve, porque la diferencia está en la GANANCIA A LARGO
 * PLAZO, que es del comportamiento futuro y no del perfil local. Eso lo
 * dirime el contraste ω(1)=0, que exige estimar. El dibujo descarta lo
 * incompatible barato; el contraste ve lo que el dibujo no puede.        */
typedef struct {
   double escala;
   double r2;
   double resto;         /* el mayor |z| que queda, con su signo          */
   int    i_resto;       /* dónde, en índices de la ventana               */
} IvAjuste;

int iv_ajusta( const double *h, const double *z, int n, IvAjuste *out );

/* --- el peldaño 2: el episodio entero ----------------------------------- */

/* LA DURACION DEL SUCESO EN EL NIVEL, que no es la que se ve.
 *
 * Los residuos están diferenciados: L períodos alterados en el nivel se ven
 * como L+d extremos. Restar la d es lo que devuelve el suceso a la escala en
 * la que se ESPECIFICA la intervención y en la que el analista razona.
 * Contar sobre los residuos pedía un escalón de más por cada orden de
 * diferenciación.                                                        */
int iv_duracion_nivel( const IvExtremo *ext, int next, int d );

/* LAS COLUMNAS DEL PELDAÑO 2.
 *
 * La forma general de un episodio de L períodos son L+1 escalones en el
 * nivel: en el .inp, UNA intervención «step» con L+1 coeficientes ω, no L+1
 * intervenciones. Con ganancia ω(1)=0 equivalen a L impulsos de nivel.
 *
 * H es n x nesc por COLUMNAS: la columna j es la huella de un escalón en
 * t0+j. Con nesc == 1 es iv_huella de un escalón, exactamente.           */
int iv_huella_esc( int t0, int nesc, int d, int D, int s, int base,
                   double *H, int n );

/* El ajuste por mínimos cuadrados de esas columnas, y lo que queda.
 *
 * coef[0..nesc-1] recibe los coeficientes si no es NULL. En out->escala va
 * el PRIMERO --que con nesc == 1 es la escala de siempre-- y en resto, el
 * mayor |z| que sobrevive, que es lo que decide.                         */
int iv_ajusta_esc( const double *H, int nesc, const double *z, int n,
                   IvAjuste *out, double *coef );

/* --- qué hacer, y por qué ----------------------------------------------- */

/* EL AIC NO ARBITRA LA SUBIDA DE PELDAÑO. Compara DENTRO de un peldaño, o
 * confirma una subida ya justificada por otra cosa. Una escalera que se
 * quedara con el mejor AIC subiría siempre, porque el modelo más sofisticado
 * casi siempre ajusta mejor: tiene más parámetros. Eso es lo contrario de la
 * navaja.
 *
 * Lo que justifica subir, en orden, son cuatro cosas. Dos se ven SIN
 * ESTIMAR, y son las que este módulo mira:
 *
 *   1. el episodio dura más de un período EN EL NIVEL -- una intervención
 *      escalar no puede representar más de uno;
 *   2. TREADWAY: la forma de abajo deja un vecino anómalo. La parte no
 *      modelizada del suceso cae entera ahí.
 *
 * Las otras dos --que la forma de abajo no deje ruido blanco, y que la
 * lectura simple sea implausible para esa clase de serie-- exigen estimar y
 * saber el dominio, y no se deciden aquí.
 *
 * resto_escalar y umbral vienen de iv_ajusta sobre la forma escalar: si el
 * que llama no los tiene, que pase 0 y sólo se mirará la duración.       */
typedef struct {
   IvForma forma;         /* la escalar, para el peldaño 1               */
   int     nesc;          /* escalones: 1 es la lectura escalar          */
   int     peldano;       /* 1 o 2                                       */
   int     duracion;      /* L, en el nivel                              */
   char    razon[512];    /* por qué esa forma escalar                   */
   char    subir[512];    /* por qué subir; "" si no hay motivo          */
} IvPlan;

int iv_plan( const IvExtremo *ext, int next, int d,
             double resto_escalar, double umbral, IvPlan *out );

#endif /* ATSW_INTERVENCION_H */
