/*
 * main_window.c -- la ventana de mtram.
 *
 * Primera capa: las series, su orden, la ventana comun y la compatibilidad de
 * operadores. Es lo que hay que resolver ANTES de que el motor pueda correr, y
 * es justo lo que hoy no tiene sitio en ninguna interfaz.
 *
 * El orden de las series no es cosmetico: la primera es la SALIDA, y ese orden
 * es el indice al que se refieren q[i,j], phi_i, theta_i y mu[i] en el .cns.
 * El estudio de la escalera midio lo que cuesta equivocarse -- permutar dos
 * series mueve la verosimilitud en 35,6 y cambia de signo una covarianza, sin
 * un solo aviso -- asi que aqui el orden se ve y se cambia a la vista.
 */

#include <string.h>

#include "gui.h"

enum { COL_PAPEL, COL_NOMBRE, COL_NOBS, COL_DESDE, COL_HASTA,
       COL_OPERADOR, COL_RUTA, COL_PTR, N_COLS };

/* Se guarda el puntero a la Serie en cada fila. Identificar la fila por su
 * RUTA parece mas limpio y no lo es: cargar dos veces el mismo .pre --que es
 * legitimo-- daria dos filas indistinguibles.                            */

/* ------------------------------------------------------------------------ */

static void barra(Mtram *m, const char *fmt, ...) G_GNUC_PRINTF(2, 3);

static void barra(Mtram *m, const char *fmt, ...)
{
    va_list ap;
    gchar  *s;

    va_start(ap, fmt);
    s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    gtk_label_set_text(GTK_LABEL(m->estado), s);
    gtk_widget_set_tooltip_text(m->estado, s);
    g_free(s);
}

/* ------------------------------------------------------------------------ */
/* La lista                                                                  */
/* ------------------------------------------------------------------------ */

static void refresca_lista(Mtram *m)
{
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model(
                                          GTK_TREE_VIEW(m->lista)));
    GtkTreeIter it;
    int i;

    m->recolocando = TRUE;
    gtk_list_store_clear(st);
    for (i = 0; i < m->c.n; i++) {
        Serie *s = m->c.s[i];
        gchar *d = serie_fecha(s, 1);
        gchar *h = serie_fecha(s, s->ts.nobs);

        gtk_list_store_append(st, &it);
        gtk_list_store_set(st, &it,
            COL_PAPEL,    i == 0 ? "SALIDA  Y" : "entrada",
            COL_NOMBRE,   s->ts.name ? s->ts.name : "(sin nombre)",
            COL_NOBS,     s->ts.nobs,
            COL_DESDE,    d,
            COL_HASTA,    h,
            COL_OPERADOR, s->operador,
            COL_RUTA,     s->path,
            COL_PTR,      s,
            -1);
        g_free(d); g_free(h);
    }
    m->recolocando = FALSE;
}

/* La ventana comun, y lo que cuesta */
static void refresca_ventana(Mtram *m)
{
    int  desde[GUI_MAX_SER], hasta[GUI_MAX_SER];
    char why[512];
    GString *t;
    int i;

    if (m->c.n < 2) {
        gtk_label_set_text(GTK_LABEL(m->ventana),
            "Carga al menos dos .pre: la primera es la salida.");
        return;
    }
    if (!conjunto_ventana_comun(&m->c, desde, hasta, why, sizeof why)) {
        gchar *s = g_strdup_printf("Sin ventana común: %s", why);
        gtk_label_set_text(GTK_LABEL(m->ventana), s);
        g_free(s);
        return;
    }

    t = g_string_new(NULL);
    {
    gchar *d = serie_fecha(m->c.s[0], desde[0]);
    gchar *h = serie_fecha(m->c.s[0], hasta[0]);
    g_string_append_printf(t, "Ventana común: %s – %s   (%d obs)\n",
                           d, h, hasta[0] - desde[0] + 1);
    g_free(d); g_free(h);
    }

    for (i = 0; i < m->c.n; i++) {
        int pierde = m->c.s[i]->ts.nobs - (hasta[i] - desde[i] + 1);
        g_string_append_printf(t, "   %-12s %s%d obs\n",
            m->c.s[i]->ts.name ? m->c.s[i]->ts.name : "?",
            pierde ? "pierde " : "completa, 0 ",
            pierde ? pierde : 0);
    }

    /* El motor NO recorta por fecha: compara nobs y nada mas. Dos series de la
     * misma longitud y distinta fecha de inicio se estiman desalineadas, en
     * silencio -- esta medido: catorce anios de desfase cambian la
     * verosimilitud en 31 unidades y drtran sale con 0.                    */
    if (hasta[0] - desde[0] + 1 != m->c.s[0]->ts.nobs)
        g_string_append(t,
            "\nOjo: el motor no recorta por fecha, sólo compara cuántas "
            "observaciones hay. Hay que escribir los .pre recortados.");

    gtk_label_set_text(GTK_LABEL(m->ventana), t->str);
    g_string_free(t, TRUE);
}

