// roots.c - Implementación de verificación de raíces
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "root.h"

#define PI 3.141592654

void cdiv(Complex a, Complex b, Complex *c) {
    double den, t;
    if (fabs(b.r) >= fabs(b.i)) {
        t = b.i / b.r;
        den = b.r + t * b.i;
        c->r = (a.r + a.i * t) / den;
        c->i = (a.i - a.r * t) / den;
    } else {
        t = b.r / b.i;
        den = b.i + t * b.r;
        c->r = (a.r * t + a.i) / den;
        c->i = (a.i * t - a.r) / den;
    }
}

double complex_abs(Complex a) {
    double x = fabs(a.r), y = fabs(a.i);
    if (x == 0.0) return y;
    if (y == 0.0) return x;
    if (x > y) return x * sqrt(1.0 + pow(y / x, 2));
    else return y * sqrt(1.0 + pow(x / y, 2));
}

void complex_sqrt(Complex a, Complex *b) {
    double x = fabs(a.r), y = fabs(a.i), u, v, w, t;
    if (a.r == 0.0 && a.i == 0.0) {
        u = 0.0;
        v = 0.0;
    } else {
        if (x >= y) {
            w = sqrt(x) * sqrt(0.5 * (1.0 + sqrt(1.0 + pow(y / x, 2))));
        } else {
            t = x / y;
            w = sqrt(y) * sqrt(0.5 * (t + sqrt(1.0 + pow(t, 2))));
        }
        if (a.r >= 0.0) {
            u = w;
            v = a.i / (2.0 * u);
        } else {
            v = (a.i >= 0.0) ? w : -w;
            u = a.i / (2.0 * v);
        }
    }
    b->r = u;
    b->i = v;
}

void laguer(ComplexArrayMp1 a, int m, Complex *x, double eps, bool polish) {
    const double epss = 6.0e-8;
    const int mxit = 100;
    double err, dxold = complex_abs(*x), cdx, abx, dum;
    Complex sq, h, gp, gm, g2, g, b, d, f, dx, x1, cdum;

    for (int iter = 1; iter <= mxit; iter++) {
        b = a[m];
        err = complex_abs(b);
        d.r = d.i = f.r = f.i = 0.0;
        abx = complex_abs(*x);
        for (int j = m - 1; j >= 0; j--) {
            dum = f.r;
            f.r = x->r * f.r - x->i * f.i + d.r;
            f.i = x->r * f.i + x->i * dum + d.i;
            dum = d.r;
            d.r = x->r * d.r - x->i * d.i + b.r;
            d.i = x->r * d.i + x->i * dum + b.i;
            dum = b.r;
            b.r = x->r * b.r - x->i * b.i + a[j].r;
            b.i = x->r * b.i + x->i * dum + a[j].i;
            err = complex_abs(b) + abx * err;
        }
        err *= epss;
        if (complex_abs(b) <= err) break;

        cdiv(d, b, &g);
        g2.r = pow(g.r, 2) - pow(g.i, 2);
        g2.i = 2 * g.r * g.i;
        cdiv(f, b, &cdum);
        h.r = g2.r - 2 * cdum.r;
        h.i = g2.i - 2 * cdum.i;
        cdum.r = (m - 1) * (m * h.r - g2.r);
        cdum.i = (m - 1) * (m * h.i - g2.i);
        complex_sqrt(cdum, &sq);
        gp.r = g.r + sq.r;
        gp.i = g.i + sq.i;
        gm.r = g.r - sq.r;
        gm.i = g.i - sq.i;
        if (complex_abs(gp) < complex_abs(gm)) gp = gm;

        Complex m_complex = {m, 0};
        cdiv(m_complex, gp, &dx);
        x1.r = x->r - dx.r;
        x1.i = x->i - dx.i;
        if (x->r == x1.r && x->i == x1.i) break;
        *x = x1;
        cdx = complex_abs(dx);
        dxold = cdx;
        if (!polish && cdx <= eps * complex_abs(*x)) break;
    }
}

