/*
 * modelo.c -- la pantalla del modelo: el .cns.
 *
 * LO QUE ESTA PANTALLA NO ES: un editor de texto con resaltado. El .cns no se
 * teclea a ciegas, porque sus nombres no son libres -- omega3[2] existe o no
 * existe segun el .dag y segun lo que traigan los .pre, y equivocarse de
 * nombre es el error mas facil de cometer y el mas tonto de diagnosticar.
 *
 * LO QUE ES: una vista de LA TABLA DE SLOTS. El modelo genera sus parametros
 * --tantos omega como diga s, tantos phi como el .pre deje libres-- y la
 * pantalla enseña esa lista y deja decir de cada uno lo que el .cns sabe
 * decir. Es la idea de TASTE llevada a donde ahora hace falta: alli el
 * formulario se desplegaba segun el orden (tecleas s y aparecen s+1 campos
 * OMEGA(j), TFMOD.PAS:149-177), aqui la LISTA es el modelo.
 *
 * Y CON LO QUE TASTE NO TENIA. En TASTE todo lo que declarabas por orden se
 * estimaba --npar := s+1+r, TFMOD.PAS:519-- y la unica forma de fijar un
 * coeficiente era bajar el orden. Aqui hay bandera por coeficiente, porque el
 * contrato lo exige: un parametro FIJO es ESPECIFICACION, no semilla.
 *
 * Las cinco formas del lenguaje, todas en el mismo sitio:
 *
 *     libre                    se estima
 *     fijo en un valor         es especificacion
 *     igual a otro             un grado de libertad en dos sitios
 *     producto de otros dos    numerador factorizado con MA compartida
 *     combinacion lineal       un factor FIJO (1-B) impone nu(1) = 0
 *
 * La tabla la construye lib/slots, que ES la del motor. mtram no puede ofrecer
 * un slot que el motor no tenga, ni escribir un .cns que el motor rechace.
 */

#include <string.h>
#include <stdlib.h>

#include "gui.h"
#include "previewhost.h"
#include "slots.h"

enum { M_NOMBRE, M_QUE, M_DICE, M_GRUPO, M_IDX, M_N };

/* De que parte del modelo es un slot. Sale del nombre, que es como lo bautiza
 * el motor, y sirve para que la lista no sea una sopa de 67 renglones.     */
static const char *grupo_de( const char *n )
{
    if (!strncmp(n, "omega_d", 7) || !strncmp(n, "delta_d", 7))
        return "deterministas";
    if (!strncmp(n, "omega", 5) || !strncmp(n, "delta", 5))
        return "transferencia";
    if (!strncmp(n, "phi_", 4) || !strncmp(n, "theta_", 6))
        return "ARMA del ruido";
    if (!strncmp(n, "mu[", 3))            return "medias";
    if (!strncmp(n, "log(var", 7))        return "varianzas";
    if (!strncmp(n, "q[", 2))             return "covarianzas";
    return "otros";
}

static const char *que_es( int kind )
{
    switch (kind) {
    case SLOT_FREE:    return "libre";
    case SLOT_FIXED:   return "FIJO";
    case SLOT_ALIAS:   return "compartido";
    case SLOT_PRODUCT: return "producto";
    case SLOT_LINCOMB: return "comb. lineal";
    }
    return "?";
}

/* ------------------------------------------------------------------------ */
/* Construir la tabla a partir de lo que hay cargado                         */
/* ------------------------------------------------------------------------ */

/* La tabla se reconstruye entera cada vez que cambia algo --cargar una serie,
 * tocar un enlace-- porque su forma DEPENDE de eso. Pero lo que el analista
 * haya dicho de cada parametro no se pierde por el camino: se lleva a la tabla
 * nueva emparejando por nombre. Perderlo en silencio seria la peor forma de
 * perderlo.                                                               */
