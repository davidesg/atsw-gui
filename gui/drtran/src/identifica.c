/*
 * identifica.c -- la pantalla de identificacion de un enlace.
 *
 * Lo que se mira para decidir (b, r, s) de una entrada, y lo que se mira para
 * decidir si esa entrada puede ser una entrada.
 *
 * TRES COSAS, Y NINGUNA ES CODIGO NUEVO:
 *
 *   la CCF preblanqueada   lib/prewhiten, que es el nucleo que estaba dentro
 *                          de drtran.c: el mismo filtro, los mismos numeros
 *   el dibujo              lib/ccfplot, con el formato de los dos prototipos
 *                          aprobados, y lib/preview para verlo en pantalla
 *   la ecuacion            lib/equation, la de fue con la transferencia
 *                          delante
 *
 * POR QUE PREBLANQUEADA. La CCF de las series crudas no identifica: la
 * autocorrelacion de cada una se propaga a la cruzada y la ensucia entera.
 * Sobre el m6, la CCF cruda de EI contra EP da P(60) = 944, y eso no es senal.
 * Preblanquear exige el modelo univariante de cada serie, que es exactamente
 * lo que trae un .pre: aqui es donde la escalera se paga sola.
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "gui.h"
#include "previewhost.h"
#include "preview.h"
#include "prewhiten.h"
#include "ccfplot.h"
#include "eqtran.h"

#define MAX_LAGS  64

/* diagnose.c escribe su informe a este global, que en el motor es el .out. El
 * GUI no quiere informe: se lo lleva stderr y lo que se enseña es el grafico.
 * El global tiene que existir de todos modos, porque diagnose.c es el mismo
 * fichero del motor y aqui no se copia ni se recorta.                      */
FILE *outputv = NULL;

/* ------------------------------------------------------------------------ */
/* Lo que el host le debe a lib/preview                                      */
/* ------------------------------------------------------------------------ */

void preview_open_external(PreviewApp *app, const gchar *path)
{
    gchar *uri = g_filename_to_uri(path, NULL, NULL);

    if (uri) {
        gtk_show_uri_on_window(GTK_WINDOW(app->ventana_p), uri,
                               GDK_CURRENT_TIME, NULL);
        g_free(uri);
    }
}

void preview_show_status(PreviewApp *app, const gchar *format, ...)
{
    va_list ap;
    gchar  *s;

    va_start(ap, format);
    s = g_strdup_vprintf(format, ap);
    va_end(ap);
    gtk_label_set_text(GTK_LABEL(app->estado), s);
    g_free(s);
}

/* ------------------------------------------------------------------------ */
/* La CCF del enlace marcado                                                 */
/* ------------------------------------------------------------------------ */

/* Calcula. Devuelve TRUE y deja la CCF en id->ccf, o FALSE con el motivo en
 * la barra de estado. */
