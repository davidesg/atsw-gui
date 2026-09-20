/*
 * main.c -- atsw_gui, la interfaz madre.
 *
 * El nombre: la FAMILIA se llama atsw y el programa atsw_gui, por el mismo
 * convenio que gui/fue -> fue_gui y gui/drtran -> drtran_gui. Un nombre señala
 * una cosa sola, y esa regla ya costo un renombrado.
 */

#include <string.h>

#include "atsw.h"

void atsw_lanza( Atsw *a, const char *programa, const char *fichero );
gboolean atsw_itera( Atsw *a, const char *serie, const char *padre,
                     char *why, size_t n );
void atsw_datos( Atsw *a );
void atsw_vistazo( Atsw *a, const char *inp, int modo, double lam );

static Atsw A;

void barra_pub( Atsw *a, const char *s )
{
    gtk_label_set_text( GTK_LABEL(a->estado), s );
    gtk_widget_set_tooltip_text( a->estado, s );
}

/* ------------------------------------------------------------------------ */

static gchar *marcada( GtkWidget *tv, int columna );

static gchar *marcada( GtkWidget *tv, int columna )
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tv) );
    GtkTreeModel     *mo;
    GtkTreeIter       it;
    gchar            *s = NULL;

    if ( gtk_tree_selection_get_selected( sel, &mo, &it ) )
        gtk_tree_model_get( mo, &it, columna, &s, -1 );
    return s;
}

/* Definidos mas abajo: el menu los usa. */
static void on_fue( GtkButton *b, Atsw *a );
static void on_fug( GtkButton *b, Atsw *a );
static gboolean que_mandar( Atsw *a, gboolean acepta_pre, char *out, size_t n );

/* EL VISTAZO: el .inp de la serie marcada, nunca el .pre. Identificar es
 * antes de que haya modelo, y esto es identificar.                     */
static void vistazo( Atsw *a, int modo, double lam )
{
    char f[PR_RUTA];

    if ( !que_mandar( a, FALSE, f, sizeof f ) )
        { barra_pub( a, "Esa serie no tiene todavía ningún fichero que mirar." );
          return; }
    atsw_vistazo( a, f, modo, lam );
}

static void on_mdt   ( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 1, 1.0 ); }
static void on_mdtlog( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 1, 0.0 ); }
static void on_serie_acf( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 0, 1.0 ); }

/* EL MENU DE LA SERIE.
 *
 * Marcar una serie con el boton izquierdo despliega lo que se puede HACER con
 * ella. Antes marcar no hacia nada visible: la rejilla se llenaba y habia que
 * saber que los botones de la derecha existian y a que apuntaban. El gesto de
 * marcar tiene que OFRECER, no quedarse mudo.
 *
 * La seleccion se hace ANTES de desplegar, asi que el menu actua sobre la
 * serie que se acaba de marcar y la rejilla de al lado ya la enseña.
 *
 * Se cierra con Escape o pulsando fuera, y la serie se queda marcada.     */
static void menu_serie( Atsw *a, GdkEventButton *ev )
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *mi;

    mi = gtk_menu_item_new_with_label( "Identificación con fug" );
    gtk_widget_set_tooltip_text( mi,
        "Los gráficos de la serie, su ACF y su PACF. Se manda el .inp: se "
        "identifica ANTES de que haya modelo." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_fug), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    mi = gtk_menu_item_new_with_label( "Modelo univariante con fue" );
    gtk_widget_set_tooltip_text( mi,
        "Estima el modelo de esta serie. Se manda el .pre si lo hay —es un "
        "óptimo reejecutable— y si no el .inp." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_fue), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );

    /* LOS ATAJOS. Dibuja fug, aqui mismo, sin levantar la herramienta
     * completa. Para mirar, no para decidir: no tocan el proyecto.    */
    mi = gtk_menu_item_new_with_label( "Media – desviación típica" );
    gtk_widget_set_tooltip_text( mi,
        "¿La dispersión crece con el nivel? Es la pregunta de la "
        "transformación, y se contesta mirando este dibujo y el de logaritmos." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_mdt), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    mi = gtk_menu_item_new_with_label( "Media – desviación típica (logaritmos)" );
    gtk_widget_set_tooltip_text( mi,
        "El mismo dibujo con λ = 0. Si aquí la nube se endereza y en el otro "
        "no, la serie pide logaritmos." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_mdtlog), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    mi = gtk_menu_item_new_with_label( "Serie y ACF / PACF…" );
    gtk_widget_set_tooltip_text( mi,
        "Con λ, d y D AL PIE DEL GRÁFICO: se tocan y el dibujo se rehace ahí "
        "mismo. Es la pregunta abierta —cuántas diferencias— y se contesta "
        "probando." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_serie_acf), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    gtk_widget_show_all( menu );
    gtk_menu_popup_at_pointer( GTK_MENU(menu), (GdkEvent *) ev );
}

