/*
 * art.h -- the identifier as an engine: array in, result out (18.2.1).
 *
 * This is the interface atsw-gui's `engines/art` builds on. It replaces the
 * file-name entry point (`ejecutar_deteccion_automatica`) for callers that have
 * the series in memory, and it returns everything a GUI shows -- the tests,
 * the empirical correlogram, every candidate with its coefficients and its
 * theoretical correlogram, and the messages -- instead of printing it.
 *
 * Reentrant: no shared mutable state. Each call keeps its context in
 * thread-local storage, so two identifications on two threads do not cross.
 */
#ifndef ART_H
#define ART_H

#include "model_detection.h"

#define ART_MAX_MSG   16
#define ART_MSG_LEN   256
#define ART_MAX_S     64

/* Return codes of art_identify. */
enum {
    ART_OK = 0,
    ART_E_ARGS = 1,        /* bad options or arguments */
    ART_E_DATA = 2,        /* the data cannot be identified (log of a
                              non-positive value, too few observations) */
    ART_E_NOTHING = 3,     /* no candidate could be scored */
    ART_E_CANCELLED = 4    /* *cancel became non-zero */
};

/* Harmonic removal before the ACF/PACF (art-python's _remove_harmonics). */
enum {
    ART_HARM_AUTO = 0,     /* when D = 0 and s > 1, as art-python */
    ART_HARM_ON = 1,
    ART_HARM_OFF = 2       /* residuals whose harmonics are already modelled */
};

typedef struct {
    int apply_log;         /* natural log before differencing */
    int d, D, s;
    int p_max, q_max, P_max, Q_max;
    int mlp_direct;        /* 1: the option-B shortlist (recommended) */
    int run_tests;         /* 1: the seasonal F test and ADF/KPSS */
    int harmonics;         /* ART_HARM_* */
    int quiet;             /* 1: nothing on stdout */
    volatile int *cancel;  /* optional: set *cancel != 0 to stop */
    ProgressCallback on_progress;   /* optional */
} ArtOptions;

typedef struct {
    ModelCandidate model;  /* the identified model, the shortlist (each
                              candidate with its coefficients, AICc and
                              theoretical ACF/PACF) and the empirical
                              ACF/PACF (`lags_used` lags) */
    double band;           /* 1.96 / sqrt(n) of the series identified on */
    int n_used;            /* observations after the transformations */

    int seasonal_tested;
    int seasonal_detected;
    double seasonal_F, seasonal_p;     /* the HAC F test (art BUG-0206) */
    int seasonal_s;
    double seasonal_dummies[ART_MAX_S];   /* 100*log units, sample order */

    int unit_root_tested;
    double adf_stat, adf_p, adf_crit;
    int adf_lags;
    double kpss_stat, kpss_p, kpss_crit;
    int kpss_lags;
    int unit_root_suspected;

    char messages[ART_MAX_MSG][ART_MSG_LEN];
    int n_messages;
} ArtResult;

/* The defaults of art-python's suggest_orders for frequency s:
 * p <= max(3, s/2), q <= 2, P <= 1, Q <= 1, mlp_direct, tests on,
 * harmonics auto, log off, d = D = 0. */
void art_default_options(ArtOptions *opt, int s);

/* Identify the orders of x[0..n-1]. Returns ART_OK or an ART_E_* code;
 * on error res->messages says why. Free the result with art_free_result. */
int art_identify(const double *x, int n, const ArtOptions *opt, ArtResult *res);

void art_free_result(ArtResult *res);

/* ---- internal: the per-call context the engine reads (thread-local) ---- */
typedef struct {
    const double *x;
    int n;
    ArtResult *res;
    int quiet;
    int run_tests;
    int harmonics;
    volatile int *cancel;
} ArtContext;

extern _Thread_local ArtContext *art_tl_ctx;
int  art_cancelled(void);
void art_msg(const char *fmt, ...);
int  art_log(const char *fmt, ...);

#endif /* ART_H */
