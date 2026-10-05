/*
 * ml_classifier.c — Forward pass MLP 3-capas para ART_18.
 * C puro, sin dependencias. Pesos en model_weights.h.
 * Arquitectura: MLP_INPUT_DIM → MLP_HIDDEN1 → MLP_HIDDEN2 → MLP_HIDDEN3 → 4 cabezas
 */

#include "ml_classifier.h"
#include <math.h>
#include <string.h>

static inline double relu(double x) { return x > 0.0 ? x : 0.0; }

static void dense(const float *w, const float *b, const double *in, double *out,
                  int od, int id) {
    for (int i = 0; i < od; i++) {
        double s = (double)b[i];
        for (int j = 0; j < id; j++) s += (double)w[i * id + j] * in[j];
        out[i] = s;
    }
}

static void softmax(double *x, int n) {
    double mx = x[0];
    for (int i = 1; i < n; i++) if (x[i] > mx) mx = x[i];
    double sm = 0.0;
    for (int i = 0; i < n; i++) { x[i] = exp(x[i] - mx); sm += x[i]; }
    if (sm > 0.0) for (int i = 0; i < n; i++) x[i] /= sm;
}

static int argmax(const double *x, int n) {
    int b = 0;
    for (int i = 1; i < n; i++) if (x[i] > x[b]) b = i;
    return b;
}

int mlp_predict(const double *features, MLPPrediction *pred) {
    if (!features || !pred) return -1;

    /* Normalize */
    double xn[MLP_INPUT_DIM];
    for (int i = 0; i < MLP_INPUT_DIM; i++) {
        double s = (double)mlp_norm_std[i];
        if (s < 1e-8) s = 1.0;
        xn[i] = ((double)features[i] - (double)mlp_norm_mean[i]) / s;
    }

    /* Capa 1 */
    double h1[MLP_HIDDEN1];
    dense((const float*)mlp_fc1_weight, mlp_fc1_bias, xn, h1, MLP_HIDDEN1, MLP_INPUT_DIM);
    for (int i = 0; i < MLP_HIDDEN1; i++) h1[i] = relu(h1[i]);

    /* Capa 2 */
    double h2[MLP_HIDDEN2];
    dense((const float*)mlp_fc2_weight, mlp_fc2_bias, h1, h2, MLP_HIDDEN2, MLP_HIDDEN1);
    for (int i = 0; i < MLP_HIDDEN2; i++) h2[i] = relu(h2[i]);

    /* Capa 3 */
    double h3[MLP_HIDDEN3];
    dense((const float*)mlp_fc3_weight, mlp_fc3_bias, h2, h3, MLP_HIDDEN3, MLP_HIDDEN2);
    for (int i = 0; i < MLP_HIDDEN3; i++) h3[i] = relu(h3[i]);

    /* Cabezas */
    double lp[MLP_NUM_p]; dense((const float*)mlp_head_p_weight, mlp_head_p_bias, h3, lp, MLP_NUM_p, MLP_HIDDEN3);
    softmax(lp, MLP_NUM_p); memcpy(pred->prob_p, lp, MLP_NUM_p * sizeof(double));

    double lq[MLP_NUM_q]; dense((const float*)mlp_head_q_weight, mlp_head_q_bias, h3, lq, MLP_NUM_q, MLP_HIDDEN3);
    softmax(lq, MLP_NUM_q); memcpy(pred->prob_q, lq, MLP_NUM_q * sizeof(double));

    double lP[MLP_NUM_SP]; dense((const float*)mlp_head_P_weight, mlp_head_P_bias, h3, lP, MLP_NUM_SP, MLP_HIDDEN3);
    softmax(lP, MLP_NUM_SP); memcpy(pred->prob_P, lP, MLP_NUM_SP * sizeof(double));

    double lQ[MLP_NUM_SQ]; dense((const float*)mlp_head_Q_weight, mlp_head_Q_bias, h3, lQ, MLP_NUM_SQ, MLP_HIDDEN3);
    softmax(lQ, MLP_NUM_SQ); memcpy(pred->prob_Q, lQ, MLP_NUM_SQ * sizeof(double));

    pred->orders[0] = argmax(pred->prob_p, MLP_NUM_p);
    pred->orders[1] = argmax(pred->prob_q, MLP_NUM_q);
    pred->orders[2] = argmax(pred->prob_P, MLP_NUM_SP);
    pred->orders[3] = argmax(pred->prob_Q, MLP_NUM_SQ);

    pred->confidence = pred->prob_p[pred->orders[0]] * pred->prob_q[pred->orders[1]]
                     * pred->prob_P[pred->orders[2]] * pred->prob_Q[pred->orders[3]];
    return 0;
}

int mlp_predict_orders(const double *features, int *p, int *q, int *P, int *Q) {
    MLPPrediction pred;
    if (mlp_predict(features, &pred) != 0) return -1;
    *p = pred.orders[0]; *q = pred.orders[1]; *P = pred.orders[2]; *Q = pred.orders[3];
    return 0;
}
