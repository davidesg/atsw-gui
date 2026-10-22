/*
 * main.c -- atsw_gui, la interfaz madre.
 *
 * El nombre: la FAMILIA se llama atsw y el programa atsw_gui, por el mismo
 * convenio que gui/fue -> fue_gui y gui/drtran -> drtran_gui. Un nombre señala
 * una cosa sola, y esa regla ya costo un renombrado.
 */

#include <locale.h>
#include <string.h>

#include <glib/gstdio.h>

#include "datos.h"

#include "atsw.h"
#include "anfitrion.h"
#include "inpfile.h"
#include "preview.h"
#include "rutas.h"
#include "sitio.h"
#include "iconos.h"

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
    /* UN MENSAJE DE VERDAD GANA AL GLOBO que se estuviera enseñando: al
       salir del menu ya no se repone lo de antes (on_entrada_fuera).   */
    g_object_set_data( G_OBJECT(a->estado), "previo", NULL );
    gtk_label_set_text( GTK_LABEL(a->estado), s );
    gtk_widget_set_tooltip_text( a->estado, s );
}

/* ------------------------------------------------------------------------ */
/* LA ENTRADA DE MENU                                                        */
/*                                                                           */
/* Una para todos los menus, la barra y los del boton derecho. Al recorrerla */
/* --con el raton o con las flechas-- su globo pasa a la barra de estado, y  */
/* al salir vuelve lo que hubiera. Es la linea 25 de TASTE con el contenido  */
/* que alli faltaba: aquella repetia el rotulo (ESTUDIO-madre-organizacion   */
/* §3.1 bis). Con el raton sale ademas el globo de siempre.                  */
/* ------------------------------------------------------------------------ */

/* El globo en una linea: la barra de estado no tiene mas. */
static gchar *en_una_linea( const char *t )
{
    GString    *g = g_string_new( NULL );
    const char *c;

    for ( c = t ? t : ""; *c; c++ )
        {
        if ( *c != '\n' ) { g_string_append_c( g, *c ); continue; }
        if ( g->len && g->str[g->len-1] != ' ' ) g_string_append_c( g, ' ' );
        }
    return g_string_free( g, FALSE );
}

static void enseña_en_barra( Atsw *a, GtkWidget *w )
{
    gchar *t, *u;

    if ( a->estado == NULL ) return;
    /* Lo de antes se guarda UNA vez: de una entrada a la de al lado se
       sale y se entra, y no hay que guardar el globo de la primera.     */
    if ( g_object_get_data( G_OBJECT(a->estado), "previo" ) == NULL )
        g_object_set_data_full( G_OBJECT(a->estado), "previo",
            g_strdup( gtk_label_get_text( GTK_LABEL(a->estado) ) ), g_free );
    t = gtk_widget_get_tooltip_text( w );
    u = en_una_linea( t );
    gtk_label_set_text( GTK_LABEL(a->estado), u );
    g_free( u ); g_free( t );
}

static void repone_barra( Atsw *a )
{
    const char *p;
    gchar      *c;

    if ( a->estado == NULL ) return;
    p = g_object_get_data( G_OBJECT(a->estado), "previo" );
    if ( p == NULL ) return;
    c = g_strdup( p );
    g_object_set_data( G_OBJECT(a->estado), "previo", NULL );
    gtk_label_set_text( GTK_LABEL(a->estado), c );
    g_free( c );
}

static void on_entrada_dentro( GtkMenuItem *mi, Atsw *a )
     { enseña_en_barra( a, GTK_WIDGET(mi) ); }
static void on_entrada_fuera( GtkMenuItem *mi, Atsw *a )
     { (void) mi; repone_barra( a ); }

GtkWidget *atsw_entrada( Atsw *a, GtkWidget *menu, const char *txt,
                         const char *tip, GCallback cb, gpointer d )
{
    GtkWidget *mi = gtk_menu_item_new_with_label( txt );

    if ( tip ) gtk_widget_set_tooltip_text( mi, tip );
    if ( cb )
        {
        g_signal_connect( mi, "activate", cb, d );
        g_object_set_data( G_OBJECT(mi), "accion", (gpointer) cb );
        }
    g_signal_connect( mi, "select",   G_CALLBACK(on_entrada_dentro), a );
    g_signal_connect( mi, "deselect", G_CALLBACK(on_entrada_fuera), a );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
    return mi;
}

void atsw_entrada_tecla( GtkWidget *mi, const char *acel )
{
    GtkWidget      *l = gtk_bin_get_child( GTK_BIN(mi) );
    guint           k = 0;
    GdkModifierType m = 0;

    gtk_accelerator_parse( acel, &k, &m );
    if ( k && GTK_IS_ACCEL_LABEL(l) )
        gtk_accel_label_set_accel( GTK_ACCEL_LABEL(l), k, m );
}

void atsw_apaga( GtkWidget *mi, const char *porque )
{
    gtk_widget_set_sensitive( mi, FALSE );
    gtk_widget_set_tooltip_text( mi, porque );
}

static void separa( GtkWidget *menu )
{
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
}

/* LA TECLA, ESCRITA POR GTK Y NO A MANO. Los globos decian «Ctrl+O» con la
   cadena fija, y en macOS la tecla es Cmd desde que los atajos son
   <Primary>: el globo mentia justo donde se busca la tecla.           */
gchar *atsw_tecla( const char *acel )
{
    guint           k = 0;
    GdkModifierType m = 0;

    gtk_accelerator_parse( acel, &k, &m );
    return k ? gtk_accelerator_get_label( k, m ) : g_strdup( acel );
}

gchar *atsw_globo_tecla( const char *globo, const char *acel )
{
    gchar *t = atsw_tecla( acel );
    gchar *g = g_strdup_printf( "%s\n\n%s", globo, t );

    g_free( t );
    return g;
}

/* Ver atsw.h. */
const char *atsw_clase_de( const char *motor )
{
    if ( motor == NULL || !*motor || !strcmp( motor, "drtran" ) ) return "MT";
    if ( !strcmp( motor, "drvarma" ) ) return "VARMA";
    if ( !strcmp( motor, "drvec" ) )   return "VECM";
    return motor;
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
static void menu_serie( Atsw *a, GdkEventButton *ev );
static void on_muestra_nueva( GtkButton *b, Atsw *a );
static void on_elegir( GtkButton *b, Atsw *a );
static void on_razon( GtkButton *b, Atsw *a );

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

/* IDENTIFICAR DESDE LOS DATOS (E1, docs/ESTUDIO-identificador.md): la
 * ventana del identificador con la transformacion editable y los
 * contrastes a la vista. Parte de lo que diga el nodo de datos.        */
static void on_identifica_datos( GtkMenuItem *m, Atsw *a )
{
    char        f[PR_RUTA], msg[256];
    InpFile     inp;
    AnSerie     s;
    AnHost      h;
    const char *mu = atsw_muestra_actual( a );
    gchar      *que;

    (void) m;
    if ( !que_identificar( a, f, sizeof f ) )
        { barra_pub( a, "Esa serie no tiene datos en esta muestra: no hay "
                        "nada que identificar todavía." );
          return; }
    if ( inp_read( f, &inp, msg, sizeof msg ) != 0 ) { barra_pub( a, msg ); return; }
    memset( &s, 0, sizeof s );
    s.x = inp.data; s.n = inp.nobs; s.freq = inp.freq;
    s.per = inp.begtime; s.anio = inp.begyear;
    s.lam = inp.boxlam; s.d = inp.nrdiff; s.D = inp.nadiff;
    que = g_strdup_printf( "Identificación de %s desde los datos", a->serie );
    s.que = que;
    h = atsw_host( a );
    an_identifica_datos( &h, a->serie, mu, pr_datos_de( a->p, a->serie, mu ), &s );
    g_free( que );
    inp_free( &inp );
}
static void on_serie_acf( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 0, 1.0 ); }
static void on_hist     ( GtkMenuItem *m, Atsw *a ) { (void)m; vistazo( a, 2, 1.0 ); }

