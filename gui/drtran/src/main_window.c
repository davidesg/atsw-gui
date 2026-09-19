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
#include "nsop.h"

enum { COL_NUM, COL_NOMBRE, COL_NOBS, COL_DESDE, COL_HASTA, COL_PIERDE,
       COL_D, COL_DD, COL_F, COL_RUTA, COL_PTR, N_COLS };

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
    int desde[GUI_MAX_SER], hasta[GUI_MAX_SER];
    char why[512];
    gboolean hay_ventana;
    int i;

    hay_ventana = m->c.n >= 2 &&
                  conjunto_ventana_comun(&m->c, desde, hasta, why, sizeof why);

    m->recolocando = TRUE;
    gtk_list_store_clear(st);
    for (i = 0; i < m->c.n; i++) {
        Serie    *s = m->c.s[i];
        gchar    *d = serie_fecha(s, 1);
        gchar    *h = serie_fecha(s, s->ts.nobs);
        NsopForm  o;
        char      num[16], fcol[32], dcol[8], ddcol[8];
        int       pierde;

        /* La posicion, que ES el indice de q[i,j] en el .cns. La salida
         * lleva ademas la Y con la que sale en la ecuacion.            */
        snprintf(num, sizeof num, i == 0 ? "%d Y" : "%d", i + 1);

        /* El operador, EN FORMA CANONICA: los enteros del fichero no lo son
         * --el m6 trae nrdiff=2 en las seis series y una de ellas es otro
         * operador-- asi que se factoriza el polinomio.                */
        nsop_canon(s->tm.rnsop, s->tm.ornsop, s->tm.sper, &o);
        snprintf(dcol,  sizeof dcol,  "%d", o.d);
        snprintf(ddcol, sizeof ddcol, "%d", o.D);
        nsop_texto_f(&o, fcol, sizeof fcol);

        pierde = hay_ventana ? s->ts.nobs - (hasta[i] - desde[i] + 1) : 0;

        gtk_list_store_append(st, &it);
        gtk_list_store_set(st, &it,
            COL_NUM,    num,
            COL_NOMBRE, s->ts.name ? s->ts.name : "(sin nombre)",
            COL_NOBS,   s->ts.nobs,
            COL_DESDE,  d,
            COL_HASTA,  h,
            COL_PIERDE, hay_ventana ? pierde : -1,
            COL_D,      dcol,
            COL_DD,     ddcol,
            COL_F,      fcol,
            COL_RUTA,   s->path,
            COL_PTR,    s,
            -1);
        g_free(d); g_free(h);
    }
    m->recolocando = FALSE;
}

/* ------------------------------------------------------------------------ */
/* Los dos veredictos                                                        */
/*                                                                           */
/* De UNA LINEA y de altura FIJA. Los dos marcos que habia aqui crecian con  */
/* los datos --uno n renglones, el otro n(n-1)/2-- y con seis series se      */
/* comian 470 de los 620 px de la ventana: a la lista le quedaban dos filas. */
/* El detalle se pide; el veredicto esta siempre.                            */
/* ------------------------------------------------------------------------ */

#define VERDE  "#1a7f37"
#define AMBAR  "#9a6700"
#define ROJO   "#b3261e"

static void pon_verdicto(GtkWidget *w, const char *color, const char *fmt, ...)
    G_GNUC_PRINTF(3, 4);

static void pon_verdicto(GtkWidget *w, const char *color, const char *fmt, ...)
{
    va_list ap;
    gchar  *t, *esc, *mk;

    va_start(ap, fmt);
    t = g_strdup_vprintf(fmt, ap);
    va_end(ap);

    esc = g_markup_escape_text(t, -1);
    mk  = g_strdup_printf("<span foreground=\"%s\">\xe2\x97\x8f</span>  %s",
                          color, esc);
    gtk_label_set_markup(GTK_LABEL(w), mk);
    g_free(mk); g_free(esc); g_free(t);
}

