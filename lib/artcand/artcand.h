/*
 * artcand.h -- the identifier's result, read: DATA_art.cand (engines/art).
 *
 * art writes its proposal in a line format (engines/art/README.md, "The .cand
 * format"): one record per line, the key first. This is the one reader of it,
 * for the identification window (lib/analisis/an_identifica) and its tests.
 *
 * Pure C: no GTK, no glib, so it is tested on its own (test_artcand.c).
 * A reader must ignore keys it does not know: the format grows by adding
 * keys, never by changing one.
 */
#ifndef ATSW_ARTCAND_H
#define ATSW_ARTCAND_H

#define AC_MAX_LAGS  64
#define AC_MAX_CAND  16
#define AC_MAX_ORD   10
#define AC_MAX_S     64
#define AC_MAX_MSG   16
#define AC_MSG_LEN   256

typedef struct {
    int    p, q, P, Q;
    double sim;               /* pattern similarity: the rank (option B)    */
    double weight;            /* Akaike weight of the conditional AICc      */
    double aicc;
    int    scored, proposed;
    double phi[AC_MAX_ORD], theta[AC_MAX_ORD];      /* Box-Jenkins signs    */
    double Phi[AC_MAX_ORD], Theta[AC_MAX_ORD];
    double tacf[AC_MAX_LAGS], tpacf[AC_MAX_LAGS];  /* [k-1] is lag k       */
} AcCand;

typedef struct {
    char   series[128];
    int    log, d, D, s;
    int    n_used, lags;
    double band;              /* 1.96 / sqrt(n_used)                         */

    int    has_seasonal, seasonal_detected, seasonal_s;
    double seasonal_F, seasonal_p;
    double dummies[AC_MAX_S];

    int    has_unit_root;
    double adf_stat, adf_p, adf_crit;   int adf_lags;
    double kpss_stat, kpss_p, kpss_crit; int kpss_lags;

    double acf[AC_MAX_LAGS], pacf[AC_MAX_LAGS];     /* [k-1] is lag k       */

    int    ncand;
    AcCand cand[AC_MAX_CAND];  /* in rank order; cand[0] is the proposal    */

    int    nmsg;
    char   msg[AC_MAX_MSG][AC_MSG_LEN];

    int    complete;           /* the closing `end` was read                */
} ArtCand;

/* What went wrong. The front end words it. */
enum {
    AC_OK = 0,
    AC_ENOFILE,               /* could not be opened                         */
    AC_EFORMAT,               /* a known key with a bad value (line in *line) */
    AC_ETRUNC                 /* no `end`: the file was cut short            */
};

/* Read path into c. Returns AC_OK or an AC_E* code; *line (if not NULL) is
 * the line of a format error. */
int ac_leer( const char *path, ArtCand *c, int *line );

/* The candidate's model as text, "(1,0)(0,1)" -> "AR(1) x SMA(1)". */
const char *ac_kind( const AcCand *k, char *out, int n );

#endif
