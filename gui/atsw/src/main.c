/*
 * main.c -- atsw_gui, la interfaz madre.
 *
 * El nombre: la FAMILIA se llama atsw y el programa atsw_gui, por el mismo
 * convenio que gui/fue -> fue_gui y gui/drtran -> drtran_gui. Un nombre señala
 * una cosa sola, y esa regla ya costo un renombrado.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "datos.h"

#include "atsw.h"
#include "anfitrion.h"

void atsw_lanza( Atsw *a, const char *programa, const char *fichero );
gboolean atsw_modelo_nuevo( Atsw *a, const char *serie, const char *muestra,
                            char *id_out, size_t nid,
                            char *ruta_out, size_t nruta, char *why, size_t n );
gboolean atsw_itera( Atsw *a, const char *serie, const char *muestra,
                     const char *padre, char *why, size_t n );
void atsw_datos( Atsw *a );
void atsw_vistazo( Atsw *a, const char *inp, int modo, double lam );

static Atsw A;

void barra_pub( Atsw *a, const char *s )
{
    gtk_label_set_text( GTK_LABEL(a->estado), s );
    gtk_widget_set_tooltip_text( a->estado, s );
}

/* ------------------------------------------------------------------------ */


gchar *atsw_marcada( GtkWidget *tv, int columna )
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
static void on_nuevo_modelo( GtkButton *b, Atsw *a );
static void on_iterar( GtkButton *b, Atsw *a );
static GtkWidget *menu_muestras( Atsw *a, const char *padre,
                                 const char *desde );
static gboolean que_mandar( Atsw *a, gboolean acepta_pre, char *out, size_t n );

/* IDENTIFICAR ES SOBRE LOS DATOS, Y PUNTO.
 *
 * Esto lo usan fug y los atajos graficos, y los dos hacen lo mismo: enseñar
 * la serie, su ACF y su PACF para decidir la transformacion. Eso se hace
 * ANTES de que haya modelo, asi que el sujeto es el nodo de DATOS -- no el
 * modelo que estuviera marcado.
 *
 * Mandar el .inp de un modelo no fallaba, y por eso no se notaba: fug dibuja
 * los mismos datos y se trae de paso la lambda y las diferencias de ese
 * modelo. Pero eso es empezar a mirar por donde ya se habia decidido, que es
 * lo contrario de identificar -- y las mueve uno al pie del grafico, asi que
 * el punto de partida no es una capacidad, es una comodidad que confunde.
 *
 * ES EL m00 DE LA HOJA EN QUE SE ESTE: en «pre-covid» se identifica sobre la
 * serie recortada, que es para lo que esa hoja tiene su propio nodo.     */
static gboolean que_identificar( Atsw *a, char *out, size_t n )
{
    const char *mu, *datos;

    out[0] = '\0';
    if ( !a->hay || !a->serie[0] ) return FALSE;

    mu    = atsw_muestra_actual( a );
    datos = pr_datos_de( a->p, a->serie, mu );
    if ( !*datos ) return FALSE;

    return pr_ruta( a->p, a->serie, mu, datos, ".inp", out, n ) == 0 &&
           g_file_test( out, G_FILE_TEST_EXISTS );
}

static void vistazo( Atsw *a, int modo, double lam )
{
    char f[PR_RUTA];

    if ( !que_identificar( a, f, sizeof f ) )
        { barra_pub( a, "Esa serie no tiene datos en esta muestra: no hay "
                        "nada que mirar todavía." );
          return; }
    atsw_vistazo( a, f, modo, lam );
}

static void on_mdt   ( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 1, 1.0 ); }
static void on_serie_acf( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 0, 1.0 ); }

static void on_editar_serie( GtkMenuItem *m, Atsw *a )
{
    (void) m;
    if ( a->hay && a->serie[0] ) atsw_serie_edita( a, a->serie );
}

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
        "Los gráficos de la serie, su ACF y su PACF. Va el nodo de datos de "
        "esta muestra: se identifica ANTES de que haya modelo." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_fug), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    /* LO QUE VA A PASAR, DICHO EN LA ETIQUETA. Que el envio a fue derive un
     * modelo cuando lo unico que hay son los datos es correcto, pero si el
     * menu no lo dice el analista no sabe que puede empezar uno: «no es
     * obvio como hacerlo» era exactamente esto.                        */
    {
    const char *porde = atsw_modelo_por_defecto( a->p, a->serie );
    gboolean    solo_datos = ( !*porde || pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), porde ) );
    gchar      *txt;

    if ( solo_datos )
        txt = g_strdup( "Especificar el primer modelo con fue" );
    else
        txt = g_strdup_printf( "Estimar %s en fue", porde );

    mi = gtk_menu_item_new_with_label( txt );
    g_free( txt );
    gtk_widget_set_tooltip_text( mi, solo_datos
        ? "Los datos no se estiman: nace un modelo colgado de ellos —el "
          "primero de la serie— y es ése el que se abre en fue. El .inp de "
          "los datos sigue intacto."
        : "Se manda el .pre si lo hay —es un óptimo reejecutable— y si no "
          "el .inp, que es la especificación." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_fue), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    /* Y empezar OTRO desde los datos, que no es lo mismo que iterar. */
    if ( !solo_datos && pr_datos_de( a->p, a->serie, atsw_muestra_actual( a ) )[0] )
        {
        mi = gtk_menu_item_new_with_label( "Otro modelo, desde los datos" );
        gtk_widget_set_tooltip_text( mi,
            "Empieza de cero: cuelga de los datos, no del modelo de ahora. "
            "Para seguir desde un óptimo está «Iterar»." );
        g_signal_connect( mi, "activate", G_CALLBACK(on_nuevo_modelo), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        }
    }

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );

    {
    GtkWidget *sub = menu_muestras( a, NULL, "\x01" );   /* ninguna excluida */

    if ( sub && a->nhojas > 1 )
        {
        mi = gtk_menu_item_new_with_label( "Modelo nuevo, en otra muestra…" );
        gtk_widget_set_tooltip_text( mi,
            "Empieza un modelo sobre una ventana declarada. Cuelga de los "
            "datos, y su .inp se genera del datos.csv recortado." );
        gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        }
    else if ( sub )
        gtk_widget_destroy( sub );
    }

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );

    mi = gtk_menu_item_new_with_label( "Editar la serie…" );
    gtk_widget_set_tooltip_text( mi,
        "Qué es, en qué unidades, de dónde se bajó y cuándo. Nada de esto "
        "cabe en el .inp —el motor sólo lee un nombre— y es la mitad de lo "
        "que hace falta para volver a este análisis dentro de seis meses." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_editar_serie), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );

    /* LOS ATAJOS. Dibuja fug, aqui mismo, sin levantar la herramienta
     * completa. Para mirar, no para decidir: no tocan el proyecto.    */
    mi = gtk_menu_item_new_with_label( "Media – desviación típica" );
    gtk_widget_set_tooltip_text( mi,
        "¿La dispersión crece con el nivel? Empieza en la serie EN NIVEL "
        "(λ = 1) y se mueve λ al pie del gráfico: si la nube se endereza con "
        "logaritmos, la serie los pide." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_mdt), a );
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

    /* EL BOTON DERECHO, COMO EN LA REJILLA. Estaba en el izquierdo para que
     * el menu se encontrara, pero eso cobraba un menu por cada vez que se
     * marcaba una serie -- y marcar es lo que mas se hace aqui, porque es
     * lo que llena la rejilla de al lado. Marcar y pedir son dos gestos
     * distintos y ahora son dos botones distintos, el mismo reparto en las
     * dos listas.                                                      */
    if ( ev->type != GDK_BUTTON_PRESS || ev->button != 3 ) return FALSE;
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

    s = atsw_marcada( a->l_series, 0 );
    if ( s == NULL ) return;          /* deseleccion: se conserva la marca */

    snprintf( a->serie, sizeof a->serie, "%s", s );
    g_free( s );
    /* Marcar una serie es pedir sus modelos: la derecha deja el caso. */
    a->viendo_caso = FALSE;
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

