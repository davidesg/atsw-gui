/*
 * red.c -- la pantalla de la red: el .dag.
 *
 * Es la pantalla que no existia en ninguna interfaz, porque el objeto no
 * existia. TASTE hacia transferencia --hasta doce entradas, con delta(B) de
 * verdad-- pero su menu lo declaraba: "FUNCION DE TRANSFERENCIA CON UN SOLO
 * OUTPUT". Con un output, la cadena de pasos ES el modelo y no hace falta
 * dibujar nada. drtran resuelve una RED, y entonces si.
 *
 * LO QUE HAY QUE ENTENDER DE ESTA PANTALLA. El motor resuelve el sistema por
 * RECURSION en orden topologico: una serie solo se puede construir despues de
 * TODAS las que la alimentan. Asi que la red no es un dibujo: es lo que decide
 * si el modelo se puede estimar.
 *
 * Y por eso el ciclo se caza MIENTRAS SE DIBUJA, no al lanzar el motor. Un
 * ciclo no es un error de sintaxis: es un sistema SIMULTANEO, que no se puede
 * triangularizar restando transferencias. Eso no es un fallo del analista, es
 * un modelo que no es de este peldaño -- el que toca es drvarma. Es el mismo
 * veredicto que da la CCF bidireccional cuando los retardos negativos salen de
 * la banda, dicho sobre la red entera en vez de sobre un enlace.
 *
 * El .dag lo lee y lo escribe lib/netfile, que ES el lector del motor. Lo que
 * mtram acepte es lo que acepta drtran, por construccion.
 */

#include <string.h>
#include <stdlib.h>

#include "gui.h"
#include "previewhost.h"
#include "netfile.h"

enum { R_SALIDA, R_FLECHA, R_ENTRADA, R_B, R_R, R_S, R_NOTA, R_N };

/* ------------------------------------------------------------------------ */
/* Los nombres, como los quiere lib/netfile: 1..n                            */
/* ------------------------------------------------------------------------ */

static void nombres( Mtram *m, const char **nom )
{
    int i;

    for (i = 1; i <= m->c.n; i++)
        nom[i] = m->c.s[i - 1]->ts.name;
    nom[0] = NULL;
}

/* El nombre de la serie i (1..n) para enseñarlo. */
static const char *nom_de( Mtram *m, int i )
{
    if (i < 1 || i > m->c.n) return "?";
    return m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "(sin nombre)";
}

/* ------------------------------------------------------------------------ */
/* El veredicto: se puede estimar o no                                       */
/* ------------------------------------------------------------------------ */