static void on_editar_serie( GtkMenuItem *m, Atsw *a )
{
    (void) m;
    if ( a->hay && a->serie[0] ) atsw_serie_edita( a, a->serie );
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

/* La tecla de menú sobre la lista de series: lo mismo que el botón derecho,
   sobre la serie marcada.                                             */
static gboolean on_serie_menu_tecla( GtkWidget *tv, Atsw *a )
{
    (void) tv;
    if ( !a->hay || !a->serie[0] ) return FALSE;
    menu_serie( a, NULL );
    return TRUE;
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
    /* LA CONFIRMACION DE SOBRESCRIBIR LA HACE LA MADRE, NO GTK. La de GTK
       («ya existe, ¿lo reemplazo?») no dice QUE se pierde, y se aceptaba
       sobre el CSV de los datos: el manifiesto vacio quedaba encima de las
       series y «Datos…» ya no tenia de donde leer. Paso de verdad con
       ipc_wti.csv. Abajo: un proyecto es un .yaml, y un fichero que no es
       un proyecto no se pisa nunca.                                   */
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), FALSE );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), "proyecto.yaml" );

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar  *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
        gchar  *base = g_path_get_basename( p );
        PrError e;
        char   *dot;

        if ( !g_str_has_suffix( p, ".yaml" ) && !g_str_has_suffix( p, ".yml" ) )
            {
            gchar *m = g_strdup_printf( "No creo el proyecto: «%s» no es un "
                "fichero .yaml, y un proyecto lo es. ¿Eran los datos? Ponle "
                "otro nombre, como proyecto.yaml.", base );
            barra_pub( a, m );
            g_free( m ); g_free( base ); g_free( p );
            gtk_widget_destroy( d );
            return;
            }
        if ( g_file_test( p, G_FILE_TEST_EXISTS ) )
            {
            Proyecto *viejo = g_new0( Proyecto, 1 );
            PrError   ev;
            int       es = pr_leer( p, viejo, &ev ) == 0;
            gint      r = GTK_RESPONSE_CANCEL;

            if ( es )
                {
                GtkWidget *q = gtk_message_dialog_new( GTK_WINDOW(a->ventana),
                    GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
                    "¿Reemplazo el proyecto «%s»?", viejo->id );
                gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(q),
                    "%s ya es un proyecto con %d serie%s y %d modelo%s. El "
                    "manifiesto nuevo lo sustituye; sus ficheros se quedan "
                    "donde están, pero dejan de estar registrados.", p,
                    viejo->ns, viejo->ns == 1 ? "" : "s",
                    viejo->nm, viejo->nm == 1 ? "" : "s" );
                r = gtk_dialog_run( GTK_DIALOG(q) );
                gtk_widget_destroy( q );
                }
            else
                {
                gchar *m = g_strdup_printf( "No creo el proyecto: «%s» ya existe "
                    "y NO es un proyecto (¿los datos?). No lo piso: elige otro "
                    "nombre.", base );
                barra_pub( a, m );
                g_free( m );
                }
            g_free( viejo );
            if ( r != GTK_RESPONSE_OK )
                { g_free( base ); g_free( p ); gtk_widget_destroy( d ); return; }
            }

        if ( !a->p ) a->p = g_new0( Proyecto, 1 );
        dot = strrchr( base, '.' ); if ( dot ) *dot = '\0';
        pr_nuevo( a->p, base, "", "." );
        snprintf( a->p->path, sizeof a->p->path, "%s", p );
        a->hay = TRUE; a->nr = 0; a->serie[0] = '\0';
        atsw_hojas( a );      /* las del proyecto anterior ya no son */

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
                   gboolean acepta_pre, const char *que )
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
    atsw_lanza_dice( a, programa, f, que );
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
    {
    /* EL GESTO, CON SU CLASE Y SU PROGRAMA: «Estimar m01 (US) en fue». */
    gchar *que = ( id && *id ) ? g_strdup_printf( "Estimar %s (US) en fue", id )
                               : g_strdup( "Estimar (US) en fue" );

    manda( a, "fue_gui", "estimarla", TRUE, que );
    g_free( que );
    }
    g_free( marca );
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
/* NO HAY drtran SIN CASO (§6, fase 3): drtran siempre necesita los modelos
 * de la salida y de las entradas, con su .pre, y eso es lo que fija un caso.
 * Sin el, el analista los buscaria a mano y lo estimado no quedaria escrito.
 * A MT se entra por «Nuevo caso…» o por «Estimar» sobre un caso.      */

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

/* EL GRAFICO QUE FUE DEJA AL ESTIMAR: residuos, ACF/PACF y el modelo
 * escrito debajo. Se llama como el modelo con una «A» delante, junto al
 * .out (Aipc_m01.eps). El editor lo compone igual (editor.c, eps_de).  */
static int atsw_grafico_de( Atsw *a, const char *serie, const char *muestra,
                            const char *id, char *out, size_t n )
{
    char base[PR_RUTA];

    out[0] = '\0';
    if ( pr_ruta( a->p, serie, muestra, id, ".eps", base, sizeof base ) != 0 )
        return 1;
    return ruta_componer( base, "A", NULL, out, n );
}

/* EL RESUMEN DEL MODELO ESTIMADO, para «Ver modelo…»: el
 * PDF que fue escribe al estimar (ipc_m01.pdf, report.c) con el gráfico de
 * residuos y su ACF/PACF, el modelo estimado escrito debajo con cada d.t., y
 * la tabla de residuos grandes. El .eps de arriba es sólo el gráfico: era lo
 * que se abría, y el modelo no se veía sin entrar en fue_gui. Si no hay PDF
 * (un modelo estimado con un fue anterior), el .eps, que algo es.      */
static int atsw_resumen_de( Atsw *a, const char *serie, const char *muestra,
                            const char *id, char *out, size_t n )
{
    out[0] = '\0';
    if ( pr_ruta( a->p, serie, muestra, id, ".pdf", out, n ) == 0 &&
         g_file_test( out, G_FILE_TEST_EXISTS ) )
        return 0;
    if ( atsw_grafico_de( a, serie, muestra, id, out, n ) == 0 &&
         g_file_test( out, G_FILE_TEST_EXISTS ) )
        return 0;
    out[0] = '\0';
    return 1;
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

    /* EL GRAFICO DE FUE lleva una «A» delante (Aipc_m02.eps): con sólo
       la lista de arriba se quedaba huérfano al borrar.             */
    {
    char f[PR_RUTA];

    if ( atsw_grafico_de( a, serie, muestra, id, f, sizeof f ) == 0 )
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

/* «VER MODELO…»: el resumen que fue deja al estimar
   (atsw_resumen_de), en la ventana de graficos, sin abrir fue_gui. Solo
   se ofrece encendido con el modelo estimado.                        */
static void on_grafico( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );
    char   f[PR_RUTA];

    (void) m;
    if ( !a->hay || !a->serie[0] || !id ) { g_free( id ); return; }
    if ( atsw_resumen_de( a, a->serie, atsw_muestra_actual( a ), id, f, sizeof f ) != 0 )
        {
        gchar *t = g_strdup_printf( "Todavía no hay gráfico de %s: sale al estimar.", id );
        barra_pub( a, t );
        g_free( t );
        }
    else if ( !preview_show( (PreviewApp *) a, f ) )
        {
        gchar *t = g_strdup_printf( "No pude abrir %s.", f );
        barra_pub( a, t );
        g_free( t );
        }
    g_free( id );
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

static void formales( Atsw *a, AnFmFamilia fam )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_formales( &h, a->serie, atsw_muestra_actual( a ), id, fam ); }
    g_free( id );
}

static void on_integracion( GtkMenuItem *m, Atsw *a )
{ (void) m; formales( a, AN_FM_INTEGRACION ); }

static void on_estacionalidad( GtkMenuItem *m, Atsw *a )
{ (void) m; formales( a, AN_FM_ESTACIONAL ); }

static void on_ganancia( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_ganancia( &h, a->serie, atsw_muestra_actual( a ), id ); }
    g_free( id );
}

static void on_factores( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_factores_ar( &h, a->serie, atsw_muestra_actual( a ), id ); }
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

static void on_identifica_res( GtkMenuItem *m, Atsw *a )
{
    gchar *id = atsw_marcada( a->l_modelos, M_ID );

    (void) m;
    if ( a->hay && a->serie[0] && id )
        { AnHost h = atsw_host( a );
          an_identifica_residuos( &h, a->serie, atsw_muestra_actual( a ), id ); }
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
    GtkWidget *d, *caja, *rej, *e_id, *e_razon, *cd, *ch, *aviso;
    GtkWidget *e_desde = NULL, *e_hasta = NULL;
    AtFecha    f_desde, f_hasta;
    char       id2[PR_ID], b1[32] = "", b2[32] = "";
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
        /* A MANO, y SE LEEN: antes estas dos entradas se creaban y lo que
           se escribía en ellas no iba a ninguna parte.                  */
        cd = ch = NULL;
        e_desde = atsw_fila( rej, 1, "Desde ", "", "Período/año: «1/2015»." );
        e_hasta = atsw_fila( rej, 2, "Hasta ", "", "Período/año: «12/2019»." );
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

    aviso = atsw_aviso_nuevo( caja );
    gtk_widget_show_all( d );

    /* SI NO VALE, SE DICE Y EL DIALOGO SIGUE: el nombre, las fechas y la
       razón que se escribieron no se pierden por un nombre repetido.   */
    for ( ;; )
        {
        char why[512];

        r = gtk_dialog_run( GTK_DIALOG(d) );
        if ( r != GTK_RESPONSE_OK ) break;

        a_id( gtk_entry_get_text( GTK_ENTRY(e_id) ), id2, sizeof id2 );
        b1[0] = b2[0] = '\0';
        if ( cd ) atsw_fecha_texto( &f_desde, b1, sizeof b1 );
        if ( ch ) atsw_fecha_texto( &f_hasta, b2, sizeof b2 );
        if ( e_desde ) g_snprintf( b1, sizeof b1, "%s", gtk_entry_get_text( GTK_ENTRY(e_desde) ) );
        if ( e_hasta ) g_snprintf( b2, sizeof b2, "%s", gtk_entry_get_text( GTK_ENTRY(e_hasta) ) );

        /* El texto, no lo limpio: a_id llama «serie» a lo vacío. */
        {
        gchar *tx = g_strstrip( g_strdup( gtk_entry_get_text( GTK_ENTRY(e_id) ) ) );

        if ( !*tx ) id2[0] = '\0';
        g_free( tx );
        }
        if ( !id2[0] )
            {
            atsw_aviso( a, aviso, "La muestra necesita un nombre: es lo que va "
                        "en la pestaña." );
            gtk_widget_grab_focus( e_id );
            continue;
            }
        if ( cd && ch &&
             ( f_desde.freq > 1
               ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f_desde.anio) ) * 100 +
                 gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f_desde.per) )
               : gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f_desde.anio) ) ) >
             ( f_hasta.freq > 1
               ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f_hasta.anio) ) * 100 +
                 gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f_hasta.per) )
               : gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f_hasta.anio) ) ) )
            {
            atsw_aviso( a, aviso, "«Desde» va después de «Hasta»: así la ventana "
                        "no tiene ninguna observación." );
            continue;
            }
        if ( pr_muestra_add( a->p, id2, b1, b2,
                             gtk_entry_get_text( GTK_ENTRY(e_razon) ), &e ) != 0 )
            {
            pr_error_es( &e, why, sizeof why );
            atsw_aviso( a, aviso, why );
            gtk_widget_grab_focus( e_id );
            continue;
            }
        break;
        }

    if ( r == GTK_RESPONSE_OK )
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
    gtk_widget_destroy( d );
}

