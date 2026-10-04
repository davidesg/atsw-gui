/*
 * diagnosis_gui.c -- la ventana de diagnosis, que es el dictamen pintado.
 *
 * AQUI NO SE JUZGA NADA. Todo lo que se decide está en lib/dictamen, y esto
 * sólo le pone color y un botón de exportar. Es deliberado: el día que el
 * analista mueva un umbral, lo mueve en un sitio y esta ventana ni se entera.
 *
 * Es el hermano pequeño de la página de diagnosis de drtran_gui --seis
 * pestañas para el caso multivariante-- con la misma forma y menos cosas.
 */

#include <string.h>

#include "dictamen.h"
#include "tabla.h"

#include "analisis.h"


enum { DG_ESTADO, DG_TITULO, DG_DATO, DG_DICE, DG_COLOR, DG_N };

typedef struct {
    AnHost    h;
    char      serie[PR_ID], muestra[PR_ID], id[PR_ID];
    Dictamen  d;
    FueOut    o;
    char      estruct[64];
} Dg;

/* EL COLOR VA DEL ESTADO, y el estado del dictamen. Cuatro, no dos: «no
 * consta» tiene que verse distinto de «cuadra», que es de lo que iba todo. */
static const char *color_de( DxEstado e )
{
    switch ( e )
        {
        case DX_CUADRA: return "#1a7f37";      /* verde   */
        case DX_MIRAR:  return "#9a6700";      /* ámbar   */
        case DX_NO:     return "#b42318";      /* rojo    */
        default:        return "#57606a";      /* gris: no se sabe */
        }
}

static void pinta( Dg *g, GtkWidget *lista )
{
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(lista) ) );
    GtkTreeIter   it;
    int           i;

    gtk_list_store_clear( st );
    for ( i = 0; i < g->d.n; i++ )
        {
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            DG_ESTADO, dx_estado_es( g->d.l[i].estado ),
            DG_TITULO, g->d.l[i].titulo,
            DG_DATO,   g->d.l[i].dato,
            DG_DICE,   g->d.l[i].dice,
            DG_COLOR,  color_de( g->d.l[i].estado ),
            -1 );
        }
}

/* ------------------------------------------------------------------------ */
/* Exportar: la misma tabla, tres renderizados                               */
/* ------------------------------------------------------------------------ */

static void on_exportar( GtkButton *b, Dg *g )
{
    GtkWidget *d;
    Tabla     *t;
    char       titulo[256];
    int        i;

    (void) b;
    g_snprintf( titulo, sizeof titulo, "Diagnosis de %s / %s%s%s",
                g->serie, g->id,
                g->muestra[0] ? ", muestra " : "", g->muestra );
    t = tb_new( titulo );
    if ( t == NULL ) return;

    tb_col( t, "",            NULL, TB_TXT, 0 );
    tb_col( t, "Veredicto",   NULL, TB_TXT, 0 );
    tb_col( t, "Los números", NULL, TB_TXT, 0 );
    tb_col( t, "Qué dice",    NULL, TB_TXT, 0 );

    for ( i = 0; i < g->d.n; i++ )
        {
        tb_fila( t );
        tb_pon_txt( t, 0, g->d.l[i].titulo );
        tb_pon_txt( t, 1, dx_estado_es( g->d.l[i].estado ) );
        tb_pon_txt( t, 2, g->d.l[i].dato );
        tb_pon_txt( t, 3, g->d.l[i].dice );
        }

    /* DE DONDE SALIO, dentro de la tabla. Una diagnosis suelta en un papel,
       sin decir de qué modelo es, no vale para nada dentro de un mes.   */
    {
    char b[128];

    tb_procedencia( t, "Serie",   g->serie );
    tb_procedencia( t, "Modelo",  g->id );
    tb_procedencia( t, "Muestra", g->muestra[0] ? g->muestra : "completa" );
    if ( g->estruct[0] ) tb_procedencia( t, "Estructura", g->estruct );
    g_snprintf( b, sizeof b, "%d observaciones, %d parámetros",
                g->o.nobs, g->o.npar );
    tb_procedencia( t, "Ajuste", b );
    }

    d = gtk_file_chooser_dialog_new( "Exportar la diagnosis",
            g->h.padre, GTK_FILE_CHOOSER_ACTION_SAVE,
            "Cancelar", GTK_RESPONSE_CANCEL, "Guardar", GTK_RESPONSE_ACCEPT,
            NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
    {
    char sug[256];

    g_snprintf( sug, sizeof sug, "%s_%s_dx.txt", g->serie, g->id );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), sug );
    }

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );

        /* EL FORMATO LO DICE LA EXTENSION, y eso ya lo sabe lib/tabla: es
           lo que el analista escribió al ponerle nombre.               */
        if ( tb_write( t, p ) == 0 )
            {
            gchar *s = g_strdup_printf( "Diagnosis en %s.", p );

            an_di( &g->h, "%s", s );
            g_free( s );
            }
        else
            an_di( &g->h, "No pude escribirla." );
        g_free( p );
        }
    gtk_widget_destroy( d );
    tb_free( t );
}

