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

enum { M_NOMBRE, M_QUE, M_DICE, M_IDX, M_N };

/* Los grupos, en el orden en que se enseñan. Ya existian como columna; aqui
 * son la ESTRUCTURA, porque 67 renglones heterogeneos en una lista plana no se
 * recorren -- y lo que el analista toca de verdad son seis.              */
static const char *GRUPOS[] = {
    "transferencia", "ARMA del ruido", "deterministas",
    "medias", "varianzas", "covarianzas", "otros"
};
#define N_GRUPOS ((int)(sizeof GRUPOS / sizeof GRUPOS[0]))

/* De que grupo es un slot. Sale del nombre, que es como lo bautiza el motor. */
static const char *grupo_de( const char *n )
{
    if (!strncmp(n, "omega_d", 7) || !strncmp(n, "delta_d", 7)) return GRUPOS[2];
    if (!strncmp(n, "omega", 5)   || !strncmp(n, "delta", 5))   return GRUPOS[0];
    if (!strncmp(n, "phi_", 4)    || !strncmp(n, "theta_", 6))  return GRUPOS[1];
    if (!strncmp(n, "mu[", 3))     return GRUPOS[3];
    if (!strncmp(n, "log(var", 7)) return GRUPOS[4];
    if (!strncmp(n, "q[", 2))      return GRUPOS[5];
    return GRUPOS[6];
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

/* Cuantas covarianzas hay libres, y cuantas hay. */
static void covarianzas( const SlotTable *st, int *libres, int *total )
{
    int i;

    *libres = *total = 0;
    for (i = 1; i <= st->n; i++)
        if (!strncmp( st->name[i], "q[", 2 )) {
            (*total)++;
            if (st->kind[i] == SLOT_FREE) (*libres)++;
        }
}

/* Los enlaces contemporaneos con su covarianza libre: explican lo mismo dos
 * veces. Devuelve cuantos, y el primero en *k.                           */
static int colineales( Mtram *m, int *primero )
{
    int k, n = 0;

    for (k = 0; k < m->red.n; k++) {
        char nm[40];
        int  s1, s2;

        if (m->red.lnk[k].b != 0) continue;
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].out, m->red.lnk[k].inp );
        s1 = slots_find( &m->mod.st, nm );
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].inp, m->red.lnk[k].out );
        s2 = slots_find( &m->mod.st, nm );

        if ((s1 && m->mod.st.kind[s1] == SLOT_FREE) ||
            (s2 && m->mod.st.kind[s2] == SLOT_FREE)) {
            if (!n++ && primero) *primero = k;
        }
    }
    return n;
}

/* ------------------------------------------------------------------------ */
/* El arbol                                                                  */
/* ------------------------------------------------------------------------ */

static void refresca_lista( Mtram *m )
{
    Modelo       *M  = &m->mod;
    GtkTreeStore *st = GTK_TREE_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(M->lista) ) );
    GtkTreeIter   grupo, fila;
    char          dice[256];
    int           g, i;

    gtk_tree_store_clear( st );
    if (!M->vale) return;

    for (g = 0; g < N_GRUPOS; g++) {
        int hay = 0, libres = 0, puestos = 0;
        gchar *cab;

        /* Cuantos caen en este grupo, y cuantos de ellos se van a enseñar. */
        for (i = 1; i <= M->st.n; i++)
            if (grupo_de( M->st.name[i] ) == GRUPOS[g]) {
                hay++;
                if (M->st.kind[i] == SLOT_FREE) libres++;
                if (!M->solo || slots_line( &M->st, i, dice, sizeof dice ))
                    puestos++;
            }
        if (!hay || !puestos) continue;

        cab = g_strdup_printf( "%s (%d)", GRUPOS[g], hay );
        gtk_tree_store_append( st, &grupo, NULL );
        gtk_tree_store_set( st, &grupo,
            M_NOMBRE, cab,
            /* En las covarianzas, lo que importa del grupo es cuantas se han
             * liberado: nacen fijas en cero y liberarlas es la decision.  */
            M_QUE, g == 5 ? g_strdup_printf( "%d libre%s", libres,
                                             libres == 1 ? "" : "s" ) : "",
            M_IDX, 0,
            -1 );
        g_free( cab );

        for (i = 1; i <= M->st.n; i++) {
            if (grupo_de( M->st.name[i] ) != GRUPOS[g]) continue;
            if (!slots_line( &M->st, i, dice, sizeof dice )) {
                if (M->solo) continue;
                dice[0] = 0;
            }
            gtk_tree_store_append( st, &fila, &grupo );
            gtk_tree_store_set( st, &fila,
                M_NOMBRE, M->st.name[i],
                M_QUE,    que_es( M->st.kind[i] ),
                M_DICE,   dice,
                M_IDX,    i,
                -1 );
        }
    }

    /* Con el filtro puesto son pocas filas: se abren. Sin el, plegado, que es
     * lo que hace que 67 renglones quepan en la pantalla.               */
    if (M->solo) gtk_tree_view_expand_all( GTK_TREE_VIEW(M->lista) );
    else         gtk_tree_view_collapse_all( GTK_TREE_VIEW(M->lista) );
}

