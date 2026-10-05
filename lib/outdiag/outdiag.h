/*
 * outdiag.h -- la diagnosis, leida del .out que escribe drtran.
 *
 * AQUI NO SE CALCULA NADA. El motor ya hace toda la diagnosis, y la hace bien:
 * por cada serie de residuos da media, desviacion, asimetria, curtosis, el
 * histograma con el PORCENTAJE OBSERVADO CONTRA EL ESPERADO, la ACF con el
 * Ljung-Box escalonado en la columna derecha y la PACF; luego las correlaciones
 * cruzadas entre residuos; luego el portmanteau multivariante de Hosking y el
 * Jarque-Bera multivariante; y por ultimo, POR CADA ENLACE, la CCF entre el
 * ruido estimado y la entrada preblanqueada, con su veredicto.
 *
 * Es, casi linea por linea, lo que el estudio de TASTE señalo como lo mejor de
 * su diseño --la lectura pegada al grafico-- ya implementado en el motor.
 *
 * Lo que falta es que 2000 lineas de .out se conviertan en algo con lo que se
 * pueda DECIDIR. Eso es lo unico que hace este modulo: leer.
 *
 * SE LEE UN INFORME, NO SE REHACE UN CALCULO. Es distinto de los otros lectores
 * de lib/: aqui no hay forma de compartir codigo con el motor, porque el motor
 * no emite nada legible por maquina. Un lector de texto formateado es fragil
 * por naturaleza, asi que la prueba se corre contra un .out DE VERDAD, recien
 * escrito por el motor: si el formato cambia, la prueba se entera el mismo dia.
 *
 * LO QUE EL MOTOR DICE POR ENLACE, y es lo accionable:
 *
 *     significativo en k >= 0  ->  falta estructura en LA TRANSFERENCIA
 *     significativo en k <  0  ->  RETROALIMENTACION: la entrada no es exogena
 *
 * Son dos diagnosticos opuestos y se arreglan de forma opuesta: el primero
 * cambiando (b, r, s), el segundo quitando el enlace -- o subiendo al VARMA.
 */

#ifndef ATSW_OUTDIAG_H
#define ATSW_OUTDIAG_H

#include <stddef.h>

#define OD_MAX_SER   16
#define OD_MAX_LINK  64
#define OD_NOMBRE    64

/* Un contraste: el estadistico, sus grados de libertad y su p. */
typedef struct {
   int    hay;
   double q;
   int    df;
   double p;
   int    tiene_p;
} OdTest;

/* Los residuos de una ecuacion. */
typedef struct {
   char   nombre[OD_NOMBRE];   /* "a[1]", tal como lo titula el motor   */
   int    nobs;
   double media, sd, skew, kurt;
   int    tiene_stats;

   /* El histograma: cuantos se salen y cuantos se esperaban. Es el contraste
    * de normalidad mas barato que hay, y el motor lo pone al pie.       */
   double fuera1, esp1;        /* % fuera de (-1,+1) y el esperado       */
   double fuera2, esp2;        /* idem para (-2,+2)                      */
   int    tiene_hist;

   OdTest lb;                  /* el ULTIMO Ljung-Box de la ACF          */
} OdSerie;

/* Un enlace, con los dos contrastes que lo juzgan. */
typedef struct {
   int    num;                       /* "input 3"                        */
   char   entrada[OD_NOMBRE];        /* el nombre entre parentesis       */
   OdTest transfer;                  /* k >= 0: la forma de la transferencia */
   OdTest exogen;                    /* k <  0: la exogeneidad           */
   int    exogen_signif;             /* "[n significant]"                */
   int    adecuado;                  /* lo que el motor sentencia        */
   int    exogeno;
   char   veredicto[200];            /* la frase del motor, tal cual     */
} OdEnlace;

typedef struct {
   OdSerie  s[OD_MAX_SER];
   int      ns;
   OdEnlace e[OD_MAX_LINK];
   int      ne;

   OdTest   hosking;                 /* portmanteau multivariante        */
   int      hosking_lag;
   int      hosking_blanco;          /* el motor no rechaza H0           */
   OdTest   jb;                      /* Jarque-Bera multivariante        */
   int      jb_normal;

   double   logl;
   int      tiene_logl;

   /* UNA CORRIDA DIAGONAL (drtran -0): todas las entradas con s = -1 en
    * "Transfer function orders" y sin "Transfer network". Es la que tiene
    * que reproducir la suma de los univariantes; las demas, no.          */
   int      diagonal;
} Diagnosis;

/* Lee el .out. Devuelve 0 si encontro algo de diagnosis, 1 si no.
 * texto puede ser el .out entero o la salida por pantalla.              */
int od_parse( const char *texto, Diagnosis *d );

/* Lo mismo, desde un fichero. -1 si no se puede abrir.                  */
int od_parse_file( const char *path, Diagnosis *d );

/* Cuantos enlaces salen mal, por cada motivo. */
int od_no_adecuados( const Diagnosis *d );
int od_no_exogenos( const Diagnosis *d );

/* ------------------------------------------------------------------------ */
/* Los residuos, como NUMEROS (el fichero de drtran -e)                      */
/* ------------------------------------------------------------------------ */

#define OD_MAX_OBS  2048

typedef struct {
   char   nombre[OD_MAX_SER][OD_NOMBRE];
   double v[OD_MAX_SER][OD_MAX_OBS];      /* [serie][0..n-1]                */
   char   fecha[OD_MAX_OBS][16];
   int    n, m, freq;
} OdResiduos;

/* Lee el fichero que escribe "drtran -e". 0 si pudo. */
int od_residuos( const char *path, OdResiduos *r );

/* ------------------------------------------------------------------------ */
/* La tabla de parametros del .out                                           */
/*                                                                           */
/* La que trae la DESVIACION TIPICA, y la que dice, de cada slot atado, por   */
/* que lo esta. Es la autoritativa: el resumen de stdout leia por posicion y  */
/* se descolocaba con el .cns.                                                */
/* ------------------------------------------------------------------------ */

#define OD_MAX_PAR  512

typedef struct {
   char   nombre[64];
   double valor;
   double dt;            /* 0 si esta atado                                 */
   double t, p;
   int    libre;         /* 0 = atado por el .cns                           */
   char   atado[128];    /* "= omega1[0] * theta_2[B^1]", si lo esta        */
} OdPar;

typedef struct {
   OdPar p[OD_MAX_PAR];
   int   n;
} OdParams;

int od_params( const char *texto, OdParams *o );
int od_params_file( const char *path, OdParams *o );

#endif /* ATSW_OUTDIAG_H */
