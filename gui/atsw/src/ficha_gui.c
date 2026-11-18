/*
 * ficha_gui.c -- el coste y la ficha de confianza, en la madre
 * (docs/ESTUDIO-terminal-mcp.md §7.7 y §7.8).
 *
 * TODO SALE DE lib/ficha Y lib/coste, las mismas funciones que «atsw-cli
 * ficha» y «atsw-cli coste resumen»: la ficha que se ve aqui es, byte a byte,
 * el Markdown de fi_markdown. Aqui solo se enseña:
 *
 *   - la linea de coste de un modelo, al pie de «Ver modelo…»;
 *   - Diagnosticar ▸ «Ficha de confianza…», con «Comprobar» (corre fue
 *     sobre el .pre y lo apunta en costes.csv) y «Al cuaderno»;
 *   - el coste de cada paso y el del camino, en la ventana del linaje
 *     (atsw_coste_por_objeto).
 *
 * UNA ENERGIA SIN SU METODO NO SE ENSEÑA, y lo que no consta dice «no
 * consta»: las reglas de lib/coste y lib/ficha, que aqui no se repiten.
 */

#include <string.h>

#include "atsw.h"
#include "ficha.h"
#include "coste.h"
#include "cuaderno.h"
#include "engine.h"
#include "preview.h"

void barra_pub( Atsw *a, const char *s );

/* Un numero con coma decimal, como en la ficha. */
static gchar *coma( double x, int dec )
{
    gchar *s = g_strdup_printf( "%.*f", dec, x ), *p;

    for ( p = s; *p; p++ ) if ( *p == '.' ) *p = ',';
    return s;
}

/* ------------------------------------------------------------------------ */
/* LA LINEA DE COSTE (§7.7)                                                  */
/* ------------------------------------------------------------------------ */

/* «Coste: 0,21 s de CPU (fue 1.14, C), ~0,4 J estimados; 2,9 s de CPU en
 * todo el recorrido (7 corridas)». NULL si no hay proyecto o modelo.    */
gchar *atsw_coste_linea( Atsw *a, const char *serie, const char *muestra, const char *id )
{
    Ficha   *f;
    char     why[256];
    GString *t;

    if ( !a->hay ) return NULL;
    f = g_new0( Ficha, 1 );
    if ( fi_ficha( a->p, serie, muestra, id, f, why, sizeof why ) != 0 )
        { g_free( f ); return NULL; }

    t = g_string_new( "Coste: " );
    if ( !f->estimacion.consta )
        g_string_append( t, "no consta la estimación en costes.csv" );
    else
        {
        gchar *c = coma( f->estimacion.cpu_s, 2 );

        g_string_append_printf( t, "%s s de CPU (%s, %s)", c, f->estimacion.motor,
                                f->estimacion.via );
        g_free( c );
        if ( f->estimacion.energia_j >= 0 && f->estimacion.metodo[0] )
            {
            gchar *e = coma( f->estimacion.energia_j, 1 );
            g_string_append_printf( t, ", ~%s J estimados", e );
            g_free( e );
            }
        }
    if ( f->corridas > 0 )
        {
        gchar *c = coma( f->cpu_s, 1 );

        g_string_append_printf( t, "; %s s de CPU en todo el recorrido (%d corrida%s%s)",
                                c, f->corridas, f->corridas == 1 ? "" : "s",
                                f->en_python ? ", alguna en Python" : ", todo en C" );
        g_free( c );
        }
    g_free( f );
    return g_string_free( t, FALSE );
}

/* El pie de «Ver modelo…»: la linea de coste, en pequeño. */
void atsw_coste_pie( Atsw *a, const char *f, const char *serie, const char *muestra,
                     const char *id )
{
    gchar     *t = atsw_coste_linea( a, serie, muestra, id );
    GtkWidget *l;
    gchar     *m;

    if ( t == NULL ) return;
    l = gtk_label_new( NULL );
    m = g_markup_printf_escaped( "<small>%s</small>", t );
    gtk_label_set_markup( GTK_LABEL(l), m );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_ellipsize( GTK_LABEL(l), PANGO_ELLIPSIZE_END );
    gtk_widget_set_tooltip_text( l, "Lo que costó, de costes.csv: el tiempo de CPU es la cifra "
                                    "medida; la energía es una estimación y lleva su método "
                                    "(Diagnosticar ▸ Ficha de confianza…)." );
    gtk_widget_set_margin_start( l, 8 );
    gtk_widget_set_margin_bottom( l, 4 );
    gtk_widget_show( l );
    if ( !preview_set_footer( f, l ) ) gtk_widget_destroy( l );
    g_free( m ); g_free( t );
}