/* ------------------------------------------------------------------------ */
/* LAS SECCIONES                                                             */
/*                                                                           */
/* UN MENU, DOS SITIOS (ESTUDIO-madre-organizacion §3.2). El menu US de la   */
/* barra no es una lista nueva: son las secciones con que se arman los menus */
/* del boton derecho, puestas en el orden del ciclo y armadas cada vez que   */
/* se abre. Dos copias de una regla son dos reglas, y la primera entrada que */
/* dijera en la barra otra cosa que en el contextual seria la primera        */
/* divergencia. Por eso los contextuales tambien se arman aqui.              */
/* ------------------------------------------------------------------------ */

#define SIN_PROYECTO "Abre un proyecto antes: Proyecto ▸ Abrir… o Nuevo…."
#define SIN_SERIE    "Marca una serie en la lista de la izquierda."

static gboolean hay_serie( Atsw *a ) { return a->hay && a->serie[0]; }

/* LOS DOS VISTAZOS. Dibuja fug, aqui mismo, sin levantar la herramienta
 * completa. Para mirar, no para decidir: no tocan el proyecto. Van en Datos
 * --describen la serie-- y al principio de US, que es el primer paso del
 * analista: la misma funcion en los dos sitios (§3.1).                  */
static void sec_vistazos( Atsw *a, GtkWidget *menu, gboolean con_hist )
{
    GtkWidget *mi;

    mi = atsw_entrada( a, menu, "Media – desviación típica",
        "¿La dispersión crece con el nivel? Empieza en la serie EN NIVEL "
        "(λ = 1) y se mueve λ al pie del gráfico: si la nube se endereza con "
        "logaritmos, la serie los pide.", G_CALLBACK(on_mdt), a );
    if ( !hay_serie( a ) ) atsw_apaga( mi, SIN_SERIE );

    mi = atsw_entrada( a, menu, "Serie y ACF / PACF…",
        "Con λ, d y D AL PIE DEL GRÁFICO: se tocan y el dibujo se rehace ahí "
        "mismo. Es la pregunta abierta —cuántas diferencias— y se contesta "
        "probando.", G_CALLBACK(on_serie_acf), a );
    if ( !hay_serie( a ) ) atsw_apaga( mi, SIN_SERIE );

    /* EL HISTOGRAMA, el tercer vistazo: el mismo fug, con -d. Describe los
       datos, asi que va en Datos y no en la identificacion de US (§3.1). */
    if ( con_hist )
        {
        mi = atsw_entrada( a, menu, "Histograma…",
            "La serie con λ, d y D AL PIE, estandarizada, con la densidad "
            "normal encima y su asimetría, curtosis y Jarque-Bera. Lo dibuja "
            "fug, como en su herramienta.", G_CALLBACK(on_hist), a );
        if ( !hay_serie( a ) ) atsw_apaga( mi, SIN_SERIE );
        }
}

/* IDENTIFICAR, en el orden en que trabaja el analista: λ, d y D, el juego
   completo de fug y la propuesta de art (§3.1, decision 8).

   «Listado de identificacion» es el nombre de la escuela para la primera
   identificacion completa: el juego de fug, para cada numero de
   diferencias. «Identificar la estructura ARIMA» dice lo que hace art: la
   propone, con sus contrastes y sus candidatos.                       */
#define GLOBO_LISTADO \
    "Primera identificación completa (fug): la serie, el histograma, la " \
    "media–desviación típica, la ACF y la PACF para cada número de " \
    "diferencias.\n\nVa el nodo de datos de esta muestra: se identifica " \
    "ANTES de que haya modelo."

static void sec_identificar( Atsw *a, GtkWidget *menu )
{
    GtkWidget *mi;

    sec_vistazos( a, menu, FALSE );
    mi = atsw_entrada( a, menu, "Listado de identificación…", GLOBO_LISTADO,
                       G_CALLBACK(on_fug), a );
    if ( !hay_serie( a ) ) atsw_apaga( mi, SIN_SERIE );

    mi = atsw_entrada( a, menu, "Identificar la estructura ARIMA…",
        "art sobre la serie: los contrastes (la F estacional, ADF y KPSS) "
        "dichos con lo que dicen, el correlograma y los candidatos. La "
        "transformación se cambia en la propia ventana.\n\nPropone; no "
        "estima. El modelo lo eliges tú.", G_CALLBACK(on_identifica_datos), a );
    if ( !hay_serie( a ) ) atsw_apaga( mi, SIN_SERIE );
}

/* ¿SOLO HAY DATOS? Entonces lo que toca no es estimar sino especificar el
   primer modelo, y la etiqueta lo dice.                               */
static gboolean solo_datos( Atsw *a )
{
    const char *porde = atsw_modelo_por_defecto( a->p, a->serie );

    return !*porde || pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), porde );
}

/* LOS NOMBRES DE LOS ENVIOS, uno por accion (ESTUDIO §1.2):
 *
 *   Estimar mNN                     a fue, el .pre si lo hay. Ctrl+E.
 *   Especificar el primer modelo    lo mismo cuando solo hay datos: nace un
 *                                   modelo de ellos y es ese el que se abre.
 *   Modelo nuevo, desde los datos   otro de cero, colgado de los datos.
 *   Modelo nuevo, desde los datos   lo mismo sobre otra ventana: copia los
 *     de otra muestra ▸             DATOS, recortados.
 *   Llevar mNN a otra muestra ▸     copia ESTE MODELO a otra ventana.
 *
 * Los dos ultimos se confundian («Modelo nuevo, en otra muestra» y «En otra
 * muestra»): el nombre dice ahora QUE se copia --los datos o el modelo-- y
 * ADONDE.
 *
 * con_fue: en la barra, si hay un modelo marcado, sus envios los pone la
 * seccion del modelo y aqui no se repiten. En el boton derecho sobre la
 * serie van siempre.                                                   */
#define GLOBO_ESTIMAR \
    "Estimar %s (US) en fue: se manda el .pre si lo hay —es un óptimo " \
    "reejecutable— y si no el .inp, que es la especificación. Lo mismo " \
    "que el doble clic."
#define GLOBO_PRIMERO \
    "Especificar el primer modelo (US) en fue: los datos no se estiman. " \
    "Nace un modelo colgado de ellos y es ése el que se abre; el .inp de " \
    "los datos sigue intacto."
#define GLOBO_NUEVO \
    "Empieza de cero (US, en fue): cuelga de los datos, no del modelo de " \
    "ahora. Para seguir desde un óptimo está «Iterar»."

static void sec_serie_envios( Atsw *a, GtkWidget *menu, gboolean con_fue )
{
    GtkWidget *mi;

    if ( !hay_serie( a ) )
        {
        mi = atsw_entrada( a, menu, "Especificar el primer modelo",
                           NULL, G_CALLBACK(on_fue), a );
        atsw_apaga( mi, a->hay ? SIN_SERIE : SIN_PROYECTO );
        return;
        }

    /* LO QUE VA A PASAR, DICHO EN LA ETIQUETA. Que el envio a fue derive un
     * modelo cuando lo unico que hay son los datos es correcto, pero si el
     * menu no lo dice el analista no sabe que puede empezar uno: «no es
     * obvio como hacerlo» era exactamente esto.                        */
    if ( con_fue )
    {
    const char *porde = atsw_modelo_por_defecto( a->p, a->serie );
    gboolean    sd = solo_datos( a );
    gchar      *txt, *tip;

    if ( sd )
        {
        txt = g_strdup( "Especificar el primer modelo" );
        tip = g_strdup( GLOBO_PRIMERO );
        }
    else
        {
        txt = g_strdup_printf( "Estimar %s", porde );
        tip = g_strdup_printf( GLOBO_ESTIMAR, porde );
        }
    mi = atsw_entrada( a, menu, txt, tip, G_CALLBACK(on_fue), a );
    g_free( txt ); g_free( tip );
    atsw_entrada_tecla( mi, "<Primary>e" );

    /* Y empezar OTRO desde los datos, que no es lo mismo que iterar. */
    if ( !sd && pr_datos_de( a->p, a->serie, atsw_muestra_actual( a ) )[0] )
        atsw_entrada( a, menu, "Modelo nuevo, desde los datos", GLOBO_NUEVO,
                      G_CALLBACK(on_nuevo_modelo), a );
    }

    {
    GtkWidget *sub = menu_muestras( a, NULL, "\x01" );   /* ninguna excluida */

    if ( sub && a->nhojas > 1 )
        {
        mi = atsw_entrada( a, menu, "Modelo nuevo, desde los datos de otra muestra",
            "Empieza un modelo de cero sobre otra ventana declarada: copia "
            "los DATOS, no un modelo. Cuelga de los datos, y su .inp se "
            "genera del datos.csv recortado.", NULL, NULL );
        gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );
        }
    else if ( sub )
        gtk_widget_destroy( sub );
    }
}

/* LA SERIE COMO DATO: lo que no cabe en el .inp, y las ventanas sobre ella. */
static void sec_serie_datos( Atsw *a, GtkWidget *menu )
{
    GtkWidget *mi;

    mi = atsw_entrada( a, menu, "Editar la serie…",
        "Qué es, en qué unidades, de dónde se bajó y cuándo. Nada de esto "
        "cabe en el .inp —el motor sólo lee un nombre— y es la mitad de lo "
        "que hace falta para volver a este análisis dentro de seis meses.",
        G_CALLBACK(on_editar_serie), a );
    if ( !hay_serie( a ) ) atsw_apaga( mi, a->hay ? SIN_SERIE : SIN_PROYECTO );

    /* LA MUESTRA NUEVA, que solo era el «+» de las pestañas. */
    mi = atsw_entrada( a, menu, "Muestra nueva…",
        "Declarar una submuestra: hasta dónde y por qué.\n\nLos modelos de "
        "cada muestra viven en su hoja, porque dos modelos estimados sobre "
        "ventanas distintas no se comparan.", G_CALLBACK(on_muestra_nueva), a );
    if ( !a->hay ) atsw_apaga( mi, SIN_PROYECTO );
}