static gboolean calcula(Mtram *m, Ident *id)
{
    Serie *sal, *ent;
    real **res = NULL;
    real   Q = 0.0, p = 0.0;
    char   why[512] = "";

    id->vale = FALSE;
    if (m->c.n < 2) return FALSE;
    if (id->entrada < 1 || id->entrada >= m->c.n) id->entrada = 1;

    sal = m->c.s[0];
    ent = m->c.s[id->entrada];

    id->nlags = prewhiten_nlags(sal->ts.nobs);
    if (id->nlags > MAX_LAGS) id->nlags = MAX_LAGS;

    if (prewhiten_ccf(&ent->tm, &ent->ts, ent->datamat,
                      &sal->tm, &sal->ts, sal->datamat,
                      id->nlags, id->ccf, id->nu, &id->n,
                      &res, why, sizeof why) != 0) {
        preview_show_status(m, "%s", why[0] ? why : "no pude preblanquear");
        return FALSE;
    }

    /* El portmanteau multivariante de Hosking, con la rutina del motor y
     * sobre los residuos PREBLANQUEADOS, que es donde significa algo.      */
    hosking_test(res, id->n, 2, id->nlags, &Q, &p);
    free_matrix(res, 1, id->n, 1, 2);

    id->Q  = (double) Q;
    id->df = 4 * id->nlags;          /* m^2 (k - p - q), m = 2, sin ajustar */
    id->banda = 2.0 / sqrt((double) id->n);
    id->vale = TRUE;

    if (why[0]) preview_show_status(m, "%s", why);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* La lectura: (b, r, s) y la exogeneidad                                    */
/* ------------------------------------------------------------------------ */

/* Primer retardo positivo que sale de la banda: el b que propone el grafico. */
static int b_propuesto(const Ident *id)
{
    int k;

    for (k = 0; k <= id->nlags; k++)
        if (fabs(id->ccf[id->nlags + k]) > id->banda) return k;
    return -1;
}

/* Cuantos retardos NEGATIVOS salen de la banda: el contraste de exogeneidad.
 * Si hay, la salida antecede a la entrada y el modelo no se sostiene.      */
static int negativos_fuera(const Ident *id)
{
    int k, n = 0;

    for (k = 1; k <= id->nlags; k++)
        if (fabs(id->ccf[id->nlags - k]) > id->banda) n++;
    return n;
}

static void refresca_lectura(Mtram *m, Ident *id)
{
    GString *t = g_string_new(NULL);
    int      b, neg, k, ultimo = -1;

    if (!id->vale) {
        gtk_label_set_text(GTK_LABEL(id->lectura),
            "Marca una entrada. Hace falta la salida y al menos una entrada.");
        g_string_free(t, TRUE);
        return;
    }

    b   = b_propuesto(id);
    neg = negativos_fuera(id);
    for (k = 0; k <= id->nlags; k++)
        if (fabs(id->ccf[id->nlags + k]) > id->banda) ultimo = k;

    g_string_append_printf(t,
        "%d observaciones estacionarias, %d retardos, banda ±%.3f\n\n",
        id->n, id->nlags, id->banda);

    /* --- los positivos: la transferencia --- */
    if (b < 0)
        g_string_append(t,
            "Ningún retardo positivo sale de la banda: la CCF preblanqueada "
            "no ve transferencia.\n");
    else {
        g_string_append_printf(t,
            "Transferencia: el primer retardo significativo es k = %d, "
            "y el último, k = %d.\n"
            "   propuesta:  b = %d   s = %d   r = 0\n",
            b, ultimo, b, ultimo - b);
        if (ultimo - b >= 3)
            g_string_append(t,
                "   (una cola larga suele ser r = 1 con pocos ω, no un s "
                "grande: mira si decae geométricamente)\n");
    }

    /* --- los negativos: la exogeneidad --- */
    g_string_append_c(t, '\n');
    if (neg == 0)
        g_string_append(t,
            "Exogeneidad: ningún retardo negativo fuera de la banda. "
            "La entrada puede tratarse como exógena.\n");
    else
        g_string_append_printf(t,
            "OJO — exogeneidad: %d retardo%s negativo%s fuera de la banda. "
            "La salida antecede a la entrada.\nUn modelo de transferencia "
            "supone que la entrada NO responde a la salida; si eso no se "
            "sostiene,\nel escalón que toca es el VARMA simultáneo, no éste.\n",
            neg, neg == 1 ? "" : "s", neg == 1 ? "" : "s");

    g_string_append_printf(t,
        "\nHosking sobre los preblanqueados:  P(%d) = %.1f\n", id->df, id->Q);

    gtk_label_set_text(GTK_LABEL(id->lectura), t->str);
    g_string_free(t, TRUE);
}

/* ------------------------------------------------------------------------ */
/* La ecuacion                                                               */
/* ------------------------------------------------------------------------ */

static void refresca_ecuacion(Mtram *m, Ident *id)
{
    EqLink lnk[GUI_MAX_SER];
    double omega[GUI_MAX_SER][8];
    char   texto[4096];
    int    j, k, n = 0;

    if (m->c.n < 2) {
        gtk_label_set_text(GTK_LABEL(id->ecuacion), "");
        return;
    }

    /* Un enlace por entrada, con los ordenes que hay puestos y ω a 1: es la
     * ESPECIFICACION que se va a estimar, no una estimacion. La ecuacion se
     * enseña antes de estimar justamente para poder mirarla antes.        */
    for (j = 1; j < m->c.n; j++) {
        Serie *e = m->c.s[j];
        int    b = 0, s = 0;

        if (j == id->entrada && id->vale) {
            int bb = b_propuesto(id), ul = -1;

            for (k = 0; k <= id->nlags; k++)
                if (fabs(id->ccf[id->nlags + k]) > id->banda) ul = k;
            if (bb >= 0) { b = bb; s = ul - bb; }
        }
        if (s > 7) s = 7;

        for (k = 0; k <= s; k++) omega[n][k] = 1.0;

        lnk[n].entrada  = e->ts.name ? e->ts.name : "?";
        lnk[n].b        = b;
        lnk[n].s        = s;
        lnk[n].r        = 0;
        lnk[n].omega    = omega[n];
        lnk[n].delta    = NULL;
        lnk[n].omega_se = NULL;
        lnk[n].delta_se = NULL;
        n++;
    }

    eqtran_texto(texto, sizeof texto,
                 m->c.s[0]->ts.name ? m->c.s[0]->ts.name : "Y", lnk, n, NULL);
    gtk_label_set_text(GTK_LABEL(id->ecuacion), texto);
}

/* ------------------------------------------------------------------------ */
/* El grafico                                                                */
/* ------------------------------------------------------------------------ */

static void on_ver_ccf(GtkButton *b, Mtram *m)
{
    Ident *id = &m->id;
    gchar *path;

    if (!id->vale) {
        preview_show_status(m, "No hay CCF que enseñar todavía.");
        return;
    }

    /* El EPS se escribe de verdad, y es el que se ve: lib/preview interpreta
     * el fichero de fugdraw, asi que la pantalla y el papel no pueden
     * discrepar.                                                          */
    path = g_build_filename(g_get_user_cache_dir(), "mtram", NULL);
    g_mkdir_with_parents(path, 0700);
    g_free(path);
    path = g_build_filename(g_get_user_cache_dir(), "mtram", "ccf.eps", NULL);

    if (ccf_write_eps(path, id->ccf, id->nlags, id->n,
                      m->c.s[id->entrada]->ts.name, m->c.s[0]->ts.name,
                      id->Q, id->df) != 0)
        preview_show_status(m, "No pude escribir %s", path);
    else if (!preview_show(m, path))
        preview_show_status(m, "No pude dibujar %s", path);

    g_free(path);
}

/* ------------------------------------------------------------------------ */

static void on_entrada(GtkComboBox *cb, Mtram *m)
{
    int i = gtk_combo_box_get_active(cb);

    if (i < 0) return;
    m->id.entrada = i + 1;              /* la 0 es la salida */
    calcula(m, &m->id);
    refresca_lectura(m, &m->id);
    refresca_ecuacion(m, &m->id);
}

void identifica_refresca(Mtram *m)
{
    Ident       *id = &m->id;
    GtkComboBoxText *cb = GTK_COMBO_BOX_TEXT(id->combo);
    int i;

    g_signal_handlers_block_by_func(id->combo, G_CALLBACK(on_entrada), m);
    gtk_combo_box_text_remove_all(cb);
    for (i = 1; i < m->c.n; i++)
        gtk_combo_box_text_append_text(cb,
            m->c.s[i]->ts.name ? m->c.s[i]->ts.name : "(sin nombre)");
    if (id->entrada < 1 || id->entrada >= m->c.n) id->entrada = 1;
    if (m->c.n > 1) gtk_combo_box_set_active(GTK_COMBO_BOX(id->combo),
                                             id->entrada - 1);
    g_signal_handlers_unblock_by_func(id->combo, G_CALLBACK(on_entrada), m);

    calcula(m, id);
    refresca_lectura(m, id);
    refresca_ecuacion(m, id);
}

GtkWidget *identifica_pagina_new(Mtram *m)
{
    Ident     *id = &m->id;
    GtkWidget *caja, *fila, *b, *marco, *vb, *sc;

    caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(caja), 8);

    /* --- que entrada --- */
    fila = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(fila), gtk_label_new("Entrada:"), FALSE, FALSE, 0);

    id->combo = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(id->combo,
        "La salida es siempre la primera de la lista. Aquí se elige contra "
        "cuál de las entradas se mira la CCF.");
    g_signal_connect(id->combo, "changed", G_CALLBACK(on_entrada), m);
    gtk_box_pack_start(GTK_BOX(fila), id->combo, FALSE, FALSE, 0);

    b = gtk_button_new_with_label("Ver la CCF");
    gtk_widget_set_tooltip_text(b,
        "El gráfico bidireccional: a la derecha la transferencia, a la "
        "izquierda la retroalimentación.");
    g_signal_connect(b, "clicked", G_CALLBACK(on_ver_ccf), m);
    gtk_box_pack_start(GTK_BOX(fila), b, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(caja), fila, FALSE, FALSE, 0);

    /* --- la lectura --- */
    marco = gtk_frame_new("CCF preblanqueada");
    vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 6);
    id->lectura = gtk_label_new(
        "Carga la salida y al menos una entrada.");
    gtk_widget_set_halign(id->lectura, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(id->lectura), TRUE);
    gtk_container_add(GTK_CONTAINER(vb), id->lectura);
    gtk_container_add(GTK_CONTAINER(marco), vb);

    sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sc), marco);
    gtk_box_pack_start(GTK_BOX(caja), sc, TRUE, TRUE, 0);

    /* --- la ecuacion --- */
    marco = gtk_frame_new("La ecuación que se va a estimar");
    vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 6);
    id->ecuacion = gtk_label_new("");
    gtk_widget_set_halign(id->ecuacion, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(id->ecuacion), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(id->ecuacion), TRUE);
    gtk_container_add(GTK_CONTAINER(vb), id->ecuacion);
    gtk_container_add(GTK_CONTAINER(marco), vb);
    gtk_box_pack_start(GTK_BOX(caja), marco, FALSE, FALSE, 0);

    return caja;
}
