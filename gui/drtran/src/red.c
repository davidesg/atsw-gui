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

enum { R_SALIDA, R_FLECHA, R_ENTRADA, R_B, R_R, R_S, R_PAR, R_NOTA, R_N };

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

/* El nombre CON su numero, "1 EP". El numero es el mismo de la pagina Series
 * y el de q[i,j] en el .cns; el nombre solo dejaria a las dos paginas
 * hablando idiomas distintos, y el numero solo --"1 <- 2"-- seria ilegible.
 * Devuelve un puntero a un corro de buffers: vale hasta la octava llamada. */
static const char *nom_num( Mtram *m, int i )
{
    static char b[8][64];
    static int  t = 0;

    t = (t + 1) % 8;
    if (i < 1 || i > m->c.n) snprintf( b[t], sizeof b[t], "?" );
    else snprintf( b[t], sizeof b[t], "%d %s", i, nom_de( m, i ) );
    return b[t];
}

/* ------------------------------------------------------------------------ */
/* Los dos veredictos                                                        */
/*                                                                           */
/* De UNA LINEA. El marco que habia aqui escribia de 8 a 12 lineas SEGUN EL  */
/* CASO --el del ciclo es el mas largo porque es el que mas hay que          */
/* explicar-- asi que la lista DABA SALTOS mientras se editaba la red. Un    */
/* sitio donde se trabaja no puede moverse bajo la mano.                     */
/* ------------------------------------------------------------------------ */

/* Los tres numeros de cada serie: cuantos enlaces entran, cuantos salen, y de
 * ahi su papel. Es lo que distingue una RED de una estrella y hoy no se ve. */
static void papeles( Mtram *m, int *intermedias, int *sueltas, int *par )
{
    Red *r = &m->red;
    int  i, k;

    *intermedias = *sueltas = *par = 0;

    for (i = 1; i <= m->c.n; i++) {
        int ent = net_indegree( r->lnk, r->n, i );
        int sal = net_outdegree( r->lnk, r->n, i );

        if (ent > 0 && sal > 0) (*intermedias)++;
        if (ent == 0 && sal == 0) (*sueltas)++;
    }
    for (k = 0; k < r->n; k++) *par += r->lnk[k].s + 1 + r->lnk[k].r;
}

static void refresca_veredicto( Mtram *m )
{
    Red     *r = &m->red;
    int      topo[NET_MAX_SER + 1];
    int      ciclo[NET_MAX_SER + 2], nc = 0;
    int      intermedias, sueltas, par, i;

    if (m->c.n < 2) {
        mtram_verdicto( r->ver_topo, MT_AMBAR,
            "Carga al menos dos .pre en la pestaña Series." );
        gtk_label_set_text( GTK_LABEL(r->ver_forma), "" );
        return;
    }
    if (r->n == 0) {
        mtram_verdicto( r->ver_topo, MT_AMBAR,
            "La red está vacía · sin enlaces esto es la homologación "
            "con fue, no un modelo de transferencia" );
        mtram_verdicto( r->ver_forma, MT_AMBAR,
            "Añade un enlace, o pulsa «Estrella»: todas las entradas a la "
            "salida, que es lo que el motor supone" );
        return;
    }

    papeles( m, &intermedias, &sueltas, &par );

    /* --- el ciclo, que es lo unico que impide estimar ------------------- */
    if (!net_topo( r->lnk, r->n, m->c.n, topo )) {
        GString *c = g_string_new( "CICLO:  " );

        if (net_cycle( r->lnk, r->n, m->c.n, ciclo, &nc ))
            for (i = 0; i < nc; i++)
                g_string_append_printf( c, "%s%s", i ? " → " : "",
                                        nom_de( m, ciclo[i] ) );
        else
            g_string_append( c, "la red no admite orden de construcción" );

        mtram_verdicto( r->ver_topo, MT_ROJO,
            "%s · el sistema es simultáneo — esto es drvarma",
            c->str );
        mtram_verdicto( r->ver_forma, MT_AMBAR,
            "Mira la CCF del enlace que lo cierra: si sus retardos negativos "
            "están dentro de la banda, ese enlace sobra" );
        g_string_free( c, TRUE );
        return;
    }

    /* --- se puede ------------------------------------------------------- */
    {
    GString *o = g_string_new( NULL );

    for (i = 1; i <= m->c.n; i++)
        g_string_append_printf( o, "%s%s", i > 1 ? " → " : "",
                                nom_de( m, topo[i] ) );

    mtram_verdicto( r->ver_topo, MT_VERDE,
        "Acíclica · orden  %s · %d enlace%s, %d parámetro%s",
        o->str, r->n, r->n == 1 ? "" : "s", par, par == 1 ? "" : "s" );
    g_string_free( o, TRUE );
    }

    if (intermedias)
        mtram_verdicto( r->ver_forma, MT_VERDE,
            "%d serie%s %s salida Y entrada: es una RED, no una estrella "
            "· %d suelta%s",
            intermedias, intermedias == 1 ? "" : "s",
            intermedias == 1 ? "es" : "son", sueltas, sueltas == 1 ? "" : "s" );
    else
        mtram_verdicto( r->ver_forma, MT_AMBAR,
            "Ninguna serie es salida y entrada a la vez: esto es una ESTRELLA "
            "· %d suelta%s", sueltas, sueltas == 1 ? "" : "s" );
}

