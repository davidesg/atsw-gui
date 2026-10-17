/*
 * inpdet.h -- meter una intervención en el .inp de fue.
 *
 * POR QUE UN MODULO Y NO UNAS LINEAS EN LA VENTANA
 *
 * El .inp es POSICIONAL aunque no lo parezca. Las líneas «**» son etiquetas
 * para humanos, pero el número de ellas no: fue reabre el fichero y se
 * posiciona contando fgets a pelo, así que una línea de más desincroniza los
 * dos lectores y el .pre sale con nombres de determinista basura, SIN ERROR.
 * Y fue hace 115 fscanf sin comprobar el retorno ni una vez: un campo que
 * falta no da error, deja la variable con su valor anterior.
 *
 * Añadir un determinista no es añadir una línea. Es:
 *
 *     subir el número de deterministas
 *     añadir su línea al bloque de nombres
 *     añadir su cuenta de omegas a la fila de omegas
 *     añadir su bloque de omegas (semilla 0.0, estimado)
 *     añadir su cuenta de deltas a la fila de deltas
 *
 * Cinco sitios. Hacerlo a mano en un diálogo es la forma de que un día salga
 * un .pre con basura y nadie sepa por qué.
 *
 * LA REGLA DE ESTE MODULO
 *
 * Lo de antes y lo de después del bloque de deterministas sale BYTE A BYTE
 * IGUAL. Sólo el bloque se reescribe, y se reescribe en la forma que ya usan
 * todos los ficheros del corpus -- lo cual se comprueba: añadir CERO
 * variables a cualquier .inp tiene que devolver el fichero idéntico. Si
 * algún fichero no lo cumple, la batería lo dice antes que el motor.
 *
 * Y NO SE FIA DE SI MISMO: quien llame debe pasar el resultado por
 * inp_check_fue --la puerta del propio motor-- antes de darlo por bueno.
 * Escribir un .inp que el motor no acepta es peor que no escribirlo.
 */

#ifndef ATSW_INPDET_H
#define ATSW_INPDET_H

#include <stddef.h>

#define ID_MAX_DET   64        /* el MAX_COUNT de inpcheck es mayor; esto es
                                  lo que cabe en una ventana sin mentir     */
#define ID_LINEA     128

/* Las intervenciones (impulse, compimp, step, ramp) que el fichero YA trae,
 * en det[0..max-1] como el motor las lee ("step 10 2008"). Devuelve cuántas,
 * o -1 con el motivo en porque[n].
 *
 * Existe para no poner DOS intervenciones sobre el mismo suceso: eso no da
 * error, da un omega no significativo por síntoma, que se lee como «no hacía
 * falta» cuando lo que pasa es que está contada dos veces.                */
int id_intervenciones( const char *origen, char det[][ID_LINEA], int max,
                       char *porque, size_t n );

/* TODOS los deterministas, EN SU ORDEN, tal como el motor los lee:
 * det[0] es la variable 1, det[1] la 2, y asi.
 *
 * Existe porque el .out NO los nombra: dice «Omegas for deterministic
 * variable 12» y nada mas. Para poder decir «step 3 2022» en vez de «la
 * intervencion 12» hay que casar por POSICION con el .inp -- que es una
 * busqueda, no un parseo de nombre: el orden es parte del formato.
 *
 * Devuelve cuantos, o -1 con el motivo en porque[n].                   */
int id_nombres( const char *origen, char det[][ID_LINEA], int max,
                char *porque, size_t n );

/* Copia origen en destino añadiendo los deterministas de nuevo[0..nn-1] al
 * final del bloque, con semilla 0.0, estimados y sin denominador.
 *
 * nomega[k] es LA CUENTA DEL FICHERO, no el número de coeficientes: el
 * bloque lleva nomega[k]+1 valores. Así lo lee el motor, y así hay que
 * decirlo para que no se confunda con el otro:
 *
 *     nomega = 0   UN coeficiente   la lectura escalar, el peldaño 1
 *     nomega = L   L+1 coeficientes la forma general de un episodio de L
 *                                   períodos, el peldaño 2
 *
 * El peldaño 2 es UNA intervención con L+1 omegas, NO L+1 intervenciones:
 * es la familia anidada ω(B) aplicada a un escalón de nivel, y con ganancia
 * ω(1)=0 equivale a L impulsos.
 *
 * nomega puede ser NULL: entonces todos a 0.
 *
 * Con nn == 0 copia el fichero tal cual, y eso es la prueba del módulo.
 *
 * Devuelve 0 si pudo; si no, 1 y el motivo en porque[n], y destino no se
 * toca.                                                                   */
int id_anade( const char *origen, const char *destino,
              const char *const *nuevo, const int *nomega, int nn,
              char *porque, size_t n );