static void on_identificar( GtkButton *b, Dg *g )
{
    (void) b;
    an_identifica_residuos( &g->h, g->serie, g->muestra, g->id );
}

static void on_cerrar( GtkWidget *w, Dg *g ) { (void) w; g_free( g ); }


/* ------------------------------------------------------------------------ */

void an_diagnosis( const AnHost *h, const char *serie, const char *muestra,
                     const char *id )
{
    Dg           *g;
    Convergence   c;
    GtkWidget    *win, *raiz, *cab, *lista, *barra, *b;
    GtkListStore *st;
    char          out[PR_RUTA];
    int           i;

    if ( !h || !h->p || !serie || !*serie || !id || !*id ) return;

    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 )
        { an_di( h, "No pude componer la ruta." ); return; }

    g = g_new0( Dg, 1 );
    g->h = *h;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );

    if ( !fueout_read( out, &g->o ) )
        {
        /* SE DICE QUE NO ESTA ESTIMADO. Abrir una ventana vacía dejaría al
           analista preguntándose si falló el programa.                 */
        gchar *s = g_strdup_printf( "«%s» no está estimado: no hay .out que "
                                    "diagnosticar.", id );

        an_di( h, "%s", s );
        g_free( s ); g_free( g );
        return;
        }
    fueout_estructura( &g->o, g->estruct, sizeof g->estruct );
    if ( !convergence_of( out, &c ) ) memset( &c, 0, sizeof c );
    {
    /* LOS NOMBRES, del .inp: el .out no los trae y sin ellos el dictamen
       habla de «la intervención 12», que obliga a contar líneas.    */
    static char det[AN_MAX_DET][AN_LDET];
    const char *ptr[AN_MAX_DET];
    int         nd, i;

    nd = an_deterministas( h->p, serie, muestra, id, det, AN_MAX_DET );
    for ( i = 0; i < nd; i++ ) ptr[i] = det[i];
    dx_dictamen( &g->o, &c, nd ? ptr : NULL, nd, &g->d );
    }
    convergence_clear( &c );

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( h->padre ) gtk_window_set_transient_for( GTK_WINDOW(win), h->padre );
    gtk_window_set_default_size( GTK_WINDOW(win), 760, 340 );
    {
    gchar *t = g_strdup_printf( "Diagnosis — %s / %s", serie, id );

    gtk_window_set_title( GTK_WINDOW(win), t );
    g_free( t );
    }

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(win), raiz );

    /* LA CABECERA: de qué modelo se habla, y el resumen de una ojeada. */
    cab = gtk_label_new( NULL );
    {
    gchar *t = g_markup_printf_escaped(
        "<b>%s / %s</b>%s%s   <span foreground=\"%s\"><b>%s</b></span>\n"
        "<small>%s · %d observaciones, %d parámetros</small>",
        serie, id,
        ( muestra && *muestra ) ? "   muestra " : "",
        ( muestra && *muestra ) ? muestra : "",
        color_de( g->d.peor ), dx_estado_es( g->d.peor ),
        g->estruct[0] ? g->estruct : "", g->o.nobs, g->o.npar );

    gtk_label_set_markup( GTK_LABEL(cab), t );
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    g_free( t );
    }
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    st = gtk_list_store_new( DG_N, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );

    for ( i = 0; i < 4; i++ )
        {
        static const char *tit[] = { "", "", "Los números", "Qué dice" };
        GtkCellRenderer   *r = gtk_cell_renderer_text_new();

        /* EL SALTO DE LINEA VA EN EL PINTADO, NO EN EL DATO.
         *
         * La escalera del Ljung-Box son cuatro peldaños en una línea, y esa
         * línea empujaba fuera de la ventana lo que el bloque DICE -- que es
         * justo lo que hay que leer.
         *
         * Metiéndolo en el dato se arreglaría la ventana y se rompería el
         * CSV: un salto de línea dentro de un campo obliga a entrecomillar y
         * deja de ser una tabla que se lee en cualquier sitio. El ancho es
         * cosa de quien pinta.                                          */
        if ( i == DG_DATO )
            g_object_set( r, "family", "monospace", "wrap-width", 250,
                          "wrap-mode", PANGO_WRAP_WORD_CHAR,
                          "yalign", 0.0, NULL );
        if ( i == DG_DICE )
            g_object_set( r, "wrap-width", 330, "wrap-mode", PANGO_WRAP_WORD,
                          "yalign", 0.0, NULL );
        if ( i == DG_ESTADO || i == DG_TITULO )
            g_object_set( r, "yalign", 0.0, NULL );
        gtk_tree_view_insert_column_with_attributes(
            GTK_TREE_VIEW(lista), -1, tit[i], r, "text", i,
            "foreground", DG_COLOR, NULL );
        }
    pinta( g, lista );

    {
    GtkWidget *s = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(s),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(s), lista );
    gtk_box_pack_start( GTK_BOX(raiz), s, TRUE, TRUE, 0 );
    }

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    b = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(b),
        "<small>Esto dice lo que los números dicen. Qué hacer con ello —qué "
        "quitar, qué intervenir— es una decisión, y es tuya.</small>" );
    gtk_label_set_xalign( GTK_LABEL(b), 0.0 );
    gtk_box_pack_start( GTK_BOX(barra), b, TRUE, TRUE, 0 );

    /* E4: CUANDO EL Q FALLA, el identificador sobre los mismos residuos.
     * La diagnosis sigue sin recomendar: el botón abre otra ventana, que
     * enseña su evidencia, y lo que se haga lo decide el analista. */
    {
    int i, falla = 0;
    for ( i = 0; i < g->d.n; i++ )
        if ( !strcmp( g->d.l[i].titulo, "Autocorrelación" ) &&
             ( g->d.l[i].estado == DX_NO || g->d.l[i].estado == DX_MIRAR ) ) falla = 1;
    b = gtk_button_new_with_label( "Identificar los residuos…" );
    gtk_widget_set_sensitive( b, falla );
    gtk_widget_set_tooltip_text( b, falla
        ? "Los residuos aún tienen autocorrelación. art lee su correlograma y "
          "propone qué le falta al modelo; derivar lo añade como un factor más."
        : "La autocorrelación de los residuos cuadra: no hay nada que identificar." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_identificar), g );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );
    }

    b = gtk_button_new_with_label( "Exportar…" );
    gtk_widget_set_tooltip_text( b,
        "La misma tabla en TXT, CSV o TeX — lo dice la extensión que le "
        "pongas. Con la procedencia dentro: una diagnosis suelta en un papel, "
        "sin decir de qué modelo es, no vale nada dentro de un mes." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_exportar), g );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    g_signal_connect( win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( win );
}