/* ------------------------------------------------------------------------ */
/* EL COSTE POR MODELO, para el linaje                                        */
/* ------------------------------------------------------------------------ */

/* El objeto de costes.csv de un modelo: «serie/id» o «serie/muestra/id». */
gchar *atsw_coste_objeto( const char *serie, const char *muestra, const char *id )
{
    return ( muestra && *muestra ) ? g_strdup_printf( "%s/%s/%s", serie, muestra, id )
                                   : g_strdup_printf( "%s/%s", serie, id );
}

/* objeto -> segundos de CPU (gdouble*), sumando todas sus corridas. NULL
 * si no se pudo leer. g_hash_table_destroy.                             */
GHashTable *atsw_coste_por_objeto( Atsw *a )
{
    char        raiz[PR_RUTA], why[256];
    CosteLinea *l = NULL;
    int         n = 0, i;
    GHashTable *h;

    if ( !a->hay || pr_raiz( a->p, raiz, sizeof raiz ) != 0 ) return NULL;
    if ( coste_lee( raiz, &l, &n, NULL, why, sizeof why ) != 0 ) return NULL;
    h = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, g_free );
    for ( i = 0; i < n; i++ )
        {
        gdouble *v;

        if ( !l[i].objeto || !*l[i].objeto ) continue;
        v = g_hash_table_lookup( h, l[i].objeto );
        if ( v == NULL )
            { v = g_new0( gdouble, 1 ); g_hash_table_insert( h, g_strdup( l[i].objeto ), v ); }
        *v += l[i].cpu_s;
        }
    coste_lineas_libera( l, n );
    return h;
}

/* «0,21 s», o «—» si no consta. */
gchar *atsw_segundos( double s, gboolean consta )
{
    gchar *c, *r;

    if ( !consta ) return g_strdup( "—" );
    c = coma( s, 2 );
    r = g_strdup_printf( "%s s", c );
    g_free( c );
    return r;
}

/* ------------------------------------------------------------------------ */
/* LA FICHA DE CONFIANZA (§7.8)                                              */
/* ------------------------------------------------------------------------ */

/* EL MARKDOWN DE LA FICHA, EN PANGO. Es poco Markdown --«## » titulos,
 * **negritas** y «- » listas-- y se traduce aqui para verlo; lo que se
 * guarda y se copia es el Markdown, que es el que no cambia.          */
gchar *atsw_md_pango( const char *md )
{
    gchar  **ln = g_strsplit( md, "\n", -1 );
    GString *t = g_string_new( NULL );
    int      i;

    for ( i = 0; ln[i]; i++ )
        {
        const char *s = ln[i];
        gboolean    tit = g_str_has_prefix( s, "## " );
        gboolean    item = g_str_has_prefix( s, "- " );
        gchar      *e = g_markup_escape_text( tit ? s + 3 : item ? s + 2 : s, -1 ), *p;
        GString    *x = g_string_new( NULL );
        gboolean    abierta = FALSE;

        for ( p = e; *p; p++ )
            if ( p[0] == '*' && p[1] == '*' )
                { g_string_append( x, abierta ? "</b>" : "<b>" ); abierta = !abierta; p++; }
            else
                g_string_append_c( x, *p );
        if ( abierta ) g_string_append( x, "</b>" );
        if ( tit )       g_string_append_printf( t, "<span size='large'><b>%s</b></span>", x->str );
        else if ( item ) g_string_append_printf( t, "  • %s", x->str );
        else             g_string_append( t, x->str );
        if ( ln[i + 1] ) g_string_append_c( t, '\n' );
        g_string_free( x, TRUE );
        g_free( e );
        }
    g_strfreev( ln );
    return g_string_free( t, FALSE );
}

typedef struct {
    Atsw      *a;
    char       serie[PR_ID], muestra[PR_ID], id[PR_ID];
    gchar     *md;
    GtkWidget *win, *texto;
} FichaW;

static void llena( FichaW *w )
{
    Ficha *f = g_new0( Ficha, 1 );
    char   why[512];
    gchar *pg;

    g_free( w->md );
    if ( fi_ficha( w->a->p, w->serie, w->muestra, w->id, f, why, sizeof why ) != 0 )
        w->md = g_strdup_printf( "No hay ficha de %s: %s", w->id, why );
    else
        w->md = fi_markdown( f );
    g_free( f );
    pg = atsw_md_pango( w->md );
    gtk_label_set_markup( GTK_LABEL(w->texto), pg );
    g_free( pg );
}