static void construye( Mtram *m )
{
    Modelo          *M = &m->mod;
    struct Tusmodel  Tm[GUI_MAX_SER + 1];
    SlotTable        viejo;
    gboolean         habia;
    int              i;

    habia   = M->vale;
    if (habia) viejo = M->st;

    M->vale = FALSE;
    M->st.n = 0;
    M->perdidas = 0;
    if (m->c.n < 2) return;

    for (i = 1; i <= m->c.n; i++) Tm[i] = m->c.s[i - 1]->tm;

    /* fix = NULL: mtram no clava nada desde fuera. Lo que el .pre declare
     * fijo se respeta, que es lo unico que hay que respetar.              */
    slots_build( &M->st, Tm, m->c.n, m->red.lnk, m->red.n, NULL );
    M->vale = TRUE;

    /* Salvo que las series se hayan MOVIDO: los nombres llevan la posicion
     * dentro --q[3,2], phi_2[B^1], mu[4]-- asi que despues de reordenar el
     * mismo nombre significa otra cosa, y emparejar por nombre seria
     * exactamente lo contrario de conservar.                            */
    if (habia && !M->orden_cambio)
        slots_carry( &M->st, &viejo, &M->perdidas );

    M->orden_cambio = FALSE;
}

/* ------------------------------------------------------------------------ */

static void refresca_lista( Mtram *m )
{
    Modelo       *M  = &m->mod;
    GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(
                                              GTK_TREE_VIEW(M->lista) ) );
    GtkTreeIter   it;
    char          dice[256];
    int           i;

    gtk_list_store_clear( store );
    for (i = 1; i <= M->st.n; i++) {
        const char *n = M->st.name[i];

        if (!slots_line( &M->st, i, dice, sizeof dice )) dice[0] = '\0';

        gtk_list_store_append( store, &it );
        gtk_list_store_set( store, &it,
            M_NOMBRE, n,
            M_QUE,    que_es( M->st.kind[i] ),
            M_DICE,   dice,
            M_GRUPO,  grupo_de( n ),
            M_IDX,    i,
            -1 );
    }
}

static void refresca_cuenta( Mtram *m )
{
    Modelo  *M = &m->mod;
    GString *t = g_string_new( NULL );
    int      libres, i, covar_libres = 0;

    if (!M->vale) {
        gtk_label_set_text( GTK_LABEL(M->cuenta),
            "Carga las series y define la red: los parámetros salen de ahí, "
            "no se escriben." );
        g_string_free( t, TRUE );
        return;
    }

    libres = slots_nfree( &M->st );
    for (i = 1; i <= M->st.n; i++)
        if (!strncmp( M->st.name[i], "q[", 2 ) && M->st.kind[i] == SLOT_FREE)
            covar_libres++;

    g_string_append_printf( t,
        "%d parámetros estructurales.   %d libres,  %d fijos o atados.\n",
        M->st.n, libres, M->st.n - libres );

    if (M->perdidas)
        g_string_append_printf( t,
            "\n%d restricción%s se quedó%s por el camino: nombraba%s un "
            "parámetro que este modelo\nya no tiene. Revísalas antes de "
            "estimar.\n",
            M->perdidas, M->perdidas == 1 ? "" : "es",
            M->perdidas == 1 ? "" : "n", M->perdidas == 1 ? "" : "n" );

    g_string_append_printf( t,
        "\nDe las %d covarianzas de las innovaciones hay %d liberada%s. "
        "Nacen FIJAS en cero:\nla diagonal es el caso por defecto y liberar "
        "una es una decisión, no algo que se active en bloque.\n"
        "El m6-1 no libera las 15 de su sistema: libera tres.",
        (m->c.n * (m->c.n - 1)) / 2, covar_libres,
        covar_libres == 1 ? "" : "s" );

    /* El aviso de casi-colinealidad, que el motor da despues de estimar y aqui
     * se puede dar ANTES, que es cuando sirve.                            */
    {
    int k, avisos = 0;

    for (k = 0; k < m->red.n; k++) {
        char nm[40];
        int  s1, s2;

        if (m->red.lnk[k].b != 0) continue;
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].out, m->red.lnk[k].inp );
        s1 = slots_find( &M->st, nm );
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].inp, m->red.lnk[k].out );
        s2 = slots_find( &M->st, nm );

        if ((s1 && M->st.kind[s1] == SLOT_FREE) ||
            (s2 && M->st.kind[s2] == SLOT_FREE)) {
            if (!avisos++)
                g_string_append( t, "\n\nOJO — casi-colinealidad:" );
            g_string_append_printf( t,
                "\n   %s ← %s es CONTEMPORÁNEO (b = 0) y su covarianza está "
                "libre a la vez.",
                m->c.s[m->red.lnk[k].out - 1]->ts.name,
                m->c.s[m->red.lnk[k].inp - 1]->ts.name );
        }
    }
    if (avisos)
        g_string_append( t,
            "\n   En el retardo k = 0 las dos explican exactamente lo mismo; "
            "sólo se separan por cómo\n   decae la covarianza cruzada en k > 0 "
            "(φ_X^k la transferencia, φ_N^k la covarianza).\n"
            "   Si los dos AR se parecen, la verosimilitud tiene una cresta "
            "casi plana: el ajuste apenas\n   mejora mientras ω y la "
            "correlación se van a una esquina con t enormes. Usa UNA de las "
            "dos." );
    }

    gtk_label_set_text( GTK_LABEL(M->cuenta), t->str );
    g_string_free( t, TRUE );
}