/* ------------------------------------------------------------------------ */
/* Los dos paneles                                                           */
/* ------------------------------------------------------------------------ */

static void on_orden( GtkButton *b, Mtram *m )
{
    Red     *r = &m->red;
    GString *t = g_string_new( NULL );
    int      topo[NET_MAX_SER + 1], ciclo[NET_MAX_SER + 2], nc = 0, i;

    if (m->c.n < 2 || r->n == 0) {
        g_string_append( t, "Carga las series y define al menos un enlace." );
        goto pinta;
    }

    g_string_append( t,
        "El motor resuelve el sistema por RECURSIÓN: una serie sólo se puede\n"
        "construir después de TODAS las que la alimentan. Ese orden es esto.\n\n" );

    if (net_topo( r->lnk, r->n, m->c.n, topo )) {
        for (i = 1; i <= m->c.n; i++)
            g_string_append_printf( t, "   %d. %s\n", i, nom_num( m, topo[i] ) );
        g_string_append( t,
            "\nCada una va después de todo lo que le entra. Con este orden el\n"
            "sistema se triangulariza y la verosimilitud es la exacta." );
    } else {
        g_string_append( t, "NO HAY ORDEN: la red tiene un ciclo.\n\n" );
        if (net_cycle( r->lnk, r->n, m->c.n, ciclo, &nc )) {
            g_string_append( t, "   " );
            for (i = 0; i < nc; i++)
                g_string_append_printf( t, "%s%s", i ? " → " : "",
                                        nom_de( m, ciclo[i] ) );
            g_string_append( t, "\n\n" );
        }
        g_string_append( t,
            "Con un ciclo ese orden no existe: el sistema no se puede\n"
            "triangularizar restando transferencias, y drtran se niega a\n"
            "estimar en vez de devolver algo sin sentido.\n\n"
            "NO ES UN FALLO DE ESCRITURA. Es un modelo que no es de este\n"
            "escalón: un sistema simultáneo se estima con drvarma (sima).\n\n"
            "Si crees que el ciclo no debería estar, mira la CCF del enlace\n"
            "que lo cierra: si sus retardos negativos están dentro de la\n"
            "banda, ese enlace sobra." );
    }

pinta:
    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
    g_string_free( t, TRUE );
}