/* La matriz de compatibilidad */
static void refresca_compat(Mtram *m)
{
    GString *t = g_string_new(NULL);
    int i, j, malos = 0;

    if (m->c.n < 2) {
        gtk_label_set_text(GTK_LABEL(m->compat), "");
        return;
    }

    for (i = 0; i < m->c.n; i++)
        for (j = i + 1; j < m->c.n; j++) {
            Compat k = conjunto_compat(&m->c, i, j);
            if (k == OP_INCOMPATIBLES) malos++;
            g_string_append_printf(t, "%-10s ↔ %-10s  %s\n",
                m->c.s[i]->ts.name ? m->c.s[i]->ts.name : "?",
                m->c.s[j]->ts.name ? m->c.s[j]->ts.name : "?",
                compat_texto(k));
        }

    g_string_append_printf(t, "\nCast: %s",
        malos ? "-S (por resta): hay operadores incompatibles"
              : "-V (empotrado): la verosimilitud es la exacta");
    gtk_label_set_text(GTK_LABEL(m->compat), t->str);
    g_string_free(t, TRUE);
}

static void refresca(Mtram *m)
{
    refresca_lista(m);
    refresca_ventana(m);
    refresca_compat(m);
    red_refresca(m);
    identifica_refresca(m);
    modelo_refresca(m);
    estima_refresca(m);
}

/* ------------------------------------------------------------------------ */
/* Botones                                                                   */
/* ------------------------------------------------------------------------ */

static void on_anadir(GtkButton *b, Mtram *m)
{
    GtkWidget     *d;
    GtkFileFilter *f;

    if (m->c.n >= GUI_MAX_SER) {
        barra(m, "El motor lleva %d series como mucho (1 salida + %d entradas).",
              GUI_MAX_SER, GUI_MAX_SER - 1);
        return;
    }

    d = gtk_file_chooser_dialog_new("Abrir .pre", GTK_WINDOW(m->ventana_p),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    "_Cancelar", GTK_RESPONSE_CANCEL,
                                    "_Abrir",    GTK_RESPONSE_ACCEPT, NULL);
    /* De golpe: un sistema son seis o siete .pre y abrirlos de uno en uno es
     * un peaje sin motivo. Con Ctrl+A entran todos los de la carpeta.    */
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(d), TRUE);

    f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "Modelos estimados (*.pre)");
    gtk_file_filter_add_pattern(f, "*.pre");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        GSList  *lista = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(d));
        GSList  *l;
        GString *fallos = g_string_new(NULL);
        int      puestas = 0, sitio = 1;

        /* Por nombre, para que el resultado no dependa de en que orden los
         * devuelva el dialogo: con varios ficheros eso no esta garantizado y
         * aqui el orden SIGNIFICA algo -- la primera es la salida.       */
        lista = g_slist_sort(lista, (GCompareFunc) g_strcmp0);

        for (l = lista; l; l = l->next) {
            char   why[512];
            Serie *s;

            if (m->c.n >= GUI_MAX_SER) { sitio = 0; break; }

            s = serie_cargar((const char *) l->data, why, sizeof why);
            if (s) {
                m->c.s[m->c.n++] = s;
                puestas++;
            } else {
                gchar *base = g_path_get_basename((const char *) l->data);

                g_string_append_printf(fallos, "%s%s: %s",
                                       fallos->len ? "; " : "", base, why);
                g_free(base);
            }
        }

        refresca(m);

        if (!sitio)
            barra(m, "%d cargada%s. El motor lleva %d series como mucho, así "
                     "que las demás se quedaron fuera.",
                  puestas, puestas == 1 ? "" : "s", GUI_MAX_SER);
        else if (fallos->len)
            barra(m, "%d cargada%s. No pude con %s", puestas,
                  puestas == 1 ? "" : "s", fallos->str);
        else if (puestas > 1)
            barra(m, "%d series. La SALIDA es «%s», la primera por orden "
                     "alfabético: arrastra otra arriba si no es la que toca.",
                  puestas, m->c.s[0]->ts.name ? m->c.s[0]->ts.name : "?");
        else if (puestas == 1)
            barra(m, "%s: %d obs, frecuencia %d, operador %s",
                  m->c.s[m->c.n - 1]->ts.name ? m->c.s[m->c.n - 1]->ts.name : "(sin nombre)",
                  m->c.s[m->c.n - 1]->ts.nobs, m->c.s[m->c.n - 1]->ts.freq,
                  m->c.s[m->c.n - 1]->operador);

        g_string_free(fallos, TRUE);
        g_slist_free_full(lista, g_free);
    }
    gtk_widget_destroy(d);
}

