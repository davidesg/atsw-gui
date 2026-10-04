/*
 * art.c -- the identifier's command line in atsw-gui (engines/art).
 *
 *   art DATA [-s S] [-l] [-d D] [-D D] [-p P] [-q Q] [-P P] [-Q Q]
 *            [--harmonics auto|on|off] [--no-tests] [--classic]
 *
 * DATA is a data file read by lib/datos (the house's single data door: text,
 * CSV or .xlsx; column 1 is the series; "# freq" / "# start" headers or a date
 * column give the frequency). -s gives it when the file does not.
 *
 * It writes, next to DATA (lib/rutas):
 *   DATA_art.out    the identification for a person to read;
 *   DATA_art.cand   the same for the GUI's window, one keyword per line
 *                   (docs/ESTUDIO-identificador.md, decision 4).
 *
 * It PROPOSES; it does not estimate. Estimating is fue's.
 *
 * Exit status, as lib/engine/engine.h reads it:
 *   0  the results are written
 *   1  the command line or the data file could not be read
 *   2  the data cannot be identified (bad options, log of a non-positive
 *      value, too few observations); nothing is written
 *   3  no candidate could be scored; the .out says why
 *   4  run-time error
 *
 * Output in English: a declared property of the engines (lib/datos keeps the
 * Spanish messages for the GUIs).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "art.h"
#include "datos.h"
#include "rutas.h"

#define ART_ENGINE_VERSION "18.2.1"

static void usage(FILE *f)
{
    fprintf(f,
        "usage: art DATA [-s S] [-l] [-d D] [-D D] [-p P] [-q Q] [-P P] [-Q Q]\n"
        "                [--harmonics auto|on|off] [--no-tests] [--classic]\n"
        "  -s S    seasonal period, when DATA does not declare its frequency\n"
        "  -l      natural log before differencing\n"
        "  -d, -D  regular and seasonal differences (default 0)\n"
        "  -p -q -P -Q   upper limits (default art-python's: p<=max(3,s/2), q<=2, P,Q<=1)\n"
        "  --harmonics   remove the deterministic harmonics: auto (D=0, s>1), on, off\n"
        "  --no-tests    skip the seasonal F test and ADF/KPSS\n"
        "  --classic     the classic grid search instead of the option-B shortlist\n"
        "writes DATA_art.out and DATA_art.cand next to DATA\n");
}

/* "AR(1)", "MA(1) x SAR(1)", "white noise". */
static const char *kind(const OrderCandidate *c, char *b, size_t n)
{
    int k = 0;
    b[0] = 0;
    if (c->p) k += snprintf(b + k, n - k, "AR(%d)", c->p);
    if (c->q) k += snprintf(b + k, n - k, "%sMA(%d)", k ? "+" : "", c->q);
    if (c->P || c->Q) {
        k += snprintf(b + k, n - k, "%s", k ? " x " : "");
        if (c->P) k += snprintf(b + k, n - k, "SAR(%d)", c->P);
        if (c->Q) k += snprintf(b + k, n - k, "%sSMA(%d)", c->P ? "+" : "", c->Q);
    }
    if (!k) snprintf(b, n, "white noise");
    return b;
}

static void vec(FILE *f, const char *key, const double *v, int from, int to)
{
    fprintf(f, "%s", key);
    for (int i = from; i <= to; i++) fprintf(f, " %.10g", v[i]);
    fprintf(f, "\n");
}

static void write_cand(FILE *f, const char *name, const ArtOptions *o, const ArtResult *r)
{
    const ModelCandidate *m = &r->model;
    int L = m->lags_used;
    fprintf(f, "# art %s -- candidates for the identification window\n", ART_ENGINE_VERSION);
    fprintf(f, "series %s\n", name);
    fprintf(f, "transform log %d d %d D %d s %d\n", o->apply_log, o->d, o->D, o->s);
    fprintf(f, "sample n_used %d lags %d band %.10g\n", r->n_used, L, r->band);
    if (r->seasonal_tested) {
        fprintf(f, "seasonal F %.10g p %.10g detected %d s %d\n",
                r->seasonal_F, r->seasonal_p, r->seasonal_detected, r->seasonal_s);
        vec(f, "dummies", r->seasonal_dummies, 0, r->seasonal_s - 1);
    }
    if (r->unit_root_tested) {
        fprintf(f, "adf stat %.10g p %.10g crit %.10g lags %d\n",
                r->adf_stat, r->adf_p, r->adf_crit, r->adf_lags);
        fprintf(f, "kpss stat %.10g p %.10g crit %.10g lags %d\n",
                r->kpss_stat, r->kpss_p, r->kpss_crit, r->kpss_lags);
    }
    if (m->acf_empirical) vec(f, "acf", m->acf_empirical, 1, L);
    if (m->pacf_empirical) vec(f, "pacf", m->pacf_empirical, 1, L);
    for (int i = 0; i < m->n_candidates; i++) {
        const OrderCandidate *c = &m->candidates[i];
        fprintf(f, "candidate %d p %d q %d P %d Q %d sim %.10g weight %.10g aicc %.10g "
                   "scored %d proposed %d\n",
                i + 1, c->p, c->q, c->P, c->Q, c->sim, c->prob,
                c->aicc < 1e29 ? c->aicc : 0.0, c->scored, i == 0);
        if (c->p) vec(f, "phi", c->phi, 0, c->p - 1);
        if (c->q) vec(f, "theta", c->theta, 0, c->q - 1);
        if (c->P) vec(f, "Phi", c->Phi, 0, c->P - 1);
        if (c->Q) vec(f, "Theta", c->Theta, 0, c->Q - 1);
        if (c->scored) {
            vec(f, "tacf", c->acf_theoretical, 1, L);
            vec(f, "tpacf", c->pacf_theoretical, 1, L);
        }
    }
    for (int i = 0; i < r->n_messages; i++) fprintf(f, "message %s\n", r->messages[i]);
    fprintf(f, "end\n");
}

