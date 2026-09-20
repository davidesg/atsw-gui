/*
 * identifica.c -- la pantalla de identificacion.
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
 *
 * UNA FILA POR ENTRADA CANDIDATA, NO POR ENLACE. Identificar es decidir CUALES
 * merecen estar en la red, asi que se calculan todas las CCF y se comparan de
 * un vistazo. Con un combo habia que ir una por una recordando de memoria lo
 * que decia la anterior -- y esa comparacion ES la decision de esta pantalla.
 *
 * Y LA COLUMNA QUE MANDA ES "neg". Los retardos negativos fuera de banda son
 * el contraste de exogeneidad, y es el que decide si ese enlace puede existir
 * siquiera. Va en columna, no escondido tras una seleccion, porque es lo que
 * hace que una fila merezca mirarse.
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

enum { I_ENT, I_B, I_S, I_KMAX, I_NEG, I_P, I_NOTA, I_IDX, I_N };

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
/* Calcular: una CCF por entrada candidata                                   */
/* ------------------------------------------------------------------------ */

/* Lo que se lee del grafico, que es lo que va a las columnas. */
static void lee( IdentUno *u )
{
    int k;

    u->b = u->ultimo = -1;
    u->kmax = 0;
    u->neg  = 0;

    for (k = 0; k <= u->nlags; k++)
        if (fabs( u->ccf[u->nlags + k] ) > u->banda) {
            if (u->b < 0) u->b = k;
            u->ultimo = k;
        }
    for (k = 0; k <= u->nlags; k++)
        if (fabs( u->ccf[u->nlags + k] ) > fabs( u->ccf[u->nlags + u->kmax] ))
            u->kmax = k;

    /* El contraste de exogeneidad: si la salida antecede a la entrada, el
     * modelo de transferencia no se sostiene y no hay (b,s) que lo arregle. */
    for (k = 1; k <= u->nlags; k++)
        if (fabs( u->ccf[u->nlags - k] ) > u->banda) u->neg++;

    u->s = u->b >= 0 ? u->ultimo - u->b : -1;
}

static void calcula( Mtram *m )
{
    Ident *id = &m->id;
    int    j;

    id->nent = 0;
    if (m->c.n < 2) return;

    for (j = 1; j < m->c.n; j++) {
        IdentUno *u = &id->u[id->nent];
        Serie    *sal = m->c.s[0], *ent = m->c.s[j];
        real    **res = NULL;
        real      Q = 0.0, p = 0.0;
        char      why[512] = "";

        memset( u, 0, sizeof *u );
        u->serie = j;
        u->nlags = id->nlags > 0 ? id->nlags
                                 : prewhiten_nlags( sal->ts.nobs );
        if (u->nlags > IDENT_MAX_LAGS) u->nlags = IDENT_MAX_LAGS;

        if (prewhiten_ccf( &ent->tm, &ent->ts, ent->datamat,
                           &sal->tm, &sal->ts, sal->datamat,
                           u->nlags, u->ccf, u->nu, &u->n,
                           &res, why, sizeof why ) == 0) {
            /* El portmanteau con la rutina del motor, sobre los residuos
             * PREBLANQUEADOS, que es donde significa algo.               */
            hosking_test( res, u->n, 2, u->nlags, &Q, &p );
            free_matrix( res, 1, u->n, 1, 2 );

            u->Q     = (double) Q;
            u->df    = 4 * u->nlags;
            u->banda = 2.0 / sqrt( (double) u->n );
            u->vale  = TRUE;
            lee( u );
        }
        id->nent++;
    }

    if (id->marcada >= id->nent) id->marcada = id->nent ? 0 : -1;
    if (id->marcada < 0 && id->nent)  id->marcada = 0;
}

/* ------------------------------------------------------------------------ */

