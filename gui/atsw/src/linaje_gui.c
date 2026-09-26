/*
 * linaje_gui.c -- la cadena, y lo que cada nodo DEBE.
 *
 * POR QUE UNA VISTA Y NO UNA COLUMNA MAS EN LA REJILLA
 *
 * La rejilla enseña modelos, uno por fila, y contesta «¿cómo va éste?». El
 * linaje contesta otra cosa: «¿cómo va el RECORRIDO?» -- y eso es una forma
 * de árbol, no de tabla. Un hijo que cuelga de su padre se ve; una columna
 * «padre: m01» hay que reconstruirla en la cabeza.
 *
 * LO QUE ENSEÑA ES LO QUE FALTA, y a propósito. Todo el mundo sabe mirar lo
 * que tiene delante; lo que no se ve es el nodo que nunca se estimó, la
 * razón que no se escribió y la cadena que no eligió nada. Eso es lo que
 * dentro de seis meses hace irreproducible un análisis, y es barato decirlo
 * ahora.
 *
 * Y LA SEPARACION DE SIEMPRE: el LINAJE y la RAZON salen del manifiesto, que
 * es donde viven las decisiones; ESTIMADO y el DICTAMEN salen del .out, que
 * es el registro. Ninguna de las dos se guarda en la otra.
 */

#include <string.h>

#include "atsw.h"

void barra_pub( Atsw *a, const char *s );

enum { LN_NODO, LN_ESTADO, LN_DICTAMEN, LN_RAZON, LN_DEBE, LN_COLOR, LN_COLS };

typedef struct {
    int sin_razon, sin_estimar, sin_elegido, cadenas;
} Cuenta;


/* Los hijos de este nodo, colgados de él. Recursiva: la cadena puede ser
 * larga pero no es ancha, y la recursión es lo que dice la forma.      */
static void cuelga( Atsw *a, GtkTreeStore *st, GtkTreeIter *padre,
                    const char *serie, const char *muestra, const char *de,
                    Cuenta *c )
{
    int i;

    for ( i = 0; i < a->p->nm; i++ )
        {
        const PrModelo *m = &a->p->m[i];
        const AtRes    *r;
        GtkTreeIter     it;
        const char     *estado, *dict, *color;
        char            debe[256] = "";
        gboolean        datos;

        if ( strcmp( m->serie, serie ) || strcmp( m->muestra, muestra ) ) continue;
        if ( strcmp( m->padre, de ) ) continue;

        datos = ( pr_es_datos( a->p, serie, muestra, m->id ) != 0 );
        r = datos ? NULL : atsw_resultado( a, serie, muestra, m->id );

        if ( datos )       { estado = "datos";        dict = "";   color = "#666666"; }
        else if ( !r || !r->hay )
            {
            estado = "SIN ESTIMAR";  dict = "";  color = "#b51c17";
            c->sin_estimar++;
            g_strlcat( debe, "estimarlo", sizeof debe );
            }
        else
            {
            estado = "estimado";
            dict   = dx_estado_es( r->peor );
            color  = ( r->peor == DX_NO ) ? "#b51c17"
                   : ( r->peor == DX_MIRAR ) ? "#9a6700" : "#1a7f37";
            }

        /* LA RAZON NO SE EXIGE, PERO SE ENSEÑA CUANDO FALTA. Los datos no
           llevan razón: son la raíz, no una decisión.                 */
        if ( !datos && !m->razon[0] )
            {
            c->sin_razon++;
            if ( debe[0] ) g_strlcat( debe, " · ", sizeof debe );
            g_strlcat( debe, "escribir su razón", sizeof debe );
            }

        gtk_tree_store_append( st, &it, padre );
        gtk_tree_store_set( st, &it,
            LN_NODO,     m->id,
            LN_ESTADO,   estado,
            LN_DICTAMEN, dict,
            LN_RAZON,    m->razon[0] ? m->razon : "",
            LN_DEBE,     debe,
            LN_COLOR,    color, -1 );

        if ( m->elegido )
            {
            char et[PR_ID + 16];

            g_snprintf( et, sizeof et, "%s  ← elegido", m->id );
            gtk_tree_store_set( st, &it, LN_NODO, et, -1 );
            }

        cuelga( a, st, &it, serie, muestra, m->id, c );
        }
}

