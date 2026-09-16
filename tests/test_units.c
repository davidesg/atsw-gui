/* test_units.c -- what the GUI does without a window: the name it derives
 * from what the user types, and the way it runs the engines.
 *
 * Run it with tests/run_tests.sh (make check).                             */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "engine.h"
#include "utils.h"

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

int main(void) {
    test_token_name();
    test_engine_status();
    test_engine_signal();
    test_engine_not_found();
    test_engine_argv();

    printf("\n%d checks, %d failures\n", checks, fails);
    return fails == 0 ? 0 : 1;
}