static void refresca_lista( Mtram *m )
{
    Ident        *id = &m->id;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(id->lista) ) );
    GtkTreeIter   it;
    char          b[16], s[16], k[16], p[32];
    int           i;

    gtk_list_store_clear( st );
    for (i = 0; i < id->nent; i++) {
        const IdentUno *u = &id->u[i];
        gchar          *nm;
        const char     *nota;

        nm = g_strdup_printf( "%d %s", u->serie + 1,
                 m->c.s[u->serie]->ts.name ? m->c.s[u->serie]->ts.name : "?" );

        if (!u->vale)        { nota = "no se pudo preblanquear";
                               strcpy(b,"—"); strcpy(s,"—"); strcpy(k,"—");
                               strcpy(p,"—"); }
        else {
            if (u->b < 0) { strcpy(b,"—"); strcpy(s,"—"); strcpy(k,"—"); }
            else {
                snprintf( b, sizeof b, "%d", u->b );
                snprintf( s, sizeof s, "%d", u->s );
                snprintf( k, sizeof k, "%d", u->kmax );
            }
            snprintf( p, sizeof p, "%.0f (%d)", u->Q, u->df );

            if (u->neg)      nota = u->b < 0
                                  ? "sin transferencia · OJO retroalimentación"
                                  : "transferencia · OJO retroalimentación";
            else if (u->b < 0) nota = "sin transferencia";
            else if (u->s == 0) nota = "transferencia, un solo ω";
            else               nota = "transferencia";
        }

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            I_ENT,  nm,
            I_B,    b,
            I_S,    s,
            I_KMAX, k,
            I_NEG,  u->vale ? u->neg : -1,
            I_P,    p,
            I_NOTA, nota,
            I_IDX,  i,
            -1 );
        g_free( nm );
    }

    if (id->marcada >= 0 && id->marcada < id->nent) {
        GtkTreePath *path = gtk_tree_path_new_from_indices( id->marcada, -1 );

        gtk_tree_view_set_cursor( GTK_TREE_VIEW(id->lista), path, NULL, FALSE );
        gtk_tree_path_free( path );
    }
}

static void refresca_veredicto( Mtram *m )
{
    Ident          *id = &m->id;
    const IdentUno *u;
    const char     *nom;

    if (m->c.n < 2 || id->marcada < 0 || id->marcada >= id->nent) {
        mtram_verdicto( id->ver_tran, MT_AMBAR,
            "Carga la salida y al menos una entrada en la pestaña Series." );
        gtk_label_set_text( GTK_LABEL(id->ver_exo), "" );
        return;
    }

    u   = &id->u[id->marcada];
    nom = m->c.s[u->serie]->ts.name ? m->c.s[u->serie]->ts.name : "?";

    if (!u->vale) {
        mtram_verdicto( id->ver_tran, MT_ROJO,
            "%s: no se pudo preblanquear", nom );
        gtk_label_set_text( GTK_LABEL(id->ver_exo), "" );
        return;
    }

    if (u->b < 0)
        mtram_verdicto( id->ver_tran, MT_AMBAR,
            "%s · ningún retardo k ≥ 0 sale de la banda "
            "· banda ±%.3f sobre %d obs estacionarias",
            nom, u->banda, u->n );
    else
        mtram_verdicto( id->ver_tran, MT_VERDE,
            "%s · b=%d  s=%d · pico en k=%d · banda "
            "±%.3f sobre %d obs estacionarias",
            nom, u->b, u->s, u->kmax, u->banda, u->n );

    if (u->neg == 0)
        mtram_verdicto( id->ver_exo, MT_VERDE,
            "Exogeneidad: ningún retardo negativo fuera de la banda · "
            "%s puede tratarse como exógena", nom );
    else
        mtram_verdicto( id->ver_exo, MT_ROJO,
            "Exogeneidad: %d retardo%s negativo%s fuera · la salida "
            "antecede a %s — esto no lo arregla (b,s)",
            u->neg, u->neg == 1 ? "" : "s", u->neg == 1 ? "" : "s", nom );
}

void identifica_refresca( Mtram *m )
{
    calcula( m );
    refresca_lista( m );
    refresca_veredicto( m );
}

/* ------------------------------------------------------------------------ */
/* El grafico y la ecuacion                                                  */
/* ------------------------------------------------------------------------ */

static void on_ccf( GtkButton *b, Mtram *m )
{
    Ident          *id = &m->id;
    const IdentUno *u;
    gchar          *path;

    if (id->marcada < 0 || id->marcada >= id->nent ||
        !id->u[id->marcada].vale) {
        preview_show_status( m, "Marca una entrada." );
        return;
    }
    u = &id->u[id->marcada];

    /* El EPS se escribe de verdad, y es el que se ve: lib/preview interpreta
     * el fichero de fugdraw, asi que la pantalla y el papel no discrepan. */
    path = g_build_filename( g_get_user_cache_dir(), "mtram", NULL );
    g_mkdir_with_parents( path, 0700 );
    g_free( path );
    path = g_build_filename( g_get_user_cache_dir(), "mtram", "ccf.eps", NULL );

    if (ccf_write_eps( path, u->ccf, u->nlags, u->n,
                       m->c.s[u->serie]->ts.name, m->c.s[0]->ts.name,
                       u->Q, u->df ) != 0)
        preview_show_status( m, "No pude escribir %s", path );
    else if (!preview_show( m, path ))
        preview_show_status( m, "No pude dibujar %s", path );

    g_free( path );
}