void modelo_refresca( Mtram *m )
{
    construye( m );
    refresca_lista( m );
    refresca_cuenta( m );
}

/* ------------------------------------------------------------------------ */
/* Cambiar lo que dice un slot                                               */
/* ------------------------------------------------------------------------ */

static int slot_marcado( Mtram *m )
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(
                                GTK_TREE_VIEW(m->mod.lista) );
    GtkTreeModel *mod;
    GtkTreeIter   it;
    int           i = 0;

    if (!gtk_tree_selection_get_selected( sel, &mod, &it )) return 0;
    gtk_tree_model_get( mod, &it, M_IDX, &i, -1 );
    return i;
}

static void on_libre( GtkButton *b, Mtram *m )
{
    int i = slot_marcado( m );

    if (!i) { preview_show_status( m, "Marca primero un parámetro." ); return; }
    m->mod.st.kind[i]  = SLOT_FREE;
    m->mod.st.alias[i] = 0;
    modelo_refresca( m );
    preview_show_status( m, "%s queda libre: se estima.", m->mod.st.name[i] );
}

static void on_fijar( GtkButton *b, Mtram *m )
{
    int        i = slot_marcado( m );
    GtkWidget *d, *e, *caja, *av;
    double     v = 0.0;

    if (!i) { preview_show_status( m, "Marca primero un parámetro." ); return; }

    d = gtk_dialog_new_with_buttons( "Fijar un parámetro",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Fijar", GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    gtk_container_add( GTK_CONTAINER(caja),
        gtk_label_new( m->mod.st.name[i] ) );
    e = gtk_entry_new();
    gtk_entry_set_text( GTK_ENTRY(e),
        m->mod.st.kind[i] == SLOT_FIXED ? g_strdup_printf( "%g",
            (double) m->mod.st.value[i] ) : "0" );
    gtk_entry_set_activates_default( GTK_ENTRY(e), TRUE );
    gtk_container_add( GTK_CONTAINER(caja), e );

    av = gtk_label_new( "Un parámetro fijo es ESPECIFICACIÓN, no una semilla:\n"
                        "no se estima, y su valor es parte de lo que el modelo\n"
                        "afirma." );
    gtk_container_add( GTK_CONTAINER(caja), av );

    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_ACCEPT );
    gtk_widget_show_all( d );

    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        v = g_ascii_strtod( gtk_entry_get_text( GTK_ENTRY(e) ), NULL );
        m->mod.st.kind[i]  = SLOT_FIXED;
        m->mod.st.value[i] = v;
        gtk_widget_destroy( d );
        modelo_refresca( m );
        preview_show_status( m, "%s = %g, fijo.", m->mod.st.name[i], v );
        return;
    }
    gtk_widget_destroy( d );
}