/* ------------------------------------------------------------------------ */
/* Los dos veredictos                                                        */
/*                                                                           */
/* El segundo es el primero que tiene que ELEGIR QUE CONTAR: lo que impide o  */
/* compromete la estimacion va antes que lo que solo informa.                 */
/* ------------------------------------------------------------------------ */

static void refresca_cuenta( Mtram *m )
{
    Modelo *M = &m->mod;
    int     libres, cl, ct, mal, primero = 0;

    if (!M->vale) {
        mtram_verdicto( M->ver_cuenta, MT_AMBAR,
            "Carga las series y define la red: los parámetros salen de ahí, "
            "no se escriben." );
        gtk_label_set_text( GTK_LABEL(M->ver_ojo), "" );
        return;
    }

    libres = slots_nfree( &M->st );
    covarianzas( &M->st, &cl, &ct );
    mal = colineales( m, &primero );

    mtram_verdicto( M->ver_cuenta, MT_VERDE,
        "%d parámetros · %d libres · %d fijos o atados",
        M->st.n, libres, M->st.n - libres );

    if (mal)
        mtram_verdicto( M->ver_ojo, MT_ROJO,
            "OJO — %s ← %s es contemporáneo (b=0) y su covarianza está libre%s",
            m->c.s[m->red.lnk[primero].out - 1]->ts.name,
            m->c.s[m->red.lnk[primero].inp - 1]->ts.name,
            mal > 1 ? " · y no es el único" : "" );
    else if (M->perdidas)
        mtram_verdicto( M->ver_ojo, MT_AMBAR,
            "%d restricción%s se quedó%s por el camino: nombraba%s un "
            "parámetro que este modelo ya no tiene",
            M->perdidas, M->perdidas == 1 ? "" : "es",
            M->perdidas == 1 ? "" : "n", M->perdidas == 1 ? "" : "n" );
    else
        mtram_verdicto( M->ver_ojo, MT_VERDE,
            "%d de %d covarianzas liberadas · nacen FIJAS en cero", cl, ct );
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
/* Sigma, como matriz y EDITABLE                                             */
/*                                                                           */
/* Sigma ES una matriz, y verla como matriz es ver su estructura de un golpe. */
/* Y aqui se pulsa, al reves que la matriz de operadores de la pagina Series: */
/* aquella es de solo lectura porque el operador viene del .pre y no se       */
/* decide ahi; esta SI es una decision del analista, asi que se decide donde  */
/* se ve. Liberar covarianzas es el uso mas comun del .cns --el m6-1 libera   */
/* tres de quince-- y buscarlas entre 67 renglones es absurdo.                */
/*                                                                           */
/* Va en DIALOGO y no en panel: editar Sigma es un acto deliberado y conviene */
/* poder cancelarlo entero. Un panel que se cierra al perder el foco no es    */
/* sitio para varios clics seguidos.                                          */
/* ------------------------------------------------------------------------ */

static void on_covarianzas( GtkButton *bt, Mtram *m )
{
    Modelo    *M = &m->mod;
    GtkWidget *d, *caja, *rej, *av;
    GtkWidget *bot[NET_MAX_SER + 1][NET_MAX_SER + 1];
    int        i, j, n = m->c.n;

    if (!M->vale) {
        preview_show_status( m, "Carga las series y define la red primero." );
        return;
    }

    d = gtk_dialog_new_with_buttons( "Covarianzas de las innovaciones",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL,
            "_Aceptar",  GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    gtk_box_set_spacing( GTK_BOX(caja), 10 );

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 3 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 3 );
    gtk_container_add( GTK_CONTAINER(caja), rej );

    /* Solo por debajo de la diagonal: q[i,j] con i > j, que es como las
     * nombra el motor y como las cuenta -- n(n-1)/2.                    */
    for (j = 1; j < n; j++) {
        GtkWidget *h = gtk_label_new( NULL );
        gchar     *mk = g_strdup_printf( "<b>%s</b>",
                            m->c.s[j - 1]->ts.name ? m->c.s[j - 1]->ts.name : "?" );

        gtk_label_set_markup( GTK_LABEL(h), mk );
        g_free( mk );
        gtk_grid_attach( GTK_GRID(rej), h, j, 0, 1, 1 );
    }

    for (i = 2; i <= n; i++) {
        GtkWidget *h = gtk_label_new( NULL );
        gchar     *mk = g_strdup_printf( "<b>%d %s</b>", i,
                            m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "?" );

        gtk_label_set_markup( GTK_LABEL(h), mk );
        g_free( mk );
        gtk_widget_set_halign( h, GTK_ALIGN_START );
        gtk_grid_attach( GTK_GRID(rej), h, 0, i - 1, 1, 1 );

        for (j = 1; j < i; j++) {
            char nm[40];
            int  k;

            snprintf( nm, sizeof nm, "q[%d,%d]", i, j );
            k = slots_find( &M->st, nm );
            if (!k) { bot[i][j] = NULL; continue; }

            bot[i][j] = gtk_toggle_button_new_with_label(
                            M->st.kind[k] == SLOT_FREE ? "libre" : "0" );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(bot[i][j]),
                                          M->st.kind[k] == SLOT_FREE );
            gtk_widget_set_tooltip_text( bot[i][j], nm );
            gtk_widget_set_size_request( bot[i][j], 56, -1 );
            gtk_grid_attach( GTK_GRID(rej), bot[i][j], j, i - 1, 1, 1 );
        }
    }

    av = gtk_label_new(
        "Las covarianzas nacen FIJAS en cero: la diagonal es el caso por "
        "defecto\ny liberar una es una decisión, no algo que se active en "
        "bloque.\nEl m6-1 no libera las quince de su sistema: libera tres." );
    gtk_widget_set_halign( av, GTK_ALIGN_START );
    gtk_container_add( GTK_CONTAINER(caja), av );

    gtk_widget_show_all( d );

    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        int cambios = 0;

        for (i = 2; i <= n; i++)
            for (j = 1; j < i; j++) {
                char nm[40];
                int  k, quiere;

                if (!bot[i][j]) continue;
                snprintf( nm, sizeof nm, "q[%d,%d]", i, j );
                k = slots_find( &M->st, nm );
                if (!k) continue;

                quiere = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(bot[i][j]) );
                if (quiere && M->st.kind[k] != SLOT_FREE) {
                    M->st.kind[k] = SLOT_FREE;  M->st.alias[k] = 0;  cambios++;
                } else if (!quiere && M->st.kind[k] != SLOT_FIXED) {
                    M->st.kind[k] = SLOT_FIXED; M->st.value[k] = 0.0; cambios++;
                }
            }
        gtk_widget_destroy( d );

        if (cambios) {
            refresca_lista( m );
            refresca_cuenta( m );
            preview_show_status( m, "%d covarianza%s cambiada%s.",
                                 cambios, cambios == 1 ? "" : "s",
                                 cambios == 1 ? "" : "s" );
        }
        return;
    }
    gtk_widget_destroy( d );
}