static void on_proyecto( GtkButton *b, Atsw *a )
{
    (void) b;
    atsw_proyecto_edita( a );
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

        if ( atsw_guarda( a, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Proyecto nuevo. Abre una serie en fue para empezar." );

        g_free( base ); g_free( p );
        atsw_refresca( a );
        }
    gtk_widget_destroy( d );
}

static void on_linaje( GtkButton *b, Atsw *a )
{
    (void) b;
    atsw_linaje( a );
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
    gchar *id = atsw_marcada( a->l_modelos, 0 );
    const char *m;

    if ( !a->hay || !a->serie[0] ) { out[0] = '\0'; return FALSE; }

    m = id ? id : atsw_modelo_por_defecto_en( a->p, a->serie,
                                              atsw_muestra_actual( a ) );
    if ( !m || !*m ) { g_free( id ); out[0] = '\0'; return FALSE; }

    if ( acepta_pre &&
         pr_ruta( a->p, a->serie, atsw_muestra_actual( a ), m, ".pre", out, n ) == 0 &&
         g_file_test( out, G_FILE_TEST_EXISTS ) )
        { g_free( id ); return TRUE; }

    if ( pr_ruta( a->p, a->serie, atsw_muestra_actual( a ), m, ".inp", out, n ) == 0 &&
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

/* A fue SE LE MANDA UN MODELO, NUNCA LOS DATOS.
 *
 * Especificar en fue y guardar reescribe el .inp. Si eso cayera sobre el nodo
 * de DATOS, ese fichero dejaria de ser los datos: se perderia el .inp de los
 * graficos y la cadena se quedaria sin raiz. Asi que si lo marcado son los
 * datos se deriva un modelo ANTES de mandarlo, con su linaje ya puesto.   */
/* A FUE SE VA A ESTIMAR, Y LOS DATOS NO SE ESTIMAN.
 *
 * m00 son los datos tal como entraron, y de ahi cuelga todo: es el .inp que
 * fug dibuja y la raiz del linaje. Si fue escribiera encima, el proyecto se
 * quedaria sin el uno y sin el otro. Asi que cuando lo que toca mandar son
 * los datos, se DERIVA un modelo de ellos -- el mismo gesto que el boton
 * «Modelo nuevo», para que no haya dos maneras de hacer lo mismo -- y va ese.
 *
 * Se mira el id EFECTIVO --el marcado, y si no el que la madre mandaria-- y
 * no solo el marcado: recien cargada la serie no hay nada marcado, y ese es
 * justo el caso en que el fichero que tocaba era el de los datos.      */
static void on_fue( GtkButton *b, Atsw *a )
{
    gchar      *marca = atsw_marcada( a->l_modelos, 0 );
    const char *id    = marca;

    (void) b;
    if ( a->hay && a->serie[0] && !id )
        id = atsw_modelo_por_defecto( a->p, a->serie );

    if ( a->hay && a->serie[0] && id && *id &&
         pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id ) )
        {
        char ruta[PR_RUTA], why[512];

        if ( !atsw_modelo_nuevo( a, a->serie, atsw_muestra_actual( a ), NULL, 0,
                                 ruta, sizeof ruta, why, sizeof why ) )
            { barra_pub( a, why ); g_free( marca ); return; }

        atsw_refresca( a );
        atsw_lanza( a, "fue_gui", ruta );
        barra_pub( a, why );
        g_free( marca );
        return;
        }
    g_free( marca );
    manda( a, "fue_gui", "estimarla", TRUE );
}
/* A fug NO se le manda un .pre: se identifica antes de que haya modelo. */
static void on_fug( GtkButton *b, Atsw *a )
{
    char f[PR_RUTA];

    (void) b;
    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }
    if ( !que_identificar( a, f, sizeof f ) )
        {
        barra_pub( a, a->serie[0]
            ? "Esa serie no tiene datos en esta muestra: fug identifica sobre "
              "los datos."
            : "Marca una serie para identificarla." );
        return;
        }

    /* SE DICE QUE VA EL m00, aunque hubiera otro marcado: si no, el analista
       cree que esta mirando el modelo que marco.                       */
    {
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    if ( id && !pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id ) )
        {
        gchar *t = g_strdup_printf( "fug identifica sobre los datos, así que "
                                    "va %s y no %s.",
                                    pr_datos_de( a->p, a->serie,
                                                 atsw_muestra_actual( a ) ),
                                    id );

        barra_pub( a, t );
        g_free( t );
        }
    g_free( id );
    }
    atsw_lanza( a, "gtk_fmg", f );
}
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