static gboolean on_serie_click( GtkWidget *tv, GdkEventButton *ev, Atsw *a )
{
    GtkTreePath *ruta = NULL;

    if ( ev->type != GDK_BUTTON_PRESS || ev->button != 1 ) return FALSE;
    if ( !gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(tv), (gint) ev->x,
                                         (gint) ev->y, &ruta, NULL, NULL, NULL ) )
        return FALSE;                       /* se pulso fuera de toda fila */

    /* Primero se MARCA --y eso llena la rejilla de al lado-- y despues se
       ofrece: el menu actua sobre lo que ya esta marcado.              */
    gtk_tree_view_set_cursor( GTK_TREE_VIEW(tv), ruta, NULL, FALSE );
    gtk_tree_path_free( ruta );

    menu_serie( a, ev );
    return TRUE;                            /* la seleccion ya esta hecha */
}

static void on_serie( GtkTreeSelection *sel, gpointer d )
{
    Atsw  *a = d;
    gchar *s;

    (void) sel;
    /* REPINTANDO: la lista se esta rehaciendo y "changed" no dice nada del
       analista. Leerla aqui borraba la marca que acababa de ponerse.  */
    if ( a->recolocando ) return;

    s = marcada( a->l_series, 0 );
    if ( s == NULL ) return;          /* deseleccion: se conserva la marca */

    snprintf( a->serie, sizeof a->serie, "%s", s );
    g_free( s );
    atsw_refresca( a );
}

static void on_abrir( GtkButton *b, Atsw *a )
{
    GtkWidget *d;

    (void) b;
    d = gtk_file_chooser_dialog_new( "Abrir un proyecto",
            GTK_WINDOW(a->ventana), GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Abrir", GTK_RESPONSE_ACCEPT,
            NULL );
    {
    GtkFileFilter *f = gtk_file_filter_new();

    gtk_file_filter_set_name( f, "Proyecto (*.yaml)" );
    gtk_file_filter_add_pattern( f, "*.yaml" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    }

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
        char   why[512];

        if ( !atsw_abre( a, p, why, sizeof why ) )
            /* UN MANIFIESTO ROTO NO SE PISA: se dice y no se abre. */
            barra_pub( a, why );
        else
            atsw_refresca( a );
        g_free( p );
        }
    gtk_widget_destroy( d );
}

static void on_nuevo( GtkButton *b, Atsw *a )
{
    GtkWidget *d;

    (void) b;
    d = gtk_file_chooser_dialog_new( "Proyecto nuevo",
            GTK_WINDOW(a->ventana), GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Crear", GTK_RESPONSE_ACCEPT,
            NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), "proyecto.yaml" );

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar  *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
        gchar  *base = g_path_get_basename( p );
        PrError e;
        char   *dot;

        if ( !a->p ) a->p = g_new0( Proyecto, 1 );
        dot = strrchr( base, '.' ); if ( dot ) *dot = '\0';
        pr_nuevo( a->p, base, "", "." );
        snprintf( a->p->path, sizeof a->p->path, "%s", p );
        a->hay = TRUE; a->nr = 0; a->serie[0] = '\0';

        if ( pr_escribir( a->p, p, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Proyecto nuevo. Abre una serie en fue para empezar." );

        g_free( base ); g_free( p );
        atsw_refresca( a );
        }
    gtk_widget_destroy( d );
}

