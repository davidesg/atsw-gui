/* api_check.c -- art_identify() on arrays (18.2.1).
 *
 *   api_check FILE S D LOG    print the shortlist line as the CLI does
 *   api_check --selftest      errors, s = 24, cancel, two threads
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "art.h"

static double *read_file(const char *f, int *n)
{
    FILE *fp = fopen(f, "r");
    if (!fp) return NULL;
    int cap = 1024; double *x = malloc(cap * sizeof *x); *n = 0;
    while (fscanf(fp, "%lf", &x[*n]) == 1) if (++*n == cap) x = realloc(x, (cap *= 2) * sizeof *x);
    fclose(fp);
    return x;
}

static void shortlist_line(const ArtResult *r, char *buf, size_t sz)
{
    int k = snprintf(buf, sz, "MLP shortlist (%d):", r->model.n_candidates);
    for (int i = 0; i < r->model.n_candidates; i++) {
        const OrderCandidate *c = &r->model.candidates[i];
        k += snprintf(buf + k, sz - k, " (%d,%d)(%d,%d) sim=%.3f wAICc=%.3f",
                      c->p, c->q, c->P, c->Q, c->sim, c->prob);
    }
}

typedef struct { const double *x; int n; char line[4096]; int rc; } Job;

static void *job(void *a)
{
    Job *j = a;
    ArtOptions o; art_default_options(&o, 12);
    o.apply_log = 1; o.d = 1; o.quiet = 1;
    ArtResult r;
    j->rc = art_identify(j->x, j->n, &o, &r);
    shortlist_line(&r, j->line, sizeof j->line);
    art_free_result(&r);
    return NULL;
}

static int selftest(void)
{
    int bad = 0, n;
    double *x = read_file("data/wti.txt", &n);
    ArtOptions o; ArtResult r;

    /* log of a non-positive value is an error */
    double neg[50]; for (int i = 0; i < 50; i++) neg[i] = (i == 7) ? 0.0 : 10.0 + i;
    art_default_options(&o, 1); o.apply_log = 1; o.quiet = 1;
    int rc = art_identify(neg, 50, &o, &r);
    printf("log of 0: rc=%d (want %d) msg=\"%s\"\n", rc, ART_E_DATA, r.n_messages ? r.messages[0] : "");
    bad += rc != ART_E_DATA; art_free_result(&r);

    /* s = 24 runs (the seasonal test stopped at 12) */
    double *y = malloc(400 * sizeof *y); srand(3);
    for (int i = 0; i < 400; i++) y[i] = 100 + 0.05 * i + 2 * sin(2 * M_PI * i / 24) + (rand() / (double)RAND_MAX - 0.5);
    art_default_options(&o, 24); o.d = 1; o.quiet = 1;
    rc = art_identify(y, 400, &o, &r);
    printf("s=24: rc=%d tested=%d detected=%d F=%.2f n_cand=%d\n", rc, r.seasonal_tested,
           r.seasonal_detected, r.seasonal_F, r.model.n_candidates);
    bad += rc != ART_OK || !r.seasonal_tested; art_free_result(&r);

    /* cancel */
    volatile int stop = 1;
    art_default_options(&o, 12); o.apply_log = 1; o.d = 1; o.quiet = 1; o.cancel = &stop;
    rc = art_identify(x, n, &o, &r);
    printf("cancel: rc=%d (want %d)\n", rc, ART_E_CANCELLED);
    bad += rc != ART_E_CANCELLED; art_free_result(&r);

    /* the result carries the tests and per-candidate theory */
    art_default_options(&o, 12); o.apply_log = 1; o.d = 1; o.quiet = 1;
    rc = art_identify(x, n, &o, &r);
    const OrderCandidate *c0 = &r.model.candidates[0];
    printf("wti: rc=%d seasonal F=%.4f p=%.4f ADF=%.4f KPSS=%.4f lags=%d band=%.4f msgs=%d\n",
           rc, r.seasonal_F, r.seasonal_p, r.adf_stat, r.kpss_stat, r.model.lags_used, r.band, r.n_messages);
    printf("     top (%d,%d)(%d,%d) scored=%d aicc=%.3f phi1=%.4f acf_th[1]=%.4f acf_emp[1]=%.4f\n",
           c0->p, c0->q, c0->P, c0->Q, c0->scored, c0->aicc, c0->phi[0], c0->acf_theoretical[1],
           r.model.acf_empirical ? r.model.acf_empirical[1] : NAN);
    bad += rc != ART_OK || !r.unit_root_tested || !c0->scored; art_free_result(&r);

    /* two threads at once give the serial answer */
    Job a = { x, n, "", 0 }, b = { x, n, "", 0 }, s0 = { x, n, "", 0 };
    job(&s0);
    pthread_t ta, tb;
    pthread_create(&ta, NULL, job, &a); pthread_create(&tb, NULL, job, &b);
    pthread_join(ta, NULL); pthread_join(tb, NULL);
    int same = !strcmp(a.line, s0.line) && !strcmp(b.line, s0.line);
    printf("threads: %s\n", same ? "same as serial" : "DIFFERENT");
    bad += !same;

    free(x); free(y);
    printf("%s\n", bad ? "SELFTEST FAILED" : "SELFTEST OK");
    return bad != 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc < 5) { fprintf(stderr, "usage: api_check FILE S D LOG\n"); return 2; }
    int n; double *x = read_file(argv[1], &n);
    ArtOptions o; art_default_options(&o, atoi(argv[2]));
    o.d = atoi(argv[3]); o.apply_log = atoi(argv[4]); o.quiet = 1;
    ArtResult r;
    int rc = art_identify(x, n, &o, &r);
    char line[4096]; shortlist_line(&r, line, sizeof line);
    printf("%s\n", line);
    art_free_result(&r); free(x);
    return rc;
}
