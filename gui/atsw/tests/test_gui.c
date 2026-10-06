/* test_gui.c -- la madre de verdad, conducida desde el codigo.
 *
 * Levanta la ventana de atsw_gui con su activate() de verdad y la recorre
 * como lo haria el analista: crea un proyecto, le pone titulo, carga datos
 * de un .csv, marca una serie, la mira con el vistazo (que llama a fug y
 * DIBUJA), empieza un modelo, lo edita, lo guarda mal y bien, lo estima,
 * itera, elige, declara una muestra, mira el linaje, borra, reabre, y lanza
 * a los hermanos. Lo que se comprueba es LO QUE QUEDA A LA VISTA -- la
 * barra, las filas de las listas, los textos de las ventanas, los ficheros
 * del proyecto, los pixeles del grafico-- y no las tripas.
 *
 *   test_gui_atsw <datos.csv>     (lo compila y lo corre tests/run_gui_tests.sh)
 *
 * POR QUE SE INCLUYE main.c EN VEZ DE ENLAZARLO. Los gestos son static
 * --on_nuevo, on_fue, on_borrar...-- y son lo que hay que pulsar: llamar a
 * las funciones publicas de debajo seria probar otra cosa que lo que hace
 * el boton. Con «main» renombrado, el main de aqui es el que manda. Y por
 * lo mismo editor.c (su lista de ventanas abiertas) y vistazo.c (el EPS que
 * enseña).
 *
 * DONDE VIVE EL BINARIO IMPORTA. atsw_programa() busca a los hermanos
 * relativo al directorio del EJECUTABLE QUE CORRE -- sitio_mi_dir() -- asi
 * que esta prueba se compila EN gui/atsw/, al lado de atsw_gui, y lo que
 * resuelve es exactamente lo que resolveria el programa. Compilarla en un
 * temporal habria probado el PATH, que es justo lo que el arreglo evita.
 *
 * LOS DIALOGOS MODALES BLOQUEAN: gtk_dialog_run no vuelve hasta que alguien
 * contesta. Antes de pulsar se encola quien contestara (responde()), y un
 * temporizador lo busca entre las ventanas abiertas y lo atiende desde el
 * bucle anidado del propio dialogo.
 *
 * Sin servidor grafico no falla: dice «no hay ...» y sale con 0. Con el,
 * cada fallo es una linea «FAIL: ...» y la salida es distinta de 0.
 */
#define main atsw_main_de_verdad
#include "../src/main.c"
#undef main
#include "../src/editor.c"
#include "../src/vistazo.c"

#include <stdio.h>
#include <stdlib.h>

#include "sitio.h"
#include "inpdet.h"
#include "inpcheck.h"

static int    fallos = 0;
static char   T[PR_RUTA];          /* el directorio de la prueba          */
static char   HIJO_LOG[PR_RUTA];   /* donde escribe el hijo falso         */
static GPtrArray *historial;       /* todo lo que dijo la barra           */

static void check( int ok, const char *que, const char *vio )
{
    if ( ok ) return;
    fallos++;
    printf( "FAIL: %s; se vio: \"%s\"\n", que, vio ? vio : "(nada)" );
    fflush( stdout );
}

static void nota( const char *fmt, ... ) G_GNUC_PRINTF( 1, 2 );
static void nota( const char *fmt, ... )
{
    va_list ap;

    va_start( ap, fmt );
    vprintf( fmt, ap );
    va_end( ap );
    printf( "\n" );
    fflush( stdout );
}

/* ------------------------------------------------------------------------ */
/* EL VIGILANTE: una prueba colgada es un FAIL, no un CI parado               */
/*                                                                           */
/* Un hilo aparte, porque lo que se cuelga puede no volver al bucle: un      */
/* g_spawn_sync, un gtk_dialog_run que nadie contesta. Dice en que fase iba  */
/* y sale. Sin timeout(1), que macOS no trae.                               */
/* ------------------------------------------------------------------------ */

static const char *volatile fase_actual = "arranque";

static void fase( const char *f )
{
    fase_actual = f;
    printf( "== %s\n", f );
    fflush( stdout );
}

static gpointer vigilante( gpointer d )
{
    g_usleep( (gulong) GPOINTER_TO_INT(d) * G_USEC_PER_SEC );
    printf( "FAIL: la prueba se colgo en la fase «%s» (mas de %d s)\n",
            fase_actual, GPOINTER_TO_INT(d) );
    fflush( stdout );
    _Exit( 1 );
    return NULL;
}

/* Deja correr el bucle principal ms milisegundos. */
static void pump( int ms )
{
    gint64 hasta = g_get_monotonic_time() + (gint64) ms * 1000;

    do {
        while ( gtk_events_pending() ) gtk_main_iteration_do( FALSE );
        g_usleep( 2000 );
    } while ( g_get_monotonic_time() < hasta );
}

/* ------------------------------------------------------------------------ */
/* LA BARRA: lo que dice ahora y todo lo que ha dicho                         */
/*                                                                           */
/* Hay gestos que dicen dos cosas seguidas --«Modelo nuevo» pone la ruta del */
/* fue_gui que lanzo y encima el porque--, asi que mirar solo el final       */
/* perderia la primera. Se escucha la etiqueta y se guarda cada cambio.      */
/* ------------------------------------------------------------------------ */

static void on_barra( GObject *o, GParamSpec *ps, gpointer d )
{
    (void) ps; (void) d;
    g_ptr_array_add( historial,
                     g_strdup( gtk_label_get_text( GTK_LABEL(o) ) ) );
}

static const char *barra( void )
{
    return gtk_label_get_text( GTK_LABEL(A.estado) );
}

static void olvida( void )
{
    g_ptr_array_set_size( historial, 0 );
}

/* ¿Alguna de las cosas dichas desde olvida() contiene <que>? */
static const char *dijo( const char *que )
{
    guint i;

    for ( i = 0; i < historial->len; i++ )
        if ( strstr( g_ptr_array_index( historial, i ), que ) )
            return g_ptr_array_index( historial, i );
    return NULL;
}

static const char *todo_lo_dicho( void )
{
    static GString *g;
    guint i;

    if ( !g ) g = g_string_new( NULL );
    g_string_truncate( g, 0 );
    for ( i = 0; i < historial->len; i++ )
        g_string_append_printf( g, "%s«%s»", i ? " | " : "",
                                (char *) g_ptr_array_index( historial, i ) );
    return g->str;
}

/* ------------------------------------------------------------------------ */
/* Widgets                                                                   */
/* ------------------------------------------------------------------------ */

static void junta_r( GtkWidget *w, gpointer d )
{
    GPtrArray *a = d;

    g_ptr_array_add( a, w );
    if ( GTK_IS_CONTAINER(w) )
        gtk_container_forall( GTK_CONTAINER(w), junta_r, a );
}

/* Los descendientes de raiz del tipo t, en orden de arbol. Una GtkSpinButton
   ES una GtkEntry: con solo_entradas se dejan fuera.                    */
static GPtrArray *junta( GtkWidget *raiz, GType t, gboolean solo_entradas )
{
    GPtrArray *todos = g_ptr_array_new(), *r = g_ptr_array_new();
    guint      i;

    junta_r( raiz, todos );
    for ( i = 0; i < todos->len; i++ )
        {
        GtkWidget *w = g_ptr_array_index( todos, i );

        if ( !G_TYPE_CHECK_INSTANCE_TYPE( w, t ) ) continue;
        if ( solo_entradas && GTK_IS_SPIN_BUTTON(w) ) continue;
        g_ptr_array_add( r, w );
        }
    g_ptr_array_free( todos, TRUE );
    return r;
}

static GtkWidget *boton_que_dice( GtkWidget *raiz, const char *etiqueta )
{
    GPtrArray *b = junta( raiz, GTK_TYPE_BUTTON, FALSE );
    GtkWidget *r = NULL;
    guint      i;

    for ( i = 0; i < b->len && !r; i++ )
        {
        const char *l = gtk_button_get_label( g_ptr_array_index( b, i ) );

        if ( l && !strcmp( l, etiqueta ) ) r = g_ptr_array_index( b, i );
        }
    g_ptr_array_free( b, TRUE );
    return r;
}

/* Todo el texto de las etiquetas de una ventana, junto. */
static gchar *textos_de( GtkWidget *raiz )
{
    GPtrArray *l = junta( raiz, GTK_TYPE_LABEL, FALSE );
    GString   *g = g_string_new( NULL );
    guint      i;

    for ( i = 0; i < l->len; i++ )
        {
        const char *t = gtk_label_get_text( g_ptr_array_index( l, i ) );

        if ( t && *t ) g_string_append_printf( g, "%s\n", t );
        }
    g_ptr_array_free( l, TRUE );
    return g_string_free( g, FALSE );
}

/* La ventana de nivel superior visible cuyo titulo contiene <t>. */
static GtkWindow *ventana_titulada( const char *t )
{
    GList     *l, *todas = gtk_window_list_toplevels();
    GtkWindow *r = NULL;

    for ( l = todas; l && !r; l = l->next )
        {
        const char *ti = gtk_window_get_title( l->data );

        if ( gtk_widget_get_visible( l->data ) && ti && strstr( ti, t ) )
            r = l->data;
        }
    g_list_free( todas );
    return r;
}

static int cuenta_visibles( void )
{
    GList *l, *todas = gtk_window_list_toplevels();
    int    n = 0;

    for ( l = todas; l; l = l->next )
        if ( gtk_widget_get_visible( l->data ) &&
             gtk_window_get_window_type( l->data ) == GTK_WINDOW_TOPLEVEL )
            n++;
    g_list_free( todas );
    return n;
}

/* Las filas de una lista, columna c, separadas por '|'. */
static gchar *filas( GtkWidget *tv, int c )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter   it;
    GString      *g = g_string_new( NULL );

    if ( gtk_tree_model_get_iter_first( mo, &it ) )
        do {
            gchar *s = NULL;

            gtk_tree_model_get( mo, &it, c, &s, -1 );
            g_string_append_printf( g, "%s%s", g->len ? "|" : "", s ? s : "" );
            g_free( s );
        } while ( gtk_tree_model_iter_next( mo, &it ) );
    return g_string_free( g, FALSE );
}

/* Marca la fila cuya columna c vale v, como un clic. TRUE si estaba. */
static gboolean marca( GtkWidget *tv, int c, const char *v )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter   it;

    if ( gtk_tree_model_get_iter_first( mo, &it ) )
        do {
            gchar *s = NULL;

            gtk_tree_model_get( mo, &it, c, &s, -1 );
            if ( s && !strcmp( s, v ) )
                {
                GtkTreePath *p = gtk_tree_model_get_path( mo, &it );

                gtk_tree_view_set_cursor( GTK_TREE_VIEW(tv), p, NULL, FALSE );
                gtk_tree_path_free( p );
                g_free( s );
                pump( 20 );
                return TRUE;
                }
            g_free( s );
        } while ( gtk_tree_model_iter_next( mo, &it ) );
    return FALSE;
}

/* El valor de la columna c en la fila cuya columna 'clave' vale v. */
static gchar *celda( GtkWidget *tv, int clave, const char *v, int c )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter   it;

    if ( gtk_tree_model_get_iter_first( mo, &it ) )
        do {
            gchar *s = NULL, *r = NULL;

            gtk_tree_model_get( mo, &it, clave, &s, -1 );
            if ( s && !strcmp( s, v ) )
                { gtk_tree_model_get( mo, &it, c, &r, -1 ); g_free( s );
                  return r; }
            g_free( s );
        } while ( gtk_tree_model_iter_next( mo, &it ) );
    return NULL;
}

static gchar *ruta_de( const char *serie, const char *muestra,
                       const char *id, const char *ext )
{
    char f[PR_RUTA];

    if ( pr_ruta( A.p, serie, muestra, id, ext, f, sizeof f ) != 0 )
        return g_strdup( "" );
    return g_strdup( f );
}

static gboolean existe( const char *serie, const char *muestra,
                        const char *id, const char *ext )
{
    gchar   *f = ruta_de( serie, muestra, id, ext );
    gboolean r = g_file_test( f, G_FILE_TEST_EXISTS );

    g_free( f );
    return r;
}

/* Sin \r: en Windows la madre escribe los ficheros de texto con \r\n, y lo
   que se compara es el contenido, no el fin de linea de la plataforma (el
   motor lee los dos). Igual en texto_editor(), para comparar lo mismo.  */
static void sin_cr( gchar *c )
{
    gchar *w = c;

    if ( c == NULL ) return;
    for ( ; *c; c++ ) if ( *c != '\r' ) *w++ = *c;
    *w = '\0';
}

static gchar *lee( const char *f )
{
    gchar *c = NULL;

    if ( !g_file_get_contents( f, &c, NULL, NULL ) ) return g_strdup( "" );
    sin_cr( c );
    return c;
}

/* ------------------------------------------------------------------------ */
/* LOS GRAFICOS DIBUJAN DE VERDAD                                            */
/*                                                                           */
/* El area de la ventana de graficos se pinta en una superficie de cairo y   */
/* se cuentan los pixeles que no son del color de fondo. Una pagina en blanco */
/* --un EPS que no se leyo, un on_draw que no pinta-- da cero.               */
/* ------------------------------------------------------------------------ */

typedef struct { double pagina; int oscuros; } Tinta;

static Tinta tinta( GtkWidget *area )
{
    GtkAllocation    al;
    cairo_surface_t *s;
    cairo_t         *cr;
    unsigned char   *px;
    int              x, y, st, n = 0;
    guint32          fondo;
    Tinta            t = { -1.0, 0 };

    gtk_widget_get_allocation( area, &al );
    if ( al.width < 10 || al.height < 10 ) return t;

    s  = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, al.width, al.height );
    cr = cairo_create( s );
    cairo_set_source_rgb( cr, 1, 1, 1 );
    cairo_paint( cr );
    gtk_widget_draw( area, cr );
    cairo_destroy( cr );
    cairo_surface_flush( s );
    if ( g_getenv( "ATSW_PNG" ) )
        cairo_surface_write_to_png( s, g_getenv( "ATSW_PNG" ) );

    px = cairo_image_surface_get_data( s );
    st = cairo_image_surface_get_stride( s );
    /* El fondo es el de una esquina: el gris del margen alrededor de la
       pagina, o el blanco si no se pinto nada. Lo que no es fondo es la
       pagina; lo oscuro dentro de ella, la tinta del grafico.          */
    fondo = *(guint32 *) px;
    for ( y = 0; y < al.height; y++ )
        for ( x = 0; x < al.width; x++ )
            {
            guint32 c = *(guint32 *) ( px + y * st + 4 * x );

            if ( c == fondo ) continue;
            n++;
            if ( ( ( c >> 16 ) & 0xff ) < 110 && ( ( c >> 8 ) & 0xff ) < 110 &&
                 ( c & 0xff ) < 110 )
                t.oscuros++;
            }
    cairo_surface_destroy( s );
    t.pagina = (double) n / ( (double) al.width * al.height );
    return t;
}

/* La ventana de graficos que enseña <base> (lib/preview le pone el nombre
   del fichero de titulo y el rol «atsw-graph»). */
static GtkWindow *grafico( const char *base )
{
    GList     *l, *todas = gtk_window_list_toplevels();
    GtkWindow *r = NULL;

    for ( l = todas; l && !r; l = l->next )
        {
        const char *ro = gtk_window_get_role( l->data );
        const char *ti = gtk_window_get_title( l->data );

        if ( ro && !strcmp( ro, "atsw-graph" ) && ti && strstr( ti, base ) &&
             gtk_widget_get_visible( l->data ) )
            r = l->data;
        }
    g_list_free( todas );
    return r;
}

/* Se agranda la ventana antes de mirar: con el tamaño que el gestor de
   ventanas le de --en Xvfb no hay gestor-- la pagina puede salir de sello. */
static Tinta tinta_de( GtkWindow *w )
{
    GPtrArray *a;
    Tinta      t = { -1.0, 0 };

    if ( w == NULL ) return t;
    gtk_window_resize( w, 900, 700 );
    pump( 400 );                       /* que se asigne el tamaño */
    a = junta( GTK_WIDGET(w), GTK_TYPE_DRAWING_AREA, FALSE );
    if ( a->len ) t = tinta( g_ptr_array_index( a, 0 ) );
    g_ptr_array_free( a, TRUE );
    return t;
}

/* Que dibuja: una pagina que ocupa sitio y trazos encima. */
static void dibuja_de_verdad( Tinta t, const char *que )
{
    gchar *v = g_strdup_printf( "pagina %.1f%% del area, %d pixeles de tinta",
                                100 * t.pagina, t.oscuros );

    check( t.pagina > 0.05 && t.oscuros > 300, que, v );
    nota( "%s: %s", que, v );
    g_free( v );
}

/* ------------------------------------------------------------------------ */
/* QUIEN CONTESTA A LOS DIALOGOS                                             */
/*                                                                           */
/* Se encola ANTES de pulsar: el boton entra en gtk_dialog_run y no vuelve.  */
/* El temporizador corre dentro del bucle anidado del dialogo, encuentra la  */
/* ventana por su titulo --o por el texto, en un GtkMessageDialog, que no    */
/* lleva titulo-- y llama a quien la atiende. Si la atencion devuelve FALSE  */
/* es que todavia no esta lista (un selector de ficheros que carga la        */
/* carpeta) y se vuelve a intentar.                                          */
/*                                                                           */
/* Un dialogo que no aparece es un FAIL, no un cuelgue: a los diez segundos  */
/* se da por perdido. Y un dialogo que aparece sin que nadie lo espere se    */
/* cancela para que la prueba siga, y tambien es un FAIL.                    */
/* ------------------------------------------------------------------------ */