static void refresca_ventana(Mtram *m)
{
    int  desde[GUI_MAX_SER], hasta[GUI_MAX_SER];
    char why[512];
    int  i, recortan = 0, obs;

    if (m->c.n < 2) {
        pon_verdicto(m->ver_ventana, AMBAR,
                     "Carga al menos dos .pre: la primera es la salida.");
        return;
    }
    if (!conjunto_ventana_comun(&m->c, desde, hasta, why, sizeof why)) {
        pon_verdicto(m->ver_ventana, ROJO, "Sin ventana común: %s", why);
        return;
    }

    obs = hasta[0] - desde[0] + 1;
    for (i = 0; i < m->c.n; i++)
        if (m->c.s[i]->ts.nobs != hasta[i] - desde[i] + 1) recortan++;

    {
    gchar *d = serie_fecha(m->c.s[0], desde[0]);
    gchar *h = serie_fecha(m->c.s[0], hasta[0]);

    if (!recortan)
        pon_verdicto(m->ver_ventana, VERDE,
                     "%s \xe2\x80\x93 %s \xc2\xb7 %d obs \xc2\xb7 todas completas",
                     d, h, obs);
    else
        pon_verdicto(m->ver_ventana, AMBAR,
                     "%s \xe2\x80\x93 %s \xc2\xb7 %d obs \xc2\xb7 %d serie%s "
                     "recorta%s \xe2\x80\x94 el motor NO recorta por fecha",
                     d, h, obs, recortan, recortan == 1 ? "" : "s",
                     recortan == 1 ? "" : "n");
    g_free(d); g_free(h);
    }
}

static void refresca_compat(Mtram *m)
{
    int i, j, iguales = 0, anidados = 0, malos = 0;

    if (m->c.n < 2) { gtk_label_set_text(GTK_LABEL(m->ver_oper), ""); return; }

    for (i = 0; i < m->c.n; i++)
        for (j = i + 1; j < m->c.n; j++)
            switch (conjunto_compat(&m->c, i, j)) {
            case OP_IGUALES:  iguales++;  break;
            case OP_ANIDADOS: anidados++; break;
            default:          malos++;    break;
            }

    if (malos)
        pon_verdicto(m->ver_oper, ROJO,
            "%d par%s incompatible%s \xc2\xb7 cast \xe2\x88\x92S por resta",
            malos, malos == 1 ? "" : "es", malos == 1 ? "" : "s");
    else if (anidados)
        pon_verdicto(m->ver_oper, AMBAR,
            "%d par%s igual%s, %d anidado%s \xc2\xb7 cast \xe2\x88\x92V "
            "empotrado (sigue exacto)",
            iguales, iguales == 1 ? "" : "es", iguales == 1 ? "" : "es",
            anidados, anidados == 1 ? "" : "s");
    else
        pon_verdicto(m->ver_oper, VERDE,
            "%d par%s igual%s \xc2\xb7 cast \xe2\x88\x92V empotrado "
            "(verosimilitud exacta)",
            iguales, iguales == 1 ? "" : "es", iguales == 1 ? "" : "es");
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

/* ------------------------------------------------------------------------ */
/* Los dos paneles emergentes                                                */
/* ------------------------------------------------------------------------ */

static GtkWidget *popover_texto(GtkWidget *ancla, const char *txt)
{
    GtkWidget *pop = gtk_popover_new(ancla);
    GtkWidget *l   = gtk_label_new(txt);
    GtkWidget *sc  = gtk_scrolled_window_new(NULL, NULL);

    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(l), TRUE);
    gtk_widget_set_margin_start(l, 10);   gtk_widget_set_margin_end(l, 10);
    gtk_widget_set_margin_top(l, 10);     gtk_widget_set_margin_bottom(l, 10);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(sc), TRUE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sc), TRUE);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(sc), 420);
    gtk_container_add(GTK_CONTAINER(sc), l);
    gtk_container_add(GTK_CONTAINER(pop), sc);
    gtk_widget_show_all(sc);
    return pop;
}

static void on_ventana(GtkButton *b, Mtram *m)
{
    int      desde[GUI_MAX_SER], hasta[GUI_MAX_SER];
    char     why[512];
    GString *t = g_string_new(NULL);
    int      i, recortan = 0;

    if (m->c.n < 2)
        g_string_append(t, "Carga al menos dos .pre.");
    else if (!conjunto_ventana_comun(&m->c, desde, hasta, why, sizeof why))
        g_string_append_printf(t, "No hay tramo común: %s", why);
    else {
        gchar *d = serie_fecha(m->c.s[0], desde[0]);
        gchar *h = serie_fecha(m->c.s[0], hasta[0]);

        g_string_append_printf(t, "Ventana común: %s – %s   (%d obs)\n\n",
                               d, h, hasta[0] - desde[0] + 1);
        g_free(d); g_free(h);

        for (i = 0; i < m->c.n; i++) {
            int pierde = m->c.s[i]->ts.nobs - (hasta[i] - desde[i] + 1);

            if (pierde) recortan++;
            g_string_append_printf(t, "   %-12s %d obs, %s%d\n",
                m->c.s[i]->ts.name ? m->c.s[i]->ts.name : "?",
                m->c.s[i]->ts.nobs,
                pierde ? "pierde " : "completa, pierde ", pierde);
        }

        if (recortan)
            g_string_append(t,
                "\nOJO: el motor NO recorta por fecha, sólo compara cuántas\n"
                "observaciones hay. Dos series de la misma longitud y distinta\n"
                "fecha de inicio se estiman DESALINEADAS, en silencio — está\n"
                "medido: catorce años de desfase mueven la verosimilitud 31\n"
                "unidades y drtran sale con 0.\n\n"
                "Hay que escribir los .pre ya recortados.");
    }

    {
    GtkWidget *pop = popover_texto(GTK_WIDGET(b), t->str);

    gtk_popover_popup(GTK_POPOVER(pop));
    }
    g_string_free(t, TRUE);
}

