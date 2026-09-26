/*
 * sugerir_gui.c -- qué forma pide cada suceso, y el modelo que lo lleva.
 *
 * LA VENTANA NO DECIDE. Enseña, por cada suceso marcado, la forma que dice
 * el DATO y la frase que lo justifica, y deja cambiarla: la lectura escalar
 * es evidencia, no veredicto, y hay una razón que esta ventana no puede
 * saber -- si hay un suceso conocido que explique la forma. Ése es el único
 * nodo cuya evidencia no está en los datos, y por eso se pregunta en vez de
 * suponerse.
 *
 * DERIVAR, NUNCA PISAR. La intervención va en un modelo NUEVO, hijo del que
 * se estaba mirando: el padre sigue estimado y con su .out, así que la
 * comparación «con y sin» se puede hacer después. Pisar el .inp del padre
 * borraría la mitad de la pregunta.
 *
 * Y SE PARTE DEL .pre SI LO HAY, no del .inp: el .pre es ese mismo modelo con
 * sus estimaciones como valores iniciales -- un óptimo en forma reejecutable.
 * Empezar la estimación nueva desde el óptimo del padre es lo que hace que lo
 * que se mueva sea la intervención y no el punto de partida.
 *
 * LO QUE NO HACE: estimar. El .inp queda escrito y se abre en el editor, que
 * es quien corre fue y enseña la diagnosis. Un botón que escribiera Y
 * estimara escondería el fichero, que es justo lo que en esta escuela no se
 * esconde.
 */

#include <string.h>
#include <stdarg.h>
#include <glib/gstdio.h>

#include "intervencion.h"
#include "inpdet.h"
#include "inpcheck.h"

#include "atsw.h"

void barra_pub( Atsw *a, const char *s );

#define SG_MAX  AT_MAX_SUC

typedef struct {
    Atsw     *a;
    char      serie[PR_ID], muestra[PR_ID], id[PR_ID];
    int       d, D, freq;

    AtSuceso  s[SG_MAX];
    IvLectura l[SG_MAX];
    int       ns;

    GtkWidget *win;
    GtkWidget *usa[SG_MAX], *forma[SG_MAX], *linea[SG_MAX];
} Sg;

static const IvForma ORDEN[4] = { IV_ESCALON, IV_IMPULSO, IV_COMPIMP, IV_RAMPA };

static IvForma forma_de( GtkComboBoxText *c )
{
    int i = gtk_combo_box_get_active( GTK_COMBO_BOX(c) );

    return ( i >= 0 && i < 4 ) ? ORDEN[i] : IV_ESCALON;
}

/* La línea que va a ir al .inp, recalculada al cambiar la forma: lo que se
   ve es lo que se escribe, sin traducción por el medio.                 */
static void repinta_linea( Sg *g, int k )
{
    char b[ID_LINEA];

    if ( iv_linea( forma_de( GTK_COMBO_BOX_TEXT(g->forma[k]) ), g->freq,
                   g->s[k].per, g->s[k].anno, b, sizeof b ) != 0 )
        g_snprintf( b, sizeof b, "(esa fecha no cabe en esta frecuencia)" );
    {
    gchar *m = g_markup_printf_escaped( "<tt>%s</tt>", b );

    gtk_label_set_markup( GTK_LABEL(g->linea[k]), m );
    g_free( m );
    }
}

static void on_forma( GtkComboBox *c, Sg *g )
{
    int k = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(c), "k" ) );

    if ( k >= 0 && k < g->ns ) repinta_linea( g, k );
}


/* ------------------------------------------------------------------------ */
/* Derivar                                                                   */
/* ------------------------------------------------------------------------ */

/* DE DONDE SE PARTE: el .pre si lo hay. Ver la cabecera. */
static int origen_de( Sg *g, char *out, size_t n )
{
    char pre[PR_RUTA];

    if ( pr_ruta( g->a->p, g->serie, g->muestra, g->id, ".pre", pre, sizeof pre ) == 0 &&
         g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { g_snprintf( out, n, "%s", pre ); return 1; }
    if ( pr_ruta( g->a->p, g->serie, g->muestra, g->id, ".inp", out, n ) != 0 )
        return 0;
    return g_file_test( out, G_FILE_TEST_EXISTS ) ? 2 : 0;
}

static void di( Sg *g, const char *fmt, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, fmt );
    s = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    barra_pub( g->a, s );
    g_free( s );
}