typedef gboolean (*Atiende)( GtkWindow *w, gpointer d );

typedef struct {
    char     que[128];
    Atiende  fn;
    gpointer d;
    gint64   desde;
    gboolean hecho;                /* la respuesta paso de verdad         */
    char     visto[2048];          /* el texto del dialogo, para despues  */
} Turno;

/* Los dialogos ya contestados. Siguen visibles hasta que su gtk_dialog_run
   vuelve, y en ese rato no son «un dialogo que nadie esperaba».       */
static GHashTable *contestados;

static GQueue   turnos = G_QUEUE_INIT;
static Turno    ultimo;            /* el ultimo atendido                  */
static Turno   *atendiendo;        /* el que se esta atendiendo ahora     */

static void responde( const char *que, Atiende fn, gpointer d )
{
    Turno *t = g_new0( Turno, 1 );

    g_snprintf( t->que, sizeof t->que, "%s", que );
    t->fn = fn;
    t->d  = d;
    g_queue_push_tail( &turnos, t );
}

static gboolean es_el( GtkWindow *w, const char *que )
{
    const char *ti = gtk_window_get_title( w );
    gchar      *tx = NULL;
    gboolean    r;

    if ( ti && strstr( ti, que ) ) return TRUE;
    if ( !GTK_IS_MESSAGE_DIALOG(w) ) return FALSE;
    g_object_get( w, "text", &tx, NULL );
    r = tx && strstr( tx, que );
    g_free( tx );
    return r;
}


static gboolean vigila( gpointer d )
{
    static gboolean dentro;
    Turno *t = g_queue_peek_head( &turnos );
    GList *l, *todas;

    (void) d;
    /* NO REENTRANTE. Una atencion que bombea el bucle volveria a entrar
       aqui con el mismo turno a medio atender, y lo atenderia dos veces. */
    if ( dentro ) return G_SOURCE_CONTINUE;
    dentro = TRUE;

    /* Uno cuya respuesta paso mientras no se miraba (la de un selector de
       ficheros pasa cuando GIO acaba, no cuando se pulsa).            */
    if ( t && t->hecho )
        {
        ultimo = *t;
        g_free( g_queue_pop_head( &turnos ) );
        t = g_queue_peek_head( &turnos );
        }
    todas = gtk_window_list_toplevels();

    if ( t )
        {
        if ( t->desde == 0 ) t->desde = g_get_monotonic_time();
        for ( l = todas; l; l = l->next )
            {
            GtkWindow *w = l->data;
            gchar     *tx;

            if ( !gtk_widget_get_visible( GTK_WIDGET(w) ) ) continue;
            if ( !es_el( w, t->que ) ) continue;

            tx = textos_de( GTK_WIDGET(w) );
            g_snprintf( t->visto, sizeof t->visto, "%s", tx );
            g_free( tx );
            atendiendo = t;
            if ( t->fn( w, t->d ) || t->hecho )
                {
                ultimo = *t;
                g_hash_table_add( contestados, w );
                g_free( g_queue_pop_head( &turnos ) );
                }
            g_list_free( todas );
            dentro = FALSE;
            return G_SOURCE_CONTINUE;
            }
        if ( g_get_monotonic_time() - t->desde > 10 * G_USEC_PER_SEC )
            {
            check( 0, "tenia que aparecer un dialogo", t->que );
            g_free( g_queue_pop_head( &turnos ) );
            }
        }

    /* Un dialogo modal que nadie espera: se cancela, o la prueba se
       quedaria colgada en su gtk_dialog_run.                          */
    if ( g_queue_is_empty( &turnos ) )
        for ( l = todas; l; l = l->next )
            if ( GTK_IS_DIALOG(l->data) &&
                 gtk_widget_get_visible( l->data ) &&
                 !g_hash_table_contains( contestados, l->data ) &&
                 gtk_window_get_modal( l->data ) )
                {
                gchar *tx = textos_de( l->data );
                const char *ti = gtk_window_get_title( l->data );

                check( 0, "aparecio un dialogo que nadie esperaba",
                       ti ? ti : tx );
                g_free( tx );
                g_hash_table_add( contestados, l->data );
                gtk_dialog_response( l->data, GTK_RESPONSE_CANCEL );
                break;
                }
    g_list_free( todas );
    dentro = FALSE;
    return G_SOURCE_CONTINUE;
}

/* ¿Se atendio todo lo encolado? Si no, se tira la cola y se dice. */
static void todo_atendido( const char *gesto )
{
    pump( 100 );
    if ( !g_queue_is_empty( &turnos ) )
        {
        Turno *t = g_queue_peek_head( &turnos );

        check( 0, gesto, t->que );
        g_queue_clear_full( &turnos, g_free );
        }
}

/* --- las atenciones de siempre ------------------------------------------ */

static gboolean contesta( GtkWindow *w, gpointer d )
{
    gtk_dialog_response( GTK_DIALOG(w), GPOINTER_TO_INT(d) );
    return TRUE;
}

/* LOS SELECTORES DE FICHEROS NO OBEDECEN A LA PRIMERA.
 *
 * GtkFileChooserDialog intercepta su propio «aceptar»: antes de dejarlo
 * pasar comprueba el fichero --por detras, con GIO-- y, si todavia esta
 * cargando la carpeta, se lo traga sin decir nada. Asi que no basta con
 * contestar una vez: se insiste hasta que la respuesta pasa de verdad, y
 * eso se sabe escuchando "response" DESPUES del selector, que es donde
 * solo llega una respuesta que no se trago.                            */
static void on_paso( GtkDialog *d, gint r, gpointer x )
{
    Turno *t = x;

    (void) r;
    g_object_set_data( G_OBJECT(d), "atsw-paso", GINT_TO_POINTER(1) );
    g_hash_table_add( contestados, d );
    /* Si sigue siendo el de la cabeza, vigila() lo quitara. */
    if ( t == g_queue_peek_head( &turnos ) ) t->hecho = TRUE;
}

static gboolean insiste( GtkWindow *w, int resp )
{
    gint64 *ultima = g_object_get_data( G_OBJECT(w), "atsw-ultima" );

    if ( g_object_get_data( G_OBJECT(w), "atsw-paso" ) ) return TRUE;
    if ( ultima == NULL )
        {
        ultima = g_new0( gint64, 1 );
        g_object_set_data_full( G_OBJECT(w), "atsw-ultima", ultima, g_free );
        g_signal_connect_after( w, "response", G_CALLBACK(on_paso), atendiendo );
        }
    if ( g_get_monotonic_time() - *ultima > 700 * 1000 )
        {
        *ultima = g_get_monotonic_time();
        gtk_dialog_response( GTK_DIALOG(w), resp );
        }
    return g_object_get_data( G_OBJECT(w), "atsw-paso" ) != NULL;
}

/* Un selector de fichero PARA ABRIR: se pone el fichero, se deja que lo
   cargue y se insiste en aceptar.                                      */
static gboolean abre_fichero( GtkWindow *w, gpointer d )
{
    if ( !g_object_get_data( G_OBJECT(w), "atsw-puesto" ) )
        {
        gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(w), d );
        g_object_set_data( G_OBJECT(w), "atsw-puesto", GINT_TO_POINTER(1) );
        return FALSE;
        }
    return insiste( w, GTK_RESPONSE_ACCEPT );
}

/* Un selector PARA GUARDAR: carpeta y nombre, y se insiste. */
static gboolean guarda_como( GtkWindow *w, gpointer d )
{
    if ( !g_object_get_data( G_OBJECT(w), "atsw-puesto" ) )
        {
        gchar *dir = g_path_get_dirname( d ), *base = g_path_get_basename( d );

        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(w), dir );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(w), base );
        g_free( dir ); g_free( base );
        g_object_set_data( G_OBJECT(w), "atsw-puesto", GINT_TO_POINTER(1) );
        return FALSE;
        }
    return insiste( w, GTK_RESPONSE_ACCEPT );
}

/* Rellenar un formulario de atsw_fila --etiqueta en la columna 0, entrada
 * en la 1-- POR FILA, y contestar. Un NULL deja esa fila como estaba.
 *
 * Por fila y no por orden de aparicion: una GtkGrid devuelve sus hijos al
 * reves de como se pusieron, y el «titulo» acababa en el identificador. */
typedef struct { const char *v[8]; int resp; } Relleno;