static void on_datos( GtkButton *b, Atsw *a )  { (void)b; atsw_datos( a ); }

/* EL FICHERO QUE SE MANDA, Y NO ES EL MISMO SEGUN A DONDE.
 *
 * La distincion es de METODO, no de formato:
 *
 *   a fug  SE IDENTIFICA, y se identifica ANTES de que haya modelo. Se manda
 *          el .inp -- la especificacion con sus datos. Mandarle un .pre seria
 *          sugerir que se identifica algo ya estimado, que es al reves.
 *
 *   a fue  SE ESTIMA, y ahi el .pre SI vale y vale mas: es un optimo
 *          reejecutable, asi que arrancar de el ahorra la busqueda. Si no lo
 *          hay todavia, el .inp.
 *
 * Si no hay modelo marcado se usa el elegido de la serie.              */
static gboolean que_mandar( Atsw *a, gboolean acepta_pre, char *out, size_t n )
{
    gchar *id = marcada( a->l_modelos, 0 );
    const char *m;

    if ( !a->hay || !a->serie[0] ) { out[0] = '\0'; return FALSE; }

    m = id ? id : atsw_modelo_por_defecto( a->p, a->serie );
    if ( !m || !*m ) { g_free( id ); out[0] = '\0'; return FALSE; }

    if ( acepta_pre &&
         pr_ruta( a->p, a->serie, m, ".pre", out, n ) == 0 &&
         g_file_test( out, G_FILE_TEST_EXISTS ) )
        { g_free( id ); return TRUE; }

    if ( pr_ruta( a->p, a->serie, m, ".inp", out, n ) == 0 &&
         g_file_test( out, G_FILE_TEST_EXISTS ) )
        { g_free( id ); return TRUE; }

    g_free( id );
    out[0] = '\0';
    return FALSE;
}

static void manda( Atsw *a, const char *programa, const char *para,
                   gboolean acepta_pre )
{
    char f[PR_RUTA];

    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }
    if ( !que_mandar( a, acepta_pre, f, sizeof f ) )
        {
        /* SE DICE QUE NO HAY QUE MANDAR, en vez de abrir el programa vacio y
         * dejar al analista preguntandose si fallo algo.               */
        gchar *s = g_strdup_printf(
            a->serie[0] ? "«%s» no tiene todavía ningún fichero que mandar: "
                          "carga los datos o marca un modelo."
                        : "Marca una serie para %s.",
            a->serie[0] ? a->serie : para );

        barra_pub( a, s );
        g_free( s );
        atsw_lanza( a, programa, NULL );
        return;
        }
    atsw_lanza( a, programa, f );
}

static void on_fue( GtkButton *b, Atsw *a )
     { (void)b; manda( a, "fue_gui", "estimarla", TRUE ); }
/* A fug NO se le manda un .pre: se identifica antes de que haya modelo. */
static void on_fug( GtkButton *b, Atsw *a )
     { (void)b; manda( a, "gtk_fmg", "identificarla", FALSE ); }
/* drtran es de la RED: trabaja con n series, no con una. Se abre con el
 * proyecto y alli se eligen.                                          */
static void on_drtran( GtkButton *b, Atsw *a )
     { (void)b; atsw_lanza( a, "drtran_gui", NULL ); }