void zroots(ComplexArrayMp1 a, int m, ComplexArrayMp1 roots, bool polish) {
    const double eps = 2.0e-6;
    Complex *ad = malloc(MP1 * sizeof(Complex));
    Complex b, c, x;

    for (int j = 0; j <= m; j++) ad[j] = a[j];
    for (int j = m; j >= 1; j--) {
        x.r = 0.0; x.i = 0.0;
        laguer(ad, j, &x, eps, false);
        if (fabs(x.i) <= 2.0 * pow(eps, 2) * fabs(x.r)) x.i = 0.0;
        roots[j] = x;
        b = ad[j];
        for (int jj = j - 1; jj >= 0; jj--) {
            c = ad[jj];
            ad[jj] = b;
            double dum = b.r;
            b.r = b.r * x.r - b.i * x.i + c.r;
            b.i = dum * x.i + b.i * x.r + c.i;
        }
    }

    if (polish) {
        for (int j = 1; j <= m; j++) {
            laguer(a, m, &roots[j], eps, true);
        }
    }

    // Ordenar raíces
    for (int j = 2; j <= m; j++) {
        x = roots[j];
        int i;
        for (i = j - 1; i >= 1; i--) {
            if (roots[i].r <= x.r) break;
            roots[i + 1] = roots[i];
        }
        roots[i + 1] = x;
    }

    free(ad);
}

bool check_ar_roots(double *phi, int p) {
    if (p <= 0) return true;

    ComplexArrayMp1 a, roots;

    // Construir polinomio: 1 - phi1*x - phi2*x^2 - ... - phi_p*x^p
    for (int i = 0; i < MP1; i++) {
        a[i].r = 0.0;
        a[i].i = 0.0;
    }

    a[0].r = 1.0;  // Coeficiente para x^0

    for (int i = 1; i <= p; i++) {
        a[i].r = -phi[i-1];  // Coeficientes negativos
    }

    // Calcular raíces
    zroots(a, p, roots, true);

    // Verificar que todas las raíces estén fuera del círculo unitario
    // con un pequeño margen numérico para estabilidad
    for (int i = 1; i <= p; i++) {
        double modulo = sqrt(roots[i].r * roots[i].r + roots[i].i * roots[i].i);
        if (modulo <= 1.02 + 1e-6) {  // Margen numérico añadido
 //           printf("ADVERTENCIA: Raíz AR inestable detectada - módulo: %.6f\n", modulo);
            return false;  // Raíz dentro o muy cerca del círculo unitario
        }
    }

    return true;
}

// Añadir esta función en model_detection.c
bool check_ma_roots(double *theta, int q) {
    if (q <= 0) return true;

    ComplexArrayMp1 a, roots;

    // Box-Jenkins MA polynomial: 1 - theta1*x - theta2*x^2 - ... - theta_q*x^q,
    // the convention of every theta in ART (simulator, psi weights, Hannan-
    // Rissanen, AICc). It checked 1 + theta*x: the same modulus for q = 1, the
    // wrong polynomial for q >= 2 (2026-09-30).
    for (int i = 0; i < MP1; i++) {
        a[i].r = 0.0;
        a[i].i = 0.0;
    }

    a[0].r = 1.0;  // Coeficiente para x^0

    for (int i = 1; i <= q; i++) {
        a[i].r = -theta[i-1];
    }

    // Calcular raíces
    zroots(a, q, roots, true);

    // Verificar invertibilidad: raíces deben estar fuera del círculo unitario
    for (int i = 1; i <= q; i++) {
        double modulo = sqrt(roots[i].r * roots[i].r + roots[i].i * roots[i].i);
        if (modulo <= 1.0 + 1e-6) {
 //           printf("ADVERTENCIA: Raíz MA no invertible detectada - módulo: %.6f\n", modulo);
            return false;
        }
    }

    return true;
}

