/*
 * series.c -- las series cargadas, y sus papeles. Ver series.h.
 */

#include <string.h>
#include <math.h>

#include "series.h"
#include "fue_pre_reader.h"
#include "dates.h"

/* nlatools.c del motor referencia este global, que en drtran lo inicializa
 * main() con cmacheps(). El GUI no estima -- lee .pre y prepara lo que el
 * motor va a necesitar -- pero el enlazador lo pide igual. Se define aqui,
 * en la unidad que siempre se enlaza, con el valor que le pondria el motor. */
real macheps = 2.220446049250313e-16;

/* ------------------------------------------------------------------------ */
/* Cargar                                                                    */
/* ------------------------------------------------------------------------ */

Serie *serie_cargar(const char *path, char *why, size_t size)
{
    Serie *s;
    int    rc;

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_snprintf(why, size, "no esta %s", path);
        return NULL;
    }

    s = g_new0(Serie, 1);
    s->path = g_strdup(path);

    /* EL LECTOR DEL MOTOR, tal cual. Lo que el acepte es lo que acepta
     * drtran, por construccion y no por parecido.                        */
    rc = read_fue_pre(path, &s->tm, &s->ts, &s->datamat);
    if (rc != 0) {
        g_snprintf(why, size,
                   "el lector de drtran no lo acepta (codigo %d). "
                   "Un .pre viene de estimar con fue: si es un .inp, "
                   "estimalo primero.", rc);
        g_free(s->path);
        g_free(s);
        return NULL;
    }

    s->operador = serie_operador(s);
    return s;
}

void serie_libre(Serie *s)
{
    if (!s) return;
    free_fue_pre(&s->tm, &s->ts, s->datamat);
    g_free(s->operador);
    g_free(s->path);
    g_free(s);
}

/* ------------------------------------------------------------------------ */
/* Fechas                                                                    */
/* ------------------------------------------------------------------------ */

/* La observacion obs (1..nobs) como "03/1976", o "12" si la serie no esta
 * fechada (frecuencia `number`).                                           */
gchar *serie_fecha(const Serie *s, int obs)
{
    int per, sub;

    if (s->ts.numbering || s->ts.freq <= 1)
        return g_strdup_printf("%d", s->ts.begyear + obs - 1);

    /* lib/dates, the same ObsToDate the engines use. It was written out by
     * hand here while each engine carried its own copy; those are gone.  */
    ObsToDate(s->ts.begyear, s->ts.begtime, obs, s->ts.freq, &per, &sub);
    return g_strdup_printf("%02d/%d", sub, per);
}

/* ------------------------------------------------------------------------ */
/* El operador no estacionario, legible                                      */
/* ------------------------------------------------------------------------ */

gchar *serie_operador(const Serie *s)
{
    GString *g = g_string_new(NULL);
    int i;

    for (i = 0; i < s->tm.nrdiff; i++)
        g_string_append(g, "(1-B)");
    for (i = 0; i < s->tm.nadiff; i++)
        g_string_append_printf(g, "(1-B^%d)", s->tm.sper);

    /* Los factores irreducibles de la diferencia anual. Cuando estan puestos,
     * la escuela escribe nabla nabla_4 como nrdiff=2 con ifadf=[0,1,1], que
     * es el MISMO operador que nrdiff=1, nadiff=1 -- y por eso la comparacion
     * se hace sobre el polinomio y no sobre estos enteros.                  */
    if (s->tm.ifadf) {
        int n = s->tm.sper / 2;
        for (i = 0; i <= n; i++)
            if (s->tm.ifadf[i])
                g_string_append_printf(g, "[f=%d]", i);
    }

    if (g->len == 0) g_string_append(g, "1");
    return g_string_free(g, FALSE);
}

/* ------------------------------------------------------------------------ */
/* La ventana comun                                                          */
/* ------------------------------------------------------------------------ */

/* La observacion 1 de la serie s, como un numero de periodo absoluto:
 * anio*freq + subperiodo. Sirve para comparar calendarios.                 */
static long inicio_abs(const Serie *s)
{
    return (long)s->ts.begyear * s->ts.freq + s->ts.begtime;
}