static int fila_marcada(Mtram *m)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m->lista));
    GtkTreeModel *mod;
    GtkTreeIter   it;
    GtkTreePath  *path;
    int n = -1;

    if (!gtk_tree_selection_get_selected(sel, &mod, &it)) return -1;
    path = gtk_tree_model_get_path(mod, &it);
    n = gtk_tree_path_get_indices(path)[0];
    gtk_tree_path_free(path);
    return n;
}

/* Reordenar NO es cosmetico: los enlaces del .dag se refieren a las series por
 * su POSICION, y tambien lo hacen las covarianzas q[i,j] del .cns. Mover una
 * serie sin remapear la red deja los enlaces apuntando a otra cosa -- y en
 * silencio, que es lo peor: un EP <- EI se convierte en un EI <- EP y sigue
 * estimando tan campante.
 *
 * perm[vieja] = nueva, indices 1..n.                                      */
static void remapea_red(Mtram *m, const int *perm)
{
    m->red.n = net_remap(m->red.lnk, m->red.n, perm);
}

static void mueve(Mtram *m, int de, int a)
{
    Serie *s;
    int    perm[NET_MAX_SER + 1];

    if (de < 0 || a < 0 || de >= m->c.n || a >= m->c.n || de == a) return;

    net_perm_move(m->c.n, de + 1, a + 1, perm);

    s = m->c.s[de];
    if (de < a) memmove(&m->c.s[de], &m->c.s[de + 1], (a - de) * sizeof(Serie *));
    else        memmove(&m->c.s[a + 1], &m->c.s[a], (de - a) * sizeof(Serie *));
    m->c.s[a] = s;

    remapea_red(m, perm);
    m->mod.orden_cambio = TRUE;
    refresca(m);
}

/* ------------------------------------------------------------------------ */
/* Arrastrar y soltar dentro de la lista                                     */
/*                                                                           */
/* GTK reordena SU modelo; el orden que manda es el de m->c.s, asi que hay que */
/* traerlo de vuelta. Se lee el modelo y se reconstruye el conjunto en ese     */
/* orden, identificando cada fila por el PUNTERO a la Serie.                  */
/* ------------------------------------------------------------------------ */

static void sincroniza_orden(GtkTreeModel *mod, Mtram *m)
{
    Serie       *nuevo[GUI_MAX_SER];
    int          perm[NET_MAX_SER + 1];
    GtkTreeIter  it;
    int          n = 0, i, cambio = 0;

    if (m->recolocando) return;           /* lo estamos repintando nosotros */
    if (!gtk_tree_model_get_iter_first(mod, &it)) return;

    do {
        Serie *s = NULL;

        gtk_tree_model_get(mod, &it, COL_PTR, &s, -1);
        if (!s || n >= GUI_MAX_SER) return;

        for (i = 0; i < m->c.n; i++)
            if (m->c.s[i] == s) { perm[i + 1] = n + 1; break; }
        if (i == m->c.n) return;          /* una fila que no es de nadie */

        nuevo[n++] = s;
    } while (gtk_tree_model_iter_next(mod, &it));

    /* A media faena el modelo tiene una fila de mas o de menos: no tocar. */
    if (n != m->c.n) return;

    for (i = 0; i < n; i++) if (m->c.s[i] != nuevo[i]) cambio = 1;
    if (!cambio) return;

    for (i = 0; i < n; i++) m->c.s[i] = nuevo[i];
    remapea_red(m, perm);
    m->mod.orden_cambio = TRUE;
    refresca(m);

    barra(m, "%s es ahora la salida. Los enlaces se han remapeado; las "
             "restricciones del .cns, NO: sus nombres llevan la posición "
             "dentro.",
          m->c.s[0]->ts.name ? m->c.s[0]->ts.name : "?");
}