/* LOS ENVIOS DEL MODELO: especificar y estimar, en el orden del ciclo. */
static void sec_modelo_envios( Atsw *a, GtkWidget *menu, const char *id,
                               gboolean datos )
{
    GtkWidget *mi;
    gchar     *txt, *tip;

    /* SOBRE LOS DATOS, estimar ES empezar un modelo de ellos (on_fue
       deriva), asi que va UNA entrada: la que dice lo que pasa. Antes iban
       dos con dos nombres para lo mismo.                               */
    if ( datos && solo_datos( a ) )
        { txt = g_strdup( "Especificar el primer modelo" ); tip = g_strdup( GLOBO_PRIMERO ); }
    else if ( datos )
        { txt = g_strdup( "Modelo nuevo, desde los datos" ); tip = g_strdup( GLOBO_NUEVO ); }
    else
        { txt = g_strdup_printf( "Estimar %s", id ); tip = g_strdup_printf( GLOBO_ESTIMAR, id ); }
    mi = atsw_entrada( a, menu, txt, tip, G_CALLBACK(on_fue), a );
    g_free( txt ); g_free( tip );
    atsw_entrada_tecla( mi, "<Primary>e" );

    if ( !datos )
        {
        atsw_entrada( a, menu, "Editar el .inp…",
            "El fichero, a mano, con la salida del motor al lado y fue a un "
            "botón. La otra forma de iterar: el formulario de fue_gui sólo "
            "puede expresar lo que tiene widgets; el .inp, todo lo que el "
            "motor lee.", G_CALLBACK(on_editar), a );
        atsw_entrada( a, menu, "Iterar: seguir desde su óptimo",
            "Copia su .pre a un .inp nuevo. Un .pre que se toca vuelve a ser "
            "un .inp: sus valores vuelven a ser semillas.",
            G_CALLBACK(on_iterar), a );
        }

    if ( !datos )
        atsw_entrada( a, menu, "Modelo nuevo, desde los datos", GLOBO_NUEVO,
                      G_CALLBACK(on_nuevo_modelo), a );

    /* LLEVARLO A OTRA VENTANA. Deriva y genera; lo de aqui no se toca. */
    if ( !datos )
        {
        GtkWidget *sub = menu_muestras( a, id, atsw_muestra_actual( a ) );

        if ( sub )
            {
            txt = g_strdup_printf( "Llevar %s a otra muestra", id );
            mi = atsw_entrada( a, menu, txt,
                "Copia ESTE MODELO a otra ventana, colgado de él: el .inp se "
                "GENERA del datos.csv con la ventana nueva, no es una copia "
                "con otro nobs.\n\nÉste no se toca: su .out describe una "
                "estimación sobre estas observaciones.", NULL, NULL );
            g_free( txt );
            gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );
            }
        }
}

/* «DIAGNOSTICAR ▸», Y «VER MODELO…», «ANOMALOS…» E «IDENTIFICAR LOS
 * RESIDUOS…» DEBAJO.
 *
 * Un submenu por la longitud del menu (decision 11): las cinco piden un .out
 * al dia y se apagan juntas por la misma razon, asi que se apaga la entrada
 * entera con un solo porque. «Ver modelo…» va FUERA y justo debajo
 * (decision 12): es lo primero que se mira al volver de estimar, y no debe
 * costar entrar en el submenu.
 *
 * La regla de «listo» es la de lib/analisis, la misma que usa fue_gui: dos
 * copias de una regla son dos reglas. Y apagado no es mudo -- el globo dice
 * por que.                                                            */
static void sec_diagnosticar( Atsw *a, GtkWidget *menu, const char *id )
{
    GtkWidget *mi, *sub = gtk_menu_new();
    char       porque[512];
    AnEstado   est = an_estado( a->p, a->serie, atsw_muestra_actual( a ), id,
                                porque, sizeof porque );
    gboolean   listo = ( est == AN_LISTO );

    mi = atsw_entrada( a, menu, "Diagnosticar", listo
        ? "Lo que se le pregunta a un modelo estimado: la diagnosis, el "
          "orden de integración, la estacionalidad, la ganancia y la "
          "factorización del AR."
        : porque, NULL, NULL );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );
    gtk_widget_set_sensitive( mi, listo );

    mi = atsw_entrada( a, sub, "Diagnosis…", listo
        ? "Los cinco bloques: estimación, media, autocorrelación, "
          "normalidad y parámetros, cada uno con su veredicto.\n\nDice lo "
          "que los números dicen; qué hacer con ello es tuyo."
        : porque, G_CALLBACK(on_diagnosis), a );
    gtk_widget_set_sensitive( mi, listo );

    /* LOS CONTRASTES FORMALES, SIN PASAR POR LA DIAGNOSIS, y en dos: el
       orden de integracion regular y la estacionalidad, que preguntan
       cosas distintas. La estacionalidad, solo con s > 1.            */
    mi = atsw_entrada( a, sub, "Orden de integración…",
                       listo ? AN_FM_GLOBO_INTEGRACION : porque,
                       G_CALLBACK(on_integracion), a );
    gtk_widget_set_sensitive( mi, listo );

    {
    int s = listo ? an_formales_s( a->p, a->serie, atsw_muestra_actual( a ), id ) : 0;

    mi = atsw_entrada( a, sub, "Estacionalidad (MEG)…", !listo ? porque
                       : s > 1 ? AN_FM_GLOBO_ESTACIONAL : AN_FM_GLOBO_ANUAL,
                       G_CALLBACK(on_estacionalidad), a );
    gtk_widget_set_sensitive( mi, listo && s > 1 );
    }

    mi = atsw_entrada( a, sub, "Ganancia…", listo
        ? "¿El suceso dejó algo PARA SIEMPRE o revirtió?\n\nLa ganancia "
          "de cada intervención con su contraste. No es diagnosis: es una "
          "hipótesis sobre la naturaleza del incidente."
        : porque, G_CALLBACK(on_ganancia), a );
    gtk_widget_set_sensitive( mi, listo );

    /* LA FACTORIZACION DEL AR, sin pasar por la diagnosis: antes solo se
       llegaba desde su boton. No juzga ni contrasta: describe el operador
       estimado, y desde ella se deriva el modelo factorizado.          */
    mi = atsw_entrada( a, sub, "Factorización del AR…", listo
        ? "Las raíces de cada factor AR, y sus factores reales y complejos: "
          "de cada par, su amortiguamiento, su frecuencia y su periodo. Un "
          "par en una frecuencia estacional y cerca de la unidad es un AR "
          "estacional escondido.\n\nDesde ella se deriva el modelo "
          "factorizado."
        : porque, G_CALLBACK(on_factores), a );
    gtk_widget_set_sensitive( mi, listo );

    {
    char     f[PR_RUTA];
    gboolean hay = listo &&
        atsw_resumen_de( a, a->serie, atsw_muestra_actual( a ), id, f, sizeof f ) == 0;

    mi = atsw_entrada( a, menu, "Ver modelo…", hay
        ? "Los residuos con su ACF y su PACF, el modelo estimado escrito "
          "debajo con sus desviaciones típicas, y los residuos grandes: el "
          "resumen que fue dejó al estimar, sin abrir fue_gui."
        : listo ? "fue no dejó el gráfico de este modelo: vuelve a estimarlo."
                : porque, G_CALLBACK(on_grafico), a );
    gtk_widget_set_sensitive( mi, hay );
    }

    /* Y DETRAS DE «VER MODELO…», TAMBIEN FUERA: lo que se hace despues
       de mirarle los residuos (lo pidio el analista, 2026-10-22).    */
    mi = atsw_entrada( a, menu, "Anómalos…", listo
        ? "Los episodios de residuos extremos, y los correlogramas CON y "
          "SIN ellos.\n\nContesta lo que decide: ¿la estructura que veo "
          "es del proceso o del anómalo? Y al revés —si no cambia nada, "
          "intervenirlo no compra nada."
        : porque, G_CALLBACK(on_anomalos), a );
    gtk_widget_set_sensitive( mi, listo );

    mi = atsw_entrada( a, menu, "Identificar los residuos…", listo
        ? "art propone qué ARMA ponerle a este modelo, leyendo el "
          "correlograma de sus residuos (E3): el modelo base con sus "
          "armónicos, intervenciones y media, sin la parte ARMA.\n\n"
          "Propone; no estima. Derivar crea un hijo con el candidato que "
          "elijas, sobre su .pre."
        : porque, G_CALLBACK(on_identifica_res), a );
    gtk_widget_set_sensitive( mi, listo );

}

static void sec_prever( Atsw *a, GtkWidget *menu )
{
    atsw_entrada( a, menu, "Prever con fuf…",
        "Lo mismo que el botón «Forecast» de fue: corre «fue -f» para "
        "escribir la entrada de previsión, corre fuf sobre ella y deja "
        "las dos cosas en la pestaña de previsión.\n\nLos ficheros se "
        "escriben AL LADO del modelo.", G_CALLBACK(on_prever), a );
}

/* LAS DECISIONES, AL FINAL: elegir y el porque, y lo destructivo aparte.
   Elegir y Razón no estaban en el boton derecho; solo en la fila.    */