static void on_derivar( GtkButton *b, Sg *g )
{
    char        lin[SG_MAX][ID_LINEA];
    const char *ptr[SG_MAX];
    char        ya[ID_MAX_DET][ID_LINEA];
    char        origen[PR_RUTA], destino[PR_RUTA], porque[512], msg[512];
    char        nuevo[PR_ID];
    GString    *razon;
    PrError     e;
    gchar      *dir;
    int         k, n = 0, nya, cual;

    (void) b;

    for ( k = 0; k < g->ns; k++ )
        {
        if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(g->usa[k]) ) ) continue;
        if ( iv_linea( forma_de( GTK_COMBO_BOX_TEXT(g->forma[k]) ), g->freq,
                       g->s[k].per, g->s[k].anno, lin[n], sizeof lin[n] ) != 0 )
            { di( g, "La fecha %s no cabe en una serie de frecuencia %d.",
                  g->s[k].fecha, g->freq ); return; }
        ptr[n] = lin[n];
        n++;
        }
    if ( n == 0 ) { di( g, "No has dejado ninguna intervención marcada." ); return; }

    cual = origen_de( g, origen, sizeof origen );
    if ( !cual ) { di( g, "No encuentro el fichero de «%s».", g->id ); return; }

    /* DOS INTERVENCIONES SOBRE EL MISMO SUCESO NO DAN ERROR: dan un omega no
       significativo por síntoma, que se lee como «no hacía falta» cuando lo
       que pasa es que está contada dos veces. Así que se mira antes.   */
    nya = id_intervenciones( origen, ya, ID_MAX_DET, porque, sizeof porque );
    if ( nya < 0 ) { di( g, "%s", porque ); return; }
    for ( k = 0; k < n; k++ )
        {
        const char *f = strchr( lin[k], ' ' );
        int         j;

        if ( !f ) continue;
        for ( j = 0; j < nya; j++ )
            {
            const char *g2 = strchr( ya[j], ' ' );

            if ( g2 && !strcmp( f, g2 ) )
                {
                di( g, "«%s» ya lleva «%s» en esa misma fecha. Ponerle otra "
                       "encima no da error: da dos intervenciones sobre un "
                       "suceso y un ω no significativo por síntoma. Cambia la "
                       "que hay en el editor.", g->id, ya[j] );
                return;
                }
            }
        }

    if ( pr_deriva( g->a->p, g->serie, g->muestra, g->id, nuevo, sizeof nuevo,
                    destino, sizeof destino, &e ) != 0 )
        { pr_error_es( &e, msg, sizeof msg ); di( g, "%s", msg ); return; }

    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    if ( id_anade( origen, destino, ptr, n, porque, sizeof porque ) != 0 )
        {
        pr_borra( g->a->p, g->serie, g->muestra, nuevo, &e );
        di( g, "%s", porque );
        return;
        }

    /* EL JUEZ ES EL MOTOR. Si su propia puerta no lo acepta, el nodo se
       deshace: un .inp que fue rechaza es peor que no haberlo escrito. */
    if ( inp_check_fue( destino, msg, sizeof msg ) != 0 )
        {
        g_unlink( destino );
        pr_borra( g->a->p, g->serie, g->muestra, nuevo, &e );
        di( g, "Lo que salió no lo acepta fue: %s", msg );
        return;
        }

    /* LA RAZON, CON LA FRASE QUE LA JUSTIFICA. El linaje lo escribe
       pr_deriva solo; la razón es lo que dentro de seis meses explica por
       qué esta iteración existe, y aquí se sabe.                      */
    razon = g_string_new( "" );
    for ( k = 0, n = 0; k < g->ns; k++ )
        {
        if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(g->usa[k]) ) ) continue;
        g_string_append_printf( razon, "%s%s en %s: %s", n++ ? ". " : "",
            iv_nombre_es( forma_de( GTK_COMBO_BOX_TEXT(g->forma[k]) ) ),
            g->s[k].fecha, g->l[k].razon );
        }
    g_string_append_printf( razon, ". Derivado de %s%s.", g->id,
                            ( cual == 1 ) ? " (de su .pre, el óptimo)" : "" );
    pr_razon( g->a->p, g->serie, g->muestra, nuevo, razon->str, &e );
    g_string_free( razon, TRUE );

    if ( atsw_guarda( g->a, &e ) != 0 )
        { di( g, "El .inp está en %s, pero no pude guardar el proyecto.", nuevo ); }

    atsw_refresca( g->a );

    /* LO QUE HAGA FALTA DESPUES, A MANO ANTES: destruir la ventana libera g
       --lo hace su «destroy»-- y lo que sigue ya no puede mirarlo.     */
    {
    Atsw *a = g->a;
    char  serie[PR_ID], muestra[PR_ID], padre[PR_ID];
    gchar *aviso;

    g_snprintf( serie, sizeof serie, "%s", g->serie );
    g_snprintf( muestra, sizeof muestra, "%s", g->muestra );
    g_snprintf( padre, sizeof padre, "%s", g->id );
    aviso = g_strdup_printf( "%s nace de %s con %d intervención%s. Estímalo "
                             "para ver si compra algo.", nuevo, padre, n,
                             n == 1 ? "" : "es" );

    gtk_widget_destroy( g->win );
    atsw_editor( a, serie, muestra, nuevo );
    barra_pub( a, aviso );
    g_free( aviso );
    }
}

