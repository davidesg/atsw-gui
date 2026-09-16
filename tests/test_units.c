/* test_units.c -- what the GUI does without a window: the name it derives
 * from what the user types, and the way it runs the engines.
 *
 * Run it with tests/run_tests.sh (make check).                             */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <glib/gstdio.h>

#include "engine.h"
#include "utils.h"
#include "inpcheck.h"
#include "outfile.h"

static int fails = 0;
static int checks = 0;

static void check(int ok, const char *fmt, ...) {
    va_list ap;
    checks++;
    if (ok) return;
    fails++;
    printf("FAIL: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* ------------------------------------------------------------------------ */
/* The name that goes in the .inp and gives the files their name             */

static void test_token_name(void) {
    static const struct { const char *in, *out; } cases[] = {
        { "D1",               "D1"               },
        { "ES_CPI",           "ES_CPI"           },  /* the _ has to survive */
        { "forecast_DE.3.1",  "forecast_DE.3.1"  },  /* and the . as well    */
        { "IPC ES",           "IPCES"            },  /* the space, out       */
        { "  ",               ""                 },  /* this used to hang    */
        { ";",                ""                 },
        { "a; rm -rf /tmp",   "arm-rftmp"        },  /* nothing to a shell   */
        { "",                 ""                 },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *got = token_name(cases[i].in);
        check(strcmp(got, cases[i].out) == 0,
              "token_name(\"%s\") = \"%s\", expected \"%s\"",
              cases[i].in, got, cases[i].out);
        g_free(got);
    }
    /* NULL must not blow up */
    {
        char *got = token_name(NULL);
        check(got && *got == '\0', "token_name(NULL) should be the empty string");
        g_free(got);
    }
}

/* ------------------------------------------------------------------------ */
/* Running the engine: the exit status, and what it said                     */

static void test_engine_status(void) {
    struct { const char *arg; int status; int wrote; const char *in_message; } cases[] = {
        { "ok",     0, 1, "finished"              },
        { "nofile", 1, 0, "Error opening input file" },
        { "bad",    2, 0, "the file ends too soon"   },
        { "noest",  3, 1, "initial values"        },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        EngineResult r = engine_run(NULL, "fue", cases[i].arg, NULL);
        check(r.status == cases[i].status,
              "fue %s: status %d, expected %d", cases[i].arg, r.status, cases[i].status);
        check(engine_wrote_results(&r) == (cases[i].wrote != 0),
              "fue %s: wrote_results %d, expected %d", cases[i].arg,
              engine_wrote_results(&r), cases[i].wrote);
        check(r.message && strstr(r.message, cases[i].in_message) != NULL,
              "fue %s: the message says \"%s\", it should carry \"%s\"",
              cases[i].arg, r.message ? r.message : "(null)", cases[i].in_message);
        engine_result_clear(&r);
    }
}

static void test_engine_signal(void) {
    EngineResult r = engine_run(NULL, "fue", "crash", NULL);

    check(r.status == ENGINE_SIGNAL, "a crash should be reported as a signal, not as %d",
          r.status);
    check(r.message && strstr(r.message, "signal") != NULL,
          "the message of a crash should name the signal: \"%s\"",
          r.message ? r.message : "(null)");
    engine_result_clear(&r);
}

static void test_engine_not_found(void) {
    EngineResult r = engine_run(NULL, "no-such-engine-at-all", "x", NULL);

    check(r.status == ENGINE_NORUN, "a program that is not there should be ENGINE_NORUN, not %d",
          r.status);
    check(!engine_wrote_results(&r), "a program that is not there wrote nothing");
    engine_result_clear(&r);
}

/* One argument is ONE argument: through the shell, "IPC ES" became two, and
 * "a; rm -rf" became a second command.                                     */
static void test_engine_argv(void) {
    EngineResult r = engine_run(NULL, "fue", "argv", "IPC ES", NULL);

    check(r.output && strncmp(r.output, "2\n", 2) == 0,
          "the engine should get 2 arguments, it got: %s",
          r.output ? r.output : "(nothing)");
    check(r.output && strstr(r.output, "argvIPC ES") != NULL,
          "the space should reach the engine inside the argument: %s",
          r.output ? r.output : "(nothing)");
    engine_result_clear(&r);
}

/* ------------------------------------------------------------------------ */
/* El avance: el optimizador escribe la iteracion y el valor de la funcion   */
/* todas seguidas y sin salto de linea, asi que el lector tiene que sacarlas */
/* del chorro de bytes segun llegan.                                        */

typedef struct { int n, last_k; double last_f; GMainLoop *loop; int status; } Seen;

static void seen_iteration(int k, double f, gpointer data) {
    Seen *s = data;

    s->n++;
    s->last_k = k;
    s->last_f = f;
}

static void seen_done(const EngineResult *r, gpointer data) {
    Seen *s = data;

    s->status = r->status;
    g_main_loop_quit(s->loop);
}

static void test_engine_progress(void) {
    const char *args[] = { "iter", NULL };
    Seen s = { 0, -1, 0.0, NULL, -99 };

    s.loop = g_main_loop_new(NULL, FALSE);
    check(engine_run_async(NULL, "fue", args, seen_iteration, seen_done, &s),
          "el motor deberia poder lanzarse");
    g_main_loop_run(s.loop);
    g_main_loop_unref(s.loop);

    check(s.n == 13, "el motor conto 13 iteraciones (0..12), se leyeron %d", s.n);
    check(s.last_k == 12, "la ultima es la 12, se leyo la %d", s.last_k);
    check(s.last_f > 0.911 && s.last_f < 0.913,
          "y su valor 0.912, se leyo %.10f", s.last_f);
    check(s.status == 0, "y acabo bien, no con %d", s.status);
}

/* ------------------------------------------------------------------------ */
/* Antes de leerlo: el .inp que le dan al GUI es de fue, de fuf, o no vale   */

static void test_inp_check(const char *dir) {
    char  why[512];
    gchar *model    = g_build_filename(dir, "D1.inp",  NULL);   /* de fue  */
    gchar *forecast = g_build_filename(dir, "S.3.inp", NULL);   /* de fuf  */
    gchar *broken   = g_build_filename(dir, "roto.inp", NULL);
    gchar *text     = NULL;
    gsize  len      = 0;

    check(inp_check_fue(model, why, sizeof(why)) == 0,
          "D1.inp es un modelo y fue deberia aceptarlo: %s", why);
    check(inp_check_fuf(model, why, sizeof(why)) != 0,
          "D1.inp no es un fichero de previsiones y fuf deberia rechazarlo");

    check(inp_check_fuf(forecast, why, sizeof(why)) == 0,
          "S.3.inp es de previsiones y fuf deberia aceptarlo: %s", why);
    check(inp_check_fue(forecast, why, sizeof(why)) != 0,
          "S.3.inp es de previsiones: fue tiene que rechazarlo -- el lector del "
          "GUI se llevaba el monton por delante con el");
    if (inp_check_fue(forecast, why, sizeof(why)) != 0)
        check(strstr(why, "input file of fuf") != NULL,
              "y decir de quien es: \"%s\"", why);

    /* un fichero cortado por la mitad */
    if (g_file_get_contents(model, &text, &len, NULL)) {
        gsize half = len / 3;
        if (g_file_set_contents(broken, text, half, NULL))
            check(inp_check_fue(broken, why, sizeof(why)) != 0,
                  "un .inp cortado tiene que rechazarse");
        g_unlink(broken);
        g_free(text);
    }

    check(inp_check_fue("no-existe.inp", why, sizeof(why)) != 0,
          "un fichero que no esta tiene que rechazarse");

    g_free(model); g_free(forecast); g_free(broken);
}

/* ------------------------------------------------------------------------ */
/* Como acabo la estimacion, sacado del .out                                 */

static void test_convergence(const char *dir) {
    gchar *path = g_build_filename(dir, "conv.out", NULL);
    gchar *how;

    g_file_set_contents(path,
        "Observations: 216.\n"
        "Parameters  : 12.\n"
        "\n"
        "**** GRADIENT STOPPING CRITERIUM SATISFIED TO WITHIN TOLERANCE LIMITS\n"
        "**** CONVERGENCE OBTAINED AFTER 20 ITERATIONS [GRADIENT NORM = 0.0000]\n"
        "\n", -1, NULL);
    how = convergence_of(path);
    check(how != NULL, "la convergencia tiene que salir del .out");
    if (how != NULL) {
        check(strstr(how, "20 ITERATIONS") != NULL,
              "con el numero de iteraciones: \"%s\"", how);
        check(strstr(how, "GRADIENT STOPPING") != NULL,
              "y con el criterio de parada: \"%s\"", how);
    }
    g_free(how);

    /* uno que no llego a converger */
    g_file_set_contents(path,
        "**** ITERATION LIMIT REACHED\n"
        "**** CONVERGENCE OBTAINED AFTER 500 ITERATIONS [GRADIENT NORM = 1.3000]\n", -1, NULL);
    how = convergence_of(path);
    check(how != NULL && strstr(how, "ITERATION LIMIT") != NULL,
          "el limite de iteraciones tiene que decirse: \"%s\"", how ? how : "(null)");
    g_free(how);

    /* un .out sin esas lineas */
    g_file_set_contents(path, "nada que ver\n", -1, NULL);
    how = convergence_of(path);
    check(how == NULL, "un .out sin lineas **** no dice nada de convergencia");
    g_free(how);

    g_unlink(path);
    g_free(path);
}

int main(int argc, char **argv) {
    test_token_name();
    test_engine_status();
    test_engine_signal();
    test_engine_not_found();
    test_engine_argv();
    test_engine_progress();
    if (argc > 1) test_inp_check(argv[1]);
    test_convergence(g_get_tmp_dir());

    printf("\n%d checks, %d failures\n", checks, fails);
    return fails == 0 ? 0 : 1;
}