static void sec_decidir( Atsw *a, GtkWidget *menu )
{
    GtkWidget *mi;

    atsw_entrada( a, menu, "Elegir",
        "Declara que éste es EL modelo de la serie.", G_CALLBACK(on_elegir), a );
    atsw_entrada( a, menu, "Razón…",
        "El porqué de esta iteración. Se puede poner después —mirando el "
        ".out, que es cuando de verdad se sabe— o no ponerse.",
        G_CALLBACK(on_razon), a );
    separa( menu );
    mi = atsw_entrada( a, menu, "Borrar…",
        "El nodo y sus ficheros. Se pregunta antes. Un modelo del que "
        "cuelgue otro no se borra: rompería el linaje.", G_CALLBACK(on_borrar), a );
    atsw_entrada_tecla( mi, "Delete" );
}

/* LO QUE SIGUE A ESTIMAR, en el orden del ciclo: diagnosticar, prever y
   decidir. Los datos no se diagnostican.                              */
static void secciones_estimado( Atsw *a, GtkWidget *menu, const char *id )
{
    if ( pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id ) ) return;
    separa( menu );
    sec_diagnosticar( a, menu, id );
    separa( menu );
    sec_prever( a, menu );
    separa( menu );
    sec_decidir( a, menu );
}

/* LAS SECCIONES DEL MODELO: el boton derecho sobre la rejilla. */
static void secciones_modelo( Atsw *a, GtkWidget *menu, const char *id )
{
    sec_modelo_envios( a, menu, id,
                       pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id ) );
    secciones_estimado( a, menu, id );
}

/* EL MENU DE LA SERIE, con el boton derecho sobre la lista: lo que se puede
 * HACER con ella, en el orden del ciclo y con las mismas secciones que la
 * barra. La seleccion se hace ANTES de desplegar, asi que el menu actua
 * sobre la serie que se acaba de marcar y la rejilla de al lado ya la
 * enseña. Se cierra con Escape o pulsando fuera, y la serie se queda.  */
static void menu_serie( Atsw *a, GdkEventButton *ev )
{
    GtkWidget *menu = gtk_menu_new();

    sec_identificar( a, menu );
    separa( menu );
    sec_serie_envios( a, menu, TRUE );
    separa( menu );
    sec_serie_datos( a, menu );

    gtk_widget_show_all( menu );
    atsw_menu_en( menu, a->l_series, ev );
}

/* EL MENU DEL MODELO. Lo que se puede hacer con ESTE, no con la serie. */
static void menu_modelo( Atsw *a, GdkEventButton *ev )
{
    GtkWidget *menu = gtk_menu_new();
    gchar     *id = atsw_marcada( a->l_modelos, M_ID );

    if ( id ) secciones_modelo( a, menu, id );
    g_free( id );
    gtk_widget_show_all( menu );
    atsw_menu_en( menu, a->l_modelos, ev );
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

gboolean atsw_on_menu_tecla( GtkWidget *tv, Atsw *a )
{
    gchar *id;

    (void) tv;
    if ( !a->hay || !a->serie[0] ) return FALSE;
    if ( ( id = atsw_marcada( a->l_modelos, M_ID ) ) == NULL ) return FALSE;
    g_free( id );
    menu_modelo( a, NULL );
    return TRUE;
}

gboolean atsw_on_tecla( GtkWidget *tv, GdkEventKey *ev, Atsw *a )
{
    (void) tv;
    if ( ( ev->keyval == GDK_KEY_Delete || ev->keyval == GDK_KEY_KP_Delete ) &&
         !( ev->state & ( GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK ) ) )
        {
        gchar *id = atsw_marcada( a->l_modelos, M_ID );
        gboolean datos = id && pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id );

        g_free( id );
        /* EL NODO DE DATOS NO SE BORRA: el menú ni lo ofrece. */
        if ( datos ) { barra_pub( a, "Los datos de la serie no se borran desde aquí." );
                       return TRUE; }
        on_borrar( NULL, a );
        return TRUE;
        }
    return FALSE;
}

/* Ver atsw.h. */
void atsw_menu_en( GtkWidget *menu, GtkWidget *tv, GdkEventButton *ev )
{
    GtkTreePath  *ruta = NULL;
    GdkRectangle  r;

    if ( ev != NULL || tv == NULL || !GTK_IS_TREE_VIEW(tv) )
        { gtk_menu_popup_at_pointer( GTK_MENU(menu), (GdkEvent *) ev ); return; }

    gtk_tree_view_get_cursor( GTK_TREE_VIEW(tv), &ruta, NULL );
    if ( ruta == NULL )
        { gtk_menu_popup_at_widget( GTK_MENU(menu), tv, GDK_GRAVITY_NORTH_WEST,
                                    GDK_GRAVITY_NORTH_WEST, NULL ); return; }
    gtk_tree_view_get_cell_area( GTK_TREE_VIEW(tv), ruta, NULL, &r );
    gtk_tree_path_free( ruta );
    /* La celda viene en coordenadas de la ventana de dibujo de la lista,
       que es a la que se ancla el menú.                                */
    gtk_menu_popup_at_rect( GTK_MENU(menu),
                            gtk_tree_view_get_bin_window( GTK_TREE_VIEW(tv) ), &r,
                            GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL );
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

static void on_cuaderno( GtkButton *b, Atsw *a ) { (void) b; atsw_cuaderno( a ); }

/* ------------------------------------------------------------------------ */
/* LA BARRA DE MENUS (ESTUDIO-madre-organizacion §3)                         */
/*                                                                           */
/* Proyecto · Datos · US · MT · Ayuda. Por CLASE de modelo y no por objeto:  */
/* las series y sus modelos son US y los casos y los suyos son MT, y decirlo */
/* asi enseña que hay una escalera y en que peldaño se esta. VARMA y VECM    */
/* entran con sus GUI (§3.5): una entrada que nunca se enciende enseña que   */
/* el menu miente.                                                           */
/*                                                                           */
/* La fila de arriba se queda entera. El boton drtran, «→ fug» y los dos    */
/* «Borrar» de la fila del caso ya no estan (§6, fase 3): lo raro y lo       */
/* destructivo viven en la barra, el boton derecho y Supr.                   */
/* ------------------------------------------------------------------------ */

static void relee( Atsw *a )
{
    char why[512];

    if ( !a->hay ) { barra_pub( a, "No hay proyecto abierto." ); return; }
    if ( atsw_relee( a, why, sizeof why ) )
        barra_pub( a, "El proyecto ha cambiado fuera de aquí: releído." );
    else
        barra_pub( a, why[0] ? why : "Releído: no había nada nuevo." );
    atsw_refresca( a );
}

static void on_relee_mi( GtkMenuItem *m, Atsw *a ) { (void) m; relee( a ); }
static void on_sale_mi ( GtkMenuItem *m, Atsw *a ) { (void) m; gtk_window_close( GTK_WINDOW(a->ventana) ); }

/* EL MANUAL, donde este. Instalado esta en sitios distintos en cada sistema
 * (packaging/): en Linux en share/doc/atsw, en Windows al lado de bin/; en
 * el arbol, el que construye docs/manual/construir.py, y si no su fuente.
 * El de macOS va en el .dmg, fuera de la aplicacion, asi que alli --y si no
 * hay ninguno-- el publicado, que es el de la ultima version.          */
static gchar *manual_uri( void )
{
    static const char *const sitios[] = {
        "../share/doc/atsw/Manual de ATSW.pdf",     /* Linux, instalado    */
        "../Manual de ATSW.pdf",                    /* Windows, el bundle  */
        "../../docs/manual/MANUAL.pdf",             /* el arbol, construido */
        "../../docs/manual/MANUAL.html",
        "../../docs/manual/MANUAL.md",
        NULL };
    gchar *dir = sitio_mi_dir(), *r = NULL;
    int    i;

    for ( i = 0; dir && sitios[i] && !r; i++ )
        {
        gchar *f = g_build_filename( dir, sitios[i], NULL );

        if ( g_file_test( f, G_FILE_TEST_IS_REGULAR ) )
            r = g_filename_to_uri( f, NULL, NULL );
        g_free( f );
        }
    g_free( dir );
    return r ? r : g_strdup( "https://davidesg.github.io/atsw-gui/manual/MANUAL.pdf" );
}

static void abre_manual( Atsw *a )
{
    gchar  *u = manual_uri();
    GError *e = NULL;

    if ( !gtk_show_uri_on_window( GTK_WINDOW(a->ventana), u, GDK_CURRENT_TIME, &e ) )
        {
        gchar *t = g_strdup_printf( "No pude abrir el manual (%s): %s", u,
                                    e ? e->message : "?" );
        barra_pub( a, t );
        g_free( t );
        }
    else
        barra_pub( a, "El manual, con el programa del sistema." );
    g_clear_error( &e );
    g_free( u );
}

static void on_manual_mi( GtkMenuItem *m, Atsw *a ) { (void) m; abre_manual( a ); }

static gboolean on_atajo_manual( GtkAccelGroup *g, GObject *o, guint k,
                                 GdkModifierType m, Atsw *a )
{
    (void) g; (void) o; (void) k; (void) m;
    abre_manual( a );
    return TRUE;
}

/* LAS TECLAS, en una ventana que no bloquea. La de Ctrl se escribe como la
   escribe GTK en este sistema: en macOS es Cmd.                      */
static void on_teclas_mi( GtkMenuItem *m, Atsw *a )
{
    static const struct { const char *acel, *que; } T[] = {
        { "<Primary>o", "Abrir un proyecto" },
        { "<Primary>n", "Proyecto nuevo" },
        { "<Primary>r", "Releer lo que haya cambiado fuera" },
        { "<Primary>e", "Estimar lo que se ve a la derecha: el modelo (US) en fue o el caso (MT) en drtran" },
        { "<Primary>q", "Salir" },
        { "F1",         "El manual" },
        { "F10",        "La barra de menús; Alt y la letra subrayada, un menú" },
        { "Menu",       "El menú de la fila marcada (también Mayús+F10)" },
        { "Delete",     "Borrar el modelo marcado, preguntando antes" },
        { "Return",     "Estimar el modelo marcado (también doble clic)" } };
    GString   *g = g_string_new( NULL );
    GtkWidget *d;
    guint      i;

    (void) m;
    for ( i = 0; i < G_N_ELEMENTS(T); i++ )
        {
        guint           k = 0;
        GdkModifierType mo = 0;
        gchar          *l;

        gtk_accelerator_parse( T[i].acel, &k, &mo );
        l = gtk_accelerator_get_label( k, mo );
        g_string_append_printf( g, "%-12s  %s\n", l, T[i].que );
        g_free( l );
        }
    d = gtk_message_dialog_new( GTK_WINDOW(a->ventana), GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Las teclas de la madre" );
    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d), "%s", g->str );
    g_signal_connect( d, "response", G_CALLBACK(gtk_widget_destroy), NULL );
    gtk_widget_show( d );
    g_string_free( g, TRUE );
}