static gboolean rellena( GtkWindow *w, gpointer d )
{
    Relleno   *r = d;
    GPtrArray *g = junta( GTK_WIDGET(w), GTK_TYPE_GRID, FALSE );
    int        i;

    if ( g->len )
        for ( i = 0; i < 8; i++ )
            {
            GtkWidget *e = gtk_grid_get_child_at( g_ptr_array_index( g, 0 ), 1, i );

            if ( r->v[i] && e && GTK_IS_ENTRY(e) )
                gtk_entry_set_text( GTK_ENTRY(e), r->v[i] );
            }
    g_ptr_array_free( g, TRUE );
    gtk_dialog_response( GTK_DIALOG(w), r->resp );
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* EL BOTON DERECHO, sintetizado                                             */
/*                                                                           */
/* Los menus de la serie y del modelo salen de un clic derecho sobre la fila.*/
/* Se fabrica el evento en el sitio de la fila, se le da a la lista y se      */
/* busca el menu que se abrio. Asi se prueba que el menu existe y dice lo que */
/* tiene que decir, y sus entradas se activan como las activaria el raton.   */
/* ------------------------------------------------------------------------ */

static GtkMenu *menu_abierto( void )
{
    GList   *l, *todas = gtk_window_list_toplevels();
    GtkMenu *m = NULL;

    for ( l = todas; l && !m; l = l->next )
        {
        GtkWidget *hijo;

        if ( !gtk_widget_get_visible( l->data ) ) continue;
        hijo = gtk_bin_get_child( GTK_BIN(l->data) );
        if ( hijo && GTK_IS_MENU(hijo) ) m = GTK_MENU(hijo);
        }
    g_list_free( todas );
    return m;
}

static GtkMenu *clic_derecho( GtkWidget *tv, int fila )
{
    GtkTreePath  *p = gtk_tree_path_new_from_indices( fila, -1 );
    GdkRectangle  r;
    GdkEvent     *ev;
    gboolean      tomado = FALSE;
    GdkSeat      *seat = gdk_display_get_default_seat( gdk_display_get_default() );

    gtk_tree_view_get_cell_area( GTK_TREE_VIEW(tv), p, NULL, &r );
    gtk_tree_path_free( p );

    ev = gdk_event_new( GDK_BUTTON_PRESS );
    ev->button.window = g_object_ref( gtk_tree_view_get_bin_window( GTK_TREE_VIEW(tv) ) );
    ev->button.x      = r.x + 4;
    ev->button.y      = r.y + r.height / 2;
    ev->button.button = 3;
    ev->button.time   = GDK_CURRENT_TIME;
    if ( seat ) gdk_event_set_device( ev, gdk_seat_get_pointer( seat ) );

    g_signal_emit_by_name( tv, "button-press-event", ev, &tomado );
    gdk_event_free( ev );
    pump( 100 );
    return tomado ? menu_abierto() : NULL;
}

static GtkWidget *entrada_de( GtkMenu *m, const char *empieza )
{
    GList     *l, *hijos = gtk_container_get_children( GTK_CONTAINER(m) );
    GtkWidget *r = NULL;

    for ( l = hijos; l && !r; l = l->next )
        if ( GTK_IS_MENU_ITEM(l->data) )
            {
            const char *t = gtk_menu_item_get_label( l->data );

            if ( t && g_str_has_prefix( t, empieza ) ) r = l->data;
            }
    g_list_free( hijos );
    return r;
}

static gchar *entradas( GtkMenu *m )
{
    GList   *l, *hijos = gtk_container_get_children( GTK_CONTAINER(m) );
    GString *g = g_string_new( NULL );

    for ( l = hijos; l; l = l->next )
        if ( GTK_IS_MENU_ITEM(l->data) && gtk_menu_item_get_label( l->data ) )
            g_string_append_printf( g, "%s«%s»", g->len ? " " : "",
                                    gtk_menu_item_get_label( l->data ) );
    g_list_free( hijos );
    return g_string_free( g, FALSE );
}

static void cierra_menu( GtkMenu *m )
{
    if ( m ) gtk_menu_popdown( m );
    pump( 50 );
}

/* ------------------------------------------------------------------------ */
/* El hijo falso                                                              */
/* ------------------------------------------------------------------------ */

/* Espera a que el hijo escriba y devuelve sus argumentos (g_strfreev). */
static gchar **lo_que_recibio( void )
{
    gint64 hasta = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;

    while ( g_get_monotonic_time() < hasta )
        {
        gchar *c = NULL;

        if ( g_file_get_contents( HIJO_LOG, &c, NULL, NULL ) &&
             strstr( c, "FIN\n" ) )
            {
            gchar **v = g_strsplit( c, "\n", -1 );

            g_free( c );
            g_unlink( HIJO_LOG );
            return v;
            }
        g_free( c );
        pump( 50 );
        }
    return NULL;
}

/* Lo que la barra dice al lanzar <exe> con el .inp de ipc/<id>: la ruta
   del binario --para que una instalacion vieja se vea-- y el fichero. */
static gchar *lanzado( const char *exe, const char *id )
{
    gchar *f = ruta_de( "ipc", "", id, ".inp" );
    gchar *b = g_path_get_basename( f );
    gchar *q = g_strdup_printf( "%s, con %s.", exe ? exe : "(no encontrado)", b );

    g_free( f ); g_free( b );
    return q;
}

/* LOS HERMANOS DE VERDAD, SIN VENTANA. Se lanzan --es lo que se prueba: que
 * se encuentran y que el proceso arranca-- con un GDK_BACKEND que no existe,
 * asi que abren, no encuentran pantalla y se van. Ni se ven ni se quedan
 * vivos despues de la prueba, en ninguna de las tres plataformas.        */
static void hijos_sin_pantalla( gboolean si )
{
    static gchar *antes;
    static gboolean guardado;

    if ( si )
        {
        g_free( antes );
        antes = g_strdup( g_getenv( "GDK_BACKEND" ) );
        guardado = TRUE;
        g_setenv( "GDK_BACKEND", "ninguno_atsw_prueba", TRUE );
        }
    else if ( guardado )
        {
        if ( antes ) g_setenv( "GDK_BACKEND", antes, TRUE );
        else         g_unsetenv( "GDK_BACKEND" );
        guardado = FALSE;
        }
}

/* ======================================================================== */
/* LAS PRUEBAS                                                               */
/* ======================================================================== */

/* SIN PROYECTO: cada gesto dice que falta uno, ninguno revienta. */
static void sin_proyecto( void )
{
    gchar *t;

    check( !strcmp( gtk_label_get_text( GTK_LABEL(A.l_proy) ), "(sin proyecto)" ),
           "sin proyecto, la cabecera lo dice",
           gtk_label_get_text( GTK_LABEL(A.l_proy) ) );
    t = g_strdup( gtk_label_get_text( GTK_LABEL(A.ver_cuenta) ) );
    check( strstr( t, "No hay proyecto abierto" ) != NULL,
           "y el veredicto dice que hay que abrir uno", t );
    g_free( t );
    check( !gtk_widget_get_sensitive( A.b_drtran ) &&
           !gtk_widget_get_sensitive( A.b_fue ) &&
           !gtk_widget_get_sensitive( A.b_nuevo ),
           "sin proyecto, los envios estan apagados", NULL );

    on_datos( NULL, &A );
    check( strstr( barra(), "Abre un proyecto antes" ) != NULL,
           "«Datos…» sin proyecto pide uno", barra() );
    on_linaje( NULL, &A );
    check( strstr( barra(), "No hay proyecto abierto" ) != NULL,
           "«Linaje…» sin proyecto lo dice", barra() );
    on_fug( NULL, &A );
    check( strstr( barra(), "Abre un proyecto antes" ) != NULL,
           "«→ fug» sin proyecto lo dice", barra() );
    on_muestra_nueva( NULL, &A );
    check( strstr( barra(), "Abre un proyecto antes" ) != NULL,
           "«+» sin proyecto lo dice", barra() );
    on_proyecto( NULL, &A );
    check( strstr( barra(), "Abre un proyecto antes" ) != NULL,
           "«Proyecto…» sin proyecto lo dice", barra() );
}

/* LOS HERMANOS, EN EL ARBOL DE COMPILACION.
 *
 * Es el arreglo de portabilidad: atsw_programa() encuentra fue_gui, gtk_fmg
 * y drtran_gui --y los motores que el editor y el vistazo corren-- AL LADO
 * de la madre, relativo a su ejecutable, en las tres plataformas (con .exe
 * en Windows). Y los encuentra ANTES que el PATH: el PATH de la bateria
 * tiene los motores, y aun asi lo que tiene que salir es la ruta del arbol.
 *
 * Los demas agentes pueden estar recompilando un hermano justo ahora: si
 * falta, se reintenta unos segundos antes de darlo por perdido.         */
static void hermanos( void )
{
    static const struct { const char *prog, *donde; } h[] = {
        { "fue_gui",    "../fue/bin" },
        { "gtk_fmg",    "../fug" },
        { "drtran_gui", "../drtran" },
        { "fue",        "../../engines/fue/bin" },
        { "fug",        "../../engines/fug" },
        { "fuf",        "../../engines/fuf/bin" },
    };
    gchar *yo = sitio_mi_dir();
    int    i;

    check( yo != NULL, "sitio_mi_dir sabe donde esta el ejecutable", NULL );
    if ( yo == NULL ) return;

    for ( i = 0; i < (int) G_N_ELEMENTS(h); i++ )
        {
        gchar *exe  = sitio_exe( h[i].prog );
        gchar *rel  = g_build_filename( yo, h[i].donde, exe, NULL );
        gchar *got  = NULL;
        int    k;

        /* Las barras se igualan antes de comparar: en Windows
           g_build_filename mezcla '\\' y '/', y la ruta es la misma. */
        g_strdelimit( rel, "\\", '/' );
        for ( k = 0; k < 40; k++ )
            {
            g_free( got );
            got = atsw_programa( h[i].prog );
            if ( got ) g_strdelimit( got, "\\", '/' );
            if ( got && !strcmp( got, rel ) ) break;
            g_usleep( 250000 );
            }
        {
        gchar *q = g_strdup_printf( "atsw_programa(\"%s\") lo encuentra en "
                                    "el arbol, en %s", h[i].prog, rel );

        check( got && !strcmp( got, rel ), q, got );
        g_free( q );
        }
        check( got && g_file_test( got, G_FILE_TEST_IS_EXECUTABLE ),
               "y lo que devuelve es un ejecutable", got );
        g_free( got ); g_free( rel ); g_free( exe );
        }

    {
    gchar *no = atsw_programa( "atsw_no_existe_ningun_programa" );

    check( no == NULL, "un programa que no esta en ningun sitio: NULL", no );
    g_free( no );
    }
    g_free( yo );
}

/* «Nuevo…»: el selector de guardar, y el manifiesto escrito. */
static void proyecto_nuevo( char *manifiesto, size_t n )
{
    gchar *f = g_build_filename( T, "prueba.yaml", NULL );

    /* LO QUE NO SE PISA. Paso de verdad: «Nuevo…» sobre ipc_wti.csv dejo el
       manifiesto vacio encima de las series.                           */
    {
    const char *datos = "date,ipc,wti\n2002-01-01,69.53,19.67\n";
    gchar *csv  = g_build_filename( T, "datos.csv", NULL );
    gchar *yml  = g_build_filename( T, "otro.yaml", NULL );
    gchar *c    = NULL;

    g_file_set_contents( csv, datos, -1, NULL );
    responde( "Proyecto nuevo", guarda_como, csv );
    on_nuevo( NULL, &A );
    todo_atendido( "«Nuevo…» tenia que preguntar donde" );
    g_file_get_contents( csv, &c, NULL, NULL );
    check( !A.hay && c && !strcmp( c, datos ),
           "«Nuevo…» sobre un CSV no crea nada y los datos siguen intactos", c );
    check( strstr( barra(), "no es un fichero .yaml" ) != NULL,
           "y dice por que", barra() );
    g_free( c ); c = NULL;

    g_file_set_contents( yml, "esto: [no es un manifiesto\n", -1, NULL );
    responde( "Proyecto nuevo", guarda_como, yml );
    on_nuevo( NULL, &A );
    todo_atendido( "«Nuevo…» tenia que preguntar donde" );
    g_file_get_contents( yml, &c, NULL, NULL );
    check( !A.hay && c && strstr( c, "no es un manifiesto" ),
           "un .yaml que no es un proyecto tampoco se pisa", c );
    check( strstr( barra(), "NO es un proyecto" ) != NULL, "y lo dice", barra() );
    g_free( c ); g_free( csv ); g_free( yml );
    }

    g_snprintf( manifiesto, n, "%s", f );
    responde( "Proyecto nuevo", guarda_como, f );
    on_nuevo( NULL, &A );
    todo_atendido( "«Nuevo…» tenia que preguntar donde" );

    check( A.hay, "despues de «Nuevo…» hay proyecto", NULL );
    check( g_file_test( f, G_FILE_TEST_EXISTS ),
           "«Nuevo…» escribe el manifiesto donde se dijo", f );
    check( strstr( barra(), "Proyecto nuevo" ) != NULL,
           "y la barra lo dice", barra() );
    check( strstr( gtk_label_get_text( GTK_LABEL(A.l_proy) ), "prueba" ) != NULL,
           "y la cabecera nombra el proyecto",
           gtk_label_get_text( GTK_LABEL(A.l_proy) ) );
    check( gtk_widget_get_sensitive( A.b_drtran ),
           "con proyecto, «drtran» se enciende", NULL );

    /* Sobre un proyecto que ya existe, se pregunta; cancelar no toca nada. */
    {
    gchar *antes = NULL, *despues = NULL;

    g_file_get_contents( f, &antes, NULL, NULL );
    responde( "Proyecto nuevo", guarda_como, f );
    responde( "¿Reemplazo el proyecto", contesta, GINT_TO_POINTER(GTK_RESPONSE_CANCEL) );
    on_nuevo( NULL, &A );
    todo_atendido( "reemplazar un proyecto tenia que preguntar" );
    g_file_get_contents( f, &despues, NULL, NULL );
    check( antes && despues && !strcmp( antes, despues ),
           "si no se confirma, el proyecto que habia sigue igual", NULL );
    g_free( antes ); g_free( despues );
    }
    g_free( f );
}

/* «Proyecto…»: titulo y analista, guardados en el manifiesto. */
static void proyecto_info( const char *manifiesto )
{
    static Relleno r = { { NULL, "Prueba de la madre", "el banco" },
                         GTK_RESPONSE_OK };
    gchar *c;

    responde( "Información del proyecto", rellena, &r );
    on_proyecto( NULL, &A );
    todo_atendido( "«Proyecto…» tenia que abrir su dialogo" );

    check( strstr( ultimo.visto, "Manifiesto:" ) != NULL,
           "el dialogo enseña donde esta el manifiesto", ultimo.visto );
    check( !strcmp( barra(), "Proyecto guardado." ),
           "«Proyecto…» guarda y lo dice", barra() );
    check( strstr( gtk_label_get_text( GTK_LABEL(A.l_proy) ),
                   "Prueba de la madre" ) != NULL,
           "el titulo sale en la cabecera",
           gtk_label_get_text( GTK_LABEL(A.l_proy) ) );
    c = lee( manifiesto );
    check( strstr( c, "Prueba de la madre" ) && strstr( c, "el banco" ),
           "y queda escrito en el manifiesto", c );
    g_free( c );
}

/* «Datos…»: un fichero roto se dice; el bueno da n series con su m00. */
typedef struct { int filas; gboolean pulsado; char aviso[512]; } Carga;

static gboolean carga_marcadas( GtkWindow *w, gpointer d )
{
    Carga     *c = d;
    GPtrArray *tv = junta( GTK_WIDGET(w), GTK_TYPE_TREE_VIEW, FALSE );
    GtkWidget *b = boton_que_dice( GTK_WIDGET(w), "Cargar las marcadas" );

    if ( tv->len )
        c->filas = gtk_tree_model_iter_n_children(
            gtk_tree_view_get_model( g_ptr_array_index( tv, 0 ) ), NULL );
    g_ptr_array_free( tv, TRUE );
    g_snprintf( c->aviso, sizeof c->aviso, "%s", atendiendo->visto );
    if ( b ) { gtk_button_clicked( GTK_BUTTON(b) ); c->pulsado = TRUE; }
    gtk_dialog_response( GTK_DIALOG(w), GTK_RESPONSE_CLOSE );
    return TRUE;
}

static void datos( const char *csv )
{
    gchar *roto = g_build_filename( T, "roto.xlsx", NULL );
    Carga  c = { 0 };
    gchar *s, *f;

    /* Un .xlsx que no es un zip: se dice, y no entra ninguna serie. */
    g_file_set_contents( roto, "esto no es un libro de calculo\n", -1, NULL );
    olvida();
    responde( "Cargar datos", abre_fichero, roto );
    on_datos( NULL, &A );
    todo_atendido( "«Datos…» tenia que abrir el selector" );
    check( barra()[0] != '\0' && A.p->ns == 0,
           "un fichero de datos roto se dice en la barra y no carga nada",
           barra() );
    nota( "barra (datos rotos): %s", barra() );

    /* El bueno: dos columnas, mensual desde 2002. */
    olvida();
    responde( "Cargar datos", abre_fichero, (gpointer) csv );
    responde( "Qué series cargar", carga_marcadas, &c );
    on_datos( NULL, &A );
    todo_atendido( "«Datos…» tenia que abrir el selector y la lista" );

    check( c.filas == 2, "la lista de columnas tiene las dos del fichero",
           c.aviso );
    check( strstr( c.aviso, "frecuencia 12" ) != NULL,
           "y antes de cargar dice la frecuencia que leyo", c.aviso );
    check( c.pulsado, "el boton «Cargar las marcadas» esta", NULL );
    check( strstr( barra(), "2 series del proyecto" ) != NULL,
           "cargar dice cuantas series entraron", barra() );

    s = filas( A.l_series, S_ID );
    check( !strcmp( s, "ipc|wti" ), "la lista de series tiene las dos", s );
    g_free( s );
    check( existe( "ipc", "", "m00", ".inp" ) && existe( "wti", "", "m00", ".inp" ),
           "cada serie tiene el .inp de sus datos (m00)", NULL );
    f = ruta_de( "ipc", "", NULL, NULL );
    {
    gchar *csv2 = g_build_filename( f, "datos.csv", NULL );

    check( g_file_test( csv2, G_FILE_TEST_EXISTS ),
           "y su datos.csv, fuera de work/", csv2 );
    g_free( csv2 );
    }
    g_free( f );
    s = g_strdup( gtk_label_get_text( GTK_LABEL(A.ver_cuenta) ) );
    check( strstr( s, "2 series, 0 modelos" ) != NULL,
           "el veredicto cuenta dos series y ningun modelo", s );
    g_free( s );

    /* Otra vez lo mismo: no se pisan datos que ya hay. */
    c.pulsado = FALSE;
    responde( "Cargar datos", abre_fichero, (gpointer) csv );
    responde( "Qué series cargar", carga_marcadas, &c );
    on_datos( NULL, &A );
    todo_atendido( "la segunda carga tenia que abrir sus dialogos" );
    check( strstr( barra(), "ya tiene datos" ) != NULL,
           "cargar encima de una serie con datos se niega y lo dice", barra() );
    check( A.p->ns == 2, "y no aparecen series de mas", NULL );
    g_free( roto );
}

/* La lista de series: marcar llena la rejilla, el boton derecho ofrece. */
static void serie( void )
{
    GtkMenu *m;
    gchar   *s;

    check( marca( A.l_series, S_ID, "ipc" ), "se puede marcar «ipc»", NULL );
    check( !strcmp( A.serie, "ipc" ), "marcarla la hace LA serie", A.serie );

    s = filas( A.l_modelos, M_ID );
    check( !strcmp( s, "m00" ), "la rejilla enseña los datos (m00)", s );
    g_free( s );
    s = celda( A.l_modelos, M_ID, "m00", M_ESTRUCT );
    check( s && !strcmp( s, "los datos" ), "y dice que son los datos", s );
    g_free( s );
    check( gtk_widget_get_sensitive( A.b_nuevo ),
           "con datos, «Modelo nuevo» se enciende", NULL );

    s = celda( A.l_series, S_ID, "ipc", S_GLOBO );
    check( s && strstr( s, "no consta nada" ),
           "el globo de una serie sin ficha lo dice", s );
    g_free( s );

    /* El menu de la serie, con el boton derecho. */
    m = clic_derecho( A.l_series, 0 );
    check( m != NULL, "el boton derecho sobre la serie abre su menu", NULL );
    if ( m )
        {
        gchar *e = entradas( m );

        check( strstr( e, "Identificación con fug" ) &&
               strstr( e, "Especificar el primer modelo con fue" ) &&
               strstr( e, "Editar la serie…" ) &&
               strstr( e, "Media – desviación típica" ) &&
               strstr( e, "Serie y ACF / PACF…" ),
               "el menu de la serie ofrece lo que se puede hacer", e );
        g_free( e );

        /* «Editar la serie…», desde el menu: la ficha se guarda. */
        {
        static Relleno r = { { "IPC de prueba", "índice" }, GTK_RESPONSE_OK };
        GtkWidget *mi = entrada_de( m, "Editar la serie" );

        responde( "Editar la serie", rellena, &r );
        if ( mi ) gtk_menu_item_activate( GTK_MENU_ITEM(mi) );
        cierra_menu( m );
        todo_atendido( "«Editar la serie…» tenia que abrir la ficha" );
        }
        check( !strcmp( barra(), "ipc: guardado." ),
               "la ficha de la serie se guarda y lo dice", barra() );
        s = celda( A.l_series, S_ID, "ipc", S_GLOBO );
        check( s && strstr( s, "IPC de prueba" ) && strstr( s, "índice" ),
               "y el globo de la serie lo enseña", s );
        g_free( s );
        }
}

/* EL VISTAZO: escribe un .inp en la cache, llama a fug y enseña su EPS. */
static void vistazo_prueba( void )
{
    GtkMenu   *m;
    GtkWindow *g;
    gchar     *antes, *despues;

    m = clic_derecho( A.l_series, 0 );
    if ( m == NULL ) { check( 0, "el menu de la serie para el vistazo", NULL );
                       return; }

    olvida();
    gtk_menu_item_activate( GTK_MENU_ITEM(entrada_de( m, "Media" )) );
    cierra_menu( m );
    pump( 200 );
    check( g_file_test( V.eps, G_FILE_TEST_EXISTS ),
           "«Media – desviación típica» deja el EPS de fug", V.eps );
    check( V.s_d == NULL, "y su pie no lleva d ni D (es la serie en nivel)", NULL );
    g = grafico( "vistazo.eps" );
    check( g != NULL, "y lo enseña en una ventana de graficos", todo_lo_dicho() );
    dibuja_de_verdad( tinta_de( g ), "la ventana del vistazo DIBUJA" );

    /* Serie + ACF/PACF, y mover d al pie rehace el dibujo. */
    m = clic_derecho( A.l_series, 0 );
    if ( m == NULL ) return;
    gtk_menu_item_activate( GTK_MENU_ITEM(entrada_de( m, "Serie y ACF" )) );
    cierra_menu( m );
    pump( 200 );
    check( V.s_d != NULL && V.s_D != NULL,
           "«Serie y ACF / PACF…» lleva λ, d y D al pie", NULL );
    antes = lee( V.eps );
    check( strlen( antes ) > 1000, "el EPS de la serie esta", V.eps );
    if ( V.s_d )
        {
        olvida();
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(V.s_d), 1 );
        pump( 300 );
        despues = lee( V.eps );
        check( strcmp( antes, despues ) != 0,
               "mover d al pie rehace el grafico (otro EPS)", todo_lo_dicho() );
        check( dijo( "fug:" ) == NULL && dijo( "No encuentro" ) == NULL,
               "y fug no se queja", todo_lo_dicho() );
        g_free( despues );
        }
    g_free( antes );
    dibuja_de_verdad( tinta_de( grafico( "vistazo.eps" ) ),
                      "y el grafico rehecho tambien dibuja" );

    /* IDENTIFICAR desde el mismo grafico (E2): art sobre la serie con la
       transformacion que se esta mirando, y su ventana con candidatos. */
    {
    GtkWindow *vg = grafico( "vistazo.eps" );
    GtkWidget *bi = vg ? boton_que_dice( GTK_WIDGET(vg), "Identificar…" ) : NULL;
    GtkWindow *wi;

    check( bi != NULL, "el pie del vistazo ofrece «Identificar…»", NULL );
    if ( bi )
        {
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(V.s_lam), 1.0 );
        pump( 300 );
        olvida();
        gtk_button_clicked( GTK_BUTTON(bi) );
        pump( 1500 );
        wi = ventana_titulada( "Identificación — ipc / m00" );
        check( wi != NULL, "«Identificar…» abre la identificación de los datos",
               todo_lo_dicho() );
        if ( wi )
            {
            GPtrArray *tv = junta( GTK_WIDGET(wi), GTK_TYPE_TREE_VIEW, FALSE );
            int nc = tv->len ? gtk_tree_model_iter_n_children(
                         gtk_tree_view_get_model( g_ptr_array_index( tv, 0 ) ), NULL ) : 0;
            gchar *txt = textos_de( GTK_WIDGET(wi) );

            g_object_add_weak_pointer( G_OBJECT(wi), (gpointer *) &wi );
            check( nc > 0, "con candidatos de art", NULL );
            check( strstr( txt, "d = 1" ) != NULL, "sobre la transformación del pie (d = 1)", txt );
            /* CON D = 0 ART QUITA LOS ARMONICOS: se avisa, y el hijo los
               lleva. Paso con IPC_ES: derivaba el AR(1) sin ellos.      */
            check( strstr( txt, "art ha quitado la estacionalidad con armónicos" ) != NULL,
                   "con D = 0, la ventana avisa de que art quito los armonicos", txt );
            /* CABE EN UNA PANTALLA PEQUEÑA. Paso en una de 1366x768: el
               minimo pedia mas alto que la pantalla, la ventana se cortaba
               y no se podia maximizar.                                 */
            {
            int hmin = 0, hnat = 0, wmin = 0, wnat = 0;

            gtk_widget_get_preferred_height( GTK_WIDGET(wi), &hmin, &hnat );
            gtk_widget_get_preferred_width( GTK_WIDGET(wi), &wmin, &wnat );
            nota( "identificacion: minimo %dx%d", wmin, hmin );
            check( hmin <= 700 && wmin <= 1300,
                   "la ventana de identificacion cabe en 1366x768", NULL );
            }
            {
            GPtrArray *tvs = junta( GTK_WIDGET(wi), GTK_TYPE_TREE_VIEW, FALSE );
            GtkWidget *bd = boton_que_dice( GTK_WIDGET(wi),
                                            "Derivar modelo con el candidato elegido" );
            gchar     *antes_m = filas( A.l_modelos, M_ID );

            if ( tvs->len && bd )
                {
                GtkTreePath *pa = gtk_tree_path_new_first();
                gchar       *despues_m, *nuevo_m = NULL, *inp_m = NULL, *c = NULL;
                gchar      **va, **vd;
                int          ii, jj;

                gtk_tree_selection_select_path( gtk_tree_view_get_selection(
                    GTK_TREE_VIEW(g_ptr_array_index( tvs, 0 )) ), pa );
                gtk_tree_path_free( pa );
                olvida();
                hijos_sin_pantalla( TRUE );
                gtk_button_clicked( GTK_BUTTON(bd) );
                pump( 800 );
                hijos_sin_pantalla( FALSE );
                despues_m = filas( A.l_modelos, M_ID );
                va = g_strsplit( antes_m, "|", -1 );
                vd = g_strsplit( despues_m, "|", -1 );
                for ( ii = 0; vd[ii] && !nuevo_m; ii++ )
                    {
                    gboolean ya = FALSE;
                    for ( jj = 0; va[jj]; jj++ ) if ( !strcmp( va[jj], vd[ii] ) ) ya = TRUE;
                    if ( !ya ) nuevo_m = g_strdup( vd[ii] );
                    }
                if ( nuevo_m ) inp_m = ruta_de( "ipc", "", nuevo_m, ".inp" );
                if ( inp_m ) g_file_get_contents( inp_m, &c, NULL, NULL );
                check( c && strstr( c, "\ncos 1" ) && strstr( c, "\nsin 5" ) &&
                       strstr( c, "\nalter" ),
                       "el modelo derivado lleva los armonicos que art quito", c );
                if ( inp_m )
                    {
                    char m2[256];
                    check( inp_check_fue( inp_m, m2, sizeof m2 ) == 0,
                           "y fue acepta el .inp con los armonicos", m2 );
                    }
                /* y se deja el proyecto como estaba: las fases siguientes
                   cuentan los modelos que hay. */
                if ( nuevo_m )
                    {
                    PrError e;

                    pr_borra( A.p, "ipc", "", nuevo_m, &e );
                    if ( inp_m ) g_unlink( inp_m );
                    atsw_guarda( &A, &e );
                    atsw_refresca( &A );
                    pump( 200 );
                    }
                g_free( c ); g_free( inp_m ); g_free( nuevo_m ); g_free( despues_m );
                g_strfreev( va ); g_strfreev( vd );
                }
            g_free( antes_m );
            g_ptr_array_free( tvs, TRUE );
            }
            nota( "identificar desde el vistazo: %d candidatos", nc );
            g_free( txt );
            g_ptr_array_free( tv, TRUE );
            if ( wi ) gtk_widget_destroy( GTK_WIDGET(wi) );   /* derivar ya la cierra */
            pump( 100 );
            }

        /* art identifica en logaritmos o en niveles: otra lambda se dice */
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(V.s_lam), 0.5 );
        pump( 300 );
        olvida();
        gtk_button_clicked( GTK_BUTTON(bi) );
        pump( 300 );
        check( dijo( "λ = 0.50" ) != NULL, "con λ = 0,5 se niega y dice por qué",
               todo_lo_dicho() );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(V.s_lam), 1.0 );
        pump( 300 );
        }
    }

    /* E1: IDENTIFICAR DESDE LOS DATOS, con la transformacion en la ventana. */
    {
    GtkMenu   *m1 = clic_derecho( A.l_series, 0 );
    GtkWidget *e1 = m1 ? entrada_de( m1, "Identificar desde los datos" ) : NULL;
    GtkWindow *w1;

    check( e1 != NULL, "el menú de la serie ofrece «Identificar desde los datos…»",
           m1 ? entradas( m1 ) : NULL );
    olvida();
    if ( e1 ) gtk_menu_item_activate( GTK_MENU_ITEM(e1) );
    cierra_menu( m1 );
    pump( 1500 );
    w1 = ventana_titulada( "Identificación — ipc / m00" );
    check( w1 != NULL, "abre la identificación desde los datos", todo_lo_dicho() );
    if ( w1 )
        {
        GPtrArray *sp = junta( GTK_WIDGET(w1), GTK_TYPE_SPIN_BUTTON, FALSE );
        GtkWidget *bv = boton_que_dice( GTK_WIDGET(w1), "Volver a identificar" );
        gchar     *txt = textos_de( GTK_WIDGET(w1) );

        check( sp->len >= 2 && bv != NULL, "con d, D y «Volver a identificar»", NULL );
        check( strstr( txt, "Estacionalidad" ) != NULL || strstr( txt, "Raíz unitaria" ) != NULL,
               "y los contrastes a la vista", txt );
        g_free( txt );
        if ( sp->len >= 2 && bv )
            {
            GtkWidget *sd = g_ptr_array_index( sp, 0 );
            int        nd = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(sd) ) == 1 ? 0 : 1;
            gchar     *want = g_strdup_printf( "d = %d", nd );

            gtk_spin_button_set_value( GTK_SPIN_BUTTON(sd), nd );
            gtk_button_clicked( GTK_BUTTON(bv) );
            pump( 1500 );
            txt = textos_de( GTK_WIDGET(w1) );
            check( strstr( txt, want ) != NULL, "volver a identificar usa la d nueva", txt );
            nota( "identificar desde los datos: rehecho con %s", want );
            g_free( txt ); g_free( want );
            }
        g_ptr_array_free( sp, TRUE );
        gtk_widget_destroy( GTK_WIDGET(w1) );
        pump( 100 );
        }
    }

    /* Lo que no se puede mirar se dice, no revienta. */
    {
    gchar *no = g_build_filename( T, "no_existe.inp", NULL );

    atsw_vistazo( &A, no, 0, 1.0 );
    check( strstr( barra(), "no_existe.inp" ) != NULL,
           "un vistazo sobre un .inp que no esta lo dice", barra() );
    g_free( no );
    }
}

