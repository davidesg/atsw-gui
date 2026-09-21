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


/* ------------------------------------------------------------------------ */
/* EL .out DE fue, LEIDO PARA UNA REJILLA                                    */
/*                                                                           */
/* NO es el de drtran. lib/outdiag lee AQUEL, que es multivariante y trae el */
/* portmanteau de Hosking y un Jarque-Bera conjunto; este trae el Ljung-Box  */
/* de la ACF de los residuos y el Jarque-Bera de una sola serie. Son dos     */
/* formatos distintos y por eso son dos lectores: darle el .out de fue al    */
/* lector de drtran no falla, simplemente no encuentra nada -- que es por    */
/* que la columna "Hosking" salia siempre vacia en modelos univariantes.     */
/*                                                                           */
/* AQUI NO SE ESTIMA NADA. Lo unico que se calcula es el p-valor a partir    */
/* del estadistico Y SUS GRADOS DE LIBERTAD, que el motor imprime los dos:   */
/* es convertir dos numeros suyos, no rehacer su cuenta.                     */
/* ------------------------------------------------------------------------ */

typedef struct {
    gboolean hay;              /* se reconocio como un .out de fue          */

    /* LA ESTRUCTURA, que es lo que distingue un modelo de otro.
       Los ordenes se cuentan SUMANDO los factores: el motor los imprime
       por separado --"regular AR factor 1", "annual MA factor 2"-- porque
       la especificacion es factorizada. (p,d,q)(P,D,Q)s es el resumen.    */
    double   lambda;           /* Box-Cox                                   */
    int      s;                /* periodo estacional                        */
    int      p, d, q;          /* regulares                                 */
    int      P, D, Q;          /* anuales (en B^s)                          */
    int      factores;         /* cuantos factores habia, para el globo     */
    gboolean ffijo;            /* habia factores de frecuencia fija         */
    int      ndet;             /* variables deterministas                   */

    int      nobs, npar;

    /* LOS RESIDUOS */
    gboolean tiene_res;
    double   media, sd, skew, kurt;
    gboolean tiene_jb;
    double   jb, jb_p;         /* Jarque-Bera, 2 g.l.                       */

    /* EL LJUNG-BOX: el ULTIMO de la escalera que el motor pone al margen
       derecho de la ACF. Es el que resume toda la ventana.                */
    gboolean tiene_lb;
    double   lb_q, lb_p;
    int      lb_df;
} FueOut;

/* Lee el .out. TRUE si reconocio algo. No reserva nada: no hay que liberar. */
gboolean fueout_read( const char *path, FueOut *o );

/* La estructura en una linea: "(0,1,1)(0,1,1)12  λ=0". Devuelve out.      */
char *fueout_estructura( const FueOut *o, char *out, size_t n );

/* La cola superior de una chi-cuadrado. Publica porque la prueba la mira. */
double chisq_cola( double x, int df );

#endif