#ifndef ATSW_VERSION
#define ATSW_VERSION "en desarrollo"
#endif

static void on_comprobar_mi( GtkMenuItem *m, Atsw *a ) { (void) m; atsw_informe_instalacion( a ); }

/* ACERCA DE: sobrio, con los mismos creditos que la presentacion. */
static void on_acerca_mi( GtkMenuItem *m, Atsw *a )
{
    GtkWidget *d = gtk_about_dialog_new();
    GdkPixbuf *logo = at_logo( 128 );

    (void) m;
    gtk_window_set_transient_for( GTK_WINDOW(d), GTK_WINDOW(a->ventana) );
    gtk_window_set_destroy_with_parent( GTK_WINDOW(d), TRUE );
    gtk_about_dialog_set_program_name( GTK_ABOUT_DIALOG(d), "ATSW" );
    gtk_about_dialog_set_logo( GTK_ABOUT_DIALOG(d), logo );
    g_object_unref( logo );
    gtk_about_dialog_set_version( GTK_ABOUT_DIALOG(d), ATSW_VERSION );
    gtk_about_dialog_set_comments( GTK_ABOUT_DIALOG(d),
        "A Time Series Workshop: el taller de series temporales de la escuela "
        "Box-Jenkins-Treadway.\n\n"
        "Diseño y construcción: Arthur B. Treadway y David E. Guerrero.\n"
        "Librerías y motores: José Alberto Mauricio, con Arthur B. Treadway y "
        "David E. Guerrero." );
    gtk_about_dialog_set_copyright( GTK_ABOUT_DIALOG(d),
        "Copyright © 1995-2026 A.B. Treadway, J.A. Mauricio y D.E. Guerrero" );
    gtk_about_dialog_set_license_type( GTK_ABOUT_DIALOG(d), GTK_LICENSE_GPL_2_0 );
    gtk_about_dialog_set_website( GTK_ABOUT_DIALOG(d),
                                  "https://davidesg.github.io/atsw-gui/" );
    g_signal_connect( d, "response", G_CALLBACK(gtk_widget_destroy), NULL );
    gtk_widget_show( d );
}

static void llena_proyecto( Atsw *a, GtkWidget *m )
{
    GtkWidget *mi;

    mi = atsw_entrada( a, m, "Nuevo…", "Un proyecto nuevo, vacío.",
                       G_CALLBACK(on_nuevo), a );
    atsw_entrada_tecla( mi, "<Primary>n" );
    mi = atsw_entrada( a, m, "Abrir…", "Un proyecto.yaml. Si está roto se dice "
                       "y NO se abre: nunca se pisa.", G_CALLBACK(on_abrir), a );
    atsw_entrada_tecla( mi, "<Primary>o" );
    mi = atsw_entrada( a, m, "Releer", "Lo que haya cambiado fuera de aquí: "
                       "lo mismo que pasa al volver a la ventana.",
                       G_CALLBACK(on_relee_mi), a );
    atsw_entrada_tecla( mi, "<Primary>r" );
    if ( !a->hay ) atsw_apaga( mi, "No hay proyecto abierto." );
    separa( m );
    mi = atsw_entrada( a, m, "Propiedades…",
        "Identificador, título y analista. Y lo que no se edita pero hay que "
        "saber: cuándo se creó, dónde está el manifiesto y dónde la raíz de "
        "los datos. (Es el botón «Proyecto…».)", G_CALLBACK(on_proyecto), a );
    if ( !a->hay ) atsw_apaga( mi, SIN_PROYECTO );
    mi = atsw_entrada( a, m, "Linaje…",
        "La cadena entera y lo que cada nodo DEBE: sin estimar, sin razón, "
        "cadena sin elegido.", G_CALLBACK(on_linaje), a );
    if ( !a->hay ) atsw_apaga( mi, SIN_PROYECTO );
    mi = atsw_entrada( a, m, "Cuaderno…",
        "Lo que se miró, en orden, con su fecha y su procedencia, y las notas. "
        "Es Markdown, al lado del manifiesto: se lee sin ATSW.",
        G_CALLBACK(on_cuaderno), a );
    if ( !a->hay ) atsw_apaga( mi, SIN_PROYECTO );
    separa( m );
    mi = atsw_entrada( a, m, "Salir", NULL, G_CALLBACK(on_sale_mi), a );
    atsw_entrada_tecla( mi, "<Primary>q" );
}

static void llena_datos( Atsw *a, GtkWidget *m )
{
    GtkWidget *mi;

    mi = atsw_entrada( a, m, "Importar series…",
        "De un .xlsx, un .csv o un .txt a n SERIES del proyecto, cada una con "
        "su primer .inp. (Es el botón «Datos…».)", G_CALLBACK(on_datos), a );
    if ( !a->hay ) atsw_apaga( mi, SIN_PROYECTO );
    sec_serie_datos( a, m );
    separa( m );
    sec_vistazos( a, m, TRUE );
}

/* US: LA SERIE MARCADA Y SUS MODELOS, en el orden del ciclo de Box y
 * Jenkins (identificar, especificar y estimar, diagnosticar, prever) y con
 * las decisiones al final. Es la seccion de la serie seguida de la del
 * modelo marcado en la rejilla, que son las de los dos menus del boton
 * derecho.                                                            */
static void llena_us( Atsw *a, GtkWidget *m )
{
    gchar *id = hay_serie( a ) ? atsw_marcada( a->l_modelos, M_ID ) : NULL;

    sec_identificar( a, m );
    separa( m );
    if ( id )
        {
        /* Los envios del modelo y, detras, el de la serie que no repiten:
           el modelo nuevo en otra muestra, que cuelga de los datos.   */
        sec_modelo_envios( a, m, id,
                           pr_es_datos( a->p, a->serie, atsw_muestra_actual( a ), id ) );
        sec_serie_envios( a, m, FALSE );
        secciones_estimado( a, m, id );
        }
    else
        sec_serie_envios( a, m, TRUE );
    g_free( id );
}

static void llena_ayuda( Atsw *a, GtkWidget *m )
{
    GtkWidget *mi;

    mi = atsw_entrada( a, m, "Manual", "El manual del alumno: el caso IPC ← WTI "
                       "paso a paso, con capturas.", G_CALLBACK(on_manual_mi), a );
    atsw_entrada_tecla( mi, "F1" );
    atsw_entrada( a, m, "Teclas", "Las teclas de la madre, en una lista.",
                  G_CALLBACK(on_teclas_mi), a );
    separa( m );
    atsw_entrada( a, m, "Comprobar la instalación…", "Lo mismo que se comprueba al "
                  "arrancar: los motores, los programas y lo que usa GTK. De lo que "
                  "falte, cómo se arregla.", G_CALLBACK(on_comprobar_mi), a );
    atsw_entrada( a, m, "Acerca de ATSW", "La versión, los créditos y la licencia.",
                  G_CALLBACK(on_acerca_mi), a );
}

/* LOS TITULOS. El globo de cada uno dice su nombre completo (decision 7),
   con el formato de los globos de la madre: que es, una linea en blanco,
   lo que contiene, y la tecla.                                        */
typedef void (*Llena)( Atsw *a, GtkWidget *m );

static const struct { const char *rotulo, *globo, *acel; Llena llena; } TITULOS[] = {
    { "_Proyecto",
      "Proyecto — el manifiesto y lo que cuelga de él\n\nAbrir, crear y releer; "
      "las propiedades, el linaje y el cuaderno. Se guarda solo.",
      "<Alt>p", llena_proyecto },
    { "_Datos",
      "Datos — las series como datos, antes de cualquier modelo\n\nImportar, "
      "editar la serie, declarar una muestra; la media–desviación típica, la "
      "serie con su ACF y su PACF, y el histograma.",
      "<Alt>d", llena_datos },
    { "_US",
      "US — Modelo univariante (Box-Jenkins), con fue y fuf\n\nLa serie marcada "
      "y sus modelos: identificar, especificar y estimar, diagnosticar, prever; "
      "y las decisiones al final.",
      "<Alt>u", llena_us },
    { "_MT",
      "MT — Transferencia multivariante, con drtran\n\nEl caso marcado: varias "
      "series, cada una con su modelo univariante, de entrada y de salida.",
      "<Alt>m", atsw_menu_mt },
    { "A_yuda",
      "Ayuda — el manual, las teclas y «Acerca de»\n\nF1 abre el manual.",
      "<Alt>y", llena_ayuda } };

