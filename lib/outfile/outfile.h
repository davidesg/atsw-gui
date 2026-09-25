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

#define FO_MAX_PAR  64
#define FO_MAX_LB   16
#define FO_MAX_EXT  64      /* residuos extremos listados                 */
#define FO_MAX_CAL  64      /* tramos de la calibracion del motor         */

/* UN RESIDUO EXTREMO, tal como el motor lo lista:
       |     48        1/2006         2.08        |                      */
typedef struct {
   int    obs;              /* 1..n                                       */
   char   fecha[16];
   double z;                /* tipificado                                 */
} FoExtremo;

/* UN TRAMO DE LA CALIBRACION DEL MOTOR.
 *
 * El .out de fue trae, por cada autocorrelacion de los residuos, los tramos
 * de fechas que mas contribuyen y cuanto:
 *
 *     r(2) = -0.095       3/2018 -  5/2018       -0.026
 *
 * Es un reparto del NUMERADOR de r(k) por tramos, calculado por el motor.
 * Ojo: fug NO lo trae -- solo fue, y solo sobre los residuos de un modelo ya
 * estimado. Antes del modelo no hay nada que leer, y por eso existe
 * lib/anomalos.                                                          */
typedef struct {
   int    lag;
   double r;                /* la autocorrelacion entera                  */
   char   desde[16], hasta[16];
   double contrib;
} FoCalibra;

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

    /* LOS PARAMETROS, tal como el motor los tabula:
           -0.001610  (0.000683) [ 1]
       Los FIJOS salen sin error tipico, y entonces no hay t que calcular:
       no es que valga cero, es que no se estimo.                        */
    int      npar_leidos;
    double   par[FO_MAX_PAR], par_et[FO_MAX_PAR];
    gboolean par_estimado[FO_MAX_PAR];
    gboolean tiene_mu;            /* el modelo estima la media            */

    /* LOS PARES DE PARAMETROS CORRELACIONADOS. Salen de la matriz de
       correlaciones que el motor imprime -- dos decimales, que para
       decidir si dos parametros se pisan sobra.                         */
    int      npares;
    int      par_a[FO_MAX_PAR], par_b[FO_MAX_PAR];
    double   par_r[FO_MAX_PAR];

    /* LOS RESIDUOS */
    gboolean tiene_res;
    double   media, media_et, sd, skew, kurt;

    /* EL HISTOGRAMA, que el motor ya compara con lo esperado:
           65 values outside (-1,+1): 30.23 % (31.74 % expected)
       Es el contraste de normalidad mas barato que hay y esta impreso. */
    gboolean tiene_hist;
    double   fuera1, esp1, fuera2, esp2;
    gboolean tiene_jb;
    double   jb, jb_p;         /* Jarque-Bera, 2 g.l.                       */

    /* LOS RESIDUOS EXTREMOS, con su fecha y su |z|. El umbral que decide
       cuando son NOTICIA no esta aqui: es metodo, y vive en quien juzga. */
    int       next;
    FoExtremo ext[FO_MAX_EXT];

    /* LA CALIBRACION DEL MOTOR: que tramos distorsionan cada r(k). */
    int       ncal;
    FoCalibra cal[FO_MAX_CAL];

    /* EL LJUNG-BOX, LA ESCALERA ENTERA.
     *
     * El motor lo da escalonado --12, 24, 36 y el ultimo retardo-- y quedarse
     * solo con el ultimo pierde el diagnostico: Q(12) mal y Q(36) bien es un
     * problema CERCA, casi siempre estacional o de forma; al reves es
     * arrastre lejano. Son dos cosas distintas y asi salian iguales.     */
    int      nlb;
    double   lb_q_[FO_MAX_LB], lb_p_[FO_MAX_LB];
    int      lb_df_[FO_MAX_LB];

    /* El ultimo, que es el que resume la ventana entera. Se deja aparte
       porque es lo que la rejilla enseña en una columna.               */
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