/* Compartir: un grado de libertad en dos sitios. Sólo se ofrecen los slots que
 * de verdad hay, que es la mitad de la gracia de tener la tabla.          */
static void on_compartir( GtkButton *b, Mtram *m )
{
    int        i = slot_marcado( m );
    GtkWidget *d, *cb, *caja;
    int        k, n = 0;
    int       *idx;

    if (!i) { preview_show_status( m, "Marca primero un parámetro." ); return; }

    idx = g_new0( int, m->mod.st.n + 1 );
    d = gtk_dialog_new_with_buttons( "Compartir con otro parámetro",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Compartir", GTK_RESPONSE_ACCEPT,
            NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    gtk_container_add( GTK_CONTAINER(caja), gtk_label_new(
        g_strdup_printf( "%s  =", m->mod.st.name[i] ) ) );

    cb = gtk_combo_box_text_new();
    for (k = 1; k <= m->mod.st.n; k++) {
        if (k == i) continue;
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(cb),
                                        m->mod.st.name[k] );
        idx[n++] = k;
    }
    if (n) gtk_combo_box_set_active( GTK_COMBO_BOX(cb), 0 );
    gtk_container_add( GTK_CONTAINER(caja), cb );

    gtk_container_add( GTK_CONTAINER(caja), gtk_label_new(
        "Los dos pasan a ser el MISMO parámetro: un solo grado de\n"
        "libertad, estimado una vez y usado en dos sitios." ) );

    gtk_widget_show_all( d );
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT && n) {
        int a = gtk_combo_box_get_active( GTK_COMBO_BOX(cb) );
        int j = idx[a];

        /* seguir la cadena hasta el representante, como hace el motor */
        while (m->mod.st.kind[j] == SLOT_ALIAS) j = m->mod.st.alias[j];
        if (j == i)
            preview_show_status( m, "%s no puede compartirse consigo mismo.",
                                 m->mod.st.name[i] );
        else {
            m->mod.st.kind[i]  = SLOT_ALIAS;
            m->mod.st.alias[i] = j;
            preview_show_status( m, "%s y %s son ahora el mismo parámetro.",
                                 m->mod.st.name[i], m->mod.st.name[j] );
        }
    }
    gtk_widget_destroy( d );
    g_free( idx );
    modelo_refresca( m );
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
    gtk_file_filter_set_name( f, "Restricciones (*.cns)" );
    gtk_file_filter_add_pattern( f, "*.cns" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    if (accion == GTK_FILE_CHOOSER_ACTION_SAVE) {
        gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), "modelo.cns" );
    }
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT)
        p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
    gtk_widget_destroy( d );
    return p;
}

/* La frase de mtram. El motor tiene la suya, en inglés (cns_error_en); el
 * hecho es el mismo porque viene del mismo lector.                        */
static void por_que( Mtram *m, const CnsError *e, const char *path )
{
    gchar *base = g_path_get_basename( path );

    switch (e->err) {
    case CNS_ENOFILE:
        preview_show_status( m, "No puedo abrir %s.", base ); break;
    case CNS_EUNKNOWN:
        preview_show_status( m, "%s, línea %d: este modelo no tiene ningún "
                                "parámetro «%s». Mira la lista: los nombres "
                                "los pone el modelo, no se eligen.",
                             base, e->line, e->token ); break;
    case CNS_EOPERAND:
        preview_show_status( m, "%s, línea %d: en «%s» hay un operando que no "
                                "es un parámetro de este modelo.",
                             base, e->line, e->token ); break;
    case CNS_ESELF:
        preview_show_status( m, "%s, línea %d: «%s» no puede definirse en "
                                "función de sí mismo.",
                             base, e->line, e->lhs ); break;
    case CNS_ELC:
        preview_show_status( m, "%s, línea %d: no entiendo la combinación "
                                "lineal «%s». Es  a + b - c,  con cada término "
                                "un parámetro o un producto de dos.",
                             base, e->line, e->token ); break;
    case CNS_EPARSE:
        preview_show_status( m, "%s, línea %d: no entiendo «%s = %s». A la "
                                "derecha va: free, un número, otro parámetro, "
                                "a * b, o una suma.",
                             base, e->line, e->lhs, e->token ); break;
    case CNS_OK:
        break;
    }
    g_free( base );
}