/* MODELO NUEVO: nace de los datos y se abre en fue_gui de verdad. */
static void modelo_nuevo( void )
{
    gchar *fue_gui = atsw_programa( "fue_gui" );
    gchar *s;

    olvida();
    hijos_sin_pantalla( TRUE );
    gtk_button_clicked( GTK_BUTTON(A.b_nuevo) );
    hijos_sin_pantalla( FALSE );

    check( existe( "ipc", "", "m01", ".inp" ), "«Modelo nuevo» escribe m01.inp", NULL );
    check( strstr( barra(), "m01 nace de los datos" ) != NULL,
           "y dice de donde nace", barra() );
    {
    gchar *q = lanzado( fue_gui, "m01" );

    check( fue_gui && dijo( q ) != NULL,
           "y lanza el fue_gui DEL ARBOL con el modelo", todo_lo_dicho() );
    g_free( q );
    }
    s = filas( A.l_modelos, M_ID );
    check( !strcmp( s, "m00|m01" ), "la rejilla tiene ahora m00 y m01", s );
    g_free( s );
    s = celda( A.l_modelos, M_ID, "m01", M_PADRE );
    check( s && !strcmp( s, "m00" ), "m01 viene de m00", s );
    g_free( s );
    s = celda( A.l_modelos, M_ID, "m01", M_RAZON );
    check( s && !strcmp( s, "(sin razón)" ), "sin razon se ve como sin razon", s );
    g_free( s );
    {
    gchar *a = ruta_de( "ipc", "", "m00", ".inp" ), *b = ruta_de( "ipc", "", "m01", ".inp" );
    gchar *ca = lee( a ), *cb = lee( b );

    check( !strcmp( ca, cb ), "m01 arranca siendo copia de los datos", NULL );
    g_free( a ); g_free( b ); g_free( ca ); g_free( cb );
    }
    g_free( fue_gui );
}

/* LOS ENVIOS: fug, fue y drtran, a los programas del arbol. */
static void envios( void )
{
    gchar *p;

    /* → fug con m01 marcado: va m00 igual, y se dice. */
    marca( A.l_modelos, M_ID, "m01" );
    p = atsw_programa( "gtk_fmg" );
    olvida();
    hijos_sin_pantalla( TRUE );
    gtk_button_clicked( GTK_BUTTON(A.b_fug) );
    hijos_sin_pantalla( FALSE );
    check( dijo( "va m00 y no m01" ) != NULL,
           "«→ fug» con un modelo marcado avisa de que va el m00", todo_lo_dicho() );
    {
    gchar *q = lanzado( p, "m00" );

    check( p && dijo( q ), "y lanza el gtk_fmg del arbol con los datos",
           todo_lo_dicho() );
    g_free( q );
    }
    g_free( p );

    /* → fue con m01 marcado (sin .pre): va su .inp. */
    p = atsw_programa( "fue_gui" );
    olvida();
    hijos_sin_pantalla( TRUE );
    gtk_button_clicked( GTK_BUTTON(A.b_fue) );
    hijos_sin_pantalla( FALSE );
    {
    gchar *q = lanzado( p, "m01" );

    check( p && dijo( q ), "«→ fue» lanza fue_gui con el modelo marcado",
           todo_lo_dicho() );
    g_free( q );
    }
    g_free( p );

    /* drtran: con el proyecto, sin fichero. */
    p = atsw_programa( "drtran_gui" );
    olvida();
    hijos_sin_pantalla( TRUE );
    gtk_button_clicked( GTK_BUTTON(A.b_drtran) );
    hijos_sin_pantalla( FALSE );
    {
    gchar *q = g_strdup_printf( "%s, con este proyecto.", p ? p : "drtran_gui" );

    check( p && dijo( q ), "«drtran» lanza drtran_gui con el proyecto",
           todo_lo_dicho() );
    g_free( q );
    }
    g_free( p );
}

/* LO QUE LE LLEGA AL HIJO, con uno falso que lo escribe. */
static void hijo_falso( void )
{
    gchar **v;
    gchar  *inp = ruta_de( "ipc", "", "m01", ".inp" );

    atsw_lanza_con( &A, "atsw_hijo_falso", "--prever", inp );
    v = lo_que_recibio();
    check( v != NULL, "el hijo lanzado arranca y escribe lo que recibio",
           barra() );
    if ( v )
        {
        check( g_strv_length( v ) >= 5 && !strcmp( v[0], "--proyecto" ) &&
               !strcmp( v[1], A.p->path ) && !strcmp( v[2], "--prever" ) &&
               !strcmp( v[3], inp ) && !strcmp( v[4], "FIN" ),
               "al hijo le llega «--proyecto P --prever fichero»",
               g_strjoinv( " ", v ) );
        g_strfreev( v );
        }

    atsw_lanza( &A, "atsw_hijo_falso", NULL );
    v = lo_que_recibio();
    if ( v )
        {
        check( g_strv_length( v ) >= 3 && !strcmp( v[0], "--proyecto" ) &&
               !strcmp( v[2], "FIN" ),
               "sin fichero le llega solo «--proyecto P»", g_strjoinv( " ", v ) );
        g_strfreev( v );
        }
    else
        check( 0, "el hijo sin fichero tambien arranca", barra() );
    check( strstr( barra(), "con este proyecto" ) != NULL,
           "y la barra dice que binario lanzo", barra() );

    /* Un programa que no esta: se dice, no se lanza nada. */
    atsw_lanza( &A, "atsw_no_existe_ningun_programa", NULL );
    check( strstr( barra(), "No encuentro «atsw_no_existe_ningun_programa»" ) != NULL,
           "lanzar un programa que no esta lo dice en la barra", barra() );
    g_free( inp );
}

/* Borrar el directorio de la prueba. Sin rm -rf: tiene que valer en Windows. */
static void borra_arbol( const char *dir )
{
    GDir       *d = g_dir_open( dir, 0, NULL );
    const char *n;

    if ( d == NULL ) { g_unlink( dir ); return; }
    while ( ( n = g_dir_read_name( d ) ) != NULL )
        {
        gchar *f = g_build_filename( dir, n, NULL );

        if ( g_file_test( f, G_FILE_TEST_IS_DIR ) &&
             !g_file_test( f, G_FILE_TEST_IS_SYMLINK ) )
            borra_arbol( f );
        else
            g_unlink( f );
        g_free( f );
        }
    g_dir_close( d );
    g_rmdir( dir );
}

/* EL EDITOR DEL .inp: abrir, guardar mal, guardar bien, estimar con fue,
 * ver el grafico de residuos, y guardar encima de lo estimado (deriva).  */
static Editor *editor_de( const char *id )
{
    GSList *l;

    for ( l = g_abiertos; l; l = l->next )
        if ( !strcmp( ((Editor *) l->data)->id, id ) ) return l->data;
    return NULL;
}

static gchar *texto_editor( Editor *E )
{
    gchar *t = texto_de( E->texto );

    sin_cr( t );
    return t;
}

static const char *estado_editor( Editor *E )
{
    return gtk_label_get_text( GTK_LABEL(E->estado) );
}

/* Del .inp de los datos a un modelo de verdad: logaritmos, una diferencia
   y un MA(1) regular. Es lo que el analista escribiria a mano. */
static gchar *con_modelo( const char *datos, const char *semilla )
{
    gchar *a, *b;
    gchar *ma = g_strdup_printf( "** Number and orders of regular MA operators:\n"
                                 "1 1\n**\n%s  1\n", semilla );

    a = g_strdup( datos );
    {
    char *p = strstr( a, "** Number and orders of regular MA operators:\n0\n" );

    if ( p == NULL ) return a;
    *p = '\0';
    b = g_strconcat( a, ma, p + strlen( "** Number and orders of regular MA "
                                        "operators:\n0\n" ), NULL );
    g_free( a ); a = b;
    }
    {
    char *p = strstr( a, "\n 1.000000 0 0\n" );

    if ( p )
        { *p = '\0';
          b = g_strconcat( a, "\n 0.000000 1 0\n", p + strlen( "\n 1.000000 0 0\n" ),
                           NULL );
          g_free( a ); a = b; }
    }
    g_free( ma );
    return a;
}

