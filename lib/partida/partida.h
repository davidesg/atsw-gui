/*
 * partida.h -- el modelo de partida de una serie sin modelo: la ruta U de
 * SIM (docs/DISENO-sim.md §5.5).
 *
 * Una serie que todavia no tiene modelo no puede entrar en un sistema: el
 * sistema se monta sobre el .pre de cada una. La ruta U (Jenkins y Alavi:
 * los univariantes primero) le hace uno de partida, como lo haria el
 * analista con las herramientas de la madre, pero sin preguntar:
 *
 *   λ     la correlacion media-desviacion tipica de grupos de freq
 *         observaciones (8 si es anual), en niveles y en logaritmos: la que
 *         quede mas cerca de cero (art, describe_boxcox). Sin logaritmo si
 *         hay valores no positivos.
 *   d     ADF y KPSS para d = 0, 1, 2 (lib/raices, el codigo del art en C):
 *         la menor d en que el ADF rechaza (art, recommended_d); 0 sube a 1
 *         si una recta explica mas del 50 % del nivel; y un paso como mucho,
 *         d <= 1 (art, policy.decide_d desde d = 0).
 *   D     0. La estacionalidad, determinista: los armonicos si la F de art
 *         la detecta (la de su ventana de identificacion, con d = 1 y
 *         100 log, como el describe de art-python).
 *   ARMA  el candidato que art propone con esa λ y esa d.
 *   media libre si no hay diferencias, o con una si la serie diferenciada
 *         tiene deriva (la regla de la ventana de identificacion).
 *
 * Es la regla de characterize/build_univariate de sima con el motor de la
 * madre, el art en C. Donde los dos calculan distinto (la F estacional:
 * sima la contrasta con su λ), manda la madre.
 *
 * Y el modelo queda en el proyecto como uno mas, hijo del nodo de datos,
 * estimado con fue y marcado «sin revisar» (PrModelo.sin_revisar): es un
 * punto de partida, no un analisis univariante.
 *
 * GLib, lib/raices, lib/artcand, lib/inpdet, lib/inpcheck, lib/proyecto,
 * lib/engine y el lector de .inp de fug.
 */
#ifndef ATSW_PARTIDA_H
#define ATSW_PARTIDA_H

#include <stddef.h>
#include "artcand.h"
#include "raices.h"
#include "proyecto.h"

enum { PT_LAM_MDT = 0, PT_LAM_NOPOS = 1 };            /* de donde sale λ   */
enum { PT_D_TESTS = 0, PT_D_TENDENCIA = 1, PT_D_PASO = 2 };  /* y d     */

#define PT_TENDENCIA 0.5      /* art, THRESHOLDS["trend_dominates"]        */

typedef struct {
    /* λ */
    double lam;
    int    lam_por;           /* PT_LAM_*                                  */
    double corr_raw, corr_log;   /* con signo, el diagnostico               */
    int    nog;               /* observaciones por grupo                   */
    /* d */
    RzRaiz raiz[RZ_MAX_D + 1];
    int    nraiz;
    int    d_tests;           /* lo que dicen ADF y KPSS (recommended_d)   */
    double trend_r2;
    int    d, d_por;          /* el punto de partida, y que regla lo movio */
    /* la estacionalidad y el ARMA, de art */
    int    armonicos;         /* la F detecta: van los armonicos           */
    int    hay_F;
    double F, F_p;
    int    media_libre;
} PtPartida;

/* λ por la correlacion media-d.t. Devuelve 0 o 1 y rellena el resto. */
double pt_lambda( const double *x, int n, int freq, PtPartida *p );

/* El R² de una recta sobre el nivel transformado (log si lam = 0). */
double pt_tendencia_r2( const double *x, int n, double lam );

/* d de partida, con p->lam ya puesta. Devuelve d y rellena el resto. */
int pt_d( const double *x, int n, PtPartida *p );

/* ¿Se estima la media? Con d + D = 0, si; con d + D = 1, si la serie
 * diferenciada tiene deriva (|t| >= 2, con el error tipico corregido por
 * su r1); con mas, no. x NULL: solo el primer caso.                      */
int pt_media_libre( const double *x, int n, double lam, int d, int D, int freq );

/* Los armonicos de fue para la frecuencia s: cos k y sin k para k < s/2, y
 * alter en s/2. Devuelve cuantos.                                        */
int pt_armonicos_de( int s, char det[][128], int max );

/* El .inp del hijo: origen con la transformacion (lam, d, D), el ARMA del
 * candidato k, los armonicos si armonicos (los que no tenga) y la media
 * libre si media. *puestos: cuantos armonicos añadio. 0, o 1 con el
 * motivo. Lo usa tambien la ventana de identificacion (E2).          */
int pt_deriva_inp( const char *origen, const char *destino, double lam, int d, int D,
                   int freq, const AcCand *k, int armonicos, int media, int *puestos,
                   char *porque, size_t n );

/* λ, d y art sobre la serie (escrita en datos_txt, junto a la que art deja
 * su .cand): todo lo de arriba, con el ARMA en c. 0, o 1 con el motivo. */
int pt_identifica( const char *art, const char *datos_txt, const double *x, int n,
                   int freq, int per, int anio, PtPartida *p, ArtCand *c,
                   char *why, size_t nw );

/* La razon del modelo, en una linea: de donde sale cada cosa. sistema
 * puede ser NULL o "".                                                   */
void pt_razon( const PtPartida *p, const AcCand *k, const char *sistema,
               char *buf, size_t n );

/* TODO JUNTO: el modelo de partida de serie en la muestra, en el proyecto. Lee el
 * nodo de datos, identifica (pt_identifica, con art y sus ficheros en
 * cache), deriva el hijo del nodo de datos, lo juzga con inp_check_fue y,
 * si estimar, lo estima con fue (lib/engine: el buscador del programa lo
 * pone quien llama). Lo marca sin revisar y le pone la razon. No guarda el
 * proyecto. id: el modelo nuevo.
 *
 * 0 hecho; 1 no se pudo (nada queda en el proyecto); 2 el .inp esta, pero
 * fue no lo estimo (el modelo queda, sin .pre). why, en los dos.       */
int pt_construye( Proyecto *pr, const char *serie, const char *muestra, const char *art,
                  const char *cache, const char *sistema, int estimar, char *id, size_t nid,
                  PtPartida *p, char *why, size_t nw );

#endif /* ATSW_PARTIDA_H */