/* ------------------------------------------------------------------------ */
/* Los avisos                                                                */
/* ------------------------------------------------------------------------ */

static void on_avisos( GtkButton *b, Mtram *m )
{
    Modelo  *M = &m->mod;
    GString *t = g_string_new( NULL );
    int      k, n = 0;

    if (!M->vale) {
        g_string_append( t, "Carga las series y define la red." );
        goto pinta;
    }

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
            if (!n++)
                g_string_append( t, "CASI-COLINEALIDAD\n\n" );
            g_string_append_printf( t, "   %s ← %s  es contemporáneo (b=0) y "
                "su covarianza está libre\n",
                m->c.s[m->red.lnk[k].out - 1]->ts.name,
                m->c.s[m->red.lnk[k].inp - 1]->ts.name );
        }
    }

    if (!n) {
        g_string_append( t, "Ningún aviso.\n\n"
            "Aquí saldría la casi-colinealidad: un enlace contemporáneo (b=0)\n"
            "con su covarianza de innovaciones libre al mismo tiempo." );
        goto pinta;
    }

    g_string_append( t,
        "\nEn el retardo k = 0 las dos cosas explican EXACTAMENTE LO MISMO.\n"
        "Sólo se separan por cómo decae la covarianza cruzada en k > 0:\n"
        "   phi_X^k   si es transferencia\n"
        "   phi_N^k   si es covarianza\n\n"
        "Cuando los dos AR se parecen, la verosimilitud tiene una cresta casi\n"
        "plana: el ajuste apenas mejora mientras omega y la correlación se van\n"
        "a una esquina. Medido en IPC<-WTI (phi_X=0.30, phi_N=0.40): LR = 0.03,\n"
        "la correlación se va a -0.98, omega se multiplica por 9 y los t-ratios\n"
        "llegan a 2424.\n\n"
        "LA DOCTRINA DE LA ESCUELA ES USAR UNA DE LAS DOS, NO LAS DOS.\n"
        "El m6-1 tiene covarianzas fuera de la diagonal y NINGUNA estructura\n"
        "contemporánea. La tesis de Muñoz Polo (2001, §2.4) dice que la\n"
        "especificación de una relación bivariante «puede comenzar con la\n"
        "modificación de la matriz Sigma».\n\n"
        "El motor da este aviso DESPUÉS de estimar. Aquí se da antes, que es\n"
        "cuando sirve." );

