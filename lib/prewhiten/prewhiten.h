/*
 * prewhiten.h -- el preblanqueo de Box-Jenkins, y la CCF que identifica.
 *
 * Esto NO es codigo nuevo. Es el nucleo numerico de la identificacion que ya
 * estaba dentro de drtran.c, sacado a un fichero propio para que el GUI pueda
 * llamarlo sin arrastrar el main() del motor -- la misma razon por la que
 * DateToObs salio a lib/dates. Las dos funciones son PURAS: no tocan un solo
 * global del motor.
 *
 * EL PROCEDIMIENTO, que es el del libro:
 *
 *   1. la ENTRADA se preblanquea con SU PROPIO modelo ARMA:  a_t = phi/theta wX
 *   2. la SALIDA se filtra con EL MISMO filtro:            beta_t = phi/theta wY
 *   3. la CCF de los dos residuos es lo que se mira:
 *
 *          r(k) = corr( beta_t , a_{t-k} )
 *
 *      k > 0  la salida responde a la entrada con k de retardo: LA TRANSFERENCIA
 *      k < 0  la salida antecede a la entrada: RETROALIMENTACION, y ahi no
 *             deberia haber nada. Si la hay, la entrada no es exogena y el
 *             modelo de transferencia no se sostiene.
 *
 * Por que hace falta el paso 1: la CCF de las series crudas no identifica nada.
 * La autocorrelacion de cada serie se propaga a la correlacion cruzada y la
 * ensucia entera. Sobre el m6, la CCF cruda de EI contra EP da P(60) = 944;
 * eso no es senal, es el ruido de no haber preblanqueado.
 *
 * Y aqui es donde la escalera se paga sola: preblanquear exige el modelo
 * univariante de cada serie, y eso es EXACTAMENTE lo que trae un .pre.
 *
 * El host tiene que dar su main.h con struct Tusmodel, struct Tseries, real,
 * vector/free_vector (nlatools) y Ccf/Mean/Stdev (diagnose).
 */

#ifndef ATSW_PREWHITEN_H
#define ATSW_PREWHITEN_H

#include <stddef.h>

#include "main.h"

/* Los ordenes EXPANDIDOS del ARMA. Un factor anual de orden p ocupa los
 * retardos sper, 2*sper, ..., p*sper, asi que su orden expandido es p*sper:
 * contarlo como p es la equivocacion que hay que no cometer.            */
int total_ar_order( struct Tusmodel *Tm );
int total_ma_order( struct Tusmodel *Tm );

/* Los factores multiplicados en un solo polinomio
 *     Phi(B) = 1 - phi_1 B - ... - phi_p B^p        (phi_out[0] = 1 implicito)
 * phi_out[1..p] tiene que venir dimensionado con vector(1, p), y p ser el que
 * dan total_ar_order / total_ma_order.                                   */
void expand_ar_factors( struct Tusmodel *Tm, real *phi_out, int p );
void expand_ma_factors( struct Tusmodel *Tm, real *theta_out, int q );

/* Las transformaciones del modelo univariante --Box-Cox, restar deterministas,
 * diferenciar-- y la serie estacionaria que sale, w[1..*nstat_out].
 * DataMat[0][1..nobs] es el hueco de la serie transformada y
 * DataMat[1..NdetVar][1..nobs] las deterministas, tal como las deja
 * read_fue_pre. Si algo falla pone *w_out a NULL y *nstat_out a 0.       */
void apply_univariate_model( struct Tusmodel *Tm, struct Tseries *Ts,
                             real **DataMat, real **w_out, int *nstat_out );

/* El preblanqueo y la CCF bidireccional de una pareja.
 *
 *   entrada / salida   los dos .pre ya leidos, con su DataMat
 *   nlags              retardos a cada lado
 *   ccf                salida: ccf[0..2*nlags], con el retardo cero en el
 *                      centro (indice nlags) y k = i - nlags
 *   nu                 salida, o NULL: los pesos de la respuesta impulso,
 *                      nu(k) = r(k) * s_beta / s_a, en el mismo orden
 *   n                  salida: sobre cuantas observaciones se calculo, que es
 *                      lo que manda en la banda 2/sqrt(n)
 *   res                 salida, o NULL: los dos preblanqueados en un
 *                      matrix(1,n,1,2) --columna 1 la salida, columna 2 la
 *                      entrada-- que es justo lo que hosking_test pide. Lo
 *                      libera quien lo pide, con free_matrix(r,1,n,1,2).
 *
 * Devuelve 0 si pudo. Si las dos series no son comparables o alguna no tiene
 * variabilidad, devuelve != 0 y escribe el motivo en why[size].          */
int prewhiten_ccf( struct Tusmodel *tm_ent, struct Tseries *ts_ent,
                   real **dm_ent,
                   struct Tusmodel *tm_sal, struct Tseries *ts_sal,
                   real **dm_sal,
                   int nlags, double *ccf, double *nu, int *n,
                   real ***res, char *why, size_t size );

/* Los retardos que usa el motor: n/4, tope 24, suelo 10. */
int prewhiten_nlags( int n );

#endif /* ATSW_PREWHITEN_H */