static void refresca_veredicto( Mtram *m )
{
    Red     *r = &m->red;
    GString *t = g_string_new( NULL );
    int      topo[NET_MAX_SER + 1];
    int      ciclo[NET_MAX_SER + 2], nc = 0;
    int      i, huerfanas = 0;

    if (m->c.n < 2) {
        gtk_label_set_text( GTK_LABEL(r->veredicto),
            "Carga al menos dos .pre en la pestaña Series." );
        g_string_free( t, TRUE );
        return;
    }
    if (r->n == 0) {
        gtk_label_set_text( GTK_LABEL(r->veredicto),
            "La red está vacía. Sin enlaces, drtran estima los modelos "
            "univariantes en bloque y nada más: es la homologación con fue, "
            "útil para comprobar, pero no es un modelo de transferencia.\n"
            "Añade un enlace, o pulsa «Estrella» para que todas las entradas "
            "apunten a la salida (es lo que hace el motor por omisión)." );
        g_string_free( t, TRUE );
        return;
    }

    /* --- el ciclo, que es lo único que impide estimar ------------------- */
    if (!net_topo( r->lnk, r->n, m->c.n, topo )) {
        g_string_append( t, "CICLO: el sistema es SIMULTÁNEO.\n\n" );

        if (net_cycle( r->lnk, r->n, m->c.n, ciclo, &nc )) {
            g_string_append( t, "   " );
            for (i = 0; i < nc; i++)
                g_string_append_printf( t, "%s%s", i ? " → " : "",
                                        nom_de( m, ciclo[i] ) );
            g_string_append_c( t, '\n' );
        }
        g_string_append( t,
            "\nEl motor construye cada serie por recursión, después de todas "
            "las que la alimentan.\nCon un ciclo ese orden no existe: el "
            "sistema no se puede triangularizar restando\ntransferencias, y "
            "drtran se niega a estimar en vez de devolver algo sin sentido.\n\n"
            "No es un fallo de escritura. Es un modelo que no es de este "
            "escalón:\nun sistema simultáneo se estima con drvarma (sima), "
            "no aquí.\nSi crees que el ciclo no debería estar, mira la CCF "
            "del enlace que lo cierra:\nsi sus retardos negativos están dentro "
            "de la banda, ese enlace sobra." );

        gtk_label_set_text( GTK_LABEL(r->veredicto), t->str );
        g_string_free( t, TRUE );
        return;
    }

    /* --- se puede: cómo va a resolverlo -------------------------------- */
    g_string_append( t, "La red es acíclica: se puede estimar.\n\n"
                        "Orden de construcción:   " );
    for (i = 1; i <= m->c.n; i++)
        g_string_append_printf( t, "%s%s", i > 1 ? "  →  " : "",
                                nom_de( m, topo[i] ) );

    /* Las que no tocan a nadie: el motor las estima, pero no pintan nada. */
    for (i = 1; i <= m->c.n; i++)
        if (net_indegree( r->lnk, r->n, i ) == 0 &&
            net_outdegree( r->lnk, r->n, i ) == 0) huerfanas++;

    g_string_append_printf( t, "\n\n%d enlace%s sobre %d series.",
                            r->n, r->n == 1 ? "" : "s", m->c.n );

    if (huerfanas)
        g_string_append_printf( t,
            "\n\n%d serie%s no aparece%s en ningún enlace: el motor la%s "
            "estima igual, pero\nno recibe ni da transferencia. Si está de "
            "más, quítala en la pestaña Series.",
            huerfanas, huerfanas == 1 ? "" : "s",
            huerfanas == 1 ? "" : "n", huerfanas == 1 ? "" : "s" );

    /* Una serie que es salida Y entrada es lo que distingue una red de una
     * estrella, y es lo que hace falta decir porque no se ve solo.        */
    {
    int intermedias = 0;

    for (i = 1; i <= m->c.n; i++)
        if (net_indegree( r->lnk, r->n, i ) > 0 &&
            net_outdegree( r->lnk, r->n, i ) > 0) intermedias++;

    if (intermedias)
        g_string_append_printf( t,
            "\n\n%d serie%s es a la vez salida y entrada. Eso es una RED, no "
            "una estrella:\nsu ecuación se estima Y alimenta a otra.",
            intermedias, intermedias == 1 ? "" : "s" );
    }

    gtk_label_set_text( GTK_LABEL(r->veredicto), t->str );
    g_string_free( t, TRUE );
}

/* ------------------------------------------------------------------------ */
/* La lista de enlaces                                                       */
/* ------------------------------------------------------------------------ */

static void refresca_lista( Mtram *m )
{
    Red          *r  = &m->red;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(r->lista) ) );
    GtkTreeIter   it;
    int           ciclo[NET_MAX_SER + 2], nc = 0, k, i;
    int           topo[NET_MAX_SER + 1];
    char          en_ciclo[NET_MAX_SER + 1];

    for (i = 0; i <= NET_MAX_SER; i++) en_ciclo[i] = 0;
    if (m->c.n >= 2 && r->n > 0 && !net_topo( r->lnk, r->n, m->c.n, topo ))
        if (net_cycle( r->lnk, r->n, m->c.n, ciclo, &nc ))
            for (i = 0; i < nc; i++) en_ciclo[ciclo[i]] = 1;

    gtk_list_store_clear( st );
    for (k = 0; k < r->n; k++) {
        const NetLink *l = &r->lnk[k];
        /* Un enlace esta EN el ciclo si sus dos extremos lo estan. */
        int malo = en_ciclo[l->out] && en_ciclo[l->inp];

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            R_SALIDA,  nom_de( m, l->out ),
            R_FLECHA,  "←",
            R_ENTRADA, nom_de( m, l->inp ),
            R_B,       l->b,
            R_R,       l->r,
            R_S,       l->s,
            R_NOTA,    malo ? "cierra el ciclo" : "",
            -1 );
    }
}

void red_refresca( Mtram *m )
{
    refresca_lista( m );
    refresca_veredicto( m );
}