static void editor( void )
{
    GtkMenu *m;
    Editor  *E;
    gchar   *f = ruta_de( "ipc", "", "m01", ".inp" );
    gchar   *original, *txt, *s;

    /* Los datos no se editan, y se dice. */
    atsw_editor( &A, "ipc", "", "m00" );
    check( strstr( barra(), "Los datos no se editan" ) != NULL,
           "el editor no abre los datos y lo dice", barra() );
    check( editor_de( "m00" ) == NULL, "y no queda ninguna ventana abierta", NULL );

    /* Desde el menu del modelo, como el analista. */
    marca( A.l_modelos, M_ID, "m01" );
    m = clic_derecho( A.l_modelos, 1 );
    check( m != NULL, "el boton derecho sobre m01 abre su menu", NULL );
    if ( m )
        {
        gchar     *e = entradas( m );
        GtkWidget *dx = entrada_de( m, "Diagnosis" );

        check( strstr( e, "Abrir m01 en fue" ) && strstr( e, "Editar el .inp…" ) &&
               strstr( e, "Prever con fuf…" ) && strstr( e, "Iterar" ) &&
               strstr( e, "Borrar…" ),
               "el menu del modelo ofrece lo que se puede hacer con el", e );
        check( dx && !gtk_widget_get_sensitive( dx ),
               "sin estimar, «Diagnosis…» esta apagada", e );
        if ( dx )
            {
            gchar *tip = gtk_widget_get_tooltip_text( dx );

            check( tip && *tip, "y el globo dice por que", tip );
            g_free( tip );
            }
        g_free( e );
        if ( entrada_de( m, "Editar el .inp" ) )
            gtk_menu_item_activate( GTK_MENU_ITEM(entrada_de( m, "Editar el .inp" )) );
        cierra_menu( m );
        }
    E = editor_de( "m01" );
    check( E != NULL, "«Editar el .inp…» abre el editor sobre m01", barra() );
    if ( E == NULL ) { g_free( f ); return; }
    pump( 200 );

    check( strstr( gtk_window_get_title( GTK_WINDOW(E->win) ), "ipc / m01.inp" ) != NULL,
           "el titulo del editor dice serie y modelo",
           gtk_window_get_title( GTK_WINDOW(E->win) ) );
    original = lee( f );
    txt = texto_editor( E );
    check( !strcmp( txt, original ), "el editor enseña el .inp tal cual", NULL );
    g_free( txt );

    /* Guardar algo que el motor no leeria: no se toca el fichero. */
    pon_texto( E->texto, "esto no es un .inp\n" );
    gtk_button_clicked( GTK_BUTTON(E->b_guardar) );
    check( strstr( estado_editor( E ), "No lo guardo" ) != NULL,
           "guardar un .inp invalido se niega y dice por que", estado_editor( E ) );
    s = lee( f );
    check( !strcmp( s, original ), "y el fichero de antes sigue intacto", NULL );
    g_free( s );
    {
    gchar *tmp = g_strconcat( f, ".editando", NULL );

    check( !g_file_test( tmp, G_FILE_TEST_EXISTS ),
           "y no deja el temporal por ahi", tmp );
    g_free( tmp );
    }

    /* Un modelo de verdad: se guarda. */
    txt = con_modelo( original, "0.300000" );
    check( strcmp( txt, original ) != 0, "(la prueba sabe escribir un MA(1))", NULL );
    pon_texto( E->texto, txt );
    gtk_button_clicked( GTK_BUTTON(E->b_guardar) );
    check( strstr( estado_editor( E ), "Guardado en m01.inp" ) != NULL,
           "un .inp valido se guarda y lo dice", estado_editor( E ) );
    s = lee( f );
    check( !strcmp( s, txt ), "y lo escrito es lo que habia en el editor", NULL );
    g_free( s );
    g_free( txt );

    /* Estimar: corre el fue del arbol sin congelar la ventana. */
    gtk_button_clicked( GTK_BUTTON(E->b_estimar) );
    check( E->job != NULL, "«Guardar y estimar» lanza fue", estado_editor( E ) );
    {
    gint64 hasta = g_get_monotonic_time() + 90 * G_USEC_PER_SEC;

    while ( E->job && g_get_monotonic_time() < hasta ) pump( 100 );
    }
    check( E->job == NULL, "fue acaba (menos de 90 s)", estado_editor( E ) );
    nota( "editor tras estimar: %s", estado_editor( E ) );
    check( existe( "ipc", "", "m01", ".out" ) && existe( "ipc", "", "m01", ".pre" ),
           "estimar deja el .out y el .pre de m01", estado_editor( E ) );
    s = texto_de( E->salida );
    check( strlen( s ) > 500, "el informe sale en la pestaña Output", s );
    g_free( s );
    check( gtk_notebook_get_current_page( GTK_NOTEBOOK(E->libro) ) == 1,
           "y se pone delante", NULL );
    s = texto_de( E->consola );
    check( strstr( s, "$ cd " ) != NULL, "la consola enseña la orden que se corrio", s );
    g_free( s );
    s = celda( A.l_modelos, M_ID, "m01", M_SD );
    if ( s ) g_strdelimit( s, ",", '.' );       /* con LC_NUMERIC de coma */
    check( s && g_ascii_strtod( s, NULL ) > 0.0,
           "la rejilla de la madre enseña la d.t. del .out", s );
    g_free( s );
    s = celda( A.l_modelos, M_ID, "m01", M_ESTRUCT );
    check( s && strstr( s, "(0,1,1)" ), "y la estructura que se estimo", s );
    g_free( s );

    /* El grafico de residuos que el motor dibujo al estimar. */
    check( gtk_widget_get_sensitive( E->b_graficos ),
           "con el modelo estimado, «Gráficos…» se enciende", NULL );
    gtk_button_clicked( GTK_BUTTON(E->b_graficos) );
    pump( 200 );
    dibuja_de_verdad( tinta_de( grafico( "ipc_m01" ) ),
                      "el grafico de residuos del editor DIBUJA" );

    /* Guardar encima de lo estimado: pregunta, y derivar deja m01 entero. */
    txt = con_modelo( original, "0.200000" );
    pon_texto( E->texto, txt );
    responde( "ya está estimado", contesta, GINT_TO_POINTER(1) );
    gtk_button_clicked( GTK_BUTTON(E->b_guardar) );
    todo_atendido( "guardar encima de un modelo estimado tenia que preguntar" );
    check( strstr( estado_editor( E ), "Guardado en m02" ) != NULL,
           "«Derivar» guarda lo editado en un modelo nuevo", estado_editor( E ) );
    check( !strcmp( E->id, "m02" ) &&
           strstr( gtk_window_get_title( GTK_WINDOW(E->win) ), "m02" ),
           "y el editor pasa a ser el de m02",
           gtk_window_get_title( GTK_WINDOW(E->win) ) );
    s = ruta_de( "ipc", "", "m02", ".inp" );
    {
    gchar *c = lee( s );

    check( !strcmp( c, txt ), "m02.inp tiene lo editado", NULL );
    g_free( c );
    }
    g_free( s );
    s = lee( f );
    check( strstr( s, "0.300000" ) != NULL, "y m01.inp no se ha tocado", NULL );
    g_free( s );
    s = celda( A.l_modelos, M_ID, "m02", M_PADRE );
    check( s && !strcmp( s, "m01" ), "m02 cuelga de m01 en la rejilla", s );
    g_free( s );
    g_free( txt );

    /* Cerrar con cambios: se pregunta; «Cerrar sin guardar» cierra. */
    pon_texto( E->texto, "cambio sin guardar\n" );
    responde( "cambios sin guardar", contesta, GINT_TO_POINTER(2) );
    gtk_window_close( GTK_WINDOW(E->win) );
    todo_atendido( "cerrar el editor con cambios tenia que preguntar" );
    pump( 100 );
    check( editor_de( "m02" ) == NULL, "y «Cerrar sin guardar» cierra el editor", NULL );

    g_free( original );
    g_free( f );
}

/* ITERAR, ELEGIR, RAZON: los botones de encima de la rejilla. */
static gboolean escribe_texto( GtkWindow *w, gpointer d )
{
    GPtrArray *e = junta( GTK_WIDGET(w), GTK_TYPE_ENTRY, TRUE );

    if ( e->len ) gtk_entry_set_text( g_ptr_array_index( e, 0 ), d );
    g_ptr_array_free( e, TRUE );
    gtk_dialog_response( GTK_DIALOG(w), GTK_RESPONSE_ACCEPT );
    return TRUE;
}

static void iterar_elegir( void )
{
    gchar *s;

    /* De los datos no se itera. */
    marca( A.l_modelos, M_ID, "m00" );
    gtk_button_clicked( GTK_BUTTON(A.b_iterar) );
    check( strstr( barra(), "Los datos no se iteran" ) != NULL,
           "iterar desde los datos se niega y lo dice", barra() );

    /* m01 esta estimado: su .pre pasa a m03.inp. */
    marca( A.l_modelos, M_ID, "m01" );
    gtk_button_clicked( GTK_BUTTON(A.b_iterar) );
    check( strstr( barra(), "m01.pre → m03.inp" ) != NULL,
           "«Iterar» copia el .pre a un .inp nuevo y lo dice", barra() );
    {
    gchar *a = ruta_de( "ipc", "", "m01", ".pre" ), *b = ruta_de( "ipc", "", "m03", ".inp" );
    gchar *ca = lee( a ), *cb = lee( b );

    check( *ca && !strcmp( ca, cb ), "m03.inp es copia fisica de m01.pre", b );
    g_free( a ); g_free( b ); g_free( ca ); g_free( cb );
    }
    s = celda( A.l_modelos, M_ID, "m03", M_PADRE );
    check( s && !strcmp( s, "m01" ), "y m03 cuelga de m01", s );
    g_free( s );

    /* Elegir m01, con su porque. */
    marca( A.l_modelos, M_ID, "m01" );
    responde( "El modelo elegido", escribe_texto, "el MA(1) basta" );
    gtk_button_clicked( GTK_BUTTON(A.b_elegir) );
    todo_atendido( "«Elegir» tenia que pedir la razon" );
    check( !strcmp( barra(), "Elegido declarado y guardado." ),
           "«Elegir» declara y guarda", barra() );
    s = celda( A.l_modelos, M_ID, "m01", M_ESTRELLA );
    check( s && !strcmp( s, "★" ), "el elegido lleva su estrella en la rejilla", s );
    g_free( s );
    s = celda( A.l_series, S_ID, "ipc", S_ELEGIDO );
    check( s && !strcmp( s, "m01" ), "y la lista de series dice cual es", s );
    g_free( s );

    /* Razon de m03. */
    marca( A.l_modelos, M_ID, "m03" );
    responde( "El porqué de esta iteración", escribe_texto, "seguir desde el óptimo" );
    gtk_button_clicked( GTK_BUTTON(A.b_razon) );
    todo_atendido( "«Razón…» tenia que pedirla" );
    check( !strcmp( barra(), "Razón guardada." ), "«Razón…» guarda y lo dice", barra() );
    s = celda( A.l_modelos, M_ID, "m03", M_RAZON );
    check( s && !strcmp( s, "seguir desde el óptimo" ),
           "y la rejilla la enseña", s );
    g_free( s );
}

/* TODAS LAS VENTANAS ABIERTAS CABEN EN UNA PANTALLA DE 1366x768.
 *
 * Paso en un portatil asi: la de anomalos pedia de ANCHO minimo mas que la
 * pantalla, no se podia maximizar, el gestor la recolocaba y el segundo clic
 * caia en «cerrar». Una ventana que pide mas que la pantalla no se arregla
 * en el escritorio: se arregla aqui. 1300x700 deja sitio a las barras.  */
static void caben_todas( const char *donde )
{
    GList *l = gtk_window_list_toplevels(), *i;

    for ( i = l; i; i = i->next )
        {
        GtkWidget  *w = i->data;
        const char *ti;
        int         wmin = 0, wnat = 0, hmin = 0, hnat = 0;
        gchar      *q;

        if ( !GTK_IS_WINDOW(w) || !gtk_widget_get_visible( w ) ) continue;
        if ( gtk_window_get_window_type( GTK_WINDOW(w) ) != GTK_WINDOW_TOPLEVEL ) continue;
        ti = gtk_window_get_title( GTK_WINDOW(w) );
        gtk_widget_get_preferred_width( w, &wmin, &wnat );
        gtk_widget_get_preferred_height_for_width( w, wmin, &hmin, &hnat );
        nota( "%s: «%s» minimo %dx%d", donde, ti ? ti : "", wmin, hmin );
        q = g_strdup_printf( "«%s» cabe en 1366x768 (minimo %dx%d)", ti ? ti : "",
                             wmin, hmin );
        check( wmin <= 1300 && hmin <= 700, q, NULL );
        g_free( q );
        }
    g_list_free( l );
}

/* EL MENU DEL MODELO ESTIMADO: diagnosis, ganancia, anomalos, prever. */
static void menu_modelo_prueba( void )
{
    static const char *ent[] = { "Diagnosis", "Ganancia", "Anómalos" };
    int i;

    for ( i = 0; i < 3; i++ )
        {
        GtkMenu   *m;
        GtkWidget *mi;
        int        antes = cuenta_visibles();
        gchar     *q;

        marca( A.l_modelos, M_ID, "m01" );
        m = clic_derecho( A.l_modelos, 1 );
        mi = m ? entrada_de( m, ent[i] ) : NULL;
        q = g_strdup_printf( "estimado m01, «%s…» se enciende", ent[i] );
        check( mi && gtk_widget_get_sensitive( mi ), q, m ? entradas( m ) : NULL );
        g_free( q );
        olvida();
        if ( mi ) gtk_menu_item_activate( GTK_MENU_ITEM(mi) );
        cierra_menu( m );
        pump( 300 );
        q = g_strdup_printf( "«%s…» abre su ventana o dice por que no", ent[i] );
        check( cuenta_visibles() > antes || historial->len > 0, q, todo_lo_dicho() );
        g_free( q );
        }
    check( ventana_titulada( "Diagnosis — ipc / m01" ) != NULL,
           "la diagnosis de m01 es una ventana con su titulo", NULL );
    caben_todas( "diagnosis, ganancia y anomalos" );

    /* EL IDENTIFICADOR SOBRE LOS RESIDUOS (E3, docs/ESTUDIO-identificador.md):
       se enciende con el .out al dia, abre su ventana con los candidatos de
       art, y derivar crea un hijo de m01 con los ordenes del elegido -- que
       fue acepta. No se estima nada. */
    {
    GtkMenu   *m;
    GtkWidget *mi;
    GtkWindow *w;
    gchar     *antes = filas( A.l_modelos, M_ID );

    marca( A.l_modelos, M_ID, "m01" );
    m = clic_derecho( A.l_modelos, 1 );
    mi = m ? entrada_de( m, "Identificar los residuos" ) : NULL;
    check( mi && gtk_widget_get_sensitive( mi ),
           "estimado m01, «Identificar los residuos…» se enciende", m ? entradas( m ) : NULL );
    olvida();
    if ( mi ) gtk_menu_item_activate( GTK_MENU_ITEM(mi) );
    cierra_menu( m );
    pump( 1500 );
    w = ventana_titulada( "Identificación — ipc / m01" );
    check( w != NULL, "la identificación de los residuos de m01 abre su ventana",
           todo_lo_dicho() );
    if ( w )
        {
        GPtrArray    *tv = junta( GTK_WIDGET(w), GTK_TYPE_TREE_VIEW, FALSE );
        GtkWidget    *vista = tv->len ? g_ptr_array_index( tv, 0 ) : NULL;
        GtkTreeModel *mo = vista ? gtk_tree_view_get_model( GTK_TREE_VIEW(vista) ) : NULL;
        int           nc = mo ? gtk_tree_model_iter_n_children( mo, NULL ) : 0;
        gchar        *orden = NULL;
        GtkWidget    *b;

        g_ptr_array_free( tv, TRUE );
        check( nc > 0, "art propone candidatos para los residuos de m01", NULL );
        if ( nc > 0 )
            {
            GtkTreePath *pa = gtk_tree_path_new_first();
            GtkTreeIter  it;

            gtk_tree_selection_select_path(
                gtk_tree_view_get_selection( GTK_TREE_VIEW(vista) ), pa );
            gtk_tree_model_get_iter( mo, &it, pa );
            gtk_tree_model_get( mo, &it, 1, &orden, -1 );
            gtk_tree_path_free( pa );
            }
        b = boton_que_dice( GTK_WIDGET(w), "Derivar modelo con el candidato elegido" );
        check( b != NULL, "la ventana ofrece derivar", NULL );
        if ( b && orden )
            {
            gchar *despues, **va, **vd;
            char   nuevo[PR_ID] = "", porque[256], msg[256];
            int    i, j, p, q, P, Q, ep, eq, eP, eQ;

            olvida();
            hijos_sin_pantalla( TRUE );
            gtk_button_clicked( GTK_BUTTON(b) );
            pump( 800 );
            hijos_sin_pantalla( FALSE );
            despues = filas( A.l_modelos, M_ID );
            va = g_strsplit( antes, "|", -1 );
            vd = g_strsplit( despues, "|", -1 );
            for ( i = 0; vd[i] && !nuevo[0]; i++ )
                {
                gboolean ya = FALSE;
                for ( j = 0; va[j]; j++ ) if ( !strcmp( va[j], vd[i] ) ) ya = TRUE;
                if ( !ya ) g_snprintf( nuevo, sizeof nuevo, "%s", vd[i] );
                }
            check( nuevo[0] != '\0', "derivar crea un modelo nuevo", todo_lo_dicho() );
            if ( nuevo[0] )
                {
                gchar *padre = celda( A.l_modelos, M_ID, nuevo, M_PADRE );
                gchar *f = ruta_de( "ipc", "", nuevo, ".inp" );

                check( padre && !strcmp( padre, "m01" ), "el hijo es de m01", padre );
                nota( "identificar los residuos de m01: %d candidatos; derivado %s con %s",
                      nc, nuevo, orden );
                check( inp_check_fue( f, msg, sizeof msg ) == 0, "y fue lo acepta", msg );
                {
                /* derivar AÑADE: los órdenes del hijo son los del padre más
                   los del candidato (un factor más en cada operador) */
                gchar *fp = existe( "ipc", "", "m01", ".pre" ) ? ruta_de( "ipc", "", "m01", ".pre" )
                                                               : ruta_de( "ipc", "", "m01", ".inp" );
                int pp = 0, pq = 0, pP = 0, pQ = 0;

                id_arma_ordenes( fp, &pp, &pq, &pP, &pQ, porque, sizeof porque );
                check( id_arma_ordenes( f, &p, &q, &P, &Q, porque, sizeof porque ) == 0 &&
                       sscanf( orden, "(%d,%d)(%d,%d)", &ep, &eq, &eP, &eQ ) == 4 &&
                       p == pp + ep && q == pq + eq && P == pP + eP && Q == pQ + eQ,
                       "con lo que tenía m01 más el candidato elegido", orden );
                g_free( fp );
                }
                g_free( padre ); g_free( f );

                /* y se deja el proyecto como estaba: las fases siguientes
                   cuentan los modelos que hay. */
                {
                PrError e;
                gchar  *fi = ruta_de( "ipc", "", nuevo, ".inp" ), *ids;

                pr_borra( A.p, "ipc", "", nuevo, &e );
                g_unlink( fi );
                g_free( fi );
                atsw_guarda( &A, &e );
                atsw_refresca( &A );
                pump( 200 );
                ids = filas( A.l_modelos, M_ID );
                check( !strstr( ids, nuevo ), "la prueba deja el proyecto como estaba", ids );
                g_free( ids );
                }
                }
            g_strfreev( va ); g_strfreev( vd ); g_free( despues );
            }
        g_free( orden );
        }
    g_free( antes );
    }

    /* E4: LA DIAGNOSIS, cuando el Q falla, abre el identificador sobre los
       mismos residuos; si cuadra, el boton esta apagado y lo dice. */
    {
    GtkWindow *dw = ventana_titulada( "Diagnosis — ipc / m01" );
    GtkWidget *bi;

    if ( !dw )
        {
        GtkMenu *m;
        marca( A.l_modelos, M_ID, "m01" );
        m = clic_derecho( A.l_modelos, 1 );
        if ( m && entrada_de( m, "Diagnosis" ) )
            gtk_menu_item_activate( GTK_MENU_ITEM(entrada_de( m, "Diagnosis" )) );
        cierra_menu( m );
        pump( 300 );
        dw = ventana_titulada( "Diagnosis — ipc / m01" );
        }
    bi = dw ? boton_que_dice( GTK_WIDGET(dw), "Identificar los residuos…" ) : NULL;
    check( bi != NULL, "la diagnosis ofrece «Identificar los residuos…»", NULL );
    if ( bi && gtk_widget_get_sensitive( bi ) )
        {
        GtkWindow *wi;
        olvida();
        gtk_button_clicked( GTK_BUTTON(bi) );
        pump( 1500 );
        wi = ventana_titulada( "Identificación — ipc / m01" );
        check( wi != NULL, "y, con el Q fallando, abre el identificador", todo_lo_dicho() );
        nota( "diagnosis de m01: el Q falla, «Identificar los residuos…» abre su ventana" );
        if ( wi ) { gtk_widget_destroy( GTK_WIDGET(wi) ); pump( 100 ); }
        }
    else if ( bi )
        {
        const char *t = gtk_widget_get_tooltip_text( bi );
        check( t && strstr( t, "cuadra" ), "apagado, dice por qué", t );
        nota( "diagnosis de m01: el Q cuadra, el botón está apagado" );
        }
    }

    /* Prever: fue_gui --prever con el .inp. */
    {
    GtkMenu *m;
    gchar   *p = atsw_programa( "fue_gui" ), *q = lanzado( p, "m01" );

    marca( A.l_modelos, M_ID, "m01" );
    m = clic_derecho( A.l_modelos, 1 );
    olvida();
    hijos_sin_pantalla( TRUE );
    if ( m && entrada_de( m, "Prever" ) )
        gtk_menu_item_activate( GTK_MENU_ITEM(entrada_de( m, "Prever" )) );
    hijos_sin_pantalla( FALSE );
    cierra_menu( m );
    check( dijo( q ) != NULL, "«Prever con fuf…» lanza fue_gui con el modelo",
           todo_lo_dicho() );
    g_free( p ); g_free( q );
    }
}