static void on_fila_borrada(GtkTreeModel *mod, GtkTreePath *path, Mtram *m)
{
    /* GTK reordena insertando y borrando: cuando llega el borrado, ya esta. */
    sincroniza_orden(mod, m);
}

static void on_subir(GtkButton *b, Mtram *m)  { int i = fila_marcada(m); mueve(m, i, i - 1); }
static void on_bajar(GtkButton *b, Mtram *m)  { int i = fila_marcada(m); mueve(m, i, i + 1); }

static void on_salida(GtkButton *b, Mtram *m)
{
    int i = fila_marcada(m);

    if (i < 0) { barra(m, "Marca primero una serie."); return; }
    mueve(m, i, 0);
    barra(m, "%s es ahora la salida. Ojo: el orden es el índice de q[i,j] "
             "en el .cns.", m->c.s[0]->ts.name ? m->c.s[0]->ts.name : "?");
}

static void on_quitar(GtkButton *b, Mtram *m)
{
    int i = fila_marcada(m), k, j, quitados = 0;

    if (i < 0) return;

    /* Los enlaces que nombraban a esta serie dejan de tener sentido y se van
     * con ella; los demas se corren una plaza. Callarse y dejarlos apuntando
     * a otra serie seria el mismo fallo que no remapear al mover.      */
    {
    int perm[NET_MAX_SER + 1], antes = m->red.n;

    net_perm_drop(m->c.n, i + 1, perm);
    m->red.n = net_remap(m->red.lnk, m->red.n, perm);
    quitados = antes - m->red.n;
    }
    (void) k;  (void) j;

    serie_libre(m->c.s[i]);
    memmove(&m->c.s[i], &m->c.s[i + 1], (m->c.n - i - 1) * sizeof(Serie *));
    m->c.n--;
    refresca(m);

    if (quitados)
        barra(m, "Y con ella %d enlace%s que la nombraba%s.",
              quitados, quitados == 1 ? "" : "s", quitados == 1 ? "" : "n");
}

/* ------------------------------------------------------------------------ */
/* La ventana                                                                */
/* ------------------------------------------------------------------------ */

static GtkWidget *columna(GtkWidget *tv, const char *titulo, int col, int num)
{
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
        titulo, r, num ? "text" : "text", col, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    return NULL;
}

GtkWidget *mtram_window_new(GtkApplication *app, Mtram *m)
{
    GtkWidget *w, *raiz, *libro, *caja, *barra_b, *b, *sc, *marco, *vb;
    GtkListStore *st;

    w = gtk_application_window_new(app);
    m->ventana_p = w;
    gtk_window_set_title(GTK_WINDOW(w), "mtram — función de transferencia");
    gtk_window_set_default_size(GTK_WINDOW(w), 900, 620);

    /* El cuaderno: las dos cosas que hay que resolver, en este orden. Primero
     * QUE series y en que papel --sin eso el motor no puede ni arrancar--, y
     * despues QUE forma tiene cada enlace, que es lo que decide la CCF.    */
    raiz = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(w), raiz);

    libro = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(raiz), libro, TRUE, TRUE, 0);

    caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(caja), 8);
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), caja,
                             gtk_label_new("Series"));

    /* --- los botones --- */
    barra_b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(caja), barra_b, FALSE, FALSE, 0);