/* Rehace el menu del titulo i con el estado de ahora. */
static void rehace_menu( Atsw *a, GtkWidget *titulo )
{
    GtkWidget *m = gtk_menu_item_get_submenu( GTK_MENU_ITEM(titulo) );
    GList     *l, *h;
    Llena      fn = (Llena) g_object_get_data( G_OBJECT(titulo), "llena" );

    if ( m == NULL || fn == NULL ) return;
    h = gtk_container_get_children( GTK_CONTAINER(m) );
    for ( l = h; l; l = l->next ) gtk_widget_destroy( l->data );
    g_list_free( h );
    fn( a, m );
    gtk_widget_show_all( m );
}

/* EL RECUADRO DEL TITULO, CON EL TECLADO. Con el raton sale el globo de
 * siempre; con F10, Alt+letra o las flechas GTK3 no lo enseña, y el
 * analista lo quiere igual (§3.1 bis). Va a la derecha del menu abierto,
 * para no taparlo, y se quita al salir del titulo.                     */
static gboolean coloca_recuadro( gpointer d )
{
    Atsw      *a = d;
    GtkWidget *titulo = g_object_get_data( G_OBJECT(a->recuadro), "titulo" );
    GtkWidget *m, *top;
    GdkWindow *gw;
    gint       x = 0, y = 0, w, wr = 0, hr = 0;

    if ( titulo == NULL ) return G_SOURCE_REMOVE;
    m   = gtk_menu_item_get_submenu( GTK_MENU_ITEM(titulo) );
    top = m ? gtk_widget_get_toplevel( m ) : NULL;
    gw  = top ? gtk_widget_get_window( top ) : NULL;
    if ( gw && gtk_widget_get_mapped( top ) )
        {
        gdk_window_get_origin( gw, &x, &y );
        w = gtk_widget_get_allocated_width( top );
        x += w + 8;
        }
    else if ( ( gw = gtk_widget_get_window( titulo ) ) != NULL )
        {
        GtkAllocation al;

        gdk_window_get_origin( gw, &x, &y );
        gtk_widget_get_allocation( titulo, &al );
        x += al.x + al.width + 8;
        y += al.y + al.height;
        }
    gtk_window_get_size( GTK_WINDOW(a->recuadro), &wr, &hr );
    {
    GdkMonitor   *mo = gdk_display_get_monitor_at_point( gdk_display_get_default(), x, y );
    GdkRectangle  r;

    if ( mo )
        {
        gdk_monitor_get_workarea( mo, &r );
        if ( x + wr > r.x + r.width ) x = r.x + r.width - wr;
        }
    }
    gtk_window_move( GTK_WINDOW(a->recuadro), x, y );
    return G_SOURCE_REMOVE;
}

static GtkWidget *recuadro_nuevo( void )
{
    GtkWidget      *w = gtk_window_new( GTK_WINDOW_POPUP );
    GtkWidget      *l = gtk_label_new( NULL );
    GtkCssProvider *css = gtk_css_provider_new();

    /* Con el aspecto de un globo: el estilo «tooltip» del tema, y por si
       el tema no lo da a una ventana cualquiera, el de Adwaita.        */
    gtk_css_provider_load_from_data( css,
        "window.atsw-recuadro { background-color: rgba(0,0,0,0.85); "
        "border-radius: 5px; }"
        "window.atsw-recuadro label { color: white; padding: 6px 10px; }", -1, NULL );
    gtk_style_context_add_provider( gtk_widget_get_style_context( w ),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
    gtk_style_context_add_provider( gtk_widget_get_style_context( l ),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
    g_object_unref( css );
    gtk_style_context_add_class( gtk_widget_get_style_context( w ), "atsw-recuadro" );
    gtk_window_set_type_hint( GTK_WINDOW(w), GDK_WINDOW_TYPE_HINT_TOOLTIP );
    /* UN ANCHO DE LECTURA: envuelta sin el, la ventana emergente se
       quedaba en su minimo y salia una palabra por linea.             */
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_label_set_width_chars( GTK_LABEL(l), 44 );
    gtk_label_set_max_width_chars( GTK_LABEL(l), 48 );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_container_add( GTK_CONTAINER(w), l );
    gtk_widget_show( l );
    return w;
}

static void on_titulo_dentro( GtkMenuItem *t, Atsw *a )
{
    GdkEvent *ev = gtk_get_current_event();
    gboolean  teclado = ev == NULL || ev->type == GDK_KEY_PRESS ||
                        ev->type == GDK_KEY_RELEASE;

    if ( ev ) gdk_event_free( ev );
    rehace_menu( a, GTK_WIDGET(t) );
    enseña_en_barra( a, GTK_WIDGET(t) );
    if ( !teclado ) return;

    if ( a->recuadro == NULL ) a->recuadro = recuadro_nuevo();
    {
    gchar *g = gtk_widget_get_tooltip_text( GTK_WIDGET(t) );

    gtk_label_set_text( GTK_LABEL(gtk_bin_get_child( GTK_BIN(a->recuadro) )), g );
    g_free( g );
    }
    gtk_window_set_transient_for( GTK_WINDOW(a->recuadro), GTK_WINDOW(a->ventana) );
    g_object_set_data( G_OBJECT(a->recuadro), "titulo", t );
    gtk_widget_show( a->recuadro );
    coloca_recuadro( a );
    g_idle_add( coloca_recuadro, a );       /* cuando el menu ya tiene sitio */
}

static void on_titulo_fuera( GtkMenuItem *t, Atsw *a )
{
    (void) t;
    if ( a->recuadro )
        {
        g_object_set_data( G_OBJECT(a->recuadro), "titulo", NULL );
        gtk_widget_hide( a->recuadro );
        }
    repone_barra( a );
}

static GtkWidget *barra_de_menus( Atsw *a )
{
    GtkWidget *mb = gtk_menu_bar_new();
    guint      i;

    for ( i = 0; i < G_N_ELEMENTS(TITULOS); i++ )
        {
        GtkWidget *t = gtk_menu_item_new_with_mnemonic( TITULOS[i].rotulo );
        GtkWidget *m = gtk_menu_new();

        gchar     *g = atsw_globo_tecla( TITULOS[i].globo, TITULOS[i].acel );

        gtk_widget_set_tooltip_text( t, g );
        g_free( g );
        g_object_set_data( G_OBJECT(t), "llena", (gpointer) TITULOS[i].llena );
        gtk_menu_item_set_submenu( GTK_MENU_ITEM(t), m );
        g_signal_connect( t, "select",   G_CALLBACK(on_titulo_dentro), a );
        g_signal_connect( t, "deselect", G_CALLBACK(on_titulo_fuera), a );
        gtk_menu_shell_append( GTK_MENU_SHELL(mb), t );
        }
    return mb;
}

static gboolean on_atajo_relee( GtkAccelGroup *g, GObject *o, guint k,
                                GdkModifierType m, Atsw *a )
{
    (void) g; (void) o; (void) k; (void) m;
    relee( a );
    return TRUE;
}

static gboolean on_atajo_sale( GtkAccelGroup *g, GObject *o, guint k,
                               GdkModifierType m, Atsw *a )
{
    (void) g; (void) o; (void) k; (void) m;
    gtk_window_close( GTK_WINDOW(a->ventana) );
    return TRUE;
}

/* CERRAR LA MADRE CIERRA A SUS HIJOS. Los GUIs que lanzo vigilan su pid y
 * se van con ella (lib/sitio): un fue_gui o un drtran_gui abiertos sin la
 * madre seguirian escribiendo modelos en el proyecto sin nadie que los vea.
 * Si hay alguno, se pregunta antes: lo que no se haya guardado en ellos se
 * pierde. Salir, Ctrl+Q y la X de la ventana pasan todos por aqui.    */
static gboolean on_cierra_madre( GtkWidget *w, GdkEvent *ev, Atsw *a )
{
    char       quienes[256];
    int        n = atsw_hijos_vivos( a, quienes, sizeof quienes ), r;
    GtkWidget *d;

    (void) ev;
    if ( n == 0 ) return FALSE;
    d = gtk_message_dialog_new( GTK_WINDOW(w), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_NONE, "%s abiert%s desde aquí: %s.",
                                n == 1 ? "Hay una ventana" : "Hay varias ventanas",
                                n == 1 ? "a" : "as", quienes );
    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d),
        "Se cierran con la madre. Lo que no hayas guardado en ellas se pierde." );
    gtk_dialog_add_buttons( GTK_DIALOG(d), "_Cancelar", GTK_RESPONSE_CANCEL,
                            "Cerrar _todo", GTK_RESPONSE_ACCEPT, NULL );
    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_CANCEL );
    r = gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
    return r != GTK_RESPONSE_ACCEPT;
}

/* Ctrl+E ESTIMA LO QUE SE VE. La derecha es una pila --los modelos de la
 * serie o el caso-- y el mismo gesto vale en las dos: el programa que se
 * abre es cosa de la clase, fue para US y drtran para MT (§3.3). La barra
 * de estado lo dice, para que no sorprenda (§5).                       */
static gboolean on_atajo_estima( GtkAccelGroup *g, GObject *o, guint k,
                                 GdkModifierType m, Atsw *a )
{
    const char *v = gtk_stack_get_visible_child_name( GTK_STACK(a->pila) );

    (void) g; (void) o; (void) k; (void) m;
    if ( v && !strcmp( v, "caso" ) ) atsw_caso_lanza( a, "drtran_gui" );
    else                             on_fue( NULL, a );
    return TRUE;
}

/* La presentacion al arrancar: main() la enciende, salvo con
   --sin-presentacion. Las pruebas no pasan por main().                */
static gboolean presentacion = FALSE;