/* Un texto en una linea. Devuelve TRUE si se acepto. */
static gboolean pide_texto( Atsw *a, const char *titulo, const char *aviso,
                            const char *previo, char *out, size_t n )
{
    GtkWidget *d, *caja, *e, *l;
    gboolean   si = FALSE;

    d = gtk_dialog_new_with_buttons( titulo, GTK_WINDOW(a->ventana),
            GTK_DIALOG_MODAL, "_Cancelar", GTK_RESPONSE_CANCEL,
            "_Aceptar", GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    l = gtk_label_new( aviso );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_widget_set_halign( l, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 4 );

    e = gtk_entry_new();
    gtk_entry_set_width_chars( GTK_ENTRY(e), 56 );
    if ( previo ) gtk_entry_set_text( GTK_ENTRY(e), previo );
    gtk_entry_set_activates_default( GTK_ENTRY(e), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), e, FALSE, FALSE, 4 );
    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_ACCEPT );

    gtk_widget_show_all( d );
    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        { snprintf( out, n, "%s", gtk_entry_get_text( GTK_ENTRY(e) ) ); si = TRUE; }
    gtk_widget_destroy( d );
    return si;
}

static void on_iterar( GtkButton *b, Atsw *a )
{
    gchar *padre = marcada( a->l_modelos, 0 );
    char   why[512];

    (void) b;
    if ( !a->serie[0] ) { barra_pub( a, "Marca una serie." ); return; }
    atsw_itera( a, a->serie, padre ? padre : "", why, sizeof why );
    barra_pub( a, why );
    g_free( padre );
    atsw_refresca( a );
}

static void on_elegir( GtkButton *b, Atsw *a )
{
    gchar  *id = marcada( a->l_modelos, 0 );
    char    razon[PR_RAZON] = "";
    PrError e;

    (void) b;
    if ( !id ) { barra_pub( a, "Marca el modelo que eliges." ); return; }

    if ( pide_texto( a, "El modelo elegido",
            "Por qué éste y no otro. Se puede dejar en blanco: sin razón "
            "se verá como sin razón, que es mejor que una inventada.",
            pr_elegido( a->p, a->serie ), razon, sizeof razon ) )
        {
        if ( pr_elige( a->p, a->serie, id, razon, &e ) != 0 ||
             pr_escribir( a->p, a->p->path, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Elegido declarado y guardado." );
        atsw_refresca( a );
        }
    g_free( id );
}

static void on_razon( GtkButton *b, Atsw *a )
{
    gchar  *id = marcada( a->l_modelos, 0 );
    char    razon[PR_RAZON] = "";
    PrError e;
    int     i;

    (void) b;
    if ( !id ) { barra_pub( a, "Marca la iteración." ); return; }
    i = pr_modelo_idx( a->p, a->serie, id );

    /* SE PIDE, NO SE EXIGE, y se puede poner DESPUES -- mirando el .out, que
     * es cuando de verdad se sabe por que.                              */
    if ( pide_texto( a, "El porqué de esta iteración",
            "Qué te hizo pasar del modelo anterior a éste. Se puede dejar en "
            "blanco y ponerlo más tarde.",
            i >= 0 ? a->p->m[i].razon : "", razon, sizeof razon ) )
        {
        if ( pr_razon( a->p, a->serie, id, razon, &e ) != 0 ||
             pr_escribir( a->p, a->p->path, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, razon[0] ? "Razón guardada."
                                   : "Sin razón: se seguirá viendo como tal." );
        atsw_refresca( a );
        }
    g_free( id );
}

/* ------------------------------------------------------------------------ */

static void columna( GtkWidget *tv, const char *t, int c )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *k = gtk_tree_view_column_new_with_attributes(
                               t, r, "text", c, NULL );

    gtk_tree_view_column_set_resizable( k, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), k );
}

static GtkWidget *en_scroll( GtkWidget *w )
{
    GtkWidget *s = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(s),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(s), w );
    return s;
}

static GtkWidget *boton( GtkWidget *caja, const char *txt, const char *tip,
                         GCallback cb, Atsw *a )
{
    GtkWidget *b = gtk_button_new_with_label( txt );

    if ( tip ) gtk_widget_set_tooltip_text( b, tip );
    g_signal_connect( b, "clicked", cb, a );
    gtk_box_pack_start( GTK_BOX(caja), b, FALSE, FALSE, 0 );
    return b;
}

