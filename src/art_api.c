/*
 * art_api.c -- art_identify(): the identifier on an array (18.2.1).
 *
 * The engine's entry point for callers with the series in memory. It sets the
 * per-call context (thread-local), runs the same search the CLI runs, and
 * collects into ArtResult what used to go to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "art.h"

_Thread_local ArtContext *art_tl_ctx = NULL;

int art_cancelled(void)
{
    return art_tl_ctx && art_tl_ctx->cancel && *art_tl_ctx->cancel;
}

/* A message for the caller (and for stdout unless quiet). */
void art_msg(const char *fmt, ...)
{
    char buf[ART_MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (art_tl_ctx && art_tl_ctx->res && art_tl_ctx->res->n_messages < ART_MAX_MSG) {
        ArtResult *r = art_tl_ctx->res;
        snprintf(r->messages[r->n_messages++], ART_MSG_LEN, "%s", buf);
    }
    if (!(art_tl_ctx && art_tl_ctx->quiet))
        fprintf(stdout, "%s\n", buf);
}

/* printf, silenced by `quiet`: the engine's sources route printf here. */
int art_log(const char *fmt, ...)
{
    if (art_tl_ctx && art_tl_ctx->quiet) return 0;
    va_list ap;
    va_start(ap, fmt);
    int k = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return k;
}

void art_default_options(ArtOptions *o, int s)
{
    memset(o, 0, sizeof *o);
    o->s = s < 1 ? 1 : s;
    o->p_max = (o->s / 2 > 3) ? o->s / 2 : 3;
    if (o->p_max > ART_MAX_P - 1) o->p_max = ART_MAX_P - 1;   /* s = 24: 12 -> 9 */
    o->q_max = 2;
    o->P_max = o->s > 1 ? 1 : 0;
    o->Q_max = o->s > 1 ? 1 : 0;
    o->mlp_direct = 1;
    o->run_tests = 1;
    o->harmonics = ART_HARM_AUTO;
}

int art_identify(const double *x, int n, const ArtOptions *o, ArtResult *res)
{
    if (!res) return ART_E_ARGS;
    memset(res, 0, sizeof *res);
    ArtContext ctx = { x, n, res, o ? o->quiet : 0, o ? o->run_tests : 1,
                       o ? o->harmonics : ART_HARM_AUTO, o ? o->cancel : NULL };
    ArtContext *saved = art_tl_ctx;
    art_tl_ctx = &ctx;

    int rc = ART_OK;
    if (!x || !o || n < 3 || o->s < 1 || o->d < 0 || o->D < 0 ||
        o->p_max < 0 || o->q_max < 0 || o->P_max < 0 || o->Q_max < 0 ||
        o->p_max >= ART_MAX_P || o->q_max >= ART_MAX_P ||
        o->P_max >= ART_MAX_SP || o->Q_max >= ART_MAX_SP || o->s > ART_MAX_S) {
        art_msg("art: bad arguments");
        rc = ART_E_ARGS;
        goto out;
    }
    if (o->apply_log)
        for (int i = 0; i < n; i++)
            if (!(x[i] > 0.0)) {
                art_msg("art: log asked but observation %d is %g (not positive)", i + 1, x[i]);
                rc = ART_E_DATA;
                goto out;
            }
    if (n - o->d - o->D * o->s < 10) {
        art_msg("art: too few observations after differencing (%d)", n - o->d - o->D * o->s);
        rc = ART_E_DATA;
        goto out;
    }

    DataParameters prm;
    memset(&prm, 0, sizeof prm);
    prm.apply_log = o->apply_log;
    prm.d = o->d;
    prm.D = o->D;
    prm.s = o->s;
    prm.mlp_direct = o->mlp_direct;

    if (o->on_progress) set_progress_callback(o->on_progress);
    int ok = ejecutar_deteccion_automatica(NULL, &prm, o->p_max, o->q_max,
                                           o->P_max, o->Q_max, &res->model);
    if (o->on_progress) set_progress_callback(NULL);

    if (art_cancelled()) { rc = ART_E_CANCELLED; goto out; }
    if (!ok || res->model.n_candidates == 0) { rc = ART_E_NOTHING; goto out; }
    res->n_used = n - o->d - o->D * o->s;
    res->band = 1.96 / sqrt((double)res->n_used);

out:
    art_tl_ctx = saved;
    return rc;
}

void art_free_result(ArtResult *res)
{
    if (!res) return;
    liberar_model_candidate(&res->model);
}
