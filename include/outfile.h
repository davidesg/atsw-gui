/* outfile.h -- lo que se lee del .out que escribe el motor. */

#ifndef OUTFILE_H
#define OUTFILE_H

#include <glib.h>

/* Como acabo el optimizador. Lo deja en el .out en dos lineas que empiezan
 * por **** (qnewtopt.c, report()): el criterio de parada que se cumplio y
 * en cuantas iteraciones, con la norma del gradiente.
 *
 * Ojo con la palabra del motor: escribe "CONVERGENCE OBTAINED" para los
 * cinco criterios, tambien cuando lo que paso fue que se acabaron las
 * iteraciones. Aqui se separan los dos que son convergencia de verdad --
 * la norma del gradiente por debajo de gradtol, o el paso por debajo de
 * steptol -- de los tres que son una parada sin mas.                      */

typedef enum {
    CONV_NONE = 0,       /* el .out no lo dice                             */
    CONV_GRADTOL,        /* 1: norma del gradiente < gradtol               */
    CONV_STEPTOL,        /* 2: paso < steptol                              */
    CONV_NO_LOWER,       /* 3: el ultimo paso no encontro un punto mejor   */
    CONV_MAXITS,         /* 4: limite de iteraciones                       */
    CONV_MAXSTEPS        /* 5: cinco pasos seguidos de longitud maxima     */
} ConvKind;

typedef struct {
    ConvKind kind;
    int      iterations;   /* -1 si no se pudo leer                        */
    double   gradient;     /* la norma; -1 si no se pudo leer              */
    gchar   *brief;        /* "converged (gradtol)", para la barra         */
    gchar   *full;         /* las dos lineas del motor, para el globo      */
} Convergence;

/* TRUE si el .out lo dice. Se libera con convergence_clear().             */
gboolean convergence_of(const char *out_path, Convergence *c);
void     convergence_clear(Convergence *c);

/* TRUE cuando de verdad convergio (gradtol o steptol) */
gboolean convergence_is_good(const Convergence *c);

#endif