/* «+»: UNA MUESTRA NUEVA, hasta 12/2010. */
static gboolean declara( GtkWindow *w, gpointer d )
{
    GPtrArray *g = junta( GTK_WIDGET(w), GTK_TYPE_GRID, FALSE );

    (void) d;
    if ( g->len )
        {
        GtkGrid   *r = g_ptr_array_index( g, 0 );
        GtkWidget *nom = gtk_grid_get_child_at( r, 1, 0 );
        GtkWidget *h   = gtk_grid_get_child_at( r, 1, 2 );
        GtkWidget *raz = gtk_grid_get_child_at( r, 1, 3 );

        if ( nom ) gtk_entry_set_text( GTK_ENTRY(nom), "hasta-2010" );
        if ( raz ) gtk_entry_set_text( GTK_ENTRY(raz), "antes de la pandemia" );
        if ( h )
            {
            GPtrArray *sp = junta( h, GTK_TYPE_SPIN_BUTTON, FALSE );

            /* El mes y el año, en ese orden: lo pone atsw_fecha_nueva. */
            if ( sp->len == 2 )
                {
                gtk_spin_button_set_value( g_ptr_array_index( sp, 1 ), 2010 );
                gtk_spin_button_set_value( g_ptr_array_index( sp, 0 ), 12 );
                }
            g_ptr_array_free( sp, TRUE );
            }
        }
    g_ptr_array_free( g, TRUE );
    gtk_dialog_response( GTK_DIALOG(w), GTK_RESPONSE_OK );
    return TRUE;
}

static void muestra( void )
{
    gchar *s;

    responde( "Nueva muestra", declara, NULL );
    on_muestra_nueva( NULL, &A );
    todo_atendido( "«+» tenia que abrir el dialogo de la muestra" );
    check( strstr( ultimo.visto, "Los datos van de 1/2002 a 12/2025" ) != NULL,
           "el dialogo dice el tramo real de los datos", ultimo.visto );
    check( strstr( barra(), "Muestra «hasta-2010» declarada, hasta 12/2010" ) != NULL,
           "declarar la muestra lo dice, con su final", barra() );
    check( A.nhojas == 2 &&
           gtk_notebook_get_current_page( GTK_NOTEBOOK(A.libro) ) == 1,
           "sale una hoja nueva, y se pone delante", NULL );
    check( !strcmp( atsw_muestra_actual( &A ), "hasta-2010" ),
           "y la muestra actual es esa", atsw_muestra_actual( &A ) );
    s = filas( A.l_modelos, M_ID );
    check( !strcmp( s, "m00" ), "la hoja nace con los datos de la ventana", s );
    g_free( s );
    s = ruta_de( "ipc", "hasta-2010", "m00", ".inp" );
    {
    gchar *c = lee( s );

    check( strstr( c, " 108  1 2002 ipc" ) != NULL,
           "y su .inp lleva 108 observaciones (1/2002 a 12/2010)", s );
    g_free( c );
    }
    g_free( s );

    /* Llevar m01 a la muestra nueva, desde el menu del modelo. */
    gtk_notebook_set_current_page( GTK_NOTEBOOK(A.libro), 0 );
    pump( 50 );
    check( !strcmp( atsw_muestra_actual( &A ), "" ),
           "volver a la hoja «Completa» la pone delante", atsw_muestra_actual( &A ) );
    s = filas( A.l_modelos, M_ID );
    check( !strcmp( s, "m00|m01|m02|m03" ), "y la rejilla vuelve a sus modelos", s );
    g_free( s );
    {
    GtkMenu   *m;
    GtkWidget *mi;

    marca( A.l_modelos, M_ID, "m01" );
    m = clic_derecho( A.l_modelos, 1 );
    mi = m ? entrada_de( m, "En otra muestra" ) : NULL;
    check( mi != NULL, "el menu de m01 ofrece llevarlo a otra muestra",
           m ? entradas( m ) : NULL );
    if ( mi )
        {
        GtkWidget *sub = gtk_menu_item_get_submenu( GTK_MENU_ITEM(mi) );
        GtkWidget *x = sub ? entrada_de( GTK_MENU(sub), "hasta-2010" ) : NULL;

        check( x != NULL, "y el submenu nombra «hasta-2010»", NULL );
        if ( x ) gtk_menu_item_activate( GTK_MENU_ITEM(x) );
        }
    cierra_menu( m );
    }
    check( strstr( barra(), "m01, en la muestra «hasta-2010», con la idea de m01" ) != NULL,
           "llevar m01 a la otra muestra deriva y lo dice", barra() );
    check( !strcmp( atsw_muestra_actual( &A ), "hasta-2010" ),
           "y salta a esa hoja", atsw_muestra_actual( &A ) );
    check( existe( "ipc", "hasta-2010", "m01", ".inp" ),
           "el modelo nuevo de esa hoja tiene su .inp", NULL );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(A.libro), 0 );
    pump( 50 );

    /* «ABRIR…» CON LA VENTANA YA HECHA TRAE SUS HOJAS. Paso: se abria y
       solo salia «Completa». Se vacian las hojas como las dejaria otro
       proyecto sin muestras, y se abre este.                          */
    {
    char path[PR_RUTA], why[512];

    snprintf( path, sizeof path, "%s", A.p->path );
    A.hay = FALSE;
    atsw_hojas( &A );
    check( A.nhojas == 1, "sin proyecto, sólo la hoja «Completa»", NULL );
    check( atsw_abre( &A, path, why, sizeof why ), "se vuelve a abrir el proyecto", why );
    atsw_refresca( &A );
    pump( 100 );
    check( A.nhojas == 2 &&
           gtk_notebook_get_n_pages( GTK_NOTEBOOK(A.libro) ) == 2,
           "y «Abrir…» trae la hoja de la submuestra", NULL );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(A.libro), 0 );
    marca( A.l_series, S_ID, "ipc" );      /* abrir desmarca la serie */
    pump( 50 );
    }
}

/* EL LINAJE: la cadena entera, con lo que debe. */
static void arbol_r( GtkTreeModel *mo, GtkTreeIter *padre, int nivel, GString *g )
{
    GtkTreeIter it;

    if ( !gtk_tree_model_iter_children( mo, &it, padre ) ) return;
    do {
        gchar *n = NULL, *e = NULL;

        gtk_tree_model_get( mo, &it, 0, &n, 1, &e, -1 );
        g_string_append_printf( g, "%*s%s [%s]\n", 2 * nivel, "", n ? n : "",
                                e ? e : "" );
        g_free( n ); g_free( e );
        arbol_r( mo, &it, nivel + 1, g );
    } while ( gtk_tree_model_iter_next( mo, &it ) );
}

static void linaje( void )
{
    GtkWindow *w;
    GPtrArray *tv;
    GString   *g = g_string_new( NULL );
    gchar     *t;

    on_linaje( NULL, &A );
    pump( 100 );
    w = ventana_titulada( "Linaje" );
    check( w != NULL, "«Linaje…» abre su ventana", barra() );
    if ( w == NULL ) { g_string_free( g, TRUE ); return; }

    tv = junta( GTK_WIDGET(w), GTK_TYPE_TREE_VIEW, FALSE );
    if ( tv->len )
        arbol_r( gtk_tree_view_get_model( g_ptr_array_index( tv, 0 ) ), NULL, 0, g );
    g_ptr_array_free( tv, TRUE );
    nota( "linaje:\n%s", g->str );

    check( strstr( g->str, "ipc · muestra completa" ) &&
           strstr( g->str, "ipc · hasta-2010" ) &&
           strstr( g->str, "wti · muestra completa" ),
           "una cadena por serie y muestra", g->str );
    check( strstr( g->str, "\n  m00 [datos]\n    m01  ← elegido [estimado]\n"
                           "      m02 [SIN ESTIMAR]\n      m03 [SIN ESTIMAR]\n" ) != NULL,
           "la cadena de ipc: m00 → m01 (elegido, estimado) → m02 y m03", g->str );
    t = textos_de( GTK_WIDGET(w) );
    check( strstr( t, "cadenas" ) && strstr( t, "sin estimar" ) && strstr( t, "sin razón" ),
           "el pie cuenta lo que se debe", t );
    g_free( t );
    gtk_widget_destroy( GTK_WIDGET(w) );
    g_string_free( g, TRUE );
}

/* BORRAR: pregunta, y se lleva el nodo y sus ficheros. */
static void borrar( void )
{
    GtkMenu *m;
    gchar   *s;

    /* m01 tiene hijos: no se borra, y no hace falta preguntar. */
    marca( A.l_modelos, M_ID, "m01" );
    on_borrar( NULL, &A );
    todo_atendido( "(borrar m01)" );
    check( existe( "ipc", "", "m01", ".inp" ) && barra()[0],
           "un modelo con hijos no se borra, y se dice por que", barra() );

    /* m02 es una hoja del arbol: se pregunta y se va. */
    marca( A.l_modelos, M_ID, "m02" );
    m = clic_derecho( A.l_modelos, 2 );
    responde( "¿Borro m02", contesta, GINT_TO_POINTER(GTK_RESPONSE_OK) );
    if ( m && entrada_de( m, "Borrar" ) )
        gtk_menu_item_activate( GTK_MENU_ITEM(entrada_de( m, "Borrar" )) );
    cierra_menu( m );
    todo_atendido( "«Borrar…» tenia que preguntar" );
    check( strstr( barra(), "m02 borrado, con sus ficheros" ) != NULL,
           "borrar lo dice", barra() );
    check( !existe( "ipc", "", "m02", ".inp" ), "y el .inp de m02 ya no esta", NULL );
    s = filas( A.l_modelos, M_ID );
    check( !strcmp( s, "m00|m01|m03" ), "y la rejilla ya no lo tiene", s );
    g_free( s );
}

/* ABRIR: un manifiesto roto no se traga; el bueno se abre. Y releer lo que
 * cambio por fuera al volver el foco.                                   */
static void abrir( const char *manifiesto )
{
    gchar   *roto = g_build_filename( T, "roto.yaml", NULL );
    Proyecto *q = g_new0( Proyecto, 1 );
    PrError  e;
    gchar   *s;

    g_file_set_contents( roto, "esto no es un manifiesto }}}\n", -1, NULL );
    responde( "Abrir un proyecto", abre_fichero, roto );
    on_abrir( NULL, &A );
    todo_atendido( "«Abrir…» tenia que abrir el selector" );
    check( barra()[0] && !strcmp( A.p->path, manifiesto ) && A.p->ns == 2,
           "un manifiesto roto se dice y NO se abre: sigue el de antes", barra() );
    nota( "barra (manifiesto roto): %s", barra() );
    check( !strcmp( A.p->path, manifiesto ) && A.p->ns == 2 && A.p->nm >= 4 &&
           !strcmp( A.p->titulo, "Prueba de la madre" ),
           "y el proyecto en memoria sigue entero (ruta, series, modelos, titulo)",
           A.p->path );

    /* Otro proceso cambia el manifiesto; al volver el foco se relee. */
    if ( pr_leer( manifiesto, q, &e ) == 0 )
        {
        g_snprintf( q->titulo, sizeof q->titulo, "Cambiado desde fuera de la ventana" );
        pr_escribir( q, manifiesto, &e );
        on_foco( A.ventana, NULL, &A );
        check( strstr( barra(), "ha cambiado fuera de aquí: releído" ) != NULL,
               "un manifiesto cambiado por fuera se relee al volver el foco", barra() );
        check( strstr( gtk_label_get_text( GTK_LABEL(A.l_proy) ),
                       "Cambiado desde fuera" ) != NULL,
               "y la cabecera enseña lo nuevo",
               gtk_label_get_text( GTK_LABEL(A.l_proy) ) );
        }
    else
        check( 0, "(la prueba relee el manifiesto)", NULL );
    g_free( q );

    /* Y abrir el bueno desde el selector. */
    responde( "Abrir un proyecto", abre_fichero, (gpointer) manifiesto );
    on_abrir( NULL, &A );
    todo_atendido( "«Abrir…» tenia que abrir el selector" );
    s = filas( A.l_series, S_ID );
    check( !strcmp( s, "ipc|wti" ), "abrir el manifiesto bueno trae sus series", s );
    g_free( s );
    g_free( roto );
}

/* ======================================================================== */
/* LOS CASOS (docs/DISENO-casos.md §4)                                       */
/* ======================================================================== */

/* El descendiente de raiz con ese nombre de widget (gtk_widget_set_name). */
static GtkWidget *llamado( GtkWidget *raiz, const char *nombre )
{
    GPtrArray *t = junta( raiz, GTK_TYPE_WIDGET, FALSE );
    GtkWidget *r = NULL;
    guint      i;

    for ( i = 0; i < t->len && !r; i++ )
        if ( !strcmp( gtk_widget_get_name( g_ptr_array_index( t, i ) ), nombre ) )
            r = g_ptr_array_index( t, i );
    g_ptr_array_free( t, TRUE );
    return r;
}

/* En un arbol (el de las corridas): buscar a cualquier profundidad. */
static gboolean busca_r( GtkTreeModel *mo, GtkTreeIter *padre, int c,
                         const char *v, GtkTreeIter *out )
{
    GtkTreeIter it;

    if ( !gtk_tree_model_iter_children( mo, &it, padre ) ) return FALSE;
    do {
        gchar *s = NULL;

        gtk_tree_model_get( mo, &it, c, &s, -1 );
        if ( s && !strcmp( s, v ) ) { g_free( s ); *out = it; return TRUE; }
        g_free( s );
        if ( busca_r( mo, &it, c, v, out ) ) return TRUE;
    } while ( gtk_tree_model_iter_next( mo, &it ) );
    return FALSE;
}

static gboolean marca_arbol( GtkWidget *tv, int c, const char *v )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter   it;
    GtkTreePath  *p;

    if ( !busca_r( mo, NULL, c, v, &it ) ) return FALSE;
    p = gtk_tree_model_get_path( mo, &it );
    gtk_tree_view_expand_to_path( GTK_TREE_VIEW(tv), p );
    gtk_tree_view_set_cursor( GTK_TREE_VIEW(tv), p, NULL, FALSE );
    gtk_tree_path_free( p );
    pump( 20 );
    return TRUE;
}

static gchar *celda_arbol( GtkWidget *tv, int clave, const char *v, int c )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter   it;
    gchar        *r = NULL;

    if ( busca_r( mo, NULL, clave, v, &it ) ) gtk_tree_model_get( mo, &it, c, &r, -1 );
    return r;
}

/* Un modelo «estimado» a mano: su .inp y un .pre con el contenido dado. La
   madre no juzga si el .pre es un optimo --eso es de fue--: del .pre lee la
   ventana y le calcula el hash, que es lo que aqui se prueba.          */
static void modelo_a_mano( const char *serie, const char *pre_de, char *id,
                           size_t nid )
{
    PrError e;
    char    ruta[PR_RUTA];
    gchar  *datos = ruta_de( serie, "", pr_datos_de( A.p, serie, "" ), ".inp" );
    gchar  *c = NULL, *pre;
    gsize   n = 0;

    pr_deriva( A.p, serie, "", pr_datos_de( A.p, serie, "" ), id, nid,
               ruta, sizeof ruta, &e );
    g_file_get_contents( datos, &c, &n, NULL );
    g_file_set_contents( ruta, c ? c : "", (gssize) n, NULL );
    g_free( c ); c = NULL;
    g_file_get_contents( pre_de, &c, &n, NULL );
    pre = ruta_de( serie, "", id, ".pre" );
    g_file_set_contents( pre, c ? c : "", (gssize) n, NULL );
    g_free( c ); g_free( pre ); g_free( datos );
}

typedef struct { char def_ipc[PR_ID], def_wti[PR_ID]; gboolean puesto; } NuevoCaso;

/* El primer intento: ipc y wti marcadas, con lo que venga por defecto. Para
   wti eso es m02, cuyo .pre acaba en 12/2010: la ventana no cuadra.    */
static gboolean caso_malo( GtkWindow *w, gpointer d )
{
    NuevoCaso  *x = d;
    GtkWidget  *ci = llamado( GTK_WIDGET(w), "incluye-ipc" );
    GtkWidget  *cw = llamado( GTK_WIDGET(w), "incluye-wti" );
    GtkWidget  *mi = llamado( GTK_WIDGET(w), "modelo-ipc" );
    GtkWidget  *mw = llamado( GTK_WIDGET(w), "modelo-wti" );
    const char *s;

    if ( mi && ( s = gtk_combo_box_get_active_id( GTK_COMBO_BOX(mi) ) ) )
        g_snprintf( x->def_ipc, sizeof x->def_ipc, "%s", s );
    if ( mw && ( s = gtk_combo_box_get_active_id( GTK_COMBO_BOX(mw) ) ) )
        g_snprintf( x->def_wti, sizeof x->def_wti, "%s", s );
    x->puesto = ci && cw;
    if ( ci ) gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(ci), TRUE );
    if ( cw ) gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cw), TRUE );
    gtk_dialog_response( GTK_DIALOG(w), GTK_RESPONSE_OK );
    return TRUE;
}