/* ------------------------------------------------------------------------ */
/* Botones                                                                   */
/* ------------------------------------------------------------------------ */

static int fila_marcada( Mtram *m )
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(
                                GTK_TREE_VIEW(m->red.lista) );
    GtkTreeModel *mod;
    GtkTreeIter   it;
    GtkTreePath  *path;
    int           n;

    if (!gtk_tree_selection_get_selected( sel, &mod, &it )) return -1;
    path = gtk_tree_model_get_path( mod, &it );
    n = gtk_tree_path_get_indices( path )[0];
    gtk_tree_path_free( path );
    return n;
}

/* El diálogo de un enlace: salida, entrada y (b, r, s).
 * Devuelve TRUE si el usuario aceptó. */
static gboolean pide_enlace( Mtram *m, NetLink *l, const char *titulo )
{
    GtkWidget *d, *caja, *rej, *c_sal, *c_ent, *sb, *spin[3];
    int        i, resp;
    static const char *ORD[3] = { "retardo puro  b", "denominador  r",
                                  "numerador  s" };

    d = gtk_dialog_new_with_buttons( titulo, GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL,
            "_Aceptar",  GTK_RESPONSE_ACCEPT, NULL );

    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 8 );
    gtk_container_add( GTK_CONTAINER(caja), rej );

    c_sal = gtk_combo_box_text_new();
    c_ent = gtk_combo_box_text_new();
    for (i = 1; i <= m->c.n; i++) {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(c_sal), nom_de( m, i ) );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(c_ent), nom_de( m, i ) );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX(c_sal), l->out - 1 );
    gtk_combo_box_set_active( GTK_COMBO_BOX(c_ent), l->inp - 1 );

    gtk_grid_attach( GTK_GRID(rej), gtk_label_new("La salida"),  0, 0, 1, 1 );
    gtk_grid_attach( GTK_GRID(rej), c_sal,                        1, 0, 1, 1 );
    gtk_grid_attach( GTK_GRID(rej), gtk_label_new("recibe de"),  0, 1, 1, 1 );
    gtk_grid_attach( GTK_GRID(rej), c_ent,                        1, 1, 1, 1 );

    for (i = 0; i < 3; i++) {
        int v = i == 0 ? l->b : i == 1 ? l->r : l->s;

        spin[i] = gtk_spin_button_new_with_range( 0, 52, 1 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(spin[i]), v );
        gtk_grid_attach( GTK_GRID(rej), gtk_label_new(ORD[i]), 0, 2 + i, 1, 1 );
        gtk_grid_attach( GTK_GRID(rej), spin[i],               1, 2 + i, 1, 1 );
    }

    sb = gtk_label_new(
        "ω(B)/δ(B)·B^b  —  s es el orden del numerador (s+1 omegas),\n"
        "r el del denominador. La pestaña Identificación los propone." );
    gtk_widget_set_halign( sb, GTK_ALIGN_START );
    gtk_grid_attach( GTK_GRID(rej), sb, 0, 5, 2, 1 );

    gtk_widget_show_all( d );
    resp = gtk_dialog_run( GTK_DIALOG(d) );

    if (resp == GTK_RESPONSE_ACCEPT) {
        l->out = gtk_combo_box_get_active( GTK_COMBO_BOX(c_sal) ) + 1;
        l->inp = gtk_combo_box_get_active( GTK_COMBO_BOX(c_ent) ) + 1;
        l->b   = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(spin[0]) );
        l->r   = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(spin[1]) );
        l->s   = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(spin[2]) );
    }
    gtk_widget_destroy( d );

    if (resp != GTK_RESPONSE_ACCEPT) return FALSE;

    if (l->out == l->inp) {
        preview_show_status( m, "«%s» no puede alimentarse a sí misma.",
                             nom_de( m, l->out ) );
        return FALSE;
    }
    return TRUE;
}

static void on_anadir( GtkButton *b, Mtram *m )
{
    NetLink l;

    if (m->c.n < 2) {
        preview_show_status( m, "Hacen falta al menos dos series." );
        return;
    }
    if (m->red.n >= NET_MAX_LINK) {
        preview_show_status( m, "La red lleva %d enlaces como mucho.",
                             NET_MAX_LINK );
        return;
    }

    l.out = 1; l.inp = 2; l.b = 1; l.r = 0; l.s = 0;
    if (!pide_enlace( m, &l, "Un enlace nuevo" )) return;

    m->red.lnk[m->red.n++] = l;
    red_refresca( m );
}