#define BOTON(txt, fn, tip) \
    b = gtk_button_new_with_label(txt); \
    gtk_widget_set_tooltip_text(b, tip); \
    g_signal_connect(b, "clicked", G_CALLBACK(fn), m); \
    gtk_box_pack_start(GTK_BOX(barra_b), b, FALSE, FALSE, 0);

    BOTON("Añadir .pre…", on_anadir,
          "Un modelo YA ESTIMADO con fue. drtran parte de óptimos, no de datos "
          "crudos: el escalón univariante va antes.\n\nSe pueden abrir VARIOS de "
          "golpe: Ctrl+A en el diálogo carga todos los .pre de la carpeta.")
    BOTON("Subir", on_subir,
          "O arrastra la fila. El orden es el índice de q[i,j] en el .cns.")
    BOTON("Bajar", on_bajar,
          "O arrastra la fila. El orden es el índice de q[i,j] en el .cns.")
    BOTON("Hacer salida", on_salida, "La salida es la primera: la Y del modelo.")
    BOTON("Quitar", on_quitar, "")
#undef BOTON

    /* --- la lista --- */
    st = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_POINTER);
    m->lista = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
    columna(m->lista, "Papel",     COL_PAPEL, 0);
    columna(m->lista, "Serie",     COL_NOMBRE, 0);
    columna(m->lista, "Obs",       COL_NOBS, 1);
    columna(m->lista, "Desde",     COL_DESDE, 0);
    columna(m->lista, "Hasta",     COL_HASTA, 0);
    columna(m->lista, "Operador ∇", COL_OPERADOR, 0);
    columna(m->lista, "Fichero",   COL_RUTA, 0);

    /* Arrastrar para reordenar. El orden no es cosmetico --la primera es la
     * salida, y es el indice de q[i,j] en el .cns-- asi que se cambia a la
     * vista y con la mano.                                              */
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(m->lista), TRUE);
    g_signal_connect(st, "row-deleted", G_CALLBACK(on_fila_borrada), m);
    gtk_widget_set_tooltip_text(m->lista,
        "Arrastra para cambiar el orden. La PRIMERA es la salida, y el orden "
        "es el índice de q[i,j] en el .cns: al mover, los enlaces de la red se "
        "remapean solos.");

    sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sc), m->lista);
    gtk_box_pack_start(GTK_BOX(caja), sc, TRUE, TRUE, 0);

    /* --- la ventana comun --- */
    marco = gtk_frame_new("Ventana muestral común");
    vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 6);
    m->ventana = gtk_label_new("Carga al menos dos .pre: la primera es la salida.");
    gtk_widget_set_halign(m->ventana, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(vb), m->ventana);
    gtk_container_add(GTK_CONTAINER(marco), vb);
    gtk_box_pack_start(GTK_BOX(caja), marco, FALSE, FALSE, 0);

    /* --- la compatibilidad --- */
    marco = gtk_frame_new("Operadores no estacionarios");
    vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 6);
    m->compat = gtk_label_new("");
    gtk_widget_set_halign(m->compat, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(vb), m->compat);
    gtk_container_add(GTK_CONTAINER(marco), vb);
    gtk_box_pack_start(GTK_BOX(caja), marco, FALSE, FALSE, 0);

    /* --- la barra de estado: de la ventana, no de una pagina --- */
    m->estado = gtk_label_new("Añade los .pre. El primero es la salida.");
    gtk_widget_set_halign(m->estado, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(m->estado), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_start(m->estado, 8);
    gtk_widget_set_margin_bottom(m->estado, 4);
    gtk_box_pack_start(GTK_BOX(raiz), m->estado, FALSE, FALSE, 0);

    /* --- la red, y la identificacion --- */
    /* El orden de las pestañas es el orden del metodo: que series hay, como se
     * enlazan, y que forma tiene cada enlace. Ninguna se bloquea: cada una
     * pide por su nombre lo que la anterior fabrica.                      */
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), red_pagina_new(m),
                             gtk_label_new("Red"));
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), identifica_pagina_new(m),
                             gtk_label_new("Identificación"));
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), modelo_pagina_new(m),
                             gtk_label_new("Modelo"));
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), estima_pagina_new(m),
                             gtk_label_new("Estimación"));
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), diagnosis_pagina_new(m),
                             gtk_label_new("Diagnosis"));
    gtk_notebook_append_page(GTK_NOTEBOOK(libro), prevision_pagina_new(m),
                             gtk_label_new("Previsión"));

    return w;
}