/* La misma pregunta, para los casos (casos_gui.c): una sola forma de pedir
   una razon en toda la madre.                                         */
gboolean atsw_pide_texto( Atsw *a, const char *titulo, const char *aviso,
                          const char *previo, char *out, size_t n )
{
    return pide_texto( a, titulo, aviso, previo, out, n );
}

/* El gesto EXPLICITO. Antes especificar el primer modelo no tenia boton: se
 * mandaba la serie a fue y habia que saber que ese envio derivaba uno. Un
 * gesto que solo existe como efecto lateral de otro no se encuentra. */
static void on_nuevo_modelo( GtkButton *b, Atsw *a )
{
    char ruta[PR_RUTA], why[512];

    (void) b;
    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }
    if ( !atsw_modelo_nuevo( a, a->serie, atsw_muestra_actual( a ), NULL, 0,
                             ruta, sizeof ruta, why, sizeof why ) )
        { barra_pub( a, why ); return; }

    atsw_refresca( a );
    atsw_lanza( a, "fue_gui", ruta );
    barra_pub( a, why );
}

/* BORRAR UN MODELO, Y SUS FICHEROS CON EL.
 *
 * Dejar el .out en el disco despues de quitar el nodo seria una mentira con
 * fecha: el siguiente modelo de la serie vuelve a llamarse igual --la version
 * es el ultimo numero LIBRE-- y se encontraria con el .out de otro puesto
 * como suyo. Asi que o se va entero o no se va.
 *
 * Se pregunta antes, con la lista de lo que se va delante: es el unico gesto
 * de la madre que destruye algo.                                       */
static void borra_ficheros( Atsw *a, const char *serie, const char *muestra,
                            const char *id )
{
    static const char *ext[] = { ".inp", ".pre", ".out", ".tex", ".pdf",
                                 ".eps", "_res.tex", "_dist.tex", NULL };
    int i;

    for ( i = 0; ext[i]; i++ )
        {
        char f[PR_RUTA];

        if ( pr_ruta( a->p, serie, muestra, id, ext[i], f, sizeof f ) == 0 )
            g_unlink( f );
        }
}

static void on_borrar( GtkMenuItem *m, Atsw *a )
{
    gchar     *id = atsw_marcada( a->l_modelos, M_ID );
    GtkWidget *d;
    PrError    e;
    char       why[512], f[PR_RUTA];
    int        resp;

    (void) m;
    if ( !a->hay || !a->serie[0] || !id ) { g_free( id ); return; }

    /* Se pregunta A LA LIBRERIA ANTES de preguntar al analista: si no se
       puede borrar, la pregunta sobraba y lo que hace falta es el porque. */
    {
    /* En el monton: Proyecto ocupa ~800 KB y la pila de Windows es de
       1 MB. */
    Proyecto *tmp = g_memdup2( a->p, sizeof *a->p );
    int       no  = pr_borra( tmp, a->serie, atsw_muestra_actual( a ), id, &e );

    g_free( tmp );
    if ( no != 0 )
        { pr_error_es( &e, why, sizeof why ); barra_pub( a, why );
          g_free( id ); return; }
    }

    pr_ruta( a->p, a->serie, atsw_muestra_actual( a ), id, ".inp",
             f, sizeof f );
    d = gtk_message_dialog_new( GTK_WINDOW(a->ventana), GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
            "¿Borro %s de «%s»?", id, a->serie );
    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d),
        "Se van el nodo del manifiesto Y sus ficheros (%s y los que lleven "
        "su nombre). No se puede deshacer.\n\nLos datos de la serie no se "
        "tocan.", f );
    resp = gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
    if ( resp != GTK_RESPONSE_OK ) { g_free( id ); return; }

    borra_ficheros( a, a->serie, atsw_muestra_actual( a ), id );
    pr_borra( a->p, a->serie, atsw_muestra_actual( a ), id, &e );
    if ( atsw_guarda( a, &e ) != 0 )
        barra_pub( a, "Los ficheros se fueron, pero no pude guardar el "
                      "proyecto." );
    else
        {
        gchar *t = g_strdup_printf( "%s: %s borrado, con sus ficheros.",
                                    a->serie, id );

        barra_pub( a, t );
        g_free( t );
        }
    g_free( id );
    atsw_refresca( a );
}

/* PREVER ESTE MODELO: lo mismo que el botón «Forecast» de fue_gui, pedido
 * desde la lista.
 *
 * Se manda el .inp --«fue -f» lee el .inp, no el .pre-- y «--prever», que es
 * lo que hace que fue_gui acabe donde acabaría el botón: en su pestaña de
 * previsión, con el informe delante.
 *
 * NO se exige que el modelo esté estimado: «fue -f» lo estima ahí mismo y
 * escribe la entrada de previsión con sus parámetros. Prohibirlo sería
 * decidir por el analista algo que el motor resuelve solo.             */
static void on_prever( GtkMenuItem *m, Atsw *a )
{
    char   f[PR_RUTA];
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( !a->hay || !a->serie[0] || !id ) { g_free( id ); return; }

    if ( pr_ruta( a->p, a->serie, atsw_muestra_actual( a ), id, ".inp",
                  f, sizeof f ) != 0 ||
         !g_file_test( f, G_FILE_TEST_EXISTS ) )
        {
        barra_pub( a, "Ese modelo no tiene .inp que prever." );
        g_free( id );
        return;
        }
    g_free( id );
    atsw_lanza_con( a, "fue_gui", "--prever", f );
}

static void on_diagnosis( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_diagnosis( &h, a->serie, atsw_muestra_actual( a ), id ); }
    g_free( id );
}