/* ---------------------------------------------------------------------- */
/* THE ARMA PART (2026-10-04, for the identifier's window, an_identifica)  */
/*                                                                         */
/* The four sections that follow the deterministics -- regular AR, annual  */
/* AR, regular MA, annual MA -- each "count orders..." then, per factor, a */
/* "**" line and one "seed flag" line per coefficient (flag 1: estimated, */
/* fue.c [3.3.2]). The signs are Box-Jenkins: phi(B) = 1 - sum phi_j B^j,  */
/* theta(B) = 1 - sum theta_j B^j, as fue stores them (Ma1[i][0] = -1).    */
/* ---------------------------------------------------------------------- */

/* The ARMA orders the file declares, summed over its factors: *p, *q
 * (regular AR, MA) and *P, *Q (annual AR, MA). 0 if it could; else 1 and
 * the reason. */
int id_arma_ordenes( const char *origen, int *p, int *q, int *P, int *Q,
                     char *porque, size_t n );

/* Copy origen into destino with its four ARMA sections REPLACED by one
 * factor of each nonzero order, seeds from phi[0..p-1], theta[0..q-1],
 * Phi[0..P-1], Theta[0..Q-1] (NULL: 0.0), all estimated. Everything else is
 * copied byte for byte, the section labels included. The caller must pass
 * the result through inp_check_fue, as with id_anade. Returns 0, or 1 with
 * the reason (destino is not touched). */
int id_pon_arma( const char *origen, const char *destino,
                 int p, const double *phi, int q, const double *theta,
                 int P, const double *Phi, int Q, const double *Theta,
                 char *porque, size_t n );

/* Copy origen into destino with its transformation line -- the one after
 * "** Box-Cox lambda, regular differences and complete annual differences"
 * (fue.c [3.5]) -- set to lambda, d, D. Everything else byte for byte; the
 * individual factors of the annual difference are not touched. For the
 * identifier at E2: the child of the data node carries the transformation
 * that was identified on. 0, or 1 with the reason. */
int id_pon_transformacion( const char *origen, const char *destino,
                           double lam, int d, int D, char *porque, size_t n );

/* Copy origen into destino ADDING one factor of each nonzero order to its
 * ARMA operators: what the residuals say is missing (the identifier at E3
 * and E4). The factors already there stay as they are -- with their seeds,
 * which from a .pre are the optimum -- and the new one multiplies them:
 * (1 - phi_1 B)(1 - phi_2 B). On a model without ARMA it is id_pon_arma.
 * 0, or 1 with the reason. */
int id_anade_arma( const char *origen, const char *destino,
                   int p, const double *phi, int q, const double *theta,
                   int P, const double *Phi, int Q, const double *Theta,
                   char *porque, size_t n );

/* ---------------------------------------------------------------------- */
/* FIXING A PARAMETER (2026-10-14, for the formal tests, lib/formales)     */
/*                                                                         */
/* A restricted model is the estimated one with one parameter FIXED at its */
/* null value: the .pre with that seed changed and its flag set to 0, and  */
/* every other line byte for byte. fue then re-estimates the rest from the */
/* optimum, which is what art does with copy.deepcopy(model) and .fit().   */
/* ---------------------------------------------------------------------- */

#define ID_MAX_FAC  16
#define ID_MAX_ORD  24

/* The four ARMA sections as the file has them: section k (0 regular AR,
 * 1 annual AR, 2 regular MA, 3 annual MA) has nf[k] factors; factor i has
 * order ord[k][i], seeds v[k][i][0..] and flags fl[k][i][0..] (1 estimated,
 * 0 fixed). Box-Jenkins signs, as fue stores them.                        */
typedef struct {
   int    nf[4];
   int    ord[4][ID_MAX_FAC];
   double v[4][ID_MAX_FAC][ID_MAX_ORD];
   int    fl[4][ID_MAX_FAC][ID_MAX_ORD];
} IdArma;

int id_arma_lee( const char *origen, IdArma *a, char *porque, size_t n );

/* Copy origen into destino with coefficient j of factor i of ARMA section
 * k set to valor and FIXED. 0, or 1 with the reason.                     */
int id_fija_coef( const char *origen, const char *destino, int k, int i, int j,
                  double valor, char *porque, size_t n );

/* Copy origen into destino with regular AR factor i replaced by (1 - rho B)
 * FIXED and, when nresto > 0, a free factor of order nresto seeded with
 * resto[]: one root isolated and the rest of the operator left free (the
 * Shin-Fuller null, art BUG-0065). 0, or 1 with the reason.              */
int id_parte_ar( const char *origen, const char *destino, int i, double rho,
                 const double *resto, int nresto, char *porque, size_t n );

/* The deterministics' names, as id_nombres, and whether their FIRST omega
 * is estimated (libre[k] = 1) or fixed. Returns how many, or -1.        */
int id_det_lee( const char *origen, char det[][ID_LINEA], int libre[], int max,
                char *porque, size_t n );

/* Copy origen into destino with every omega of the deterministics whose
 * line is one of nom[0..nn-1] ("cos 2", "sin 2", "alter") set to 0.0 and
 * FIXED. *fijados, if not NULL, says how many deterministics were fixed.
 * 0, or 1 with the reason.                                               */