static void on_operadores(GtkButton *b, Mtram *m)
{
    GString *t = g_string_new(NULL);
    int      i, j;

    if (m->c.n < 1) {
        g_string_append(t, "Carga los .pre.");
        goto pinta;
    }

    /* Arriba: el polinomio entero de cada serie, que es lo que las tres
     * columnas resumen. Y cuando el fichero lo escribe de otra forma, se
     * dice AQUI: en la lista seria ruido, y callarlo dejaria al analista
     * preguntandose por que su nrdiff = 2 sale como d = 1.             */
    for (i = 0; i < m->c.n; i++) {
        Serie    *s = m->c.s[i];
        NsopForm  o;
        char      pol[128];

        nsop_canon(s->tm.rnsop, s->tm.ornsop, s->tm.sper, &o);
        nsop_texto(&o, s->tm.sper, pol, sizeof pol);

        g_string_append_printf(t, "   %-8s %-22s d=%d  D=%d",
            s->ts.name ? s->ts.name : "?", pol, o.d, o.D);
        if (nsop_difiere(&o, s->tm.nrdiff, s->tm.nadiff))
            g_string_append_printf(t, "    [el .pre lo escribe nrdiff=%d nadiff=%d]",
                                   s->tm.nrdiff, s->tm.nadiff);
        g_string_append_c(t, '\n');
    }

    if (m->c.n < 2) goto pinta;

    /* Abajo: la matriz. Es n x n y por eso NO puede estar en la pagina. */
    g_string_append(t, "\n   ");
    for (j = 0; j < m->c.n; j++)
        g_string_append_printf(t, "%-5.4s", m->c.s[j]->ts.name ?
                               m->c.s[j]->ts.name : "?");
    g_string_append_c(t, '\n');

    for (i = 0; i < m->c.n; i++) {
        g_string_append_printf(t, "%-4.4s ", m->c.s[i]->ts.name ?
                               m->c.s[i]->ts.name : "?");
        for (j = 0; j < m->c.n; j++) {
            const char *c;

            if (i == j)     c = "·";
            else if (j < i) c = " ";
            else switch (conjunto_compat(&m->c, i, j)) {
                 case OP_IGUALES:  c = "=";  break;
                 case OP_ANIDADOS: c = "\xe2\x8a\x83"; break;   /* ⊃ */
                 default:          c = "\xe2\x9c\x97"; break;   /* ✗ */
                 }
            g_string_append_printf(t, "%-5s", c);
        }
        g_string_append_c(t, '\n');
    }

    g_string_append(t,
        "\n   =  el mismo ∇            cast −V empotrado, verosimilitud exacta\n"
        "   ⊃  uno divide al otro    hay Δ(B), sigue siendo exacta\n"
        "   ✗  incompatibles         el motor despacha al cast −S por resta");

pinta:
    {
    GtkWidget *pop = popover_texto(GTK_WIDGET(b), t->str);

    gtk_popover_popup(GTK_POPOVER(pop));
    }
    g_string_free(t, TRUE);
}

/* ------------------------------------------------------------------------ */

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

/* Dejar marcada la fila i. Refrescar la lista la vacia y la vuelve a llenar,
 * asi que la marca se pierde -- y entonces subir dos plazas obliga a volver a
 * marcar entre flecha y flecha. Se remarca donde ha quedado, para poder
 * pulsar en secuencia.                                                    */
