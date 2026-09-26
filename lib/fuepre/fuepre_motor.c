/*
 * fuepre_motor.c -- lo del modelo del .pre que solo necesita un MOTOR.
 *
 * unstable_delta usa chekma, que vive en elfvarma.c con la verosimilitud.
 * Por eso no va con el lector: el GUI de drtran y sus pruebas enlazan
 * fue_pre_reader.c sin el motor, y no tienen por que arrastrarlo.
 *
 * Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 * GPL v2 o posterior; ver COPYING.
 */

#include "main.h"
#include "fue_pre_reader.h"

/* -------------------------------------------------------------------------- */
/* Estabilidad del denominador δ(B) de cada variable determinista.            */
/* El filtro 1/δ(B) es recursivo: con raíces dentro del círculo unidad la      */
/* contribución determinista explota. Se comprueba con chekma, que usa la      */
/* misma convención de polinomio (1 - δ₁B - δ₂B² - …).                        */
/* -------------------------------------------------------------------------- */
int unstable_delta(struct Tusmodel *Tmi)
{
    int i, k;
    real wr[10], wi[10], wmod[10];

    for (i = 1; i <= Tmi->NdetVar; i++) {
        int nd = Tmi->Ndelta[i];
        int ifault_chk = 0;
        real ***t1;

        if (nd <= 0) continue;

        t1 = tensor(0, nd, 1, 1, 1, 1);
        t1[0][1][1] = 1.0;
        for (k = 1; k <= nd; k++) t1[k][1][1] = Tmi->Delta[i][k];

        chekma(1, nd, t1, wr, wi, wmod, &ifault_chk);
        free_tensor(t1, 0, nd, 1, 1, 1, 1);

        if (ifault_chk != 0) return 1;
    }
    return 0;
}