static void write_out(FILE *f, const char *data, const char *name, const ArtOptions *o,
                      const ArtResult *r, int rc)
{
    const ModelCandidate *m = &r->model;
    time_t now = time(NULL);
    char when[32];
    strftime(when, sizeof when, "%Y-%m-%d %H:%M", localtime(&now));
    fprintf(f, "ART %s -- identification of ARMA/SARIMA orders\n", ART_ENGINE_VERSION);
    fprintf(f, "%s\n\n", when);
    fprintf(f, "Series: %s (%s)\n", name, data);
    fprintf(f, "Transformation: %s, d = %d, D = %d, s = %d\n",
            o->apply_log ? "log" : "levels", o->d, o->D, o->s);
    fprintf(f, "Observations identified on: %d   lags: %d   band: +/-%.4f\n\n",
            r->n_used, m->lags_used, r->band);

    if (r->seasonal_tested)
        fprintf(f, "Seasonality (HAC F on d=1, 100*log): F = %.4f, p = %.4f -- %s\n",
                r->seasonal_F, r->seasonal_p,
                r->seasonal_detected ? "deterministic pattern detected" : "not detected");
    if (r->unit_root_tested) {
        fprintf(f, "ADF:  statistic %.4f, p = %.4f (5%% critical %.4f, %d lags)\n",
                r->adf_stat, r->adf_p, r->adf_crit, r->adf_lags);
        fprintf(f, "KPSS: statistic %.4f, p = %.4f (5%% critical %.4f, %d lags)\n",
                r->kpss_stat, r->kpss_p, r->kpss_crit, r->kpss_lags);
    }
    if (r->seasonal_tested || r->unit_root_tested) fprintf(f, "\n");

    if (rc == ART_OK) {
        fprintf(f, "Candidates, ranked by pattern similarity (option B):\n");
        fprintf(f, "  %-3s %-16s %-24s %8s %8s %12s\n", "#", "(p,q)(P,Q)", "kind",
                "sim", "weight", "AICc (CSS)");
        for (int i = 0; i < m->n_candidates; i++) {
            const OrderCandidate *c = &m->candidates[i];
            char ord[32], kb[48];
            snprintf(ord, sizeof ord, "(%d,%d)(%d,%d)", c->p, c->q, c->P, c->Q);
            if (c->scored)
                fprintf(f, "  %-3d %-16s %-24s %8.4f %8.4f %12.3f%s\n", i + 1, ord, kind(c, kb, sizeof kb),
                        c->sim, c->prob, c->aicc, i == 0 ? "   <- proposed" : "");
            else
                fprintf(f, "  %-3d %-16s %-24s %8s %8s %12s\n", i + 1, ord, kind(c, kb, sizeof kb),
                        "-", "-", "not scored");
        }
        fprintf(f, "\nsim: similarity of the empirical ACF/PACF to the candidate's theoretical\n"
                   "one. weight: Akaike weight of a CONDITIONAL (CSS) AICc, comparable between\n"
                   "these candidates only. The proposal is evidence, not a verdict: the model\n"
                   "is chosen by the analyst and estimated by fue.\n");
    }
    if (r->n_messages) {
        fprintf(f, "\nMessages:\n");
        for (int i = 0; i < r->n_messages; i++) fprintf(f, "  %s\n", r->messages[i]);
    }
}

