// roots.h - Verificación de raíces para polinomios AR
#ifndef ROOTS_H
#define ROOTS_H

#include <math.h>
#include <stdbool.h>

#define MP 12
#define MP1 (MP + 1)

typedef struct {
    double r;
    double i;
} Complex;

typedef Complex ComplexArrayMp1[MP1];

// Funciones para cálculo de raíces
void cdiv(Complex a, Complex b, Complex *c);
double complex_abs(Complex a);
void complex_sqrt(Complex a, Complex *b);
void laguer(ComplexArrayMp1 a, int m, Complex *x, double eps, bool polish);
void zroots(ComplexArrayMp1 a, int m, ComplexArrayMp1 roots, bool polish);

// Función para verificar estabilidad de polinomio AR
bool check_ar_roots(double *phi, int p);
// Función para verificar estabilidad de polinomio MA
bool check_ma_roots(double *theta, int q);
#endif