/* ------------------------------------------------------------------------ */
/* EL GESTO QUE FALTABA                                                      */
/*                                                                           */
/* La red SE PUEBLA con la identificacion. Eso es el metodo, y hasta ahora no */
/* tenia forma de hacerse dentro del programa: la CCF proponia (b, s) y habia */
/* que irse a la pestaña Red a teclearlo a mano. La pregunta "¿como se puebla */
/* la red?" no tenia respuesta en la interfaz.                                */
/* ------------------------------------------------------------------------ */

static void on_a_la_red( GtkButton *b, Mtram *m )
{
    Ident          *id = &m->id;
    const IdentUno *u;
    NetLink        *l;
    int             k;

    if (id->marcada < 0 || id->marcada >= id->nent) {
        preview_show_status( m, "Marca una entrada." );
        return;
    }
    u = &id->u[id->marcada];

    if (!u->vale || u->b < 0) {
        preview_show_status( m, "La CCF de «%s» no propone transferencia: "
            "ningún retardo k ≥ 0 sale de la banda.",
            m->c.s[u->serie]->ts.name ? m->c.s[u->serie]->ts.name : "?" );
        return;
    }

    /* Si ya estaba, se le ponen los ordenes que propone la CCF en vez de
     * duplicar el enlace: reidentificar es lo normal y no debe ensuciar. */
    for (k = 0; k < m->red.n; k++)
        if (m->red.lnk[k].out == 1 && m->red.lnk[k].inp == u->serie + 1) {
            m->red.lnk[k].b = u->b;
            m->red.lnk[k].s = u->s;
            mtram_refresca( m );
            preview_show_status( m, "«%s» ya estaba: se le ponen b=%d s=%d.",
                m->c.s[u->serie]->ts.name, u->b, u->s );
            return;
        }

    if (m->red.n >= NET_MAX_LINK) {
        preview_show_status( m, "La red lleva %d enlaces como mucho.",
                             NET_MAX_LINK );
        return;
    }

    l = &m->red.lnk[m->red.n++];
    l->out = 1;                       /* la salida es siempre la primera */
    l->inp = u->serie + 1;
    l->b = u->b;  l->r = 0;  l->s = u->s;

    mtram_refresca( m );

    if (u->neg)
        preview_show_status( m, "Añadido %s ← %s con b=%d s=%d. OJO: tiene %d "
            "retardo%s negativo%s fuera de banda — mira si debe estar.",
            m->c.s[0]->ts.name, m->c.s[u->serie]->ts.name, u->b, u->s,
            u->neg, u->neg == 1 ? "" : "s", u->neg == 1 ? "" : "s" );
    else
        preview_show_status( m, "Añadido %s ← %s con b=%d  r=0  s=%d.",
            m->c.s[0]->ts.name, m->c.s[u->serie]->ts.name, u->b, u->s );
}

static void on_ecuacion( GtkButton *b, Mtram *m )
{
    EqLink  lnk[GUI_MAX_SER];
    double  omega[GUI_MAX_SER][IDENT_MAX_LAGS + 2];
    char    texto[4096];
    GString *t = g_string_new( NULL );
    int     i, k, n = 0;

    if (m->c.n < 2) {
        g_string_append( t, "Carga la salida y al menos una entrada." );
        goto pinta;
    }

    /* Un enlace por entrada, con los ordenes que propone la CCF y ω a 1: es
     * la ESPECIFICACION que se va a estimar, no una estimacion. Se enseña
     * antes de estimar justamente para poder mirarla antes.            */
    for (i = 0; i < m->id.nent; i++) {
        const IdentUno *u = &m->id.u[i];
        int             bb = 0, ss = 0;

        if (u->vale && u->b >= 0) { bb = u->b; ss = u->s; }
        if (ss > IDENT_MAX_LAGS) ss = IDENT_MAX_LAGS;

        for (k = 0; k <= ss; k++) omega[n][k] = 1.0;

        lnk[n].entrada  = m->c.s[u->serie]->ts.name
                        ? m->c.s[u->serie]->ts.name : "?";
        lnk[n].b        = bb;
        lnk[n].s        = ss;
        lnk[n].r        = 0;
        lnk[n].omega    = omega[n];
        lnk[n].delta    = NULL;
        lnk[n].omega_se = NULL;
        lnk[n].delta_se = NULL;
        n++;
    }

    eqtran_texto( texto, sizeof texto,
                  m->c.s[0]->ts.name ? m->c.s[0]->ts.name : "Y", lnk, n, NULL );

    g_string_append( t, "Con los órdenes que propone la CCF:\n\n   " );
    g_string_append( t, texto );
    g_string_append( t,
        "\n\nLos ω están a 1: esto es la ESPECIFICACIÓN, no una estimación.\n"
        "Se enseña antes de estimar justamente para poder mirarla antes.\n\n"
        "El convenio es el de Box-Jenkins:\n"
        "   ω(B) = ω₀ − ω₁B − … − ωₛBˢ      el primero SUMA, el resto RESTAN\n"
        "   δ(B) = 1  − δ₁B − … − δᵣBʳ" );

pinta:
    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
    g_string_free( t, TRUE );
}