int main(int argc, char **argv)
{
    const char *data = NULL;
    int s = 0, log_ = 0, d = 0, D = 0, p = -1, q = -1, P = -1, Q = -1;
    int harm = ART_HARM_AUTO, tests = 1, classic = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        #define NEXT_INT(v) do { if (++i >= argc) { usage(stderr); return 1; } \
                                 v = atoi(argv[i]); } while (0)
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(stdout); return 0; }
        else if (!strcmp(a, "--version")) { printf("art %s\n", ART_ENGINE_VERSION); return 0; }
        else if (!strcmp(a, "-s")) NEXT_INT(s);
        else if (!strcmp(a, "-l")) log_ = 1;
        else if (!strcmp(a, "-d")) NEXT_INT(d);
        else if (!strcmp(a, "-D")) NEXT_INT(D);
        else if (!strcmp(a, "-p")) NEXT_INT(p);
        else if (!strcmp(a, "-q")) NEXT_INT(q);
        else if (!strcmp(a, "-P")) NEXT_INT(P);
        else if (!strcmp(a, "-Q")) NEXT_INT(Q);
        else if (!strcmp(a, "--no-tests")) tests = 0;
        else if (!strcmp(a, "--classic")) classic = 1;
        else if (!strcmp(a, "--harmonics")) {
            if (++i >= argc) { usage(stderr); return 1; }
            if (!strcmp(argv[i], "auto")) harm = ART_HARM_AUTO;
            else if (!strcmp(argv[i], "on")) harm = ART_HARM_ON;
            else if (!strcmp(argv[i], "off")) harm = ART_HARM_OFF;
            else { usage(stderr); return 1; }
        }
        else if (a[0] == '-') { fprintf(stderr, "art: unknown option %s\n", a); usage(stderr); return 1; }
        else if (!data) data = a;
        else { fprintf(stderr, "art: more than one data file\n"); return 1; }
    }
    if (!data) { usage(stderr); return 1; }

    DtDatos *dt = malloc(sizeof *dt);
    DtError err;
    if (!dt) return 4;
    if (dt_leer(data, dt, &err) != 0) {
        char msg[160];
        fprintf(stderr, "art: %s: %s\n", data, dt_error_en(&err, msg, sizeof msg));
        free(dt);
        return 1;
    }
    if (dt->freq > 0) {
        if (s > 0 && s != dt->freq)
            fprintf(stderr, "art: %s declares frequency %d; -s %d ignored\n", data, dt->freq, s);
        s = dt->freq;
    }
    if (s <= 0) s = 1;

    ArtOptions o;
    art_default_options(&o, s);
    o.apply_log = log_; o.d = d; o.D = D;
    if (p >= 0) o.p_max = p;
    if (q >= 0) o.q_max = q;
    if (P >= 0) o.P_max = P;
    if (Q >= 0) o.Q_max = Q;
    o.harmonics = harm;
    o.run_tests = tests;
    o.mlp_direct = !classic;
    o.quiet = 1;

    int n = dt_nobs(dt, 0);
    char name[DT_NOMBRE + 1];
    snprintf(name, sizeof name, "%s", dt->nombre[0][0] ? dt->nombre[0] : "series");
    if (!dt->nombre[0][0]) {
        char b[DT_NOMBRE + 1];
        if (ruta_nombre(data, b, sizeof b) == 0)   /* without directory and extension */
            snprintf(name, sizeof name, "%s", b);
    }

    ArtResult r;
    int rc = art_identify(dt->v[0], n, &o, &r);
    free(dt);

    if (rc == ART_E_ARGS || rc == ART_E_DATA) {
        for (int i = 0; i < r.n_messages; i++) fprintf(stderr, "%s\n", r.messages[i]);
        art_free_result(&r);
        return 2;
    }

    char base[1024], fout[1100], fcand[1100];
    if (ruta_sin_ext(data, base, sizeof base) != 0) { art_free_result(&r); return 4; }
    snprintf(fout, sizeof fout, "%s_art.out", base);
    snprintf(fcand, sizeof fcand, "%s_art.cand", base);

    FILE *fo = fopen(fout, "w");
    if (!fo) { fprintf(stderr, "art: cannot write %s\n", fout); art_free_result(&r); return 4; }
    write_out(fo, data, name, &o, &r, rc);
    fclose(fo);
    if (rc == ART_OK) {
        FILE *fc = fopen(fcand, "w");
        if (!fc) { fprintf(stderr, "art: cannot write %s\n", fcand); art_free_result(&r); return 4; }
        write_cand(fc, name, &o, &r);
        fclose(fc);
    }
    art_free_result(&r);
    if (rc == ART_E_NOTHING) return 3;
    if (rc != ART_OK) return 4;
    printf("art: %s, %s\n", fout, fcand);
    return 0;
}
