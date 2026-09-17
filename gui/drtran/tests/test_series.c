/* test_series.c -- las series, la ventana comun y los operadores.
 *
 *   test_series <dir con .pre>
 *
 * Carga los .pre del m6 con EL LECTOR DEL MOTOR y comprueba lo que el GUI
 * aporta encima: las fechas, el operador legible, la interseccion de
 * calendarios y la compatibilidad de operadores.
 *
 * Lo importante: los numeros se contrastan con lo que dice drtran sobre los
 * mismos ficheros, no con lo que yo crea.
 */

#include <stdio.h>
#include <string.h>

#include "series.h"

static int fallos = 0;

static void check(int ok, const char *que, const char *visto)
{
    if (ok) return;
    fallos++;
    printf("FAIL: %s; se vio: \"%s\"\n", que, visto ? visto : "(nada)");
}

int main(int argc, char **argv)
{
    Conjunto c = { 0 };
    char     why[512];
    gchar   *d, *h;
    int      desde[GUI_MAX_SER], hasta[GUI_MAX_SER];
    const char *nombres[] = { "M6_EP", "M6_EI", "M6_EU", "M6_EC", "M6_EA", "M6_P" };
    int      i;

    if (argc < 2) { fprintf(stderr, "uso: test_series <dir>\n"); return 2; }

    /* --- cargar los seis, en el orden del m6 --------------------------- */
    for (i = 0; i < 6; i++) {
        gchar *p = g_strdup_printf("%s/%s.pre", argv[1], nombres[i]);
        Serie *s = serie_cargar(p, why, sizeof why);

        check(s != NULL, "el lector del motor tiene que leer el .pre", why);
        if (!s) { g_free(p); printf("\n%d fallos\n", fallos); return 1; }
        c.s[c.n++] = s;
        g_free(p);
    }
    printf("cargadas        : %d series con el lector de drtran\n", c.n);

    /* --- lo que dice el .pre, contrastado con el .out de drtran --------- */
    check(c.s[0]->ts.freq == 4, "el m6 es trimestral",
          g_strdup_printf("%d", c.s[0]->ts.freq));
    check(c.s[0]->ts.nobs == 69, "y son 69 observaciones en bruto",
          g_strdup_printf("%d", c.s[0]->ts.nobs));

    d = serie_fecha(c.s[0], 1);
    check(g_strcmp0(d, "03/1976") == 0,
          "la primera observacion es el tercer trimestre de 1976 "
          "(drtran imprime 'Start: 3 1976')", d);
    printf("primera obs     : %s\n", d);
    g_free(d);

    /* --- el operador no estacionario ------------------------------------ */
    printf("operadores      :");
    for (i = 0; i < c.n; i++) printf(" %s", c.s[i]->operador);
    printf("\n");
    check(c.s[0]->operador && strstr(c.s[0]->operador, "(1-B)") != NULL,
          "el m6 lleva diferencias regulares (drtran dice d=2)",
          c.s[0]->operador);

    /* --- la ventana comun ---------------------------------------------- */
    check(conjunto_ventana_comun(&c, desde, hasta, why, sizeof why),
          "las seis comparten calendario", why);
    d = serie_fecha(c.s[0], desde[0]);
    h = serie_fecha(c.s[0], hasta[0]);
    printf("ventana comun   : %s - %s  (%d obs)\n", d, h, hasta[0] - desde[0] + 1);
    check(hasta[0] - desde[0] + 1 == 69,
          "y como todas empiezan y acaban igual, no se pierde ninguna",
          g_strdup_printf("%d", hasta[0] - desde[0] + 1));
    g_free(d); g_free(h);

    /* --- una serie desfasada: la ventana comun tiene que encogerla ------ */
    {
    Serie *v = c.s[2];
    int    guardado = v->ts.begyear;

    v->ts.begyear += 2;                       /* dos anios mas tarde       */
    if (conjunto_ventana_comun(&c, desde, hasta, why, sizeof why)) {
        int n = hasta[0] - desde[0] + 1;
        printf("con 2 anios de desfase: %d obs comunes (de 69)\n", n);
        check(n == 69 - 8,
              "dos anios trimestrales son ocho observaciones menos",
              g_strdup_printf("%d", n));
    } else {
        check(0, "con dos anios de desfase todavia hay tramo comun", why);
    }
    v->ts.begyear = guardado;
    }

    /* --- la compatibilidad de operadores -------------------------------- */
    {
    int iguales = 0, malos = 0;

    for (i = 0; i < c.n; i++) {
        int j;
        for (j = i + 1; j < c.n; j++) {
            Compat k = conjunto_compat(&c, i, j);
            if (k == OP_IGUALES) iguales++;
            if (k == OP_INCOMPATIBLES) malos++;
        }
    }
    printf("compatibilidad  : %d pares iguales, %d incompatibles\n",
           iguales, malos);
    /* drtran estima el m6 con el cast EMPOTRADO y no avisa de resta, asi
     * que los seis operadores tienen que ser compatibles entre si.       */
    check(malos == 0,
          "drtran estima el m6 con el cast empotrado: ningun par puede ser "
          "incompatible", g_strdup_printf("%d incompatibles", malos));
    }

    for (i = 0; i < c.n; i++) serie_libre(c.s[i]);
    printf("\n%d fallos\n", fallos);
    return fallos ? 1 : 0;
}