static void on_cerrar( GtkWidget *w, Sg *g ) { (void) w; g_free( g ); }


/* ------------------------------------------------------------------------ */

void atsw_sugerir( Atsw *a, const char *serie, const char *muestra,
                   const char *id, int d, int D, int freq,
                   const AtSuceso *suc, int ns )
{
    Sg        *g;
    GtkWidget *raiz, *cab, *rej, *pie, *barra, *b;
    int        k;

    if ( !a->hay || ns < 1 ) return;
    if ( ns > SG_MAX ) ns = SG_MAX;

    g = g_new0( Sg, 1 );
    g->a = a;
    g->d = d;
    g->D = D;
    g->freq = freq;
    g->ns = ns;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );

    for ( k = 0; k < ns; k++ )
        {
        IvExtremo e[AT_MAX_EXT];
        int       j;

        g->s[k] = suc[k];
        for ( j = 0; j < suc[k].next && j < AT_MAX_EXT; j++ )
            { e[j].obs = suc[k].obs[j]; e[j].z = suc[k].z[j]; }
        iv_lectura( e, suc[k].next, d, &g->l[k] );
        }

    g->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_transient_for( GTK_WINDOW(g->win), GTK_WINDOW(a->ventana) );
    gtk_window_set_default_size( GTK_WINDOW(g->win), 720, 420 );
    gtk_window_set_title( GTK_WINDOW(g->win), "Sugerir intervención" );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 12 );
    gtk_container_add( GTK_CONTAINER(g->win), raiz );

    cab = gtk_label_new( NULL );
    {
    gchar *t = g_markup_printf_escaped(
        "<b>%s / %s</b>   ·   d = %d, frecuencia %d   ·   %d suceso%s\n"
        "<small>La forma la dice la FIRMA que el suceso deja en los residuos, "
        "no el ajuste: escalón e impulso cuestan un parámetro cada uno y no "
        "están anidados, así que el AIC no puede elegir entre ellos.</small>",
        serie, id, d, freq, ns, ns == 1 ? "" : "s" );

    gtk_label_set_markup( GTK_LABEL(cab), t );
    g_free( t );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(cab), TRUE );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 4 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 10 );
    {
    GtkWidget *sc = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), rej );
    gtk_box_pack_start( GTK_BOX(raiz), sc, TRUE, TRUE, 0 );
    }

    for ( k = 0; k < ns; k++ )
        {
        GtkWidget *raz;
        int        i, fila = 2 * k;

        g->usa[k] = gtk_check_button_new_with_label( g->s[k].fecha );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g->usa[k]), TRUE );
        gtk_grid_attach( GTK_GRID(rej), g->usa[k], 0, fila, 1, 1 );

        g->forma[k] = gtk_combo_box_text_new();
        for ( i = 0; i < 4; i++ )
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->forma[k]),
                                            iv_nombre_es( ORDEN[i] ) );
        for ( i = 0; i < 4; i++ ) if ( ORDEN[i] == g->l[k].forma )
            gtk_combo_box_set_active( GTK_COMBO_BOX(g->forma[k]), i );
        gtk_widget_set_tooltip_text( g->forma[k],
            "La marcada es la que dice el dato. Cámbiala si SABES de un suceso "
            "que explique otra forma: eso es lo único que esta ventana no "
            "puede mirar." );
        g_object_set_data( G_OBJECT(g->forma[k]), "k", GINT_TO_POINTER(k) );
        g_signal_connect( g->forma[k], "changed", G_CALLBACK(on_forma), g );
        gtk_grid_attach( GTK_GRID(rej), g->forma[k], 1, fila, 1, 1 );

        g->linea[k] = gtk_label_new( NULL );
        gtk_label_set_xalign( GTK_LABEL(g->linea[k]), 0.0 );
        gtk_widget_set_tooltip_text( g->linea[k],
            "La línea tal como va a quedar en el .inp." );
        gtk_grid_attach( GTK_GRID(rej), g->linea[k], 2, fila, 1, 1 );
        repinta_linea( g, k );

        raz = gtk_label_new( NULL );
        {
        gchar *t = g->l[k].aviso[0]
                 ? g_markup_printf_escaped( "<small>%s\n<i>%s</i></small>",
                                            g->l[k].razon, g->l[k].aviso )
                 : g_markup_printf_escaped( "<small>%s</small>", g->l[k].razon );

        gtk_label_set_markup( GTK_LABEL(raz), t );
        g_free( t );
        }
        gtk_label_set_xalign( GTK_LABEL(raz), 0.0 );
        gtk_label_set_line_wrap( GTK_LABEL(raz), TRUE );
        gtk_widget_set_margin_bottom( raz, 8 );
        gtk_widget_set_margin_start( raz, 24 );
        gtk_grid_attach( GTK_GRID(rej), raz, 0, fila + 1, 3, 1 );
        }

    /* LO QUE ESTA REGLA NO MIRA, DICHO DONDE SE USA.
     *
     * El diccionario se lee sobre la diferencia REGULAR. Con D = 1 la firma
     * del mismo suceso se repite a s períodos --∇∇_s de un escalón deja +1 en
     * T y −1 en T+s-- y esos dos extremos no son contiguos, así que la
     * lectura escalar los ve como dos sucesos. No se calla: se avisa, porque
     * «no consta» no es «cuadra».                                        */
    if ( D > 0 )
        {
        GtkWidget *av = gtk_label_new( NULL );
        gchar     *t = g_markup_printf_escaped(
            "<small>⚠ Este modelo lleva <b>D = %d</b>. La lectura de arriba "
            "usa sólo la diferencia regular: con diferencia anual, la firma "
            "del mismo suceso se repite a %d períodos, y dos extremos "
            "separados por %d se leen aquí como dos sucesos distintos. "
            "Míralos juntos antes de intervenir los dos.</small>",
            D, freq, freq );

        gtk_label_set_markup( GTK_LABEL(av), t );
        g_free( t );
        gtk_label_set_xalign( GTK_LABEL(av), 0.0 );
        gtk_label_set_line_wrap( GTK_LABEL(av), TRUE );
        gtk_box_pack_start( GTK_BOX(raiz), av, FALSE, FALSE, 0 );
        }

    pie = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(pie),
        "<small>Esto es el <b>peldaño 1</b> de la escalera: la lectura "
        "escalar, UNA intervención por suceso. Si al reestimar la forma deja "
        "un vecino anómalo o no deja ruido blanco, hay que subir de peldaño — "
        "y eso se ve estimando, no aquí. <b>Lo obvio primero.</b></small>" );
    gtk_label_set_xalign( GTK_LABEL(pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(pie), TRUE );
    gtk_box_pack_start( GTK_BOX(raiz), pie, FALSE, FALSE, 0 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    gtk_box_set_homogeneous( GTK_BOX(barra), FALSE );
    gtk_box_pack_end( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Cancelar" );
    g_signal_connect_swapped( b, "clicked", G_CALLBACK(gtk_widget_destroy), g->win );
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Derivar modelo con estas intervenciones" );
    gtk_widget_set_tooltip_text( b,
        "Crea un modelo NUEVO, hijo de éste, con las intervenciones escritas "
        "en su .inp, y lo abre en el editor.\n\nEl padre no se toca: sigue "
        "estimado y con su .out, que es la mitad «sin» de la comparación." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_derivar), g );
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    g_signal_connect( g->win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( g->win );
}