gboolean conjunto_ventana_comun(const Conjunto *c, int *desde, int *hasta,
                                char *why, size_t size)
{
    long ini = 0, fin = 0;
    int  i, freq;

    if (c->n < 2) {
        g_snprintf(why, size, "hacen falta al menos dos series");
        return FALSE;
    }

    /* La frecuencia tiene que ser la misma: cruzar una mensual con una
     * trimestral no es recortar, es otra cosa.                            */
    freq = c->s[0]->ts.freq;
    for (i = 1; i < c->n; i++)
        if (c->s[i]->ts.freq != freq) {
            g_snprintf(why, size,
                       "frecuencias distintas: %s es %d y %s es %d",
                       c->s[0]->ts.name, freq,
                       c->s[i]->ts.name, c->s[i]->ts.freq);
            return FALSE;
        }

    for (i = 0; i < c->n; i++) {
        long a = inicio_abs(c->s[i]);
        long b = a + c->s[i]->ts.nobs - 1;

        if (i == 0) { ini = a; fin = b; }
        else { if (a > ini) ini = a; if (b < fin) fin = b; }
    }

    if (fin < ini) {
        g_snprintf(why, size,
                   "los calendarios no se solapan: no hay ni una observacion "
                   "que todas compartan");
        return FALSE;
    }

    for (i = 0; i < c->n; i++) {
        desde[i] = (int)(ini - inicio_abs(c->s[i])) + 1;
        hasta[i] = (int)(fin - inicio_abs(c->s[i])) + 1;
    }
    return TRUE;
}

/* The engine's rule, not a second one. drtran requires the same frequency
 * and the same LAST date (lib/fuepre, BUG-2) and, on top of that, the same
 * number of observations: any series the common window would trim is a set
 * the engine rejects. The window above is still shown, because it says how
 * to rebuild the .pre files in art.                                        */
gboolean conjunto_alineado(const Conjunto *c, char *why, size_t size)
{
    struct Tseries ts[GUI_MAX_SER + 1];
    int i;

    if (c->n < 2) {
        g_snprintf(why, size, "hacen falta al menos dos series");
        return FALSE;
    }
    for (i = 0; i < c->n; i++) ts[i + 1] = c->s[i]->ts;
    if (fuepre_check_alignment(ts, c->n, why, size) != 0) return FALSE;
    for (i = 1; i < c->n; i++)
        if (c->s[i]->ts.nobs != c->s[0]->ts.nobs) {
            g_snprintf(why, size,
                       "%s tiene %d observaciones y %s %d: drtran exige la "
                       "misma ventana en todas",
                       c->s[i]->ts.name, c->s[i]->ts.nobs,
                       c->s[0]->ts.name, c->s[0]->ts.nobs);
            return FALSE;
        }
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* La compatibilidad de operadores                                           */
/* ------------------------------------------------------------------------ */

Compat conjunto_compat(const Conjunto *c, int i, int j)
{
    if (i == j) return OP_IGUALES;

    /* La MISMA comparacion que hace el motor: el polinomio, no el par
     * (nrdiff, nadiff). operators_differ_tm vive en fue_pre_reader.c
     * justamente para que el GUI pueda usarla.                           */
    if (!operators_differ_tm(&c->s[i]->tm, &c->s[j]->tm))
        return OP_IGUALES;

    /* Anidados: si uno divide al otro, el cast empotrado sigue valiendo
     * porque hay un Delta(B) que los reconcilia. La division exacta la
     * hace el motor (poly_div); aqui basta con el grado para distinguir
     * "puede que si" de "seguro que no".                                 */
    if (c->s[i]->tm.ornsop != c->s[j]->tm.ornsop)
        return OP_ANIDADOS;

    return OP_INCOMPATIBLES;
}

const char *compat_texto(Compat k)
{
    switch (k) {
    case OP_IGUALES:
        return "iguales — cast empotrado, verosimilitud exacta";
    case OP_ANIDADOS:
        return "anidados — hay Delta(B); sigue siendo exacto";
    default:
        return "INCOMPATIBLES — el motor pasa al cast por resta, "
               "y esa verosimilitud no es comparable con la otra";
    }
}