int id_fija_det( const char *origen, const char *destino, const char *const *nom,
                 int nn, int *fijados, char *porque, size_t n );

/* Copy origen into destino with its four ARMA sections REWRITTEN from a:
 * the factors, their seeds (exact) and their flags. The section labels and
 * everything outside them, byte for byte. With a read by id_arma_lee and not
 * touched, the operators are the same. 0, or 1 with the reason.         */
int id_escribe_arma( const char *origen, const char *destino, const IdArma *a,
                     char *porque, size_t n );

/* Copy origen into destino with the mean line (after "** Mean parameter")
 * set to valor and flag (0 fixed, 1 estimated). 0, or 1 with the reason. */
int id_fija_media( const char *origen, const char *destino, double valor, int flag,
                   char *porque, size_t n );

/* The individual factors of the annual difference, ifadf[0..6] as the file
 * has them (f = 0, 1, ..., s/2). 0, or 1 with the reason.               */
int id_ifadf( const char *origen, int ifadf[7], char *porque, size_t n );

/* The rescaling factor (after "** ACF/PACF bands ... and reescaling factor",
 * fue.c [3.7]): the engine works with refactor * BoxCox(y), and so are the
 * residuals of its .out. 0 in the file is 1, as fue reads it. 0, or 1.  */
int id_refactor_lee( const char *origen, double *refactor, char *porque, size_t n );

/* ---------------------------------------------------------------------- */
/* STOCHASTIC SEASONALITY (2026-10-15, for the MEG, lib/formales)          */
/*                                                                         */
/* The MEG turns one deterministic harmonic into its stochastic form: the  */
/* cos/sin pair (or alter) goes, the individual factor of the annual       */
/* difference at that frequency comes in, and with it a free MA witness.  */
/* ---------------------------------------------------------------------- */

/* Copy origen into destino with ifadf[0..] as its individual factors of the
 * annual difference (as many as the line has: s/2 + 1). 0, or 1.        */
int id_pon_ifadf( const char *origen, const char *destino, const int ifadf[7],
                  char *porque, size_t n );

/* Copy origen into destino WITHOUT the deterministics whose line is one of
 * nom[0..nn-1]: their name, their omega and delta counts and their blocks,
 * the five places. *quitados, if not NULL, says how many went. 0, or 1.  */
int id_quita_det( const char *origen, const char *destino, const char *const *nom,
                  int nn, int *quitados, char *porque, size_t n );

/* The regular MA(2) operators with fixed frequency (fue.c [3.3.13-14]):
 * factor i is (1 - theta1 B - v B^2) at harmonic freq[i] (omega =
 * 2 pi freq / s), with theta1 = 2 cos(omega) sqrt(-v) worked out by fue.
 * Only v is a parameter: -lambda^2, never positive, and -1 is the unit
 * circle. fl[i] 1 estimated, 0 fixed.                                    */
typedef struct {
   int    n;
   double freq[ID_MAX_FAC];
   double v[ID_MAX_FAC];
   int    fl[ID_MAX_FAC];
} IdMaf;

int id_maf_lee( const char *origen, IdMaf *m, char *porque, size_t n );

/* Copy origen into destino with that section REWRITTEN from m. 0, or 1. */
int id_escribe_maf( const char *origen, const char *destino, const IdMaf *m,
                    char *porque, size_t n );

/* ---------------------------------------------------------------------- */
/* THE WAY BACK FROM drtran (2026-10-17, DISENO-casos §3)                  */
/*                                                                         */
/* drtran re-estimates each series' univariate block beside the transfer, */
/* and those values go back to fue as an .inp: the same file with the new  */
/* seeds and every flag as it was. drtran numbers the deterministics'      */
/* coefficients by VARIABLE -- its nomega+1 omegas, then its ndelta deltas */
/* (lib/slots) -- while the file keeps all the omega blocks and then all   */
/* the delta blocks. These two functions speak drtran's order.             */
/* ---------------------------------------------------------------------- */

/* The deterministics' coefficients in drtran's order: v[i] the seed, fl[i]
 * the flag (1 estimated). Returns how many, or -1 with the reason.      */
int id_det_coefs( const char *origen, double v[], int fl[], int max,
                  char *porque, size_t n );

/* Copy origen into destino with those nc coefficients set to v[0..nc-1],
 * flags untouched. nc must be what id_det_coefs counts. 0, or 1.        */
int id_escribe_det_coefs( const char *origen, const char *destino,
                          const double v[], int nc, char *porque, size_t n );

/* The mean line: its seed and flag. 0, or 1 with the reason.            */
int id_media_lee( const char *origen, double *valor, int *flag,
                  char *porque, size_t n );

/* How many regular AR(2) operators with fixed frequency the file has
 * (fue.c [3.3.11-12]). 0, or 1 with the reason.                         */
int id_arf_cuenta( const char *origen, int *nf, char *porque, size_t n );

#endif /* ATSW_INPDET_H */