static void marca_fila(Mtram *m, int i)
{
    GtkTreePath *path;

    if (i < 0 || i >= m->c.n) return;

    path = gtk_tree_path_new_from_indices(i, -1);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(m->lista), path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(m->lista), path, NULL,
                                 FALSE, 0.0, 0.0);
    gtk_tree_path_free(path);
    gtk_widget_grab_focus(m->lista);
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
    marca_fila(m, a);
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
    if (i == 0) { barra(m, "«%s» ya es la salida.",
                        m->c.s[0]->ts.name ? m->c.s[0]->ts.name : "?"); return; }
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
    marca_fila(m, i < m->c.n ? i : m->c.n - 1);

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

    BOTON("Abrir…", on_anadir,
          "Un modelo YA ESTIMADO con fue. drtran parte de óptimos, no de datos "
          "crudos: el escalón univariante va antes.\n\nSe pueden abrir VARIOS de "
          "golpe: Ctrl+A en el diálogo carga todos los .pre de la carpeta.")
    BOTON("Quitar", on_quitar,
          "Se lleva también los enlaces de la red que nombraban a esta serie.")

    gtk_box_pack_start(GTK_BOX(barra_b),
                       gtk_separator_new(GTK_ORIENTATION_VERTICAL),
                       FALSE, FALSE, 6);

    BOTON("Salida", on_salida,
          "La salida es la primera de la lista: la Y del modelo.")
    BOTON("↑", on_subir,
          "O arrastra la fila. El orden es el índice de q[i,j] en el .cns.")
    BOTON("↓", on_bajar,
          "O arrastra la fila. El orden es el índice de q[i,j] en el .cns.")
#undef BOTON

    /* A la derecha, los dos que ABREN algo. Los "…" lo dicen. */
#define BOTON_DER(txt, fn, tip) \
    b = gtk_button_new_with_label(txt); \
    gtk_widget_set_tooltip_text(b, tip); \
    g_signal_connect(b, "clicked", G_CALLBACK(fn), m); \
    gtk_box_pack_end(GTK_BOX(barra_b), b, FALSE, FALSE, 0);

    BOTON_DER("Operadores…", on_operadores,
              "El polinomio de cada serie y la matriz de compatibilidad. Es "
              "n×n: por eso no está en la página.")
    BOTON_DER("Ventana…", on_ventana,
              "El desglose por serie del tramo común, y lo que cuesta.")
#undef BOTON_DER

    /* --- la lista --- */
    st = gtk_list_store_new(N_COLS,
                            G_TYPE_STRING,   /* #        */
                            G_TYPE_STRING,   /* serie    */
                            G_TYPE_INT,      /* obs      */
                            G_TYPE_STRING,   /* desde    */
                            G_TYPE_STRING,   /* hasta    */
                            G_TYPE_INT,      /* pierde   */
                            G_TYPE_STRING,   /* d        */
                            G_TYPE_STRING,   /* D        */
                            G_TYPE_STRING,   /* f        */
                            G_TYPE_STRING,   /* fichero  */
                            G_TYPE_POINTER);
    m->lista = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
    columna(m->lista, "#",       COL_NUM, 0);
    columna(m->lista, "Serie",   COL_NOMBRE, 0);
    columna(m->lista, "Obs",     COL_NOBS, 1);
    columna(m->lista, "Desde",   COL_DESDE, 0);
    columna(m->lista, "Hasta",   COL_HASTA, 0);
    columna(m->lista, "Pierde",  COL_PIERDE, 1);
    columna(m->lista, "d",       COL_D, 0);
    columna(m->lista, "D",       COL_DD, 0);
    columna(m->lista, "f",       COL_F, 0);
    columna(m->lista, "Fichero", COL_RUTA, 0);

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

    /* --- los dos veredictos: DOS LINEAS, altura fija --------------------
     * Todo lo demas del alto es de la lista. Los marcos que habia aqui
     * crecian con los datos y la dejaban en dos filas.                  */
    {
    GtkWidget *vv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    gtk_widget_set_margin_top(vv, 2);

    m->ver_ventana = gtk_label_new("Carga al menos dos .pre: la primera es la salida.");
    m->ver_oper    = gtk_label_new("");
    gtk_widget_set_halign(m->ver_ventana, GTK_ALIGN_START);
    gtk_widget_set_halign(m->ver_oper,    GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(m->ver_ventana), PANGO_ELLIPSIZE_END);
    gtk_label_set_ellipsize(GTK_LABEL(m->ver_oper),    PANGO_ELLIPSIZE_END);

    gtk_box_pack_start(GTK_BOX(vv), m->ver_ventana, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vv), m->ver_oper,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja), vv, FALSE, FALSE, 0);
    }

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