static void on_series( GtkButton *b, Mtram *m )
{
    Red     *r = &m->red;
    GString *t = g_string_new( NULL );
    int      i;

    if (m->c.n < 1) {
        g_string_append( t, "Carga las series." );
        goto pinta;
    }

    g_string_append( t, "              entra   sale   papel\n" );
    for (i = 1; i <= m->c.n; i++) {
        int ent = net_indegree( r->lnk, r->n, i );
        int sal = net_outdegree( r->lnk, r->n, i );
        const char *papel;

        if (ent && sal)      papel = "INTERMEDIA";
        else if (ent)        papel = "salida final";
        else if (sal)        papel = "entrada pura";
        else                 papel = "suelta";

        g_string_append_printf( t, "   %-10s  %3d    %3d   %s\n",
                                nom_num( m, i ), ent, sal, papel );
    }

    g_string_append( t,
        "\nUna serie con ENTRA > 0 y SALE > 0 es lo que hace que esto sea una\n"
        "RED: su ecuación se estima Y alimenta a otra. Si no hay ninguna, lo\n"
        "que hay es una estrella, que es lo que el motor supone por omisión.\n\n"
        "Una SUELTA no aparece en ningún enlace: el motor la estima igual\n"
        "--entra en el VARMA-- pero no recibe ni da transferencia. Si está de\n"
        "más, quítala en la pestaña Series." );

pinta:
    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
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
            R_SALIDA,  nom_num( m, l->out ),
            R_FLECHA,  "←",
            R_ENTRADA, nom_num( m, l->inp ),
            R_B,       l->b,
            R_R,       l->r,
            R_S,       l->s,
            R_PAR,     l->s + 1 + l->r,
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
    red_edita_enlace( m, i );
}

/* El mismo dialogo, desde fuera: la pagina Modelo enseña la estructura de cada
 * transferencia y tiene que poder cambiarla sin mandar al analista a otra
 * pestaña a buscar la misma fila.                                       */
gboolean red_edita_enlace( Mtram *m, int k )
{
    if (k < 0 || k >= m->red.n) return FALSE;
    if (!pide_enlace( m, &m->red.lnk[k], "El enlace" )) return FALSE;
    red_refresca( m );
    modelo_refresca( m );
    return TRUE;
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
    GtkWidget    *caja, *barra, *b, *sc, *vb;
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

    BOTON( "Nuevo…", on_anadir,
           "Quién alimenta a quién, y con qué (b, r, s)." )
    BOTON( "Editar…", on_editar, "Los órdenes del enlace marcado." )
    BOTON( "Quitar", on_quitar, "" )
    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );
    BOTON( "Estrella", on_estrella,
           "Todas las entradas apuntando a la salida: es lo que el motor "
           "supone cuando no se le da un .dag." )
    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );
    BOTON( "Abrir…", on_abrir,
           "Un .dag. Se lee con el lector del motor: lo que mtram acepte es "
           "lo que acepta drtran." )
    BOTON( "Guardar…", on_guardar, "El fichero que el motor lee con -n." )
#undef BOTON

    /* A la derecha, los dos que ABREN algo. */
#define BOTON_DER(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON_DER( "Series…", on_series,
               "Cuántos enlaces entran y salen de cada serie, y qué papel "
               "hace: eso es lo que distingue una red de una estrella." )
    BOTON_DER( "Orden…", on_orden,
               "El orden de construcción, y por qué el motor lo necesita." )
#undef BOTON_DER

    /* --- la lista --- */
    st = gtk_list_store_new( R_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
                             G_TYPE_STRING );
    r->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( r->lista, "Salida",  R_SALIDA );
    columna( r->lista, "",        R_FLECHA );
    columna( r->lista, "Entrada", R_ENTRADA );
    columna( r->lista, "b",       R_B );
    columna( r->lista, "r",       R_R );
    columna( r->lista, "s",       R_S );
    columna( r->lista, "par",     R_PAR );
    columna( r->lista, "",        R_NOTA );
    gtk_widget_set_tooltip_text( r->lista,
        "b el retardo puro, r el denominador, s el numerador. «par» = s+1+r "
        "son los parámetros que ese enlace mete en el modelo." );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), r->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    /* --- el veredicto --- */
    /* Dos lineas de altura FIJA. Todo lo demas del alto es de la lista. */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_widget_set_margin_top( vb, 2 );

    r->ver_topo  = gtk_label_new( "Carga al menos dos .pre en la pestaña Series." );
    r->ver_forma = gtk_label_new( "" );
    gtk_widget_set_halign( r->ver_topo,  GTK_ALIGN_START );
    gtk_widget_set_halign( r->ver_forma, GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(r->ver_topo),  PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(r->ver_forma), PANGO_ELLIPSIZE_END );

    gtk_box_pack_start( GTK_BOX(vb), r->ver_topo,  FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), r->ver_forma, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), vb, FALSE, FALSE, 0 );

    return caja;
}
