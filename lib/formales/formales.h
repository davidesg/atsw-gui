/*
 * formales.h -- los contrastes formales de art (su formal_tests), en el GUI.
 *
 * Contrastan hipotesis sobre el MODELO ESTIMADO, y cada uno lo hace igual:
 * el modelo restringido es el estimado con un parametro FIJO en su valor
 * nulo, fue lo reestima desde el optimo, y se comparan las dos
 * log-verosimilitudes exactas (logelf). Es lo que hace art con
 * copy.deepcopy(model) y .fit().
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

#define FM_MAX      16
#define FM_RUTA     1024

typedef enum { FM_ESTACIONAL, FM_SF, FM_DCD, FM_DCD_S } FmTipo;

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
} FmPrueba;

typedef struct {
    int      n;
    FmPrueba t[FM_MAX];
    int      nobs, s, d;
    /* lo que no se pudo plantear, y por que: una linea por contraste */
    char     no_aplica[4][256];
} FmPlan;

/* Plantea los contrastes que el modelo admite y escribe sus .inp
 * restringidos en dir. pre es el .pre del modelo estimado (su optimo) y out
 * su .out: los coeficientes se leen de este, que los trae con seis
 * decimales (el .pre, con cuatro). nobs, s y d, los del .out. Devuelve 0
 * (aunque no haya ninguno: pl->n = 0, y no_aplica dice por que) o 1 con el
 * motivo en why.                                                          */
int fm_plan( const char *pre, const char *out, const char *dir, int nobs, int s, int d,
             FmPlan *pl, char *why, size_t nwhy );

/* Lee el .out del restringido (al lado de t->inp) y calcula el contraste
 * contra la log-verosimilitud del libre. corrio: fue acabo y escribio sus
 * resultados; si no, el contraste sale no calculable.                     */
void fm_resultado( FmPrueba *t, double l_libre, int nobs, int s, int corrio );

/* Los criticos, por si alguien los quiere enseñar sin contrastar */
void fm_criticos_sf( int n, double *c10, double *c5, double *c1 );
void fm_criticos_dcd_s( int s, double *c10, double *c5, double *c1 );

#endif /* ATSW_FORMALES_H */