static void activate( GtkApplication *app, gpointer d )
{
    Atsw         *a = d;
    GtkWidget    *w, *raiz, *barra, *pan, *izq, *der, *vb, *bt;
    GtkListStore *st;
    GtkAccelGroup *atajos;
    guint          k;
    GdkModifierType prim;

    at_logo_por_defecto();
    w = gtk_application_window_new( app );
    a->ventana = w;
    gtk_window_set_title( GTK_WINDOW(w), "ATSW — el taller" );
    gtk_window_set_default_size( GTK_WINDOW(w), 1000, 620 );
    g_signal_connect( w, "focus-in-event", G_CALLBACK(on_foco), a );
    g_signal_connect( w, "delete-event", G_CALLBACK(on_cierra_madre), a );

    /* LA BARRA DE MENUS, pegada al borde, y lo de siempre debajo. */
    {
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    gtk_container_add( GTK_CONTAINER(w), caja );
    a->menubar = barra_de_menus( a );
    gtk_box_pack_start( GTK_BOX(caja), a->menubar, FALSE, FALSE, 0 );
    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
    gtk_box_pack_start( GTK_BOX(caja), raiz, TRUE, TRUE, 0 );
    }

    /* --- la barra --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    /* LOS ATAJOS, los mismos que en las hijas. fue_gui y drtran_gui se
     * manejan con Ctrl+O, Ctrl+E o Ctrl+Q, y la madre no tenía ni uno:
     * quien venía de una hija perdía las manos (ESTUDIO-taste-interfaz
     * §3.3). Van en los globos, que es donde se buscan.               */
    atajos = gtk_accel_group_new();
    gtk_window_add_accel_group( GTK_WINDOW(w), atajos );
    /* <Primary> Y NO Ctrl FIJO: en macOS es Cmd, como espera quien usa un
       Mac (ESTUDIO-madre-organizacion §3.4). En Linux y Windows, Ctrl. */
    gtk_accelerator_parse( "<Primary>o", &k, &prim );

    {
    gchar *g = atsw_globo_tecla( "Un proyecto.yaml. Si está roto se dice y NO "
                                 "se abre: nunca se pisa.", "<Primary>o" );

    bt = boton( barra, "Abrir…", g, G_CALLBACK(on_abrir), a );
    g_free( g );
    gtk_widget_add_accelerator( bt, "clicked", atajos, GDK_KEY_o,
                                prim, GTK_ACCEL_VISIBLE );
    g = atsw_globo_tecla( "Un proyecto nuevo, vacío.", "<Primary>n" );
    bt = boton( barra, "Nuevo…", g, G_CALLBACK(on_nuevo), a );
    g_free( g );
    gtk_widget_add_accelerator( bt, "clicked", atajos, GDK_KEY_n,
                                prim, GTK_ACCEL_VISIBLE );
    }
    /* Ctrl+R relee lo que haya cambiado fuera, que es lo que ya pasa al
       volver a la ventana; y Ctrl+Q sale, como en las hijas.          */
    gtk_accel_group_connect( atajos, GDK_KEY_r, prim, 0,
                             g_cclosure_new( G_CALLBACK(on_atajo_relee), a, NULL ) );
    gtk_accel_group_connect( atajos, GDK_KEY_q, prim, 0,
                             g_cclosure_new( G_CALLBACK(on_atajo_sale), a, NULL ) );
    gtk_accel_group_connect( atajos, GDK_KEY_e, prim, 0,
                             g_cclosure_new( G_CALLBACK(on_atajo_estima), a, NULL ) );
    /* F1, el manual: en TASTE decia «NO HAY AYUDA DISPONIBLE». */
    gtk_accel_group_connect( atajos, GDK_KEY_F1, 0, 0,
                             g_cclosure_new( G_CALLBACK(on_atajo_manual), a, NULL ) );
    boton( barra, "Proyecto…",
        "Identificador, título y analista. Y lo que no se edita pero hay que "
        "saber: cuándo se creó, dónde está el manifiesto y dónde la raíz de "
        "los datos.", G_CALLBACK(on_proyecto), a );
    boton( barra, "Linaje…",
        "La cadena entera y lo que cada nodo DEBE: sin estimar, sin razón, "
        "cadena sin elegido.\n\nLa rejilla contesta «¿cómo va este modelo?»; "
        "esto contesta «¿cómo va el recorrido?», que es una forma de árbol y "
        "no de tabla.", G_CALLBACK(on_linaje), a );
    boton( barra, "Cuaderno…",
        "Lo que se miró, en orden: cada ventana de análisis tiene un botón "
        "«Al cuaderno» que la manda aquí con su fecha y su procedencia, y "
        "aquí se añaden notas.\n\nEl linaje dice qué se decidió; el "
        "cuaderno, qué se tenía delante al decidirlo. Es Markdown, al lado "
        "del manifiesto: se lee sin ATSW.", G_CALLBACK(on_cuaderno), a );
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


    /* --- las dos listas --- */
    pan = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
    gtk_box_pack_start( GTK_BOX(raiz), pan, TRUE, TRUE, 0 );

    izq = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    /* LA SIGLA, DONDE VIVEN LOS OBJETOS: lo de arriba es US y lo de abajo
       MT, y la madre enseña la escalera sin decir nada (§2.3).        */
    {
    GtkWidget *h = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    GtkWidget *l = gtk_label_new( NULL );

    gtk_label_set_markup( GTK_LABEL(l), "<b>Series — US</b>" );
    gtk_widget_set_tooltip_text( l,
        "US — Modelo univariante (Box-Jenkins), con fue y fuf\n\nCada serie "
        "con sus modelos, de los datos en adelante. El menú US habla de la "
        "serie marcada aquí." );
    gtk_box_pack_start( GTK_BOX(h), l, FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX(izq), h, FALSE, FALSE, 2 );
    }
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
    g_signal_connect( a->l_series, "popup-menu",
                      G_CALLBACK(on_serie_menu_tecla), a );
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

    /* ESTIMAR va aqui, con la rejilla, porque actua sobre el modelo MARCADO
     * --el mismo sujeto que Iterar, Elegir y Razón--. «→ fug» ya no: desde
     * los vistazos, fug completo es lo raro, y vive en US como «Listado de
     * identificación…» (§6, fase 3).                                   */
    {
    gchar *t = atsw_tecla( "<Primary>e" );
    gchar *g = g_strdup_printf(
        "Estimar el modelo marcado (US) en fue.\n\nSe manda el .pre si lo "
        "hay —es un óptimo reejecutable— y si no el .inp, que es la "
        "especificación. Si lo marcado son los datos, nace el primer "
        "modelo.\n\n%s, como Estimar en las hijas: estima lo que se ve a la "
        "derecha, el modelo o el caso. Intro o doble clic sobre un modelo "
        "hace lo mismo.", t );

    a->b_fue = boton( b2, "Estimar", g, G_CALLBACK(on_fue), a );
    g_free( g ); g_free( t );
    }

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
    /* MEDIDA POR LA VISIBLE, no por la mas ancha. Homogenea, la fila de
       botones del caso (852 px) se imponia tambien a los modelos (530) y,
       con los dos lados del separador en su minimo, el separador parecia
       movible pero estaba congelado.                                   */
    gtk_stack_set_hhomogeneous( GTK_STACK(a->pila), FALSE );
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
    /* Los menus nacen llenos: se rehacen al abrirse, pero un menu vacio
       no deberia asomar nunca, ni la primera vez.                     */
    {
    GList *l, *h = gtk_container_get_children( GTK_CONTAINER(a->menubar) );

    for ( l = h; l; l = l->next ) rehace_menu( a, l->data );
    g_list_free( h );
    }
    /* LA PRESENTACION, con los creditos y la comprobacion de la
       instalacion; ella abre la madre al acabar. Solo al arrancar por
       main(): las pruebas levantan activate() directamente.           */
    if ( presentacion ) atsw_presentacion( a, w );
    else                gtk_widget_show_all( w );
}

/* LOS NUMEROS, CON PUNTO. GTK, al arrancar (en el "startup" de la
   aplicacion), pone el idioma del sistema, y con el espanol LC_NUMERIC lee y
   escribe "0,5": strtod deja de entender los ficheros (.inp, .pre, .csv) y
   fprintf escribe los que leen los motores con coma. Este manejador va
   despues del de GTK y deja los numeros en C; el resto del idioma, intacto. */
static void numeros_con_punto( GApplication *app, gpointer data )
{
    (void)app; (void)data;
    setlocale( LC_NUMERIC, "C" );
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    const char     *proy = NULL;
    gboolean        sin_pres = FALSE;
    int             rc, i;

    for ( i = 1; i < argc; i++ )
        {
        if ( !strcmp( argv[i], "--proyecto" ) && i + 1 < argc ) proy = argv[++i];
        else if ( !strncmp( argv[i], "--proyecto=", 11 ) ) proy = argv[i] + 11;
        else if ( !strcmp( argv[i], "--sin-presentacion" ) ) sin_pres = TRUE;
        else if ( !strcmp( argv[i], "-h" ) || !strcmp( argv[i], "--help" ) )
            {
            printf( "uso: %s [--proyecto FICHERO] [--sin-presentacion]\n\n"
                    "  ATSW GUI: el taller. Gestiona datos, modelos y\n"
                    "  proyectos, y lanza fue, fug y drtran con el proyecto\n"
                    "  abierto.\n\n"
                    "  --sin-presentacion  sin la ventana de los creditos ni la\n"
                    "                      comprobacion de la instalacion al\n"
                    "                      arrancar (Ayuda la hace a mano).\n",
                    argv[0] );
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

    presentacion = !sin_pres;
    atsw_escritorio_registra();
    argc = 1;
    app = gtk_application_new( "org.atsw.gui", G_APPLICATION_NON_UNIQUE );
    g_signal_connect( app, "startup", G_CALLBACK(numeros_con_punto), NULL );
    g_signal_connect( app, "activate", G_CALLBACK(activate), &A );
    rc = g_application_run( G_APPLICATION(app), argc, argv );
    g_object_unref( app );
    return rc;
}
