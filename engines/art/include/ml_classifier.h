/*
 * ml_classifier.h — Clasificador MLP para identificación de órdenes ARMA/SARIMA.
 *
 * Implementa el forward pass de un perceptrón multicapa de 2 capas ocultas
 * con 4 cabezas de clasificación (p, q, P, Q). Los pesos se importan desde
 * model_weights.h (generado por ml/export.py).
 *
 * Sin dependencias externas: solo opera con arrays float locales.
 */

#ifndef ML_CLASSIFIER_H
#define ML_CLASSIFIER_H

#include "model_weights.h"

/*
 * Estructura que contiene el resultado de la clasificación MLP:
 *   - orders[4]: órdenes predichos {p, q, P, Q}
 *   - confidence: producto de las probabilidades máximas de cada cabeza
 */
typedef struct {
    int orders[4];           /* {p, q, P, Q} predichos */
    double confidence;       /* ∏ max(softmax_i) — confianza global */
    double prob_p[MLP_NUM_p];
    double prob_q[MLP_NUM_q];
    double prob_P[MLP_NUM_SP];
    double prob_Q[MLP_NUM_SQ];
} MLPPrediction;

/*
 * Ejecuta el forward pass completo del MLP.
 *
 * Parámetros:
 *   features  — array de MLP_INPUT_DIM floats (salida de extract_feature_vector)
 *   pred      — puntero a MLPPrediction donde almacenar el resultado
 *
 * Devuelve:
 *   0 en caso de éxito, -1 si features es NULL o pred es NULL.
 */
int mlp_predict(const double *features, MLPPrediction *pred);

/*
 * Versión de conveniencia: devuelve solo los órdenes predichos.
 * Internamente llama a mlp_predict.
 */
int mlp_predict_orders(const double *features, int *p, int *q, int *P, int *Q);

#endif /* ML_CLASSIFIER_H */