static void activate( GtkApplication *app, gpointer d )
{
    Atsw         *a = d;
    GtkWidget    *w, *raiz, *barra, *pan, *izq, *der, *vb;
    GtkListStore *st;

    w = gtk_application_window_new( app );
    a->ventana = w;
    gtk_window_set_title( GTK_WINDOW(w), "ATSW — el taller" );
    gtk_window_set_default_size( GTK_WINDOW(w), 1000, 620 );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
    gtk_container_add( GTK_CONTAINER(w), raiz );

    /* --- la barra --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    boton( barra, "Abrir…", "Un proyecto.yaml. Si está roto se dice y NO se "
                            "abre: nunca se pisa.", G_CALLBACK(on_abrir), a );
    boton( barra, "Nuevo…", NULL, G_CALLBACK(on_nuevo), a );
    boton( barra, "Datos…",
        "De un .xlsx, un .csv o un .txt a n SERIES del proyecto, cada una con "
        "su primer .inp.\n\nEs el eslabón que faltaba: el camino datos → "
        ".inp(-1) no lo recorría nadie.", G_CALLBACK(on_datos), a );

    a->l_proy = gtk_label_new( "(sin proyecto)" );
    gtk_label_set_ellipsize( GTK_LABEL(a->l_proy), PANGO_ELLIPSIZE_MIDDLE );
    gtk_box_pack_start( GTK_BOX(barra), a->l_proy, TRUE, TRUE, 8 );

    a->b_drtran = boton( barra, "drtran",
        "Las transferencias. Trabaja con VARIAS series a la vez, así que se "
        "abre con el proyecto y allí se eligen.",
        G_CALLBACK(on_drtran), a );

    /* --- las dos listas --- */
    pan = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
    gtk_box_pack_start( GTK_BOX(raiz), pan, TRUE, TRUE, 0 );

    izq = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    st = gtk_list_store_new( S_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
                             G_TYPE_STRING );
    a->l_series = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( a->l_series, "Serie",   S_ID );
    columna( a->l_series, "Elegido", S_ELEGIDO );
    columna( a->l_series, "Modelos", S_NMOD );
    gtk_widget_set_tooltip_text( a->l_series,
        "El modelo ELEGIDO de cada serie. Hoy esa decisión vive en un "
        "diccionario a pelo repetido en tres guiones de cases/." );
    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_series) ),
                      "changed", G_CALLBACK(on_serie), a );
    /* Marcar con el boton izquierdo despliega lo que se puede hacer. */
    g_signal_connect( a->l_series, "button-press-event",
                      G_CALLBACK(on_serie_click), a );
    gtk_box_pack_start( GTK_BOX(izq), en_scroll( a->l_series ), TRUE, TRUE, 0 );
    gtk_paned_pack1( GTK_PANED(pan), izq, FALSE, FALSE );

    der = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    {
    GtkWidget *b2 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );

    a->b_iterar = boton( b2, "Iterar",
        "Copia el .pre del modelo marcado a un .inp nuevo y registra su "
        "linaje.\n\n.pre e .inp son el mismo formato, pero el motor exige "
        "la extensión .inp: es una copia de verdad. Es el paso que cierra "
        "el ciclo y hasta ahora no lo hacía nadie.",
        G_CALLBACK(on_iterar), a );
    a->b_elegir = boton( b2, "Elegir",
        "Declara que éste es EL modelo de la serie.", G_CALLBACK(on_elegir), a );
    a->b_razon = boton( b2, "Razón…",
        "El porqué de esta iteración. Se puede poner después —mirando el "
        ".out, que es cuando de verdad se sabe— o no ponerse.",
        G_CALLBACK(on_razon), a );

    gtk_box_pack_start( GTK_BOX(b2), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    /* LOS DOS ENVIOS. Van aqui, con la rejilla, porque actuan sobre el modelo
     * MARCADO -- el mismo sujeto que Iterar, Elegir y Razón.           */
    a->b_fug = boton( b2, "→ fug",
        "Manda esta serie a fug para IDENTIFICARLA: sus gráficos, su ACF y "
        "su PACF.\n\nSe manda el .inp, nunca el .pre: se identifica ANTES "
        "de que haya modelo.", G_CALLBACK(on_fug), a );
    a->b_fue = boton( b2, "→ fue",
        "Manda esta serie a fue para ESTIMAR su modelo.\n\nSe manda el .pre "
        "si lo hay —es un óptimo reejecutable— y si no el .inp, que es la "
        "especificación.", G_CALLBACK(on_fue), a );

    gtk_box_pack_start( GTK_BOX(der), b2, FALSE, FALSE, 0 );
    }

    st = gtk_list_store_new( M_N, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    a->l_modelos = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( a->l_modelos, "",          M_ESTRELLA );
    columna( a->l_modelos, "Modelo",    M_ID );
    columna( a->l_modelos, "Versión",   M_VER );
    columna( a->l_modelos, "Viene de",  M_PADRE );
    columna( a->l_modelos, "d.t. res.", M_SD );
    columna( a->l_modelos, "Hosking",   M_Q );
    columna( a->l_modelos, "Blancos",   M_BLANCO );
    columna( a->l_modelos, "Por qué",   M_RAZON );
    gtk_widget_set_tooltip_text( a->l_modelos,
        "Los números salen del .out, no del manifiesto, y se releen cuando el "
        "fichero cambia. Cachearlos podría mentir: si alguien reestima por "
        "fuera, el número guardado seguiría diciendo lo de antes.\n\nEl "
        "manifiesto guarda linaje y razón, que son DECISIONES." );
    gtk_box_pack_start( GTK_BOX(der), en_scroll( a->l_modelos ), TRUE, TRUE, 0 );
    gtk_paned_pack2( GTK_PANED(pan), der, TRUE, FALSE );
    gtk_paned_set_position( GTK_PANED(pan), 300 );

    /* --- los dos veredictos, y la barra --- */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    a->ver_cuenta = gtk_label_new( "" );
    a->ver_ojo    = gtk_label_new( "" );
    gtk_widget_set_halign( a->ver_cuenta, GTK_ALIGN_START );
    gtk_widget_set_halign( a->ver_ojo,    GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(a->ver_cuenta), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(a->ver_ojo),    PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(vb), a->ver_cuenta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), a->ver_ojo,    FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(raiz), vb, FALSE, FALSE, 0 );

    a->estado = gtk_label_new( "" );
    gtk_widget_set_halign( a->estado, GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(a->estado), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(raiz), a->estado, FALSE, FALSE, 0 );

    atsw_refresca( a );
    gtk_widget_show_all( w );
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    const char     *proy = NULL;
    int             rc, i;

    for ( i = 1; i < argc; i++ )
        {
        if ( !strcmp( argv[i], "--proyecto" ) && i + 1 < argc ) proy = argv[++i];
        else if ( !strncmp( argv[i], "--proyecto=", 11 ) ) proy = argv[i] + 11;
        else if ( !strcmp( argv[i], "-h" ) || !strcmp( argv[i], "--help" ) )
            {
            printf( "uso: %s [--proyecto FICHERO]\n\n"
                    "  ATSW GUI: el taller. Gestiona datos, modelos y\n"
                    "  proyectos, y lanza fue, fug y drtran con el proyecto\n"
                    "  abierto.\n", argv[0] );
            return 0;
            }
        else { fprintf( stderr, "%s: no entiendo «%s»\n", argv[0], argv[i] );
               return 2; }
        }

    if ( proy )
        {
        char why[512];

        if ( !atsw_abre( &A, proy, why, sizeof why ) )
            { fprintf( stderr, "%s: %s\n", proy, why ); return 3; }
        }

    argc = 1;
    app = gtk_application_new( "org.atsw.gui", G_APPLICATION_NON_UNIQUE );
    g_signal_connect( app, "activate", G_CALLBACK(activate), &A );
    rc = g_application_run( G_APPLICATION(app), argc, argv );
    g_object_unref( app );
    return rc;
}