static void on_ganancia( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_ganancia( &h, a->serie, atsw_muestra_actual( a ), id ); }
    g_free( id );
}

static void on_anomalos( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_anomalos( &h, a->serie, atsw_muestra_actual( a ), id ); }
    g_free( id );
}

static void on_editar( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id ) atsw_editor( a, a->serie, atsw_muestra_actual( a ), id );
    g_free( id );
}

/* A QUE MUESTRA. Un submenu con las declaradas mas la completa, y la de la
 * hoja donde ya esta el modelo NO sale: llevarlo a donde esta no es nada. */
typedef struct { Atsw *a; char serie[PR_ID]; char padre[PR_ID];
                 char muestra[PR_ID]; } AMuestra;

static void on_a_muestra( GtkMenuItem *m, AMuestra *x )
{
    char why[512];

    (void) m;
    if ( atsw_en_muestra( x->a, x->serie, x->padre, x->muestra,
                          why, sizeof why ) )
        {
        atsw_hojas( x->a );
        atsw_refresca( x->a );
        /* Se salta a la hoja donde acaba de nacer: es donde se iba. */
        {
        int i;

        for ( i = 0; i < x->a->nhojas; i++ )
            if ( !strcmp( x->a->hoja_mu[i], x->muestra ) )
                { gtk_notebook_set_current_page(
                      GTK_NOTEBOOK(x->a->libro), i ); break; }
        }
        }
    barra_pub( x->a, why );
}

static void libera_am( gpointer d, GClosure *c ) { (void) c; g_free( d ); }

/* El submenu. Devuelve NULL si no hay ninguna otra muestra a la que ir --y
 * entonces la entrada no se pone, en vez de ponerla vacia.            */
static GtkWidget *menu_muestras( Atsw *a, const char *padre,
                                 const char *desde )
{
    GtkWidget *menu = NULL;
    int        i, n = 0;

    for ( i = 0; i < a->nhojas; i++ )
        {
        AMuestra  *x;
        GtkWidget *mi;

        if ( !strcmp( a->hoja_mu[i], desde ) ) continue;

        if ( menu == NULL ) menu = gtk_menu_new();
        mi = gtk_menu_item_new_with_label(
                 a->hoja_mu[i][0] ? a->hoja_mu[i] : "Completa" );

        x = g_new0( AMuestra, 1 );
        x->a = a;
        snprintf( x->serie, PR_ID, "%s", a->serie );
        snprintf( x->padre, PR_ID, "%s", padre ? padre : "" );
        snprintf( x->muestra, PR_ID, "%s", a->hoja_mu[i] );
        g_signal_connect_data( mi, "activate", G_CALLBACK(on_a_muestra), x,
                               libera_am, 0 );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        n++;
        }
    return n ? menu : NULL;
}

/* CAMBIAR DE HOJA. Lo unico que hace falta es que "el modelo marcado" pase a
 * leerse de la rejilla nueva; el resto de la ventana no se entera de que hay
 * varias, que es justo lo que hace barato tener muchas.               */
static void on_hoja( GtkNotebook *nb, GtkWidget *pag, guint n, Atsw *a )
{
    (void) nb; (void) pag;
    if ( (int) n >= a->nhojas ) return;

    /* LA SEÑAL TRAE EL NUMERO, y es el unico sitio donde se sabe: el
       cuaderno todavia contesta la hoja anterior.                    */
    a->hoja_actual = (int) n;
    a->l_modelos = a->hoja[n];
    if ( a->recolocando ) return;

    /* Y LA LISTA DE LA IZQUIERDA SIGUE A LA HOJA. Sus columnas --cuantos
       modelos, cual es el elegido-- son de ESTA ventana; contando el
       proyecto entero pondria "3 modelos" señalando modelos que no estan
       aqui, y la lista mentiria sobre lo que se ve.                  */
    atsw_refresca( a );
    barra_pub( a, "" );
}

/* DECLARAR UNA SUBMUESTRA: hasta donde, y por que.
 *
 * Las dos preguntas juntas y en el mismo sitio porque una ventana sin razon
 * dentro de un mes es un numero que nadie sabe de donde salio. La razon se
 * PIDE, no se exige -- igual que la de una iteracion.                  */