static void on_comprueba( GtkButton *b, FichaW *w )
{
    char  dif[512], *t;
    int   rep = 0;

    (void) b;
    /* El fue de la madre, el del paquete o el del arbol: el que se comprueba. */
    engine_set_finder( atsw_programa );
    barra_pub( w->a, "Comprobando la reproducibilidad: fue sobre el .pre…" );
    while ( gtk_events_pending() ) gtk_main_iteration();
    if ( fi_comprueba( w->a->p, w->serie, w->muestra, w->id, "madre", "analista",
                       &rep, dif, sizeof dif ) != 0 )
        t = g_strdup_printf( "No se pudo comprobar: %s", dif );
    else if ( rep )
        t = g_strdup_printf( "%s se reproduce con el fue de la madre: apuntado en costes.csv.", w->id );
    else
        t = g_strdup_printf( "%s NO se reproduce: se movió %s. Apuntado en costes.csv.", w->id, dif );
    barra_pub( w->a, t );
    g_free( t );
    llena( w );
}

/* Al cuaderno: la ficha tal cual, con su procedencia. Una vez por pulsacion. */
int atsw_ficha_al_cuaderno( Atsw *a, const char *serie, const char *muestra, const char *id,
                            const char *md, char *why, size_t n )
{
    gchar    *tit = g_strdup_printf( "Ficha de confianza — %s / %s", serie, id );
    Cuaderno *c = cu_nueva( a->p->path, tit );
    int       rc;

    g_free( tit );
    if ( c == NULL ) { g_snprintf( why, n, "No hay manifiesto." ); return 1; }
    cu_dato( c, "Serie", serie );
    cu_dato( c, "Muestra", muestra && *muestra ? muestra : "completa" );
    cu_dato( c, "Modelo", id );
    cu_texto( c, md );
    rc = cu_cierra( c, why, n );
    return rc;
}

static void on_cuaderno( GtkButton *b, FichaW *w )
{
    char why[512];

    (void) b;
    barra_pub( w->a, atsw_ficha_al_cuaderno( w->a, w->serie, w->muestra, w->id, w->md,
                                             why, sizeof why ) == 0
               ? "La ficha, al cuaderno." : why );
}

static void on_cierra( GtkButton *b, FichaW *w ) { (void) b; gtk_widget_destroy( w->win ); }
static void on_destruye( GtkWidget *x, FichaW *w ) { (void) x; g_free( w->md ); g_free( w ); }

void atsw_ficha( Atsw *a, const char *serie, const char *muestra, const char *id )
{
    FichaW    *w;
    GtkWidget *caja, *sc, *bot, *b;
    gchar     *t;

    if ( !a->hay || !serie || !*serie || !id || !*id ) { barra_pub( a, "Marca un modelo." ); return; }
    w = g_new0( FichaW, 1 );
    w->a = a;
    g_strlcpy( w->serie, serie, sizeof w->serie );
    g_strlcpy( w->muestra, muestra ? muestra : "", sizeof w->muestra );
    g_strlcpy( w->id, id, sizeof w->id );

    w->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    t = g_strdup_printf( "Ficha de confianza — %s / %s", serie, id );
    gtk_window_set_title( GTK_WINDOW(w->win), t );
    g_free( t );
    gtk_window_set_transient_for( GTK_WINDOW(w->win), GTK_WINDOW(a->ventana) );
    gtk_window_set_default_size( GTK_WINDOW(w->win), 760, 560 );
    g_signal_connect( w->win, "destroy", G_CALLBACK(on_destruye), w );

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );
    gtk_container_add( GTK_CONTAINER(w->win), caja );
    w->texto = gtk_label_new( NULL );
    gtk_label_set_selectable( GTK_LABEL(w->texto), TRUE );
    gtk_label_set_line_wrap( GTK_LABEL(w->texto), TRUE );
    gtk_label_set_xalign( GTK_LABEL(w->texto), 0.0 );
    gtk_label_set_yalign( GTK_LABEL(w->texto), 0.0 );
    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER(sc), w->texto );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    bot = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    b = gtk_button_new_with_label( "Comprobar" );
    gtk_widget_set_tooltip_text( b, "¿Se reproduce? Corre fue sobre el .pre y compara: los "
                                    "parámetros no deberían moverse. Cuesta una corrida, y queda "
                                    "en costes.csv con el sha del .pre." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_comprueba), w );
    gtk_box_pack_start( GTK_BOX(bot), b, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Al cuaderno" );
    gtk_widget_set_tooltip_text( b, "La ficha, tal como está, al cuaderno del proyecto." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_cuaderno), w );
    gtk_box_pack_start( GTK_BOX(bot), b, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Cerrar" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_cierra), w );
    gtk_box_pack_end( GTK_BOX(bot), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), bot, FALSE, FALSE, 0 );

    llena( w );
    gtk_widget_show_all( w->win );
}