/* El segundo: wti con m01 --la misma ventana que ipc--, wti PRIMERO, y un
   titulo. La razon se deja en blanco: tiene que verse «sin razón».     */
static gboolean caso_bueno( GtkWindow *w, gpointer d )
{
    GtkWidget *mw = llamado( GTK_WIDGET(w), "modelo-wti" );
    GtkWidget *up = llamado( GTK_WIDGET(w), "sube-wti" );
    GtkWidget *ti = llamado( GTK_WIDGET(w), "titulo" );

    (void) d;
    if ( mw ) gtk_combo_box_set_active_id( GTK_COMBO_BOX(mw), "m01" );
    if ( up ) gtk_button_clicked( GTK_BUTTON(up) );
    if ( ti ) gtk_entry_set_text( GTK_ENTRY(ti), "inflación y petróleo" );
    gtk_dialog_response( GTK_DIALOG(w), GTK_RESPONSE_OK );
    return TRUE;
}

/* Derivar: lo que propone el dialogo, y aceptarlo tal cual. */
static gboolean deriva_tal_cual( GtkWindow *w, gpointer d )
{
    NuevoCaso  *x = d;
    GtkWidget  *mi = llamado( GTK_WIDGET(w), "modelo-ipc" );
    GtkWidget  *ci = llamado( GTK_WIDGET(w), "incluye-ipc" );
    GtkWidget  *cw = llamado( GTK_WIDGET(w), "incluye-wti" );
    const char *s;

    x->puesto = ci && cw && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(ci) ) &&
                gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cw) );
    if ( mi && ( s = gtk_combo_box_get_active_id( GTK_COMBO_BOX(mi) ) ) )
        g_snprintf( x->def_ipc, sizeof x->def_ipc, "%s", s );
    gtk_dialog_response( GTK_DIALOG(w), GTK_RESPONSE_OK );
    return TRUE;
}

static Proyecto *relee_manifiesto( void )
{
    Proyecto *q = g_new0( Proyecto, 1 );
    PrError   e;

    if ( pr_leer( A.p->path, q, &e ) != 0 ) { g_free( q ); return NULL; }
    return q;
}

static void casos( void )
{
    NuevoCaso x = { "", "", FALSE };
    char      w1[PR_ID], w2[PR_ID], id[PR_ID], ruta[PR_RUTA];
    PrError   e;
    gchar    *s, **v;

    /* --- el terreno: wti con dos modelos «estimados» ------------------- */
    {
    gchar *pre_w = ruta_de( "wti", "", "m00", ".inp" );
    gchar *corto = ruta_de( "ipc", "hasta-2010", "m00", ".inp" );

    modelo_a_mano( "wti", pre_w, w1, sizeof w1 );     /* la ventana buena */
    modelo_a_mano( "wti", corto, w2, sizeof w2 );     /* acaba en 12/2010 */
    g_free( pre_w ); g_free( corto );
    }
    check( !strcmp( w1, "m01" ) && !strcmp( w2, "m02" ),
           "(la prueba prepara wti/m01 y wti/m02)", w2 );
    atsw_guarda( &A, &e );
    atsw_refresca( &A );

    /* --- «Nuevo caso…»: la ventana que no cuadra se rechaza ------------ */
    olvida();
    responde( "Nuevo caso", caso_malo, &x );
    responde( "Nuevo caso", caso_bueno, NULL );
    gtk_button_clicked( GTK_BUTTON(A.b_nuevo_caso) );
    todo_atendido( "«Nuevo caso…» tenia que abrir su dialogo dos veces" );

    check( x.puesto, "el dialogo ofrece ipc y wti, que tienen .pre", NULL );
    check( !strcmp( x.def_ipc, "m01" ),
           "por defecto, el ELEGIDO de la serie (ipc/m01)", x.def_ipc );
    check( !strcmp( x.def_wti, "m02" ),
           "sin elegido, el ultimo estimado (wti/m02)", x.def_wti );
    check( dijo( "No es un caso" ) && dijo( "misma fecha" ),
           "la ventana que no cuadra se rechaza, y se dice por que",
           todo_lo_dicho() );
    check( strstr( ultimo.visto, "misma fecha" ) != NULL,
           "y el dialogo sigue abierto con el motivo a la vista", ultimo.visto );
    check( A.p->nca == 1 && pr_caso_idx( A.p, "C1" ) == 0,
           "el segundo intento, con wti/m01, da de alta C1", barra() );
    check( strstr( barra(), "Caso C1 dado de alta: wti/m01, ipc/m01" ) != NULL,
           "y la barra dice que entra, EN SU ORDEN", barra() );

    s = filas( A.l_casos, CA_ID );
    check( !strcmp( s, "C1" ), "C1 sale en la seccion CASOS", s );
    g_free( s );
    check( A.viendo_caso && !strcmp( gtk_stack_get_visible_child_name(
                                         GTK_STACK(A.pila) ), "caso" ),
           "y la derecha enseña el caso", NULL );
    s = filas( A.c_entradas, EN_SERIE );
    check( !strcmp( s, "wti|ipc" ), "las entradas, en el orden que se dio", s );
    g_free( s );
    s = filas( A.c_entradas, EN_MODELO );
    check( !strcmp( s, "m01|m01" ), "cada una con su modelo", s );
    g_free( s );
    s = filas( A.c_entradas, EN_PRE );
    check( !strcmp( s, "igual|igual" ), "y los .pre como en el alta", s );
    g_free( s );
    s = filas( A.c_entradas, EN_HOY );
    check( !strcmp( s, "—|m01" ), "el elegido de hoy, o «—» si no hay", s );
    g_free( s );
    check( strstr( gtk_label_get_text( GTK_LABEL(A.c_cabeza) ), "sin razón" ) &&
           strstr( gtk_label_get_text( GTK_LABEL(A.c_cabeza) ),
                   "inflación y petróleo" ),
           "la cabecera: el titulo, y «sin razón» a la vista",
           gtk_label_get_text( GTK_LABEL(A.c_cabeza) ) );
    s = lee( A.p->path );
    check( strstr( s, "entradas: wti/m01@" ) && strstr( s, " ipc/m01@" ),
           "el manifiesto guarda las entradas en orden y con su hash", s );
    g_free( s );

    /* --- «Abrir en drtran» --------------------------------------------- */
    atsw_caso_lanza( &A, "atsw_hijo_falso" );
    v = lo_que_recibio();
    check( v && g_strv_length( v ) >= 5 && !strcmp( v[0], "--proyecto" ) &&
           !strcmp( v[1], A.p->path ) && !strcmp( v[2], "--caso" ) &&
           !strcmp( v[3], "C1" ) && !strcmp( v[4], "FIN" ),
           "sin corrida marcada, al hijo le llega «--proyecto P --caso C1»",
           v ? g_strjoinv( " ", v ) : barra() );
    g_strfreev( v );
    {
    gchar *p = atsw_programa( "drtran_gui" );
    gchar *q = g_strdup_printf( "%s, con el caso C1.", p ? p : "drtran_gui" );

    olvida();
    hijos_sin_pantalla( TRUE );
    gtk_button_clicked( GTK_BUTTON(A.b_c_drtran) );
    hijos_sin_pantalla( FALSE );
    check( p && dijo( q ), "«Abrir en drtran» lanza el drtran_gui del arbol",
           todo_lo_dicho() );
    g_free( p ); g_free( q );
    }

    /* --- las corridas, como las dejaria drtran_gui --------------------- */
    pr_corrida_nueva( A.p, "C1", NULL, id, sizeof id, ruta, sizeof ruta, &e );
    {
    gchar *dir = g_path_get_dirname( ruta );

    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
    }
    g_file_set_contents( ruta, "DRTRAN 1.0\n\nLog-likelihood = -767.420000\n\n",
                         -1, NULL );
    pr_corrida_nueva( A.p, "C1", "c00", id, sizeof id, NULL, 0, &e );
    atsw_guarda( &A, &e );
    atsw_refresca( &A );
    {
    GString      *g = g_string_new( NULL );

    arbol_r( gtk_tree_view_get_model( GTK_TREE_VIEW(A.c_corridas) ), NULL, 0, g );
    check( !strcmp( g->str, "c00 []\n  c01 []\n" ),
           "las corridas, como arbol: c01 cuelga de c00", g->str );
    g_string_free( g, TRUE );
    }
    s = celda_arbol( A.c_corridas, CO_ID, "c00", CO_LOGL );
    check( s && !strcmp( s, "-767.42" ), "la logL de c00 sale de su .out", s );
    g_free( s );
    s = celda_arbol( A.c_corridas, CO_ID, "c01", CO_LOGL );
    check( s && !strcmp( s, "—" ), "sin .out, «—»", s );
    g_free( s );
    s = celda_arbol( A.c_corridas, CO_ID, "c01", CO_RAZON );
    check( s && !strcmp( s, "(sin razón)" ), "y sin razon se ve como sin razon", s );
    g_free( s );

    /* --- LA PUERTA DIAGONAL: la conjunta contra la suma de las univariantes */
    {
    const PrCaso *c = pr_caso_ver( A.p, "C1" );
    double        suma = 0.0;
    char          r2[PR_RUTA], r3[PR_RUTA], esperado[64];
    int           k;

    /* La logL de cada entrada, del .out de fue de su modelo. Si alguna no
       lo tiene todavia, se le escribe uno con su logelf: lo que se prueba
       es la suma, no fue.                                              */
    for ( k = 0; c && k < c->nen; k++ )
        {
        char    o[PR_RUTA];
        FueOut *fo = g_new0( FueOut, 1 );

        pr_ruta( A.p, c->en[k].serie, c->muestra, c->en[k].modelo, ".out", o,
                 sizeof o );
        if ( !fueout_read( o, fo ) || !fo->tiene_logelf )
            {
            gchar *t = g_strdup_printf( "logelf: %.10f\n", -100.0 * ( k + 1 ) );

            g_file_set_contents( o, t, -1, NULL );
            g_free( t );
            memset( fo, 0, sizeof *fo );
            fueout_read( o, fo );
            }
        suma += fo->logelf;
        g_free( fo );
        }

    /* c02: diagonal, y la reproduce. c03: diagonal, y no. */
    pr_corrida_nueva( A.p, "C1", "c01", id, sizeof id, r2, sizeof r2, &e );
    pr_corrida_nueva( A.p, "C1", "c01", id, sizeof id, r3, sizeof r3, &e );
    {
    gchar *t2 = g_strdup_printf( "DRTRAN 1.0\n\nTransfer function orders:\n"
                                 "  input 1: b = 0, r = 0, s = -1\n\n"
                                 "Log-likelihood = %.6f\n", suma );
    gchar *t3 = g_strdup_printf( "DRTRAN 1.0\n\nTransfer function orders:\n"
                                 "  input 1: b = 0, r = 0, s = -1\n\n"
                                 "Log-likelihood = %.6f\n", suma + 0.5 );

    g_file_set_contents( r2, t2, -1, NULL );
    g_file_set_contents( r3, t3, -1, NULL );
    g_free( t2 ); g_free( t3 );
    }
    atsw_guarda( &A, &e );
    atsw_refresca( &A );

    s = celda_arbol( A.c_corridas, CO_ID, "c02", CO_PUERTA );
    check( s && !strcmp( s, "cuadra" ),
           "una corrida diagonal que reproduce la suma de las univariantes: cuadra", s );
    g_free( s );
    s = celda_arbol( A.c_corridas, CO_ID, "c03", CO_PUERTA );
    check( s && !strcmp( s, "NO cuadra (+0.5000)" ),
           "una diagonal que no la reproduce: NO cuadra, y por cuanto", s );
    g_free( s );
    snprintf( esperado, sizeof esperado, "%+.2f", -767.42 - suma );
    s = celda_arbol( A.c_corridas, CO_ID, "c00", CO_PUERTA );
    check( s && !strcmp( s, esperado ),
           "con transferencia: lo que gana sobre los univariantes", s );
    g_free( s );
    s = celda_arbol( A.c_corridas, CO_ID, "c01", CO_PUERTA );
    check( s && !strcmp( s, "—" ), "sin .out, la puerta no se inventa", s );
    g_free( s );

    /* Se quitan: lo que viene despues cuenta las corridas que habia. */
    pr_corrida_borra( A.p, "C1", "c03", &e );
    pr_corrida_borra( A.p, "C1", "c02", &e );
    g_remove( r2 ); g_remove( r3 );
    atsw_guarda( &A, &e );
    atsw_refresca( &A );
    }

    check( marca_arbol( A.c_corridas, CO_ID, "c01" ), "se puede marcar c01", NULL );
    atsw_caso_lanza( &A, "atsw_hijo_falso" );
    v = lo_que_recibio();
    check( v && g_strv_length( v ) >= 7 && !strcmp( v[2], "--caso" ) &&
           !strcmp( v[3], "C1" ) && !strcmp( v[4], "--corrida" ) &&
           !strcmp( v[5], "c01" ) && !strcmp( v[6], "FIN" ),
           "con c01 marcada: «--proyecto P --caso C1 --corrida c01»",
           v ? g_strjoinv( " ", v ) : barra() );
    g_strfreev( v );

    /* --- Elegir y Razón, guardados ------------------------------------- */
    responde( "La corrida elegida", escribe_texto, "el más simple" );
    gtk_button_clicked( GTK_BUTTON(A.b_c_elegir) );
    todo_atendido( "«Elegir» tenia que pedir la razon" );
    check( !strcmp( barra(), "Corrida elegida y guardada." ),
           "«Elegir» una corrida lo dice", barra() );
    s = celda( A.l_casos, CA_ID, "C1", CA_ELEGIDA );
    check( s && !strcmp( s, "c01 ★" ), "CASOS enseña la elegida con su ★", s );
    g_free( s );
    s = celda_arbol( A.c_corridas, CO_ID, "c01", CO_ESTRELLA );
    check( s && !strcmp( s, "★" ), "y el arbol tambien", s );
    g_free( s );

    marca_arbol( A.c_corridas, CO_ID, "c00" );
    responde( "El porqué de esta corrida", escribe_texto, "la diagonal" );
    gtk_button_clicked( GTK_BUTTON(A.b_c_razon) );
    todo_atendido( "«Razón…» de la corrida tenia que pedirla" );
    responde( "El porqué de este caso", escribe_texto, "el WTI adelanta al IPC" );
    gtk_button_clicked( GTK_BUTTON(A.b_c_razon_caso) );
    todo_atendido( "«Razón del caso…» tenia que pedirla" );
    {
    Proyecto *q = relee_manifiesto();
    const PrCorrida *c0 = q ? pr_corrida_ver( q, "C1", "c00" ) : NULL;
    const PrCorrida *c1 = q ? pr_corrida_ver( q, "C1", "c01" ) : NULL;
    const PrCaso    *ca = q ? pr_caso_ver( q, "C1" ) : NULL;

    check( q && !strcmp( pr_corrida_elegida( q, "C1" ), "c01" ) && c1 &&
           !strcmp( c1->razon_elegido, "el más simple" ),
           "la elegida y su razon quedan en el manifiesto", NULL );
    check( c0 && !strcmp( c0->razon, "la diagonal" ),
           "la razon de la corrida queda en el manifiesto", c0 ? c0->razon : NULL );
    check( ca && !strcmp( ca->razon, "el WTI adelanta al IPC" ),
           "y la del caso tambien", ca ? ca->razon : NULL );
    g_free( q );
    }
    check( strstr( gtk_label_get_text( GTK_LABEL(A.c_cabeza) ),
                   "el WTI adelanta al IPC" ) != NULL,
           "la cabecera enseña la razon del caso",
           gtk_label_get_text( GTK_LABEL(A.c_cabeza) ) );

    /* --- Borrar: lo que no se puede, dicho ----------------------------- */
    marca_arbol( A.c_corridas, CO_ID, "c00" );
    gtk_button_clicked( GTK_BUTTON(A.b_c_borrar) );
    todo_atendido( "(borrar c00)" );
    check( strstr( barra(), "De esa corrida cuelga «C1/c01»" ) != NULL,
           "una corrida con hijas no se borra, y se dice cual cuelga", barra() );
    gtk_button_clicked( GTK_BUTTON(A.b_c_borrar_caso) );
    todo_atendido( "(borrar C1)" );
    check( strstr( barra(), "En ese caso está la corrida" ) != NULL,
           "un caso con corridas no se borra, y se dice por que", barra() );

    /* Una hoja del arbol si: pregunta, y se va con sus ficheros. */
    pr_corrida_nueva( A.p, "C1", "c01", id, sizeof id, ruta, sizeof ruta, &e );
    g_file_set_contents( ruta, "Log-likelihood = -760.0\n", -1, NULL );
    {
    char dag[PR_RUTA];

    pr_corrida_ruta( A.p, "C1", "c02", ".dag", dag, sizeof dag );
    g_file_set_contents( dag, "red\n", -1, NULL );
    atsw_guarda( &A, &e );
    atsw_refresca( &A );
    marca_arbol( A.c_corridas, CO_ID, "c02" );
    responde( "¿Borro la corrida c02", contesta, GINT_TO_POINTER(GTK_RESPONSE_OK) );
    gtk_button_clicked( GTK_BUTTON(A.b_c_borrar) );
    todo_atendido( "«Borrar corrida…» tenia que preguntar" );
    check( strstr( barra(), "corrida c02 borrada, con sus ficheros" ) != NULL,
           "borrar una corrida lo dice", barra() );
    check( !g_file_test( ruta, G_FILE_TEST_EXISTS ) &&
           !g_file_test( dag, G_FILE_TEST_EXISTS ),
           "y su .out y su .dag ya no estan", ruta );
    check( pr_corrida_idx( A.p, "C1", "c02" ) < 0, "ni en el manifiesto", NULL );
    }

    /* --- los dos desfases ---------------------------------------------- */
    {
    gchar *pre = ruta_de( "ipc", "", "m01", ".pre" );
    FILE  *f = g_fopen( pre, "ab" );

    if ( f ) { fputc( '\n', f ); fclose( f ); }
    g_free( pre );
    }
    atsw_refresca( &A );
    s = celda( A.l_casos, CA_ID, "C1", CA_MARCA );
    check( s && !strcmp( s, "⚠" ), "un .pre de entrada cambiado: ⚠ en CASOS", s );
    g_free( s );
    s = filas( A.c_entradas, EN_PRE );
    check( !strcmp( s, "igual|cambió" ), "y la entrada dice cual cambio", s );
    g_free( s );
    check( gtk_widget_get_visible( A.ver_desfase ) &&
           strstr( gtk_label_get_text( GTK_LABEL(A.ver_desfase) ),
                   "C1: ipc/m01 (cambió)" ),
           "el veredicto de abajo dice que caso y que entrada",
           gtk_label_get_text( GTK_LABEL(A.ver_desfase) ) );

    /* Otro elegido hoy para ipc, con el boton de siempre: una nota. */
    marca( A.l_series, S_ID, "ipc" );
    check( !A.viendo_caso, "marcar una serie vuelve a sus modelos", NULL );
    marca( A.l_modelos, M_ID, "m03" );
    responde( "El modelo elegido", escribe_texto, "otro para prever" );
    gtk_button_clicked( GTK_BUTTON(A.b_elegir) );
    todo_atendido( "«Elegir» tenia que pedir la razon" );
    check( marca( A.l_casos, CA_ID, "C1" ) && A.viendo_caso,
           "y marcar C1 vuelve al caso", NULL );
    s = filas( A.c_entradas, EN_HOY );
    check( !strcmp( s, "—|m03" ), "el elegido de hoy de ipc es m03", s );
    g_free( s );
    s = celda( A.c_entradas, EN_SERIE, "ipc", EN_NOTA );
    check( s && !strcmp( s, "otro elegido hoy (nota)" ),
           "y es una nota, no una alarma", s );
    g_free( s );
    s = celda( A.c_entradas, EN_SERIE, "wti", EN_NOTA );
    check( s && !*s, "wti no tiene elegido: sin nota", s );
    g_free( s );
    check( gtk_widget_get_visible( A.ver_nota ) &&
           strstr( gtk_label_get_text( GTK_LABEL(A.ver_nota) ),
                   "ipc entra con m01 y hoy su elegido es m03" ),
           "el veredicto de abajo lleva la nota",
           gtk_label_get_text( GTK_LABEL(A.ver_nota) ) );

    /* --- Derivar: C2, colgado de C1, con los .pre de hoy --------------- */
    x.puesto = FALSE; x.def_ipc[0] = '\0';
    responde( "Derivar caso de C1", deriva_tal_cual, &x );
    gtk_button_clicked( GTK_BUTTON(A.b_c_derivar) );
    todo_atendido( "«Derivar caso…» tenia que abrir su dialogo" );
    check( x.puesto, "derivar trae marcadas las series del caso", NULL );
    check( !strcmp( x.def_ipc, "m01" ),
           "el elegido de hoy (m03) no tiene .pre: se queda el del caso",
           x.def_ipc );
    {
    const PrCaso *c1 = pr_caso_ver( A.p, "C1" ), *c2 = pr_caso_ver( A.p, "C2" );

    check( c2 && !strcmp( c2->padre, "C1" ), "derivar crea C2, con padre C1",
           barra() );
    check( c1 && c2 && c2->nen == 2 && !strcmp( c2->en[0].serie, "wti" ) &&
           !strcmp( c2->en[1].serie, "ipc" ) &&
           strcmp( c2->en[1].sha, c1->en[1].sha ) != 0,
           "en el mismo orden, y con el hash del .pre de hoy", NULL );
    }
    check( strstr( barra(), "Caso C2, derivado de C1" ) != NULL,
           "y la barra lo dice", barra() );
    s = celda( A.l_casos, CA_ID, "C2", CA_MARCA );
    check( s && !strcmp( s, "" ), "C2 no esta desfasado: la nota no es ⚠", s );
    g_free( s );
    check( strstr( gtk_label_get_text( GTK_LABEL(A.c_cabeza) ), "derivado de C1" )
           != NULL, "su cabecera dice de donde se derivo",
           gtk_label_get_text( GTK_LABEL(A.c_cabeza) ) );

    /* Un caso sin corridas si se borra. */
    responde( "¿Borro el caso C2", contesta, GINT_TO_POINTER(GTK_RESPONSE_OK) );
    gtk_button_clicked( GTK_BUTTON(A.b_c_borrar_caso) );
    todo_atendido( "«Borrar caso…» tenia que preguntar" );
    check( pr_caso_idx( A.p, "C2" ) < 0 && strstr( barra(), "Caso C2 borrado" ),
           "un caso sin corridas se borra y lo dice", barra() );

    /* --- un modelo que es entrada no se borra -------------------------- */
    marca( A.l_series, S_ID, "wti" );
    marca( A.l_modelos, M_ID, "m01" );
    on_borrar( NULL, &A );
    todo_atendido( "(borrar wti/m01)" );
    check( strstr( barra(), "entrada del caso «C1»" ) != NULL &&
           existe( "wti", "", "m01", ".pre" ),
           "borrar la entrada de un caso se niega, nombrando el caso", barra() );
}