static void on_editar( GtkButton *b, Mtram *m )
{
    int i = fila_marcada( m );

    if (i < 0) { preview_show_status( m, "Marca primero un enlace." ); return; }
    if (!pide_enlace( m, &m->red.lnk[i], "El enlace" )) return;
    red_refresca( m );
}

static void on_quitar( GtkButton *b, Mtram *m )
{
    int i = fila_marcada( m );

    if (i < 0) return;
    memmove( &m->red.lnk[i], &m->red.lnk[i + 1],
             (m->red.n - i - 1) * sizeof(NetLink) );
    m->red.n--;
    red_refresca( m );
}

/* La estrella: lo que el motor hace cuando no se le da un .dag. */
static void on_estrella( GtkButton *b, Mtram *m )
{
    int j;

    if (m->c.n < 2) return;
    m->red.n = 0;
    for (j = 2; j <= m->c.n; j++) {
        m->red.lnk[m->red.n].out = 1;
        m->red.lnk[m->red.n].inp = j;
        m->red.lnk[m->red.n].b = 1;
        m->red.lnk[m->red.n].r = 0;
        m->red.lnk[m->red.n].s = 0;
        m->red.n++;
    }
    preview_show_status( m, "Estrella: las %d entradas apuntan a «%s». Es lo "
                            "que el motor supone si no se le da un .dag.",
                         m->c.n - 1, nom_de( m, 1 ) );
    red_refresca( m );
}

/* ------------------------------------------------------------------------ */
/* Abrir y guardar                                                           */
/* ------------------------------------------------------------------------ */

static gchar *elige( Mtram *m, GtkFileChooserAction accion, const char *titulo )
{
    GtkWidget     *d;
    GtkFileFilter *f;
    gchar         *p = NULL;

    d = gtk_file_chooser_dialog_new( titulo, GTK_WINDOW(m->ventana_p), accion,
            "_Cancelar", GTK_RESPONSE_CANCEL,
            accion == GTK_FILE_CHOOSER_ACTION_SAVE ? "_Guardar" : "_Abrir",
            GTK_RESPONSE_ACCEPT, NULL );

    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "Redes de transferencia (*.dag)" );
    gtk_file_filter_add_pattern( f, "*.dag" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    if (accion == GTK_FILE_CHOOSER_ACTION_SAVE) {
        gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), "red.dag" );
    }

    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT)
        p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
    gtk_widget_destroy( d );
    return p;
}

/* El motivo de un NetError, dicho en español. La frase en inglés la da
 * net_error_en y es la del motor; ésta es la de mtram. El HECHO es el mismo
 * porque viene del mismo lector.                                         */
static void por_que( Mtram *m, const NetError *e, const char *path )
{
    gchar *base = g_path_get_basename( path );

    switch (e->err) {
    case NET_ENOFILE:
        preview_show_status( m, "No puedo abrir %s.", base ); break;
    case NET_ESYNTAX:
        preview_show_status( m, "%s, línea %d: no entiendo «%s». El formato "
                                "es  SALIDA <- ENTRADA  b r s",
                             base, e->line, e->token ); break;
    case NET_EARROW:
        preview_show_status( m, "%s, línea %d: esperaba «<-» y hay «%s».",
                             base, e->line, e->token ); break;
    case NET_EUNKNOWN:
        preview_show_status( m, "%s, línea %d: no tengo cargada ninguna serie "
                                "que se llame «%s». Cárgala en la pestaña "
                                "Series, o corrige el nombre.",
                             base, e->line, e->token ); break;
    case NET_ESELF:
        preview_show_status( m, "%s, línea %d: «%s» no puede alimentarse a sí "
                                "misma.", base, e->line, e->token ); break;
    case NET_ENEG:
        preview_show_status( m, "%s, línea %d: b, r y s no pueden ser "
                                "negativos (%d %d %d).",
                             base, e->line, e->b, e->r, e->s ); break;
    case NET_EMANY:
        preview_show_status( m, "%s: más de %d enlaces.", base, NET_MAX_LINK );
        break;
    case NET_OK:
        break;
    }
    g_free( base );
}