pinta:
    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
    g_string_free( t, TRUE );
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

/* Las q[i,j] nombran a las series POR SU POSICION, y esa posicion no esta en
 * el fichero: esta en la linea de ordenes. Por eso los .cns de la escuela la
 * escriben en un comentario, y por eso conviene compararla.
 *
 * No es hipotetico: en el m6, m6.cns espera 1=P 2=EA 3=EP... y m6_net.cns
 * espera 1=EP 2=EI 3=EU... SON DISTINTOS, y abrir uno con el orden del otro
 * aplica las covarianzas a parejas que no son -- sin que nada lo diga, porque
 * el .cns solo lleva numeros.
 *
 * Es UN AVISO. No reordena nada: un comentario no manda sobre el analista.  */
static void avisa_orden( Mtram *m, const char *path )
{
    char     dice[64][SLOT_NAME];
    int      n = cns_orden_declarado( path, dice, 64 );
    GString *t;
    int      i, mal = 0;

    if ( n == 0 ) return;                 /* no lo declara: nada que decir */

    for ( i = 1; i <= n && i <= m->c.n; i++ )
        if ( g_ascii_strcasecmp( dice[i],
                 m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "" ) )
            mal++;

    if ( !mal && n == m->c.n ) return;     /* coincide: callarse */

    t = g_string_new( NULL );
    g_string_append_printf( t, "OJO — %s dice que espera este orden:\n   ",
                            g_path_get_basename( path ) );
    for ( i = 1; i <= n; i++ )
        g_string_append_printf( t, "%d=%s  ", i, dice[i] );

    g_string_append( t, "\n\ny las series cargadas van en este:\n   " );
    for ( i = 0; i < m->c.n; i++ )
        g_string_append_printf( t, "%d=%s  ", i + 1,
            m->c.s[i]->ts.name ? m->c.s[i]->ts.name : "?" );

    g_string_append( t,
        "\n\nLas q[i,j] nombran a las series por su POSICIÓN, así que con "
        "otro orden\nse aplican a parejas distintas de las que el fichero "
        "quería — y nada lo dice,\nporque el .cns sólo lleva números.\n\n"
        "Reordena en la pestaña Series si el fichero tiene razón. No se toca "
        "nada solo:\nun comentario no manda sobre el analista." );

    {
    GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(m->ventana_p),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, "%s", t->str );

    gtk_window_set_title( GTK_WINDOW(d), "El orden de las series" );
    gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
    }
    g_string_free( t, TRUE );
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
        avisa_orden( m, p );
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

