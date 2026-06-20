#!/usr/bin/env python3
"""export.py — Exporta pesos MLP (JSON) → model_weights.h (3 capas)"""

import numpy as np, argparse, json, os
from datetime import datetime

HEADER = """/*
 * model_weights.h — Pesos MLP para ART_18 (generado automáticamente)
 * Arquitectura: {input_dim} → {h1} → {h2} → {h3} → 4 cabezas
 * Parámetros: {total_params}  |  Fecha: {date}
 */

#ifndef MODEL_WEIGHTS_H
#define MODEL_WEIGHTS_H

#define MLP_INPUT_DIM   {input_dim}
#define MLP_HIDDEN1     {h1}
#define MLP_HIDDEN2     {h2}
#define MLP_HIDDEN3     {h3}
#define MLP_NUM_p  6
#define MLP_NUM_q  6
#define MLP_NUM_SP 3
#define MLP_NUM_SQ 3

/* Normalización Z-score */
static const float mlp_norm_mean[{input_dim}] = {{{norm_mean}}};
static const float mlp_norm_std[{input_dim}]  = {{{norm_std}}};

/* Capa 1: Linear({input_dim} → {h1}) + ReLU */
static const float mlp_fc1_weight[{h1}][{input_dim}] = {{{fc1_weight}}};
static const float mlp_fc1_bias[{h1}] = {{{fc1_bias}}};

/* Capa 2: Linear({h1} → {h2}) + ReLU */
static const float mlp_fc2_weight[{h2}][{h1}] = {{{fc2_weight}}};
static const float mlp_fc2_bias[{h2}] = {{{fc2_bias}}};

/* Capa 3: Linear({h2} → {h3}) + ReLU */
static const float mlp_fc3_weight[{h3}][{h2}] = {{{fc3_weight}}};
static const float mlp_fc3_bias[{h3}] = {{{fc3_bias}}};

/* Cabeza p: Linear({h3} → 6) */
static const float mlp_head_p_weight[6][{h3}] = {{{head_p_weight}}};
static const float mlp_head_p_bias[6] = {{{head_p_bias}}};

/* Cabeza q: Linear({h3} → 6) */
static const float mlp_head_q_weight[6][{h3}] = {{{head_q_weight}}};
static const float mlp_head_q_bias[6] = {{{head_q_bias}}};

/* Cabeza P (seasonal): Linear({h3} → 3) */
static const float mlp_head_P_weight[3][{h3}] = {{{head_P_weight}}};
static const float mlp_head_P_bias[3] = {{{head_P_bias}}};

/* Cabeza Q (seasonal): Linear({h3} → 3) */
static const float mlp_head_Q_weight[3][{h3}] = {{{head_Q_weight}}};
static const float mlp_head_Q_bias[3] = {{{head_Q_bias}}};

#endif
"""

def fm(mat, indent=4):
    rows = []; p = " "*indent
    for i,r in enumerate(mat):
        v = ", ".join(f"{float(x):12.8f}f" for x in r)
        rows.append(f"{p}{{{v}}}{',' if i<len(mat)-1 else ''}")
    return "\n".join(rows)

def fv(vec, indent=4):
    return " "*indent + ", ".join(f"{float(x):12.8f}f" for x in vec)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('model'); p.add_argument('-o',default='../include/model_weights.h')
    a = p.parse_args()
    with open(a.model) as f: w = {k:np.array(v,np.float32) for k,v in json.load(f).items()}
    inp = int(w['fc1_weight'].shape[1])
    h1 = int(w['h1']); h2 = int(w['h2']); h3 = int(w['h3'])
    tp = sum(v.size for v in w.values() if v.ndim > 0)
    c = HEADER.format(
        input_dim=inp, h1=h1, h2=h2, h3=h3, total_params=tp,
        date=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        norm_mean=fv(w['norm_mean']), norm_std=fv(w['norm_std']),
        fc1_weight=fm(w['fc1_weight']), fc1_bias=fv(w['fc1_bias']),
        fc2_weight=fm(w['fc2_weight']), fc2_bias=fv(w['fc2_bias']),
        fc3_weight=fm(w['fc3_weight']), fc3_bias=fv(w['fc3_bias']),
        head_p_weight=fm(w['head_p_weight']), head_p_bias=fv(w['head_p_bias']),
        head_q_weight=fm(w['head_q_weight']), head_q_bias=fv(w['head_q_bias']),
        head_P_weight=fm(w['head_P_weight']), head_P_bias=fv(w['head_P_bias']),
        head_Q_weight=fm(w['head_Q_weight']), head_Q_bias=fv(w['head_Q_bias']),
    )
    with open(a.o,'w') as f: f.write(c)
    print(f"  {a.o}: {inp}→{h1}→{h2}→{h3}  ({tp} params, {os.path.getsize(a.o)} bytes)")

if __name__=='__main__': main()