static void on_abrir( GtkButton *b, Mtram *m )
{
    const char *nom[NET_MAX_SER + 1];
    NetLink     tmp[NET_MAX_LINK];
    NetError    e;
    gchar      *p;
    int         n;

    if (m->c.n < 1) {
        preview_show_status( m, "Carga primero las series: el .dag las nombra, "
                                "así que sin ellas no se puede leer." );
        return;
    }

    p = elige( m, GTK_FILE_CHOOSER_ACTION_OPEN, "Abrir una red" );
    if (!p) return;

    nombres( m, nom );
    n = net_read( p, nom, m->c.n, tmp, NET_MAX_LINK, &e );
    if (n < 0) {
        por_que( m, &e, p );
    } else {
        memcpy( m->red.lnk, tmp, n * sizeof(NetLink) );
        m->red.n = n;
        g_free( m->red.path );
        m->red.path = g_strdup( p );
        preview_show_status( m, "%d enlace%s leído%s de %s.",
                             n, n == 1 ? "" : "s", n == 1 ? "" : "s",
                             g_path_get_basename( p ) );
        red_refresca( m );
    }
    g_free( p );
}

static void on_guardar( GtkButton *b, Mtram *m )
{
    const char *nom[NET_MAX_SER + 1];
    gchar      *p;

    if (m->red.n == 0) {
        preview_show_status( m, "La red está vacía: no hay nada que guardar." );
        return;
    }

    p = elige( m, GTK_FILE_CHOOSER_ACTION_SAVE, "Guardar la red" );
    if (!p) return;

    nombres( m, nom );
    if (net_write( p, nom, m->red.lnk, m->red.n,
                   "red de transferencias, escrita por mtram" ) != 0)
        preview_show_status( m, "No pude escribir %s.", p );
    else {
        g_free( m->red.path );
        m->red.path = g_strdup( p );
        preview_show_status( m, "Escrito %s. El motor lo lee con  -n %s",
                             g_path_get_basename( p ),
                             g_path_get_basename( p ) );
    }
    g_free( p );
}

/* ------------------------------------------------------------------------ */

static void columna( GtkWidget *tv, const char *titulo, int col )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                               titulo, r, "text", col, NULL );

    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), c );
}

GtkWidget *red_pagina_new( Mtram *m )
{
    Red          *r = &m->red;
    GtkWidget    *caja, *barra, *b, *sc, *marco, *vb;
    GtkListStore *st;

    r->n = 0;
    r->path = NULL;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    /* --- los botones --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

#define BOTON(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON( "Añadir enlace", on_anadir,
           "Quién alimenta a quién, y con qué (b, r, s)." )
    BOTON( "Editar", on_editar, "" )
    BOTON( "Quitar", on_quitar, "" )
    BOTON( "Estrella", on_estrella,
           "Todas las entradas apuntando a la salida: es lo que el motor "
           "supone cuando no se le da un .dag." )
    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );
    BOTON( "Abrir .dag", on_abrir,
           "Se lee con el lector del motor: lo que mtram acepte es lo que "
           "acepta drtran." )
    BOTON( "Guardar .dag", on_guardar, "El fichero que el motor lee con -n." )
#undef BOTON

    /* --- la lista --- */
    st = gtk_list_store_new( R_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING );
    r->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( r->lista, "Salida",     R_SALIDA );
    columna( r->lista, "",           R_FLECHA );
    columna( r->lista, "Entrada",    R_ENTRADA );
    columna( r->lista, "b",          R_B );
    columna( r->lista, "r",          R_R );
    columna( r->lista, "s",          R_S );
    columna( r->lista, "",           R_NOTA );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), r->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    /* --- el veredicto --- */
    marco = gtk_frame_new( "¿Se puede estimar?" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    r->veredicto = gtk_label_new( "Carga al menos dos .pre en la pestaña "
                                  "Series." );
    gtk_widget_set_halign( r->veredicto, GTK_ALIGN_START );
    gtk_label_set_line_wrap( GTK_LABEL(r->veredicto), TRUE );
    gtk_label_set_selectable( GTK_LABEL(r->veredicto), TRUE );
    gtk_container_add( GTK_CONTAINER(vb), r->veredicto );
    gtk_container_add( GTK_CONTAINER(marco), vb );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    return caja;
}