static void on_abrir( GtkButton *b, Mtram *m )
{
    CnsError e;
    gchar   *p;
    int      nc;

    if (!m->mod.vale) {
        preview_show_status( m, "Carga las series y define la red primero: el "
                                ".cns nombra parámetros que salen de ahí." );
        return;
    }

    p = elige( m, GTK_FILE_CHOOSER_ACTION_OPEN, "Abrir restricciones" );
    if (!p) return;

    construye( m );                       /* partir de la tabla limpia */
    nc = cns_read( p, &m->mod.st, &e );
    if (nc < 0) {
        por_que( m, &e, p );
        construye( m );                   /* no dejar media aplicada   */
    } else {
        g_free( m->mod.path );
        m->mod.path = g_strdup( p );
        preview_show_status( m, "%d restricción%s de %s.",
                             nc, nc == 1 ? "" : "es", g_path_get_basename( p ) );
    }
    refresca_lista( m );
    refresca_cuenta( m );
    g_free( p );
}

static void on_guardar( GtkButton *b, Mtram *m )
{
    gchar *p;
    int    n;

    if (!m->mod.vale) return;

    p = elige( m, GTK_FILE_CHOOSER_ACTION_SAVE, "Guardar las restricciones" );
    if (!p) return;

    n = cns_write( p, &m->mod.st, "restricciones escritas por mtram" );
    if (n < 0)
        preview_show_status( m, "No pude escribir %s.", p );
    else {
        g_free( m->mod.path );
        m->mod.path = g_strdup( p );
        preview_show_status( m, "%d línea%s en %s. El motor lo lee con  -c %s",
                             n, n == 1 ? "" : "s", g_path_get_basename( p ),
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

    gtk_tree_view_column_set_resizable( c, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), c );
}

GtkWidget *modelo_pagina_new( Mtram *m )
{
    Modelo       *M = &m->mod;
    GtkWidget    *caja, *barra, *b, *sc, *marco, *vb;
    GtkListStore *store;

    M->vale = FALSE;
    M->path = NULL;
    M->st.n = 0;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

#define BOTON(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON( "Libre", on_libre,
           "Se estima. Las covarianzas q[i,j] nacen fijas en cero: liberarlas "
           "es una decisión." )
    BOTON( "Fijar…", on_fijar,
           "Un parámetro fijo es ESPECIFICACIÓN, no una semilla: no se estima." )
    BOTON( "Compartir…", on_compartir,
           "Un solo grado de libertad, estimado una vez y usado en dos sitios." )
    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );
    BOTON( "Abrir .cns", on_abrir,
           "Se lee con el lector del motor: lo que mtram acepte es lo que "
           "acepta drtran." )
    BOTON( "Guardar .cns", on_guardar, "El fichero que el motor lee con -c." )
#undef BOTON

    store = gtk_list_store_new( M_N, G_TYPE_STRING, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT );
    M->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) );
    columna( M->lista, "Parámetro", M_NOMBRE );
    columna( M->lista, "Es",        M_QUE );
    columna( M->lista, "Dice",      M_DICE );
    columna( M->lista, "De",        M_GRUPO );
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(M->lista), M_NOMBRE );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), M->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    marco = gtk_frame_new( "Los parámetros que el modelo tiene" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    M->cuenta = gtk_label_new( "Carga las series y define la red." );
    gtk_widget_set_halign( M->cuenta, GTK_ALIGN_START );
    gtk_label_set_line_wrap( GTK_LABEL(M->cuenta), TRUE );
    gtk_label_set_selectable( GTK_LABEL(M->cuenta), TRUE );
    gtk_container_add( GTK_CONTAINER(vb), M->cuenta );
    gtk_container_add( GTK_CONTAINER(marco), vb );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    return caja;
}
