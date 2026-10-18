/* probe_rng_width.c -- BUG-61: drvec's two random generators, the bootstrap's
 * (boot_rng / boot_normal) and -multistart's jitter, must not depend on the
 * width of `long`.
 *
 * Both are the 64-bit LCG  s <- s * 6364136223846793005 + 1442695040888963407
 * with the draw taken from bits 33..63.  Written with `unsigned long`, that is
 * a 64-bit generator on LP64 (Linux, macOS) and a different, broken one on
 * LLP64 (Windows, where `long` is 32 bits): the state is truncated to 32 bits
 * and `>> 33` is wider than the type.  The Windows build hangs in boot_normal's
 * rejection loop (drvec-python, bugs/drvec-c-rng-long32).
 *
 * The functions are static in drvec.c, so repro.sh extracts their text from
 * src/drvec.c into rng_boot.h, rng_jit_decl.h, rng_jit_step.h and
 * rng_jit_draw.h, and this probe compiles that text with `long` redefined as
 * `int` -- i.e. as LLP64 sees it.  It compares the draws with an independent
 * 64-bit reference written with uint64_t.  A width-independent source gives
 * the reference sequence bit for bit; the defective one hangs or gives another
 * sequence.  Exit 1 while the defect is open, 0 when it is fixed.
 *
 *   (see tests/repro/repro.sh 61)
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
typedef double real;

#define NDRAW 4000

/* ---- the reference: the LP64 generator, written with uint64_t ---------- */
static uint64_t ref_s = 987654321ULL;
static real ref_normal(void)
{
    static int have = 0;
    static real spare = 0.0;
    real u1, u2, r, th;
    if (have) { have = 0; return spare; }
    do {
        ref_s = ref_s * 6364136223846793005ULL + 1442695040888963407ULL;
        u1 = ((real) ((ref_s >> 33) & 0x7FFFFFFF)) / 2147483648.0;
    } while (u1 <= 1.0e-12);
    ref_s = ref_s * 6364136223846793005ULL + 1442695040888963407ULL;
    u2 = ((real) ((ref_s >> 33) & 0x7FFFFFFF)) / 2147483648.0;
    r  = sqrt(-2.0 * log(u1));
    th = 2.0 * M_PI * u2;
    spare = r * sin(th); have = 1;
    return r * cos(th);
}
static void ref_jit(real *out, int n)
{
    uint64_t s = 20260818ULL;
    int k;
    for (k = 0; k < n; k++) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        out[k] = ((real) ((s >> 33) & 0x7FFFFFFF)) / 2147483647.0;
    }
}

/* ---- drvec.c's text, compiled as LLP64 sees it ------------------------- */
#define long int
#include "rng_boot.h"
static void src_jit(real *out, int n)
{
#include "rng_jit_decl.h"
    real u;
    int k;
    for (k = 0; k < n; k++) {
#include "rng_jit_step.h"
#include "rng_jit_draw.h"
        out[k] = u;
    }
}
#undef long

static void hung(int sig)
{
    (void) sig;
    static const char m[] = "  boot_normal does not return: the rejection loop never ends (HANGS)\n";
    write(1, m, sizeof m - 1);
    _exit(1);
}

int main(void)
{
    static real a[NDRAW], b[NDRAW];
    int k, bad_boot = 0, bad_jit = 0;

    printf("  boot_rng, compiled with a 32-bit long: %d bits of state\n",
           (int) (8 * sizeof boot_rng));
    fflush(stdout);
    signal(SIGALRM, hung);
    alarm(5);
    for (k = 0; k < NDRAW; k++) {
        real x = boot_normal(), y = ref_normal();
        if (x != y && !bad_boot) {
            bad_boot = k + 1;
            printf("  bootstrap draw %d: %.17g, the 64-bit generator gives %.17g\n", k + 1, x, y);
        }
    }
    alarm(0);
    src_jit(a, NDRAW);
    ref_jit(b, NDRAW);
    for (k = 0; k < NDRAW; k++)
        if (a[k] != b[k]) {
            bad_jit = k + 1;
            printf("  jitter draw %d: %.17g, the 64-bit generator gives %.17g\n", k + 1, a[k], b[k]);
            break;
        }
    printf("  bootstrap: %s;  multistart jitter: %s  (%d draws each)\n",
           bad_boot ? "DIFFERS" : "same as 64-bit", bad_jit ? "DIFFERS" : "same as 64-bit", NDRAW);
    return (bad_boot || bad_jit) ? 1 : 0;     /* 1 while the defect is open */
}