static void on_muestra_nueva( GtkButton *b, Atsw *a )
{
    GtkWidget *d, *caja, *rej, *e_id, *e_razon, *cd, *ch;
    AtFecha    f_desde, f_hasta;
    AtTramo    t;
    PrError    e;
    int        r;

    (void) b;
    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }

    if ( atsw_tramo( a->p, &t ) != 0 )
        {
        /* SIN DATOS NO HAY VENTANA QUE DECLARAR. Y se dice por que, que es
           una cosa concreta y no un "no se puede".                    */
        barra_pub( a, "Todavía no hay datos: una muestra es una ventana "
                      "sobre ellos. Carga alguna serie con «Datos…»." );
        return;
        }

    d = gtk_dialog_new_with_buttons( "Nueva muestra", GTK_WINDOW(a->ventana),
            GTK_DIALOG_MODAL, "Cancelar", GTK_RESPONSE_CANCEL,
            "Declararla", GTK_RESPONSE_OK, NULL );
    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_OK );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );
    gtk_box_set_spacing( GTK_BOX(caja), 8 );

    {
    GtkWidget *l = gtk_label_new( NULL );

    gtk_label_set_markup( GTK_LABEL(l),
        "<small>Una muestra es una <b>ventana declarada</b> sobre los datos.\n"
        "Los modelos que estimes en ella viven en su hoja: dos modelos\n"
        "estimados sobre ventanas distintas no se comparan.</small>" );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );
    }

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 8 );
    gtk_box_pack_start( GTK_BOX(caja), rej, TRUE, TRUE, 0 );

    e_id = atsw_fila( rej, 0, "Nombre ", "",
        "Corto, que es lo que va en la pestaña: «pre-covid», «hasta-2019»." );

    /* LOS EXTREMOS, RELLENOS CON EL TRAMO REAL. Declarar una muestra pasa a
     * ser MOVER un control desde algo que ya vale, no escribir desde cero.
     * Y si las frecuencias no coinciden no hay rango de periodos que valga,
     * asi que se dice y se cae a texto: un proyecto de frecuencias
     * mezcladas no se puede alimentar a drtran ni a drvarma.           */
    if ( t.mezcla )
        {
        cd = ch = NULL;
        atsw_fila( rej, 1, "Desde ", "", NULL );
        atsw_fila( rej, 2, "Hasta ", "", NULL );
        }
    else
        {
        GtkWidget *ld = gtk_label_new( "Desde " );
        GtkWidget *lh = gtk_label_new( "Hasta " );

        gtk_label_set_xalign( GTK_LABEL(ld), 1.0 );
        gtk_label_set_xalign( GTK_LABEL(lh), 1.0 );
        cd = atsw_fecha_nueva( &f_desde, t.freq, t.anio, t.per,
                               t.anio, t.fin_anio );
        ch = atsw_fecha_nueva( &f_hasta, t.freq, t.fin_anio, t.fin_per,
                               t.anio, t.fin_anio );
        gtk_grid_attach( GTK_GRID(rej), ld, 0, 1, 1, 1 );
        gtk_grid_attach( GTK_GRID(rej), cd, 1, 1, 1, 1 );
        gtk_grid_attach( GTK_GRID(rej), lh, 0, 2, 1, 1 );
        gtk_grid_attach( GTK_GRID(rej), ch, 1, 2, 1, 1 );
        }

    e_razon = atsw_fila( rej, 3, "Por qué ", "",
        "Dentro de un mes, esta ventana será un número que nadie sabe de "
        "dónde salió. Se pide, no se exige." );

    /* DE QUE TRAMO SE ESTA HABLANDO, dicho. El rango de los controles no
     * puede ser magia: sale de los datos y se ve de donde.             */
    {
    GtkWidget *l = gtk_label_new( NULL );
    char       b1[32], b2[32];
    gchar     *txt;

    dt_fecha( t.freq, t.anio, t.per, 0, b1, sizeof b1 );
    dt_fecha( t.freq, t.fin_anio, t.fin_per, 0, b2, sizeof b2 );
    txt = t.mezcla
        ? g_strdup( "<small>⚠ Las series de este proyecto <b>no tienen la "
                    "misma frecuencia</b>.\nEscribe las fechas a mano; y "
                    "revisa el proyecto, porque\nasí no se puede alimentar "
                    "drtran ni drvarma.</small>" )
        : g_markup_printf_escaped(
              "<small>Los datos van de %s a %s (%s, %d serie%s).</small>",
              b1, b2,
              t.freq == 12 ? "mensual" : t.freq == 4 ? "trimestral"
                           : t.freq == 1 ? "anual" : "?",
              t.nseries, t.nseries == 1 ? "" : "s" );

    gtk_label_set_markup( GTK_LABEL(l), txt );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );
    g_free( txt );
    }

    gtk_widget_show_all( d );
    r = gtk_dialog_run( GTK_DIALOG(d) );

    if ( r == GTK_RESPONSE_OK )
        {
        const char *id = gtk_entry_get_text( GTK_ENTRY(e_id) );
        char        id2[PR_ID], b1[32] = "", b2[32] = "";

        a_id( id, id2, sizeof id2 );
        if ( cd ) atsw_fecha_texto( &f_desde, b1, sizeof b1 );
        if ( ch ) atsw_fecha_texto( &f_hasta, b2, sizeof b2 );

        if ( !id2[0] )
            barra_pub( a, "La muestra necesita un nombre: es lo que va en la "
                          "pestaña." );
        else if ( pr_muestra_add( a->p, id2, b1, b2,
                      gtk_entry_get_text( GTK_ENTRY(e_razon) ), &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            {
            gchar *t2;

            /* LA HOJA NACE USABLE: con el m00 de cada serie dentro se
               puede mirar la ACF de la serie recortada --que es lo primero
               que se hace al truncar-- y empezar un modelo.          */
            {
            char w[256];

            atsw_puebla_muestra( a, id2, w, sizeof w );
            }
            atsw_guarda( a, &e );
            atsw_hojas( a );
            gtk_notebook_set_current_page( GTK_NOTEBOOK(a->libro),
                                           a->nhojas - 1 );
            atsw_refresca( a );
            t2 = g_strdup_printf( "Muestra «%s» declarada%s%s. Los modelos "
                                  "que estimes aquí viven en esta hoja.",
                                  id2, b2[0] ? ", hasta " : "", b2 );
            barra_pub( a, t2 );
            g_free( t2 );
            }
        }
    gtk_widget_destroy( d );
}

/* EL MENU DEL MODELO. Lo que se puede hacer con ESTE, no con la serie. */
static void menu_modelo( Atsw *a, GdkEventButton *ev )
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *mi;
    gchar     *id = atsw_marcada( a->l_modelos, M_ID );
    gboolean   datos = id && pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id );
    gchar     *txt;

    txt = datos ? g_strdup( "Especificar un modelo con fue" )
                : g_strdup_printf( "Abrir %s en fue", id ? id : "" );
    mi = gtk_menu_item_new_with_label( txt );
    g_free( txt );
    gtk_widget_set_tooltip_text( mi, datos
        ? "Los datos no se estiman: nace un modelo colgado de ellos y es ése "
          "el que se abre."
        : "Se lleva el .pre si lo hay —es un óptimo reejecutable— y si no el "
          ".inp. Es lo mismo que el doble clic." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_fue), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    if ( !datos )
        {
        mi = gtk_menu_item_new_with_label( "Editar el .inp…" );
        gtk_widget_set_tooltip_text( mi,
            "El fichero, a mano, con la salida del motor al lado y fue a un "
            "botón. La otra forma de iterar: el formulario de fue_gui sólo "
            "puede expresar lo que tiene widgets; el .inp, todo lo que el "
            "motor lee." );
        g_signal_connect( mi, "activate", G_CALLBACK(on_editar), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

        /* LOS DOS QUE PIDEN UN .out AL DIA. La regla es la de lib/analisis,
           la misma que usa fue_gui: dos copias de una regla son dos
           reglas. Y apagado no es mudo -- el globo dice por que.     */
        {
        char     porque[512];
        AnEstado est = an_estado( a->p, a->serie, atsw_muestra_actual( a ), id,
                                  porque, sizeof porque );
        gboolean listo = ( est == AN_LISTO );

        mi = gtk_menu_item_new_with_label( "Diagnosis…" );
        gtk_widget_set_sensitive( mi, listo );
        gtk_widget_set_tooltip_text( mi, listo
            ? "Los cinco bloques: estimación, media, autocorrelación, "
              "normalidad y parámetros, cada uno con su veredicto.\n\nDice lo "
              "que los números dicen; qué hacer con ello es tuyo."
            : porque );
        g_signal_connect( mi, "activate", G_CALLBACK(on_diagnosis), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

        mi = gtk_menu_item_new_with_label( "Ganancia…" );
        gtk_widget_set_sensitive( mi, listo );
        gtk_widget_set_tooltip_text( mi, listo
            ? "¿El suceso dejó algo PARA SIEMPRE o revirtió?\n\nLa ganancia "
              "de cada intervención con su contraste. No es diagnosis: es una "
              "hipótesis sobre la naturaleza del incidente."
            : porque );
        g_signal_connect( mi, "activate", G_CALLBACK(on_ganancia), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

        mi = gtk_menu_item_new_with_label( "Anómalos…" );
        gtk_widget_set_sensitive( mi, listo );
        gtk_widget_set_tooltip_text( mi, listo
            ? "Los episodios de residuos extremos, y los correlogramas CON y "
              "SIN ellos.\n\nContesta lo que decide: ¿la estructura que veo "
              "es del proceso o del anómalo? Y al revés —si no cambia nada, "
              "intervenirlo no compra nada."
            : porque );
        g_signal_connect( mi, "activate", G_CALLBACK(on_anomalos), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        }

        mi = gtk_menu_item_new_with_label( "Prever con fuf…" );
        gtk_widget_set_tooltip_text( mi,
            "Lo mismo que el botón «Forecast» de fue: corre «fue -f» para "
            "escribir la entrada de previsión, corre fuf sobre ella y deja "
            "las dos cosas en la pestaña de previsión.\n\nLos ficheros se "
            "escriben AL LADO del modelo." );
        g_signal_connect( mi, "activate", G_CALLBACK(on_prever), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

        mi = gtk_menu_item_new_with_label( "Iterar: seguir desde su óptimo" );
        gtk_widget_set_tooltip_text( mi,
            "Copia su .pre a un .inp nuevo. Un .pre que se toca vuelve a ser "
            "un .inp: sus valores vuelven a ser semillas." );
        g_signal_connect( mi, "activate", G_CALLBACK(on_iterar), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        }

    mi = gtk_menu_item_new_with_label( "Modelo nuevo, desde los datos" );
    gtk_widget_set_tooltip_text( mi,
        "Empieza de cero: cuelga de los datos, no de éste." );
    g_signal_connect( mi, "activate", G_CALLBACK(on_nuevo_modelo), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    /* LLEVARLO A OTRA VENTANA. Deriva y genera; lo de aqui no se toca. */
    if ( !datos )
        {
        GtkWidget *sub = menu_muestras( a, id, atsw_muestra_actual( a ) );

        if ( sub )
            {
            mi = gtk_menu_item_new_with_label( "En otra muestra…" );
            gtk_widget_set_tooltip_text( mi,
                "Deriva un modelo en otra ventana, colgado de éste. El .inp "
                "se GENERA del datos.csv con la ventana nueva: no es una "
                "copia con otro nobs.\n\nÉste no se toca: su .out describe "
                "una estimación sobre estas observaciones." );
            gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );
            gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
            }
        }

    if ( !datos )
        {
        gtk_menu_shell_append( GTK_MENU_SHELL(menu),
                               gtk_separator_menu_item_new() );
        mi = gtk_menu_item_new_with_label( "Borrar…" );
        gtk_widget_set_tooltip_text( mi,
            "El nodo y sus ficheros. Se pregunta antes. Un modelo del que "
            "cuelgue otro no se borra: rompería el linaje." );
        g_signal_connect( mi, "activate", G_CALLBACK(on_borrar), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        }

    g_free( id );
    gtk_widget_show_all( menu );
    gtk_menu_popup_at_pointer( GTK_MENU(menu), (GdkEvent *) ev );
}

gboolean atsw_on_click( GtkWidget *tv, GdkEventButton *ev, Atsw *a )
{
    GtkTreePath *ruta = NULL;

    if ( ev->type != GDK_BUTTON_PRESS || ev->button != 3 ) return FALSE;
    if ( !a->hay || !a->serie[0] ) return FALSE;
    if ( !gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(tv), (gint) ev->x,
                                         (gint) ev->y, &ruta, NULL, NULL, NULL ) )
        return FALSE;

    /* Se marca PRIMERO: el menu actua sobre la fila donde se pulso, no sobre
       la que estuviera marcada de antes.                                */
    gtk_tree_view_set_cursor( GTK_TREE_VIEW(tv), ruta, NULL, FALSE );
    gtk_tree_path_free( ruta );

    /* Se actua sobre la rejilla DONDE SE PULSO, no sobre la que la ventana
       creyera que estaba delante. Con varias hojas no es lo mismo.    */
    a->l_modelos = tv;
    menu_modelo( a, ev );
    return TRUE;
}

/* DOBLE CLIC EN UN MODELO: a fue, que es lo que se va a hacer con el.
 *
 * La fila ya esta marcada cuando llega esto --activar marca-- asi que se
 * delega en on_fue y no hay dos caminos que puedan decidir distinto. Si la
 * fila son los DATOS, on_fue deriva: el doble clic no es una excepcion a la
 * regla, es el mismo gesto con menos vueltas.                         */
void atsw_on_activado( GtkTreeView *tv, GtkTreePath *ruta,
                                GtkTreeViewColumn *col, Atsw *a )
{
    (void) tv; (void) ruta; (void) col;
    on_fue( NULL, a );
}

static void on_iterar( GtkButton *b, Atsw *a )
{
    gchar *padre = atsw_marcada( a->l_modelos, 0 );
    char   why[512];

    (void) b;
    if ( !a->serie[0] ) { barra_pub( a, "Marca una serie." ); return; }
    atsw_itera( a, a->serie, atsw_muestra_actual( a ), padre ? padre : "", why, sizeof why );
    barra_pub( a, why );
    g_free( padre );
    atsw_refresca( a );
}

static void on_elegir( GtkButton *b, Atsw *a )
{
    gchar  *id = atsw_marcada( a->l_modelos, 0 );
    char    razon[PR_RAZON] = "";
    PrError e;

    (void) b;
    if ( !id ) { barra_pub( a, "Marca el modelo que eliges." ); return; }

    if ( pide_texto( a, "El modelo elegido",
            "Por qué éste y no otro. Se puede dejar en blanco: sin razón "
            "se verá como sin razón, que es mejor que una inventada.",
            /* La razón que ya hubiera, no el id del elegido: venía relleno
               con «m10», y aceptar sin mirar guardaba una razón falsa.   */
            pr_razon_elegido( a->p, a->serie, atsw_muestra_actual( a ) ),
            razon, sizeof razon ) )
        {
        if ( pr_elige( a->p, a->serie, atsw_muestra_actual( a ), id, razon, &e ) != 0 ||
             atsw_guarda( a, &e ) != 0 )
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
    gchar  *id = atsw_marcada( a->l_modelos, 0 );
    char    razon[PR_RAZON] = "";
    PrError e;
    int     i;

    (void) b;
    if ( !id ) { barra_pub( a, "Marca la iteración." ); return; }
    i = pr_modelo_idx( a->p, a->serie, atsw_muestra_actual( a ), id );

    /* SE PIDE, NO SE EXIGE, y se puede poner DESPUES -- mirando el .out, que
     * es cuando de verdad se sabe por que.                              */
    if ( pide_texto( a, "El porqué de esta iteración",
            "Qué te hizo pasar del modelo anterior a éste. Se puede dejar en "
            "blanco y ponerlo más tarde.",
            i >= 0 ? a->p->m[i].razon : "", razon, sizeof razon ) )
        {
        if ( pr_razon( a->p, a->serie, atsw_muestra_actual( a ), id, razon, &e ) != 0 ||
             atsw_guarda( a, &e ) != 0 )
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

void atsw_columna( GtkWidget *tv, const char *t, int c )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *k = gtk_tree_view_column_new_with_attributes(
                               t, r, "text", c, NULL );

    gtk_tree_view_column_set_resizable( k, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), k );
}

GtkWidget *atsw_en_scroll( GtkWidget *w )
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

/* AL VOLVER A LA VENTANA, RELEER.
 *
 * Quien estima es OTRO PROCESO --fue_gui, que la madre lanza-- y escribe el
 * .out por su cuenta. La madre no se entera de nada: la rejilla se quedaba
 * con la d.t. y el contraste de antes hasta que algo la tocara.
 *
 * Volver el foco a esta ventana ES el gesto de "vengo de estimar", asi que
 * es el momento de releer. La huella (tamaño + mtime) hace que releer sea
 * barato: si el .out no se ha movido, no se vuelve a analizar.        */
static gboolean on_foco( GtkWidget *w, GdkEventFocus *ev, Atsw *a )
{
    char why[512];

    (void) w; (void) ev;
    if ( !a->hay ) return FALSE;

    /* Y EL MANIFIESTO TAMBIEN, si ha cambiado por fuera. Antes solo se
       releian los .out, asi que un modelo dado de alta desde otro sitio
       --otra madre, un agente-- no aparecia hasta reabrir el proyecto. */
    if ( atsw_relee( a, why, sizeof why ) )
        barra_pub( a, "El proyecto ha cambiado fuera de aquí: releído." );
    else if ( why[0] )
        barra_pub( a, why );

    atsw_refresca( a );
    return FALSE;
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
    g_signal_connect( w, "focus-in-event", G_CALLBACK(on_foco), a );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
    gtk_container_add( GTK_CONTAINER(w), raiz );

    /* --- la barra --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    boton( barra, "Abrir…", "Un proyecto.yaml. Si está roto se dice y NO se "
                            "abre: nunca se pisa.", G_CALLBACK(on_abrir), a );
    boton( barra, "Nuevo…", NULL, G_CALLBACK(on_nuevo), a );
    boton( barra, "Proyecto…",
        "Identificador, título y analista. Y lo que no se edita pero hay que "
        "saber: cuándo se creó, dónde está el manifiesto y dónde la raíz de "
        "los datos.", G_CALLBACK(on_proyecto), a );
    boton( barra, "Linaje…",
        "La cadena entera y lo que cada nodo DEBE: sin estimar, sin razón, "
        "cadena sin elegido.\n\nLa rejilla contesta «¿cómo va este modelo?»; "
        "esto contesta «¿cómo va el recorrido?», que es una forma de árbol y "
        "no de tabla.", G_CALLBACK(on_linaje), a );
    boton( barra, "Datos…",
        "De un .xlsx, un .csv o un .txt a n SERIES del proyecto, cada una con "
        "su primer .inp.\n\nEs el eslabón que faltaba: el camino datos → "
        ".inp(-1) no lo recorría nadie.", G_CALLBACK(on_datos), a );

    a->l_proy = gtk_label_new( "(sin proyecto)" );
    /* QUE SE GUARDA SOLO, DICHO. No hay botón de «Guardar» porque no hace
       falta --cada cambio escribe el manifiesto-- pero no decirlo deja al
       analista buscando uno que no existe.                            */
    gtk_widget_set_tooltip_text( a->l_proy,
        "El proyecto se guarda SOLO en cada cambio: dar de alta una serie, "
        "derivar, iterar, elegir, poner una razón. No hay que guardarlo a "
        "mano y no se puede perder.\n\n«Proyecto…» para su información." );
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
                             G_TYPE_STRING, G_TYPE_STRING );
    a->l_series = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    atsw_columna( a->l_series, "Serie",   S_ID );
    atsw_columna( a->l_series, "Elegido", S_ELEGIDO );
    atsw_columna( a->l_series, "Modelos", S_NMOD );
    gtk_tree_view_set_tooltip_column( GTK_TREE_VIEW(a->l_series), S_GLOBO );
    gtk_widget_set_tooltip_text( a->l_series,
        "El modelo ELEGIDO de cada serie. Hoy esa decisión vive en un "
        "diccionario a pelo repetido en tres guiones de cases/." );
    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_series) ),
                      "changed", G_CALLBACK(on_serie), a );
    /* El boton derecho despliega lo que se puede hacer con la serie. */
    g_signal_connect( a->l_series, "button-press-event",
                      G_CALLBACK(on_serie_click), a );
    gtk_box_pack_start( GTK_BOX(izq), atsw_en_scroll( a->l_series ), TRUE, TRUE, 0 );
    /* Y DEBAJO, LOS CASOS: lo que se cruza de varias series a la vez. */
    atsw_casos_panel( a, izq );
    gtk_paned_pack1( GTK_PANED(pan), izq, FALSE, FALSE );

    der = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    {
    GtkWidget *b2 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );

    a->b_nuevo = boton( b2, "Modelo nuevo",
        "Empieza un modelo DESDE LOS DATOS y lo abre en fue.\n\nCuelga "
        "siempre de m00, no del modelo marcado: uno que empieza de cero no "
        "viene del anterior, viene de la serie. Los datos no se tocan.\n\n"
        "Es el hermano de Iterar, y la diferencia es de dónde copia: Iterar "
        "sigue desde un óptimo, esto empieza otra vez.",
        G_CALLBACK(on_nuevo_modelo), a );
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
        "su PACF.\n\nVa SIEMPRE el nodo de datos de esta muestra, aunque "
        "tengas otro modelo marcado: se identifica ANTES de que haya "
        "modelo.", G_CALLBACK(on_fug), a );
    a->b_fue = boton( b2, "→ fue",
        "Manda esta serie a fue para ESTIMAR su modelo.\n\nSe manda el .pre "
        "si lo hay —es un óptimo reejecutable— y si no el .inp, que es la "
        "especificación.", G_CALLBACK(on_fue), a );

    gtk_box_pack_start( GTK_BOX(der), b2, FALSE, FALSE, 0 );
    }

    /* EL CUADERNO DE MUESTRAS, CON LAS HOJAS ABAJO.
     *
     * Como las hojas de un calculo, y por una razon de metodo: dos modelos
     * con muestras distintas NO SE COMPARAN --la d.t. residual de uno hasta
     * 2019 y la de otro hasta 2026 no miden lo mismo-- y una columna que lo
     * AVISARA seguiria dejando ponerlos uno encima de otro. La hoja lo
     * IMPIDE: no se pueden ver los dos a la vez.
     *
     * No es el "modo" que el diseño rechaza. Un modo es malo cuando es
     * invisible y esta lejos de la accion; la pestaña esta pegada a lo que
     * gobierna y es donde acabas de pulsar. Es una seleccion.
     *
     * Y lo de FUERA del cuaderno --los dos veredictos-- cuenta el proyecto
     * ENTERO, no la hoja: si contara lo visible, las hojas mentirian por
     * omision, y "lo que hay que mirar" es justo lo que no puede ir
     * filtrado.                                                         */
    a->libro = gtk_notebook_new();
    gtk_notebook_set_tab_pos( GTK_NOTEBOOK(a->libro), GTK_POS_BOTTOM );
    gtk_notebook_set_scrollable( GTK_NOTEBOOK(a->libro), TRUE );
    g_signal_connect( a->libro, "switch-page", G_CALLBACK(on_hoja), a );

    /* El "+" va al lado de las pestañas, que es donde esta en una hoja de
     * calculo. Como widget de accion y no como pagina falsa: una pagina que
     * al abrirse salta a otra es un truco que se nota.                  */
    {
    GtkWidget *mas = gtk_button_new_with_label( "+" );

    gtk_button_set_relief( GTK_BUTTON(mas), GTK_RELIEF_NONE );
    gtk_widget_set_tooltip_text( mas,
        "Declarar una submuestra: hasta dónde y por qué.\n\nLos modelos de "
        "cada muestra viven en su hoja, porque dos modelos estimados sobre "
        "ventanas distintas no se comparan." );
    g_signal_connect( mas, "clicked", G_CALLBACK(on_muestra_nueva), a );
    gtk_widget_show( mas );
    gtk_notebook_set_action_widget( GTK_NOTEBOOK(a->libro), mas, GTK_PACK_END );
    }

    gtk_box_pack_start( GTK_BOX(der), a->libro, TRUE, TRUE, 0 );
    atsw_hojas( a );

    /* LA DERECHA ES UNA PILA: los modelos de la serie marcada o el caso
       marcado. Lo que se marca a la izquierda decide cual se ve.       */
    a->pila = gtk_stack_new();
    gtk_stack_add_named( GTK_STACK(a->pila), der, "modelos" );
    gtk_stack_add_named( GTK_STACK(a->pila), atsw_caso_vista( a ), "caso" );
    gtk_paned_pack2( GTK_PANED(pan), a->pila, TRUE, FALSE );
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
    /* LAS DOS DE LOS CASOS, que solo salen si hay algo que decir. */
    a->ver_desfase = gtk_label_new( "" );
    a->ver_nota    = gtk_label_new( "" );
    gtk_widget_set_halign( a->ver_desfase, GTK_ALIGN_START );
    gtk_widget_set_halign( a->ver_nota,    GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(a->ver_desfase), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(a->ver_nota),    PANGO_ELLIPSIZE_END );
    gtk_widget_set_no_show_all( a->ver_desfase, TRUE );
    gtk_widget_set_no_show_all( a->ver_nota,    TRUE );
    gtk_box_pack_start( GTK_BOX(vb), a->ver_desfase, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), a->ver_nota,    FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), atsw_legado_caja( a ), FALSE, FALSE, 0 );
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