/* ------------------------------------------------------------------------ */

static void on_marcada( GtkTreeSelection *sel, Mtram *m )
{
    GtkTreeModel *mod;
    GtkTreeIter   it;
    int           i = -1;

    if (gtk_tree_selection_get_selected( sel, &mod, &it ))
        gtk_tree_model_get( mod, &it, I_IDX, &i, -1 );
    if (i >= 0 && i != m->id.marcada) {
        m->id.marcada = i;
        refresca_veredicto( m );
    }
}

static void on_lags( GtkSpinButton *sb, Mtram *m )
{
    m->id.nlags = gtk_spin_button_get_value_as_int( sb );
    identifica_refresca( m );
}

static void columna( GtkWidget *tv, const char *titulo, int col )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                               titulo, r, "text", col, NULL );

    gtk_tree_view_column_set_resizable( c, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), c );
}

GtkWidget *identifica_pagina_new( Mtram *m )
{
    Ident        *id = &m->id;
    GtkWidget    *caja, *barra, *b, *sc, *vb;
    GtkListStore *st;

    id->nent = 0;
    id->marcada = -1;
    id->nlags = 0;                        /* 0 = el que elige el motor */

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

#define BOTON(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON( "Añadir a la red", on_a_la_red,
           "Mete la entrada marcada en la red, con el (b, s) que propone la "
           "CCF. Es lo que puebla el .dag: la red sale de la identificación." )
    BOTON( "CCF…", on_ccf,
           "El gráfico bidireccional de la entrada marcada: a la derecha la "
           "transferencia, a la izquierda la retroalimentación." )
    BOTON( "Ecuación…", on_ecuacion,
           "La ecuación con los órdenes que propone la CCF." )
#undef BOTON

    /* Los retardos SI son un control de verdad: prewhiten_ccf los recibe, y
     * GraphMaker dejaba elegir de 8 a 39.                              */
    gtk_box_pack_end( GTK_BOX(barra),
        id->s_lags = gtk_spin_button_new_with_range( 0, IDENT_MAX_LAGS, 1 ),
        FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text( id->s_lags,
        "Retardos a cada lado. 0 = los que elige el motor: n/4, con tope 24 y "
        "suelo 10." );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(id->s_lags), 0 );
    g_signal_connect( id->s_lags, "value-changed", G_CALLBACK(on_lags), m );
    gtk_box_pack_end( GTK_BOX(barra), gtk_label_new( "Retardos" ),
                      FALSE, FALSE, 0 );

    /* --- la lista: UNA FILA POR ENTRADA CANDIDATA --- */
    st = gtk_list_store_new( I_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_INT );
    id->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( id->lista, "Entrada", I_ENT );
    columna( id->lista, "b",       I_B );
    columna( id->lista, "s",       I_S );
    columna( id->lista, "k máx",   I_KMAX );
    columna( id->lista, "neg",     I_NEG );
    columna( id->lista, "P (df)",  I_P );
    columna( id->lista, "",        I_NOTA );
    gtk_widget_set_tooltip_text( id->lista,
        "b y s los propone la CCF: primer y último retardo significativo en "
        "k ≥ 0. «k máx» es dónde está el pico.\n\n"
        "«neg» son los retardos NEGATIVOS fuera de banda: es el contraste de "
        "exogeneidad, y es el que decide si ese enlace puede existir. Un "
        "valor distinto de cero es lo que hace que una fila merezca mirarse." );

    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(id->lista) ),
                      "changed", G_CALLBACK(on_marcada), m );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), id->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    /* --- los dos veredictos, altura fija --- */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_widget_set_margin_top( vb, 2 );

    id->ver_tran = gtk_label_new( "Carga la salida y al menos una entrada." );
    id->ver_exo  = gtk_label_new( "" );
    gtk_widget_set_halign( id->ver_tran, GTK_ALIGN_START );
    gtk_widget_set_halign( id->ver_exo,  GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(id->ver_tran), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(id->ver_exo),  PANGO_ELLIPSIZE_END );

    gtk_box_pack_start( GTK_BOX(vb), id->ver_tran, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), id->ver_exo,  FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), vb, FALSE, FALSE, 0 );

    return caja;
}