static void on_solo( GtkToggleButton *b, Mtram *m )
{
    m->mod.solo = gtk_toggle_button_get_active( b );
    refresca_lista( m );
}

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
    GtkWidget    *caja, *barra, *b, *sc, *vb;
    GtkTreeStore *store;

    M->vale = FALSE;
    M->path = NULL;
    M->st.n = 0;
    M->solo = FALSE;

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
    BOTON( "Abrir…", on_abrir,
           "Un .cns. Se lee con el lector del motor: lo que mtram acepte es "
           "lo que acepta drtran." )
    BOTON( "Guardar…", on_guardar, "El fichero que el motor lee con -c." )
#undef BOTON

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    /* Lo que el .cns CONTIENE son los slots que dicen algo: en el m6, seis de
     * 67. Es la vista que casi siempre se quiere, porque es el fichero que se
     * va a escribir.                                                     */
    M->c_solo = gtk_check_button_new_with_label( "Sólo lo restringido" );
    gtk_widget_set_tooltip_text( M->c_solo,
        "Sólo los parámetros que dicen algo — que es lo que el .cns contiene. "
        "En el m6 son 6 de 67." );
    g_signal_connect( M->c_solo, "toggled", G_CALLBACK(on_solo), m );
    gtk_box_pack_start( GTK_BOX(barra), M->c_solo, FALSE, FALSE, 0 );

    /* A la derecha, los dos que ABREN algo. */
#define BOTON_DER(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON_DER( "Avisos…", on_avisos,
               "La casi-colinealidad: un enlace contemporáneo y su covarianza "
               "libre explican lo mismo dos veces." )
    BOTON_DER( "Covarianzas…", on_covarianzas,
               "Σ como matriz, y se pulsa para liberar o fijar. Es el uso más "
               "común del .cns." )
#undef BOTON_DER

    /* Un ARBOL, no una lista: 67 renglones heterogeneos en una lista plana no
     * se recorren. Plegado son seis filas.                              */
    store = gtk_tree_store_new( M_N, G_TYPE_STRING, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_INT );
    M->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) );
    columna( M->lista, "Parámetro", M_NOMBRE );
    columna( M->lista, "Es",        M_QUE );
    columna( M->lista, "Dice",      M_DICE );
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(M->lista), M_NOMBRE );
    gtk_tree_view_set_enable_tree_lines( GTK_TREE_VIEW(M->lista), TRUE );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), M->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    /* Dos lineas de altura fija. El segundo elige que contar. */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_widget_set_margin_top( vb, 2 );

    M->ver_cuenta = gtk_label_new( "Carga las series y define la red." );
    M->ver_ojo    = gtk_label_new( "" );
    gtk_widget_set_halign( M->ver_cuenta, GTK_ALIGN_START );
    gtk_widget_set_halign( M->ver_ojo,    GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(M->ver_cuenta), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(M->ver_ojo),    PANGO_ELLIPSIZE_END );

    gtk_box_pack_start( GTK_BOX(vb), M->ver_cuenta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), M->ver_ojo,    FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), vb, FALSE, FALSE, 0 );

    return caja;
}