static void llena( Atsw *a, GtkTreeStore *st, Cuenta *c )
{
    int s, u;

    memset( c, 0, sizeof *c );

    for ( s = 0; s < a->p->ns; s++ )
        for ( u = 0; u <= a->p->nmu; u++ )
            {
            const char *mu = ( u == 0 ) ? "" : a->p->mu[u-1].id;
            const char *serie = a->p->s[s].id;
            GtkTreeIter it;
            int         i, tiene = 0;
            char        et[PR_ID * 2 + 32];

            for ( i = 0; i < a->p->nm; i++ )
                if ( !strcmp( a->p->m[i].serie, serie ) &&
                     !strcmp( a->p->m[i].muestra, mu ) ) { tiene = 1; break; }
            if ( !tiene ) continue;

            c->cadenas++;
            g_snprintf( et, sizeof et, "%s · %s", serie,
                        *mu ? mu : "muestra completa" );

            gtk_tree_store_append( st, &it, NULL );
            gtk_tree_store_set( st, &it, LN_NODO, et, LN_COLOR, "#000000", -1 );

            if ( !*pr_elegido( a->p, serie, mu ) )
                {
                c->sin_elegido++;
                gtk_tree_store_set( st, &it, LN_DEBE,
                    "ninguno declarado EL modelo de esta ventana", -1 );
                }
            cuelga( a, st, &it, serie, mu, "", c );
            }
}


/* ------------------------------------------------------------------------ */

void atsw_linaje( Atsw *a )
{
    GtkWidget       *win, *raiz, *sc, *vista, *pie;
    GtkTreeStore    *st;
    GtkCellRenderer *r;
    Cuenta           c;

    if ( !a->hay ) { barra_pub( a, "No hay proyecto abierto." ); return; }

    st = gtk_tree_store_new( LN_COLS, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING );
    llena( a, st, &c );

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_transient_for( GTK_WINDOW(win), GTK_WINDOW(a->ventana) );
    gtk_window_set_default_size( GTK_WINDOW(win), 900, 520 );
    gtk_window_set_title( GTK_WINDOW(win), "Linaje" );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(win), raiz );

    vista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );

    r = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(vista), -1,
        "Nodo", r, "text", LN_NODO, "foreground", LN_COLOR, NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(vista), -1,
        "Estado", r, "text", LN_ESTADO, "foreground", LN_COLOR, NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(vista), -1,
        "Dictamen", r, "text", LN_DICTAMEN, "foreground", LN_COLOR, NULL );

    /* LO QUE DEBE, EN ROJO Y ANTES QUE LA RAZON: es lo que se viene a ver. */
    {
    GtkCellRenderer *rd = gtk_cell_renderer_text_new();

    g_object_set( rd, "foreground", "#b51c17", NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(vista), -1,
        "Debe", rd, "text", LN_DEBE, NULL );
    }

    {
    GtkCellRenderer *rr = gtk_cell_renderer_text_new();

    /* El ajuste de linea va en el RENDERIZADOR, no en el dato: asi la razon
       se exporta entera si algun dia se exporta.                      */
    g_object_set( rr, "wrap-mode", PANGO_WRAP_WORD, "wrap-width", 320, NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(vista), -1,
        "Razón", rr, "text", LN_RAZON, NULL );
    }

    gtk_tree_view_expand_all( GTK_TREE_VIEW(vista) );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), vista );
    gtk_box_pack_start( GTK_BOX(raiz), sc, TRUE, TRUE, 0 );

    pie = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(pie), TRUE );
    {
    GString *t = g_string_new( "<small>" );

    g_string_append_printf( t, "%d cadena%s", c.cadenas, c.cadenas == 1 ? "" : "s" );
    if ( c.sin_estimar )
        g_string_append_printf( t, " · <b>%d sin estimar</b>", c.sin_estimar );
    if ( c.sin_razon )
        g_string_append_printf( t, " · <b>%d sin razón</b>", c.sin_razon );
    if ( c.sin_elegido )
        g_string_append_printf( t, " · <b>%d sin elegido</b>", c.sin_elegido );
    if ( !c.sin_estimar && !c.sin_razon && !c.sin_elegido )
        g_string_append( t, " · no debe nada" );
    else
        g_string_append( t,
            "\n<i>Nada de esto es un error: es lo que dentro de seis meses "
            "hace irreproducible un análisis. La razón se pone con «Razón…» "
            "y el elegido con «Declarar elegido».</i>" );
    g_string_append( t, "</small>" );
    gtk_label_set_markup( GTK_LABEL(pie), t->str );
    g_string_free( t, TRUE );
    }
    gtk_box_pack_start( GTK_BOX(raiz), pie, FALSE, FALSE, 0 );

    gtk_widget_show_all( win );
}