/* ======================================================================== */
/* LAS CORRIDAS DE ANTES (docs/DISENO-casos.md §5)                           */
/*                                                                           */
/* Se fabrica lo que dejaba drtran_gui antes de los casos: modelos de la     */
/* serie de salida, encadenados, sin .inp, con un .out cuya cabecera dice    */
/* que .pre entraron, y su .dag.                                            */
/* ======================================================================== */

static void legado( const char *padre, const char *y, const char *x,
                    const char *fin, gboolean cns, const char *razon,
                    char *id, size_t nid )
{
    PrError e;
    gchar  *out, *f;

    pr_deriva( A.p, "ipc", "", padre, id, nid, NULL, 0, &e );
    pr_razon( A.p, "ipc", "", id, razon, &e );
    out = g_strdup_printf( "DRTRAN 1.0: Box-Jenkins transfer function models "
                           "by exact ML%s%sModel            : ipc_wti%s"
                           "Series           : 2 (1 output + 1 input(s))%s"
                           "Output (Y)       : %s%s"
                           "Input  (X1)      : %s%s"
                           "Frequency        : 12%s%s"
                           "Log-likelihood = -700.500000%s",
                           fin, fin, fin, fin, y, fin, x, fin, fin, fin, fin );
    f = ruta_de( "ipc", "", id, ".out" );
    g_file_set_contents( f, out, -1, NULL );
    g_free( f );
    f = ruta_de( "ipc", "", id, ".dag" );
    g_file_set_contents( f, "1 2 0 0 0\n", -1, NULL );
    g_free( f );
    if ( cns )
        { f = ruta_de( "ipc", "", id, ".cns" );
          g_file_set_contents( f, "\n", -1, NULL ); g_free( f ); }
    g_free( out );
}

static void legados( void )
{
    char     l1[PR_ID], l2[PR_ID], l3[PR_ID], l4[PR_ID], m5[PR_ID], ruta[PR_RUTA];
    gchar   *y = ruta_de( "ipc", "", "m01", ".pre" );
    gchar   *x = ruta_de( "wti", "", "m01", ".pre" );
    gchar   *fuera = g_build_filename( T, "fuera", "work", "OTRA_m01.pre", NULL );
    PrError  e;
    const PrCaso *c;
    gchar   *s;

    atsw_refresca( &A );
    check( !gtk_widget_get_visible( A.caja_legado ),
           "sin corridas viejas, no hay linea de veredicto", NULL );

    legado( NULL, y, x, "\n",   TRUE,  "la diagonal",    l1, sizeof l1 );
    legado( l1,   y, x, "\r\n", FALSE, "fuera omega_1",  l2, sizeof l2 );
    legado( NULL, y, fuera, "\n", FALSE, "con otra serie", l3, sizeof l3 );
    /* Una que se podria convertir, pero de ella cuelga un modelo DE VERDAD
       de fue (con su .inp): moverla dejaria a ese modelo sin padre. */
    legado( NULL, y, x, "\n", FALSE, "sostiene a otro", l4, sizeof l4 );
    pr_deriva( A.p, "ipc", "", l4, m5, sizeof m5, NULL, 0, &e );
    {
    gchar *f5 = ruta_de( "ipc", "", m5, ".inp" );

    g_file_set_contents( f5, "un .inp de fue\n", -1, NULL );
    g_free( f5 );
    }
    pr_elige( A.p, "ipc", "", l2, "la buena", &e );
    atsw_guarda( &A, &e );
    atsw_refresca( &A );

    check( atsw_legados( A.p, NULL, 0 ) == 4,
           "las cuatro corridas viejas se reconocen (.out y .dag, sin .inp), "
           "y el modelo de fue con .inp no", NULL );
    check( gtk_widget_get_visible( A.caja_legado ) &&
           strstr( gtk_label_get_text( GTK_LABEL(A.ver_legado) ),
                   "4 corridas de drtran registradas como modelos de ipc" ),
           "y el veredicto lo dice",
           gtk_label_get_text( GTK_LABEL(A.ver_legado) ) );

    /* Sin confirmar no se toca nada. */
    responde( "¿Convierto en casos", contesta, GINT_TO_POINTER(GTK_RESPONSE_CANCEL) );
    gtk_button_clicked( GTK_BUTTON(A.b_convertir) );
    todo_atendido( "«Convertir en casos…» tenia que preguntar" );
    check( atsw_legados( A.p, NULL, 0 ) == 4 && A.p->nca == 1,
           "si no se confirma, no se convierte nada", barra() );

    olvida();
    responde( "¿Convierto en casos", contesta, GINT_TO_POINTER(GTK_RESPONSE_OK) );
    gtk_button_clicked( GTK_BUTTON(A.b_convertir) );
    todo_atendido( "«Convertir en casos…» tenia que preguntar" );
    nota( "barra (conversion): %s", barra() );
    check( strstr( barra(), "2 corridas convertidas" ) &&
           strstr( barra(), "no es un modelo de este proyecto" ),
           "la barra dice que se convirtio y que no, y por que", barra() );

    c = pr_caso_ver( A.p, "C2" );
    check( c && c->nen == 2 && !strcmp( c->en[0].serie, "ipc" ) &&
           !strcmp( c->en[0].modelo, "m01" ) && !strcmp( c->en[1].serie, "wti" ) &&
           !strcmp( c->en[1].modelo, "m01" ) && c->en[0].sha[0] &&
           !c->razon[0] && !strcmp( c->motor, "drtran" ),
           "un caso nuevo con las entradas del .out, en su orden y sin razon "
           "inventada", NULL );
    {
    const PrCorrida *c0 = pr_corrida_ver( A.p, "C2", "c00" );
    const PrCorrida *c1 = pr_corrida_ver( A.p, "C2", "c01" );

    check( c0 && c1 && !c0->padre[0] && !strcmp( c1->padre, "c00" ),
           "las corridas conservan el linaje: c01 cuelga de c00", NULL );
    check( c0 && c1 && !strcmp( c0->razon, "la diagonal" ) &&
           !strcmp( c1->razon, "fuera omega_1" ),
           "y la razon de cada modelo viejo", NULL );
    check( !strcmp( pr_corrida_elegida( A.p, "C2" ), "c01" ) && c1 &&
           !strcmp( c1->razon_elegido, "la buena" ),
           "el que era el elegido es la elegida del caso, con su porque", NULL );
    }
    {
    static const struct { const char *co, *ext; } f[] = {
        { "c00", ".out" }, { "c00", ".dag" }, { "c00", ".cns" },
        { "c01", ".out" }, { "c01", ".dag" } };
    int i;

    for ( i = 0; i < (int) G_N_ELEMENTS(f); i++ )
        {
        pr_corrida_ruta( A.p, "C2", f[i].co, f[i].ext, ruta, sizeof ruta );
        check( g_file_test( ruta, G_FILE_TEST_EXISTS ),
               "los ficheros se mueven a _casos/C2/work", ruta );
        }
    }
    check( pr_modelo_idx( A.p, "ipc", "", l1 ) < 0 &&
           pr_modelo_idx( A.p, "ipc", "", l2 ) < 0 &&
           !existe( "ipc", "", l1, ".out" ) && !existe( "ipc", "", l2, ".dag" ),
           "los modelos viejos se van del manifiesto, y sus ficheros de alli",
           NULL );
    {
    int k = pr_modelo_idx( A.p, "ipc", "", l3 );

    check( k >= 0 && !strcmp( A.p->m[k].razon, "con otra serie" ) &&
           existe( "ipc", "", l3, ".out" ),
           "el que nombra un .pre de fuera se queda como estaba", NULL );
    k = pr_modelo_idx( A.p, "ipc", "", l4 );
    check( k >= 0 && existe( "ipc", "", l4, ".out" ) &&
           existe( "ipc", "", l4, ".dag" ) &&
           pr_modelo_idx( A.p, "ipc", "", m5 ) >= 0,
           "el que sostiene a un modelo de fue tambien, y el modelo con el", NULL );
    check( strstr( barra(), "de ella cuelga" ) != NULL,
           "y la barra dice por que", barra() );
    }
    check( strstr( gtk_label_get_text( GTK_LABEL(A.ver_legado) ),
                   "2 corridas de drtran registradas como modelos de ipc" ) != NULL,
           "y el veredicto cuenta el que queda",
           gtk_label_get_text( GTK_LABEL(A.ver_legado) ) );
    {
    Proyecto *q = relee_manifiesto();

    check( q && pr_caso_idx( q, "C2" ) >= 0 && pr_corrida_idx( q, "C2", "c01" ) >= 0,
           "el manifiesto convertido se vuelve a leer", NULL );
    g_free( q );
    }
    s = filas( A.l_casos, CA_ID );
    check( !strcmp( s, "C1|C2" ), "y el caso sale en CASOS", s );
    g_free( s );
    g_free( y ); g_free( x ); g_free( fuera );
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    char            manifiesto[PR_RUTA];
    gchar          *t;
    const char     *csv;

    if ( argc < 2 ) { fprintf( stderr, "uso: test_gui <csv>\n" ); return 2; }
    csv = argv[1];

    /* El directorio de la prueba, y la cache del vistazo DENTRO: lo que
       la prueba escribe no puede quedar en la del analista.            */
    t = g_dir_make_tmp( "atsw_gui_XXXXXX", NULL );
    if ( t == NULL ) { printf( "FAIL: no pude crear el temporal\n" ); return 1; }
    g_snprintf( T, sizeof T, "%s", t );
    g_free( t );
    t = g_build_filename( T, "cache", NULL );
    g_setenv( "XDG_CACHE_HOME", t, TRUE );
    g_free( t );
    t = g_build_filename( T, "hijo.txt", NULL );
    g_snprintf( HIJO_LOG, sizeof HIJO_LOG, "%s", t );
    g_setenv( "ATSW_HIJO_LOG", t, TRUE );
    g_free( t );

    if ( !gtk_init_check( &argc, &argv ) )
        {
        printf( "no hay servidor grafico: la prueba de la ventana no se corre\n" );
        borra_arbol( T );
        return 0;
    }

    g_thread_new( "vigilante", vigilante, GINT_TO_POINTER(170) );
    historial = g_ptr_array_new_with_free_func( g_free );
    contestados = g_hash_table_new( NULL, NULL );
    app = gtk_application_new( "org.atsw.gui.prueba", G_APPLICATION_NON_UNIQUE );
    g_application_register( G_APPLICATION(app), NULL, NULL );
    activate( app, &A );
    g_signal_connect( A.estado, "notify::label", G_CALLBACK(on_barra), NULL );
    g_timeout_add( 30, vigila, NULL );
    pump( 200 );

    fase( "sin_proyecto" ); sin_proyecto();
    fase( "hermanos" ); hermanos();
    fase( "proyecto_nuevo" ); proyecto_nuevo( manifiesto, sizeof manifiesto );
    if ( A.hay )
        {
        fase( "proyecto_info" ); proyecto_info( manifiesto );
        fase( "datos" ); datos( csv );
        if ( A.p->ns == 2 )
            {
            fase( "serie" ); serie();
            fase( "vistazo_prueba" ); vistazo_prueba();
            fase( "modelo_nuevo" ); modelo_nuevo();
            fase( "envios" ); envios();
            fase( "hijo_falso" ); hijo_falso();
            fase( "editor" ); editor();
            fase( "iterar_elegir" ); iterar_elegir();
            fase( "menu_modelo" ); menu_modelo_prueba();
            fase( "muestra" ); muestra();
            fase( "linaje" ); linaje();
            fase( "borrar" ); borrar();
            fase( "abrir" ); abrir( manifiesto );
            fase( "casos" ); casos();
            fase( "legados" ); legados();
            }
        }

    /* Lo que se escribio, fuera. Con ATSW_GUI_GUARDA se deja, para mirar
       el proyecto que dejo una prueba que fallo.                       */
    if ( g_getenv( "ATSW_GUI_GUARDA" ) ) printf( "proyecto en %s\n", T );
    else                                 borra_arbol( T );

    printf( "\n%d fallos\n", fallos );
    return fallos == 0 ? 0 : 1;
}
