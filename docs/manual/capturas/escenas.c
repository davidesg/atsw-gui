/*
 * escenas.c -- las capturas del manual, sacadas del programa de verdad.
 *
 * Es un modulo de GTK: se carga en cualquiera de los GUIs con
 *
 *   GTK_MODULES=/ruta/libescenas.so ESCENA=guion.txt FOTOS=dir  programa ...
 *
 * y, desde DENTRO del proceso, sigue un guion: pulsa botones por su rotulo,
 * elige filas, abre menus, rellena campos y fotografia ventanas. No toca el
 * codigo de los GUIs: si un rotulo cambia, el guion falla diciendo cual, y
 * la captura no se queda vieja sin que nadie se entere.
 *
 * Se corre con GDK_BACKEND=broadway (capturas.sh): las ventanas se dibujan
 * fuera de la pantalla, en un servidor broadwayd, y nadie ve nada.
 *
 * EL GUION, una orden por linea; # comenta. Los argumentos van separados por
 * " | " cuando son mas de uno.
 *
 *   ventana TITULO          las ordenes siguientes, sobre la ventana cuyo
 *                           titulo contiene TITULO (espera a que aparezca)
 *   espera MS
 *   boton ROTULO            pulsa el boton (o el elemento de barra) con ese rotulo
 *   pestana ROTULO          pone la pagina del cuaderno con esa pestana
 *   fila TEXTO [| N]        elige la fila de una tabla que tiene esa celda
 *                           (de la N-esima tabla que la tiene, si hay varias)
 *   doble TEXTO             doble clic (row-activated) en esa fila
 *   derecho TEXTO           clic derecho en esa fila: abre su menu
 *   menu ROTULO             activa el elemento del menu abierto
 *   submenu ROTULO          abre un submenu del menu abierto
 *   entrada N | TEXTO       escribe TEXTO en la N-esima entrada (desde 1)
 *   combo N | TEXTO         elige TEXTO en el N-esimo desplegable
 *   elige TEXTO             elige TEXTO en el desplegable que lo tenga
 *   numero N | VALOR        pone VALOR en el N-esimo campo numerico
 *   campo ETIQUETA | V ...  el campo que esta a la derecha de esa etiqueta
 *                           en una rejilla: una entrada recibe V; un grupo
 *                           de campos numericos (una fecha) recibe V1 | V2
 *   casilla ROTULO | 0/1    marca o desmarca la casilla cuyo rotulo empieza asi
 *   fichero RUTA            en un selector de ficheros: elige RUTA
 *   foto NOMBRE             fotografia la ventana actual en FOTOS/NOMBRE.png
 *   fotomenu NOMBRE         la ventana actual con el menu abierto encima
 *   tamano ANCHO ALTO       cambia el tamano de la ventana actual
 *   cierra                  cierra la ventana actual
 *   ventanas                escribe los titulos de las ventanas abiertas
 *   salir
 *
 * Si una orden no encuentra lo que busca en TIEMPO segundos (10 por
 * defecto), escribe por que y sale con 2.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gchar  **lineas;
static int      nl, il;
static gchar   *titulo;         /* la ventana de las ordenes              */
static GtkWidget *menu_abierto;
static gint64   desde;          /* cuando empezo a esperar la orden actual */
static int      paso_ms = 400;  /* entre orden y orden                    */
static int      clic_x = 40, clic_y = 40;   /* donde se abrio el menu, en la ventana */

static void video_fin( void );

static void fallo( const char *que, const char *arg )
{
    g_printerr( "escenas: linea %d: %s «%s»\n", il + 1, que, arg ? arg : "" );
    video_fin();
    exit( 2 );
}

/* ---- buscar --------------------------------------------------------- */

static GtkWidget *la_ventana( void )
{
    GList *l = gtk_window_list_toplevels(), *i;
    GtkWidget *w = NULL;

    for ( i = l; i; i = i->next )
        {
        GtkWindow *v = i->data;
        const char *t = gtk_window_get_title( v );

        if ( gtk_widget_get_mapped( GTK_WIDGET(v) ) && t
             && (titulo == NULL || strstr( t, titulo )) )
            w = GTK_WIDGET(v);
        }
    g_list_free( l );

    return w;
}

typedef gboolean (*Prueba)( GtkWidget *w, const char *arg );

static void recorre( GtkWidget *w, Prueba p, const char *arg, GtkWidget **res )
{
    if ( *res ) return;
    if ( p( w, arg ) ) { *res = w; return; }
    if ( GTK_IS_CONTAINER(w) )
        {
        GList *h = gtk_container_get_children( GTK_CONTAINER(w) ), *i;

        for ( i = h; i && !*res; i = i->next )
            recorre( i->data, p, arg, res );
        g_list_free( h );
        }
    /* el «+» de un cuaderno no es hijo suyo: es un widget de accion */
    if ( GTK_IS_NOTEBOOK(w) )
        {
        GtkWidget *x = gtk_notebook_get_action_widget( GTK_NOTEBOOK(w), GTK_PACK_END );
        GtkWidget *y = gtk_notebook_get_action_widget( GTK_NOTEBOOK(w), GTK_PACK_START );

        if ( x ) recorre( x, p, arg, res );
        if ( y ) recorre( y, p, arg, res );
        }
    /* los submenus y popovers no son hijos: se miran aparte */
}

static GtkWidget *busca( GtkWidget *raiz, Prueba p, const char *arg )
{
    GtkWidget *r = NULL;

    if ( raiz ) recorre( raiz, p, arg, &r );

    return r;
}

/* El texto visible de un widget con rotulo: el suyo o el de su GtkLabel. */
static const char *rotulo( GtkWidget *w )
{
    GtkWidget *x = NULL;

    if ( GTK_IS_TOOL_BUTTON(w) )
        {
        const char *t = gtk_tool_button_get_label( GTK_TOOL_BUTTON(w) );
        if ( t ) return t;
        }
    if ( GTK_IS_BUTTON(w) && gtk_button_get_label( GTK_BUTTON(w) ) )
        return gtk_button_get_label( GTK_BUTTON(w) );
    if ( GTK_IS_MENU_ITEM(w) && gtk_menu_item_get_label( GTK_MENU_ITEM(w) ) )
        return gtk_menu_item_get_label( GTK_MENU_ITEM(w) );
    if ( GTK_IS_LABEL(w) )
        return gtk_label_get_text( GTK_LABEL(w) );
    if ( GTK_IS_BIN(w) )
        x = gtk_bin_get_child( GTK_BIN(w) );
    if ( x && GTK_IS_LABEL(x) )
        return gtk_label_get_text( GTK_LABEL(x) );
    if ( x && GTK_IS_CONTAINER(x) )     /* boton con icono y rotulo */
        {
        GList *h = gtk_container_get_children( GTK_CONTAINER(x) ), *i;
        const char *t = NULL;

        for ( i = h; i && !t; i = i->next )
            if ( GTK_IS_LABEL(i->data) )
                t = gtk_label_get_text( GTK_LABEL(i->data) );
        g_list_free( h );
        return t;
        }

    return NULL;
}

/* Igual al rotulo, sin el _ de los atajos de teclado («_Guardar»). */
static gboolean igual( const char *t, const char *arg )
{
    for ( ; *t || *arg; t++, arg++ )
        {
        if ( *t == '_' ) t++;
        if ( *t != *arg ) return FALSE;
        if ( *t == '\0' ) break;
        }

    return TRUE;
}

static gboolean es_boton( GtkWidget *w, const char *arg )
{
    const char *t;

    if ( !(GTK_IS_BUTTON(w) || GTK_IS_TOOL_BUTTON(w)) || !gtk_widget_is_drawable( w ) )
        return FALSE;
    t = rotulo( w );

    return t && igual( t, arg );
}

static gboolean es_item( GtkWidget *w, const char *arg )
{
    const char *t = GTK_IS_MENU_ITEM(w) ? rotulo( w ) : NULL;

    return t && igual( t, arg );
}

/* Una casilla por el principio de su rotulo: los de los episodios llevan
   la z detras, que cambia con cada estimacion. */
static gboolean es_casilla( GtkWidget *w, const char *arg )
{
    return GTK_IS_CHECK_BUTTON(w) && rotulo( w )
           && strncmp( rotulo( w ), arg, strlen( arg ) ) == 0;
}

static gboolean es_cuaderno( GtkWidget *w, const char *arg )
{
    int i, n;

    if ( !GTK_IS_NOTEBOOK(w) || !gtk_widget_is_drawable( w ) ) return FALSE;
    n = gtk_notebook_get_n_pages( GTK_NOTEBOOK(w) );
    for ( i = 0; i < n; i++ )
        {
        GtkWidget *pg = gtk_notebook_get_nth_page( GTK_NOTEBOOK(w), i );
        GtkWidget *tl = gtk_notebook_get_tab_label( GTK_NOTEBOOK(w), pg );
        const char *t = tl ? (GTK_IS_LABEL(tl) ? gtk_label_get_text( GTK_LABEL(tl) ) : rotulo( tl )) : NULL;

        if ( t && strcmp( t, arg ) == 0 )
            return TRUE;
        }

    return FALSE;
}

static int cuantos;     /* para entrada N y combo N */

static gboolean es_entrada_n( GtkWidget *w, const char *arg )
{
    return GTK_IS_ENTRY(w) && !GTK_IS_SPIN_BUTTON(w) && gtk_widget_is_drawable( w )
           && ++cuantos == atoi( arg );
}

static gboolean es_numero_n( GtkWidget *w, const char *arg )
{
    return GTK_IS_SPIN_BUTTON(w) && gtk_widget_is_drawable( w ) && ++cuantos == atoi( arg );
}

static gboolean es_etiqueta( GtkWidget *w, const char *arg )
{
    gchar *t;
    gboolean r;

    if ( !GTK_IS_LABEL(w) || !GTK_IS_GRID(gtk_widget_get_parent( w )) ) return FALSE;
    t = g_strstrip( g_strdup( gtk_label_get_text( GTK_LABEL(w) ) ) );
    r = strcmp( t, arg ) == 0;
    g_free( t );

    return r;
}

static void pon_numeros( GtkWidget *w, gchar **v, int *i )
{
    if ( GTK_IS_SPIN_BUTTON(w) )
        { if ( v[*i] ) gtk_spin_button_set_value( GTK_SPIN_BUTTON(w), g_ascii_strtod( v[(*i)++], NULL ) ); }
    else if ( GTK_IS_CONTAINER(w) )
        {
        GList *h = gtk_container_get_children( GTK_CONTAINER(w) ), *x;

        for ( x = h; x; x = x->next ) pon_numeros( x->data, v, i );
        g_list_free( h );
        }
}

/* Un desplegable que tiene el elemento arg; si lo tiene, lo deja elegido. */
static gboolean pon_combo( GtkWidget *w, const char *arg )
{
    GtkTreeModel *m = gtk_combo_box_get_model( GTK_COMBO_BOX(w) );
    GtkTreeIter it;
    gboolean hay = m && gtk_tree_model_get_iter_first( m, &it );

    for ( ; hay; hay = gtk_tree_model_iter_next( m, &it ) )
        {
        int c, nc = gtk_tree_model_get_n_columns( m );

        for ( c = 0; c < nc; c++ )
            if ( gtk_tree_model_get_column_type( m, c ) == G_TYPE_STRING )
                {
                gchar *s = NULL;
                gboolean si;

                gtk_tree_model_get( m, &it, c, &s, -1 );
                si = s && strcmp( s, arg ) == 0;
                g_free( s );
                if ( si ) { gtk_combo_box_set_active_iter( GTK_COMBO_BOX(w), &it ); return TRUE; }
                }
        }

    return FALSE;
}

static gboolean es_combo_con( GtkWidget *w, const char *arg )
{
    return GTK_IS_COMBO_BOX(w) && gtk_widget_is_drawable( w ) && pon_combo( w, arg );
}

static gboolean es_combo_n( GtkWidget *w, const char *arg )
{
    return GTK_IS_COMBO_BOX(w) && gtk_widget_is_drawable( w ) && ++cuantos == atoi( arg );
}

/* Una fila con una celda igual a arg: se busca en el modelo de cada tabla. */
static GtkTreePath *camino;

static gboolean busca_fila( GtkTreeModel *m, GtkTreePath *p, GtkTreeIter *it, gpointer d )
{
    int c, n = gtk_tree_model_get_n_columns( m );

    for ( c = 0; c < n; c++ )
        if ( gtk_tree_model_get_column_type( m, c ) == G_TYPE_STRING )
            {
            gchar *s = NULL;

            gtk_tree_model_get( m, it, c, &s, -1 );
            if ( s && strcmp( s, d ) == 0 )
                { g_free( s ); camino = gtk_tree_path_copy( p ); return TRUE; }
            g_free( s );
            }

    return FALSE;
}

/* «TEXTO» o «TEXTO | N»: la N-esima tabla que tiene esa celda (la misma
   celda puede estar en dos tablas: «m01» es un modelo y tambien el elegido
   de una serie). */
static int tabla_n;

static gboolean es_tabla_con( GtkWidget *w, const char *arg )
{
    GtkTreeModel *m;
    gchar **p;
    gboolean r;

    if ( !GTK_IS_TREE_VIEW(w) || !gtk_widget_is_drawable( w ) ) return FALSE;
    if ( (m = gtk_tree_view_get_model( GTK_TREE_VIEW(w) )) == NULL ) return FALSE;
    p = g_strsplit( arg, " | ", 2 );
    camino = NULL;
    gtk_tree_model_foreach( m, busca_fila, p[0] );
    r = camino != NULL && ++tabla_n >= (p[1] ? atoi( p[1] ) : 1);
    if ( camino && !r ) { gtk_tree_path_free( camino ); camino = NULL; }
    g_strfreev( p );

    return r;
}

/* ---- fotografiar ------------------------------------------------------ */

static cairo_surface_t *pinta( GtkWidget *w )
{
    int W = gtk_widget_get_allocated_width( w ), H = gtk_widget_get_allocated_height( w );
    cairo_surface_t *s;
    cairo_t *cr;

    if ( W < 2 || H < 2 ) return NULL;
    s  = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, W, H );
    cr = cairo_create( s );
    gtk_widget_draw( w, cr );
    cairo_destroy( cr );

    return s;
}

/* Sin la sombra de la ventana: se recorta a lo que es opaco. */
static cairo_surface_t *recorta( cairo_surface_t *s )
{
    int W = cairo_image_surface_get_width( s ), H = cairo_image_surface_get_height( s );
    int st = cairo_image_surface_get_stride( s ), x, y, x0 = W, y0 = H, x1 = -1, y1 = -1;
    unsigned char *d;
    cairo_surface_t *r;
    cairo_t *cr;

    cairo_surface_flush( s );
    d = cairo_image_surface_get_data( s );
    for ( y = 0; y < H; y++ )
        for ( x = 0; x < W; x++ )
            if ( ((guint32 *) (d + y * st))[x] >> 24 == 0xff )
                {
                if ( x < x0 ) x0 = x;
                if ( x > x1 ) x1 = x;
                if ( y < y0 ) y0 = y;
                if ( y > y1 ) y1 = y;
                }
    if ( x1 < x0 ) return cairo_surface_reference( s );
    r  = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, x1 - x0 + 1, y1 - y0 + 1 );
    cr = cairo_create( r );
    cairo_set_source_surface( cr, s, -x0, -y0 );
    cairo_paint( cr );
    cairo_destroy( cr );

    return r;
}

static void guarda( cairo_surface_t *s0, const char *nombre )
{
    const char *dir = g_getenv( "FOTOS" ) ? g_getenv( "FOTOS" ) : ".";
    gchar *f = g_strdup_printf( "%s/%s.png", dir, nombre );
    cairo_surface_t *s = recorta( s0 );

    cairo_surface_write_to_png( s, f );
    g_print( "escenas: %s (%dx%d)\n", f, cairo_image_surface_get_width( s ),
             cairo_image_surface_get_height( s ) );
    cairo_surface_destroy( s );
    g_free( f );
}

/* La ventana, con el menu abierto pintado encima donde esta. */
static void foto_menu( GtkWidget *v, const char *nombre )
{
    cairo_surface_t *s = pinta( v ), *m;
    GtkWidget *top;
    GtkRequisition rq;
    GtkAllocation  a = { 0 };
    int vx, vy, mx, my;
    cairo_t *cr;

    if ( s == NULL || menu_abierto == NULL ) fallo( "no hay menu que fotografiar", nombre );
    top = gtk_widget_get_toplevel( menu_abierto );
    /* Bajo broadway el menu llega con anchura 1: se le da la que pide. */
    gtk_widget_get_preferred_size( menu_abierto, NULL, &rq );
    a.width = rq.width; a.height = rq.height;
    gtk_widget_size_allocate( menu_abierto, &a );
    m = pinta( menu_abierto );
    /* Bajo broadway el menu no sabe donde esta el puntero y sale en la
       esquina: se pone donde se hizo el clic, sin salirse de la ventana. */
    (void) top;
    vx = vy = 0;
    mx = MIN( clic_x, cairo_image_surface_get_width( s ) - a.width - 4 );
    my = MIN( clic_y, cairo_image_surface_get_height( s ) - a.height - 4 );
    cr = cairo_create( s );
    /* un borde y una sombra sencillos, que el menu no se funda con la ventana */
    cairo_set_source_rgba( cr, 0, 0, 0, 0.25 );
    cairo_rectangle( cr, mx - vx + 3, my - vy + 3, a.width, a.height );
    cairo_fill( cr );
    cairo_set_source_surface( cr, m, mx - vx, my - vy );
    cairo_paint( cr );
    cairo_set_source_rgba( cr, 0, 0, 0, 0.35 );
    cairo_set_line_width( cr, 1 );
    cairo_rectangle( cr, mx - vx + 0.5, my - vy + 0.5, a.width - 1, a.height - 1 );
    cairo_stroke( cr );
    cairo_destroy( cr );
    guarda( s, nombre );
    cairo_surface_destroy( m );
    cairo_surface_destroy( s );
}

/* ---- actuar ----------------------------------------------------------- */

/* Pulsar desde un idle propio: si el boton abre un dialogo modal, su bucle
   anidado sigue despachando el temporizador del guion. */
static gboolean pulsa( gpointer w )
{
    if ( GTK_IS_TOOL_BUTTON(w) )
        g_signal_emit_by_name( w, "clicked" );
    else if ( GTK_IS_MENU_ITEM(w) )
        gtk_menu_item_activate( w );
    else
        gtk_button_clicked( w );
    g_object_unref( w );

    return G_SOURCE_REMOVE;
}

static void en_idle( GtkWidget *w )
{
    g_idle_add( pulsa, g_object_ref( w ) );
}

static void derecho( GtkTreeView *tv, GtkTreePath *p )
{
    GdkRectangle r;
    GdkEvent *e = gdk_event_new( GDK_BUTTON_PRESS );
    GdkSeat  *seat = gdk_display_get_default_seat( gdk_display_get_default() );
    gboolean  h;

    gtk_tree_view_set_cursor( tv, p, NULL, FALSE );
    gtk_tree_view_get_cell_area( tv, p, gtk_tree_view_get_column( tv, 0 ), &r );
    e->button.window = g_object_ref( gtk_tree_view_get_bin_window( tv ) );
    e->button.send_event = TRUE;
    e->button.time = GDK_CURRENT_TIME;
    e->button.x = r.x + 10;
    e->button.y = r.y + r.height / 2;
    e->button.button = 3;
    gtk_widget_translate_coordinates( GTK_WIDGET(tv), gtk_widget_get_toplevel( GTK_WIDGET(tv) ),
                                      r.x + 10, r.y + r.height, &clic_x, &clic_y );
    gdk_event_set_device( e, gdk_seat_get_pointer( seat ) );
    g_signal_emit_by_name( tv, "button-press-event", e, &h );
    gdk_event_free( e );
}

static GtkWidget *menu_visible( void )
{
    GList *l = gtk_window_list_toplevels(), *i;
    GtkWidget *m = NULL;

    for ( i = l; i; i = i->next )
        {
        GtkWidget *c = GTK_IS_BIN(i->data) ? gtk_bin_get_child( i->data ) : NULL;

        if ( c && GTK_IS_MENU(c) && gtk_widget_get_mapped( c ) )
            m = c;      /* el ultimo abierto: el submenu, si lo hay */
        }
    g_list_free( l );

    return m;
}


/* ---- el video ----------------------------------------------------------
 *
 * Con VIDEO=fichero.mp4 en el entorno, ademas de seguir el guion se graba:
 * un cuadro cada 1/FPS segundos, compuesto sobre un lienzo de 1920x1080 --la
 * ventana principal de la escena, y encima el dialogo o la ventana con que
 * se esta trabajando--, con un puntero que VA hasta cada boton antes de
 * pulsarlo. Los cuadros van por una tuberia a ffmpeg, sin ficheros
 * intermedios. Los rotulos del guion (rotulo TEXTO) se escriben en
 * fichero.ass, con el tiempo contado en cuadros: si el programa se queda
 * ocupado (estimando) no salen cuadros, el video se salta la espera y los
 * subtitulos siguen en su sitio.
 *
 * LA VOZ. Con VOZ_DIR, cada rotulo tiene su frase ya sintetizada
 * (video/voz.py): VOZ_DIR/<sha1 del rotulo>.wav, y su duracion en .ms. El
 * rotulo siguiente --y «cierra» y «salir»-- espera a que la frase acabe, contando en
 * cuadros como los subtitulos: la voz no se pisa ni se corta. Cuando y que
 * frase suena se apunta en fichero.mp4.voz, y video.sh las mezcla.
 *
 *   rotulo TEXTO || LO QUE SE DICE    si la voz tiene que decir otra cosa
 */

#define VW 1920
#define VH 1080
#define ZONA_TXT 150            /* abajo, reservado a los subtitulos      */

static FILE   *video;
static gchar  *video_ass;
static int     fps = 10, ncuadro;
static gchar  *principal;       /* titulo de la ventana principal         */
static double  px = VW / 2, py = VH / 2;       /* el puntero, en el lienzo */
static double  ax, ay, bx, by;  /* la animacion del puntero: de a a b      */
static gint64  t_anim = -1;     /* cuando empezo; -1 = parado             */
static int     n_clic = -1;     /* cuadro del ultimo clic (el aro)        */
static double  esc = 1, ox, oy; /* ventana actual -> lienzo               */
static GString *ass;
static int     rot_desde = -1;
static gchar  *rot_txt;
static gchar  *portada_a, *portada_b;
static int     portada_hasta = -1;
static gboolean portada_habla;          /* hay rotulos dichos sobre ella */
static const char *voz_dir;     /* VOZ_DIR, o NULL: sin voz               */
static int     voz_hasta = -1;  /* cuadro en que acaba la frase que suena */
static GString *voz_lista;      /* «segundo fichero.wav», para video.sh   */
static gchar  *video_voz;

static GtkWidget *ventana_por_titulo( const char *t )
{
    GList *l = gtk_window_list_toplevels(), *i;
    GtkWidget *w = NULL;

    for ( i = l; i; i = i->next )
        {
        const char *x = gtk_window_get_title( i->data );

        if ( gtk_widget_get_mapped( i->data ) && x && t && strstr( x, t ) )
            w = i->data;
        }
    g_list_free( l );

    return w;
}

/* Dibuja la ventana w escalada para que quepa en el lienzo, centrada; deja
   en esc/ox/oy la transformacion. */
static void pon_ventana( cairo_t *cr, GtkWidget *w, double max_esc )
{
    cairo_surface_t *s = pinta( w ), *r;
    int W, H;

    if ( s == NULL ) return;
    r = recorta( s );
    cairo_surface_destroy( s );
    W = cairo_image_surface_get_width( r );
    H = cairo_image_surface_get_height( r );
    esc = MIN( max_esc, MIN( (VW - 80.0) / W, (VH - ZONA_TXT - 50.0) / H ) );
    ox = (VW - W * esc) / 2;
    oy = 25 + (VH - ZONA_TXT - 50 - H * esc) / 2;
    /* la sombra */
    cairo_set_source_rgba( cr, 0, 0, 0, 0.30 );
    cairo_rectangle( cr, ox + 8, oy + 10, W * esc, H * esc );
    cairo_fill( cr );
    cairo_save( cr );
    cairo_translate( cr, ox, oy );
    cairo_scale( cr, esc, esc );
    cairo_set_source_surface( cr, r, 0, 0 );
    cairo_pattern_set_filter( cairo_get_source( cr ), CAIRO_FILTER_GOOD );
    cairo_paint( cr );
    cairo_restore( cr );
    cairo_surface_destroy( r );
}

/* El recorte quita la sombra: las coordenadas de la ventana sin recortar
   se desplazan lo que se quito. Se calcula igual que recorta(). */
static void origen_recorte( GtkWidget *w, int *dx, int *dy )
{
    cairo_surface_t *s = pinta( w );
    int W, H, st, x, y;
    unsigned char *d;

    *dx = *dy = 0;
    if ( s == NULL ) return;
    W = cairo_image_surface_get_width( s ); H = cairo_image_surface_get_height( s );
    st = cairo_image_surface_get_stride( s );
    cairo_surface_flush( s );
    d = cairo_image_surface_get_data( s );
    *dx = W; *dy = H;
    for ( y = 0; y < H; y++ )
        for ( x = 0; x < W; x++ )
            if ( ((guint32 *) (d + y * st))[x] >> 24 == 0xff )
                { if ( x < *dx ) *dx = x; if ( y < *dy ) *dy = y; }
    if ( *dx == W ) *dx = *dy = 0;
    cairo_surface_destroy( s );
}

static void pon_puntero( cairo_t *cr )
{
    static const double flecha[][2] = {
        { 0, 0 }, { 0, 30 }, { 8, 23 }, { 14, 36 }, { 19, 34 }, { 13, 21 }, { 23, 21 } };
    int i;

    if ( n_clic >= 0 && ncuadro - n_clic < fps / 2 )
        {
        double k = (ncuadro - n_clic) / (fps / 2.0);

        cairo_set_source_rgba( cr, 0.95, 0.45, 0.10, 0.8 * (1 - k) );
        cairo_set_line_width( cr, 5 );
        cairo_arc( cr, px, py, 12 + 30 * k, 0, 2 * G_PI );
        cairo_stroke( cr );
        }
    cairo_save( cr );
    cairo_translate( cr, px, py );
    cairo_scale( cr, 1.2, 1.2 );
    cairo_move_to( cr, flecha[0][0], flecha[0][1] );
    for ( i = 1; i < 7; i++ ) cairo_line_to( cr, flecha[i][0], flecha[i][1] );
    cairo_close_path( cr );
    cairo_set_source_rgb( cr, 1, 1, 1 );
    cairo_fill_preserve( cr );
    cairo_set_source_rgb( cr, 0, 0, 0 );
    cairo_set_line_width( cr, 1.6 );
    cairo_stroke( cr );
    cairo_restore( cr );
}

static void pon_portada( cairo_t *cr )
{
    cairo_text_extents_t te;

    cairo_set_source_rgb( cr, 0.12, 0.23, 0.40 );
    cairo_paint( cr );
    cairo_select_font_face( cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD );
    cairo_set_font_size( cr, 76 );
    cairo_text_extents( cr, portada_a, &te );
    cairo_set_source_rgb( cr, 1, 1, 1 );
    cairo_move_to( cr, (VW - te.width) / 2 - te.x_bearing, VH / 2 - 20 );
    cairo_show_text( cr, portada_a );
    if ( portada_b )
        {
        cairo_select_font_face( cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL );
        cairo_set_font_size( cr, 42 );
        cairo_text_extents( cr, portada_b, &te );
        cairo_set_source_rgb( cr, 0.85, 0.90, 1 );
        cairo_move_to( cr, (VW - te.width) / 2 - te.x_bearing, VH / 2 + 60 );
        cairo_show_text( cr, portada_b );
        }
}

static gboolean cuadro( gpointer d )
{
    cairo_surface_t *s;
    cairo_t *cr;
    GtkWidget *base, *act;

    (void) d;
    if ( video == NULL ) return G_SOURCE_REMOVE;
    s  = cairo_image_surface_create( CAIRO_FORMAT_RGB24, VW, VH );
    cr = cairo_create( s );

    if ( portada_hasta > ncuadro || portada_habla )
        pon_portada( cr );
    else
        {
        /* el fondo, un escritorio neutro */
        cairo_pattern_t *g = cairo_pattern_create_linear( 0, 0, 0, VH );

        cairo_pattern_add_color_stop_rgb( g, 0, 0.83, 0.86, 0.90 );
        cairo_pattern_add_color_stop_rgb( g, 1, 0.70, 0.75, 0.81 );
        cairo_set_source( cr, g );
        cairo_paint( cr );
        cairo_pattern_destroy( g );

        base = ventana_por_titulo( principal );
        act  = la_ventana();
        if ( base && act && base != act && !GTK_IS_FILE_CHOOSER(act) )
            {
            pon_ventana( cr, base, 1.3 );
            cairo_set_source_rgba( cr, 0, 0, 0, 0.35 );   /* atenuada */
            cairo_rectangle( cr, ox, oy, VW - 2 * ox, VH - ZONA_TXT - 50 - 2 * (oy - 25) );
            cairo_fill( cr );
            }
        if ( act )
            {
            pon_ventana( cr, act, 1.3 );
            if ( menu_abierto || menu_visible() )
                {
                GtkWidget *m = menu_abierto ? menu_abierto : menu_visible();
                GtkRequisition rq;
                GtkAllocation  a = { 0 };
                cairo_surface_t *ms;
                int dx, dy;

                gtk_widget_get_preferred_size( m, NULL, &rq );
                a.width = rq.width; a.height = rq.height;
                gtk_widget_size_allocate( m, &a );
                if ( (ms = pinta( m )) != NULL )
                    {
                    origen_recorte( act, &dx, &dy );
                    cairo_save( cr );
                    cairo_translate( cr, ox + (clic_x - dx) * esc, oy + (clic_y - dy) * esc );
                    cairo_scale( cr, esc, esc );
                    cairo_set_source_rgba( cr, 0, 0, 0, 0.25 );
                    cairo_rectangle( cr, 3, 3, a.width, a.height );
                    cairo_fill( cr );
                    cairo_set_source_surface( cr, ms, 0, 0 );
                    cairo_paint( cr );
                    cairo_restore( cr );
                    cairo_surface_destroy( ms );
                    }
                }
            }

        /* el puntero, moviendose si toca */
        if ( t_anim >= 0 )
            {
            double k = (g_get_monotonic_time() - t_anim) / 600000.0;

            if ( k >= 1 ) { k = 1; }
            k = k * k * (3 - 2 * k);        /* suave al salir y al llegar */
            px = ax + (bx - ax) * k;
            py = ay + (by - ay) * k;
            }
        pon_puntero( cr );
        }

    cairo_destroy( cr );
    cairo_surface_flush( s );
    fwrite( cairo_image_surface_get_data( s ), 4, (size_t) VW * VH, video );
    cairo_surface_destroy( s );
    ncuadro++;

    return G_SOURCE_CONTINUE;
}

static void hms( GString *g, int n )
{
    double t = (double) n / fps;
    int h = (int) (t / 3600), m = (int) (t / 60) % 60;

    g_string_append_printf( g, "%d:%02d:%05.2f", h, m, t - h * 3600 - m * 60 );
}

static void subtitulo( const char *txt )
{
    if ( rot_txt && rot_desde >= 0 && ncuadro > rot_desde )
        {
        g_string_append( ass, "Dialogue: 0," );
        hms( ass, rot_desde );
        g_string_append_c( ass, ',' );
        hms( ass, ncuadro );
        g_string_append_printf( ass, ",Abajo,,0,0,0,,%s\n", rot_txt );
        }
    g_free( rot_txt );
    rot_txt = txt && *txt && strcmp( txt, "-" ) != 0 ? g_strdup( txt ) : NULL;
    rot_desde = ncuadro;
}

static void video_fin( void )
{
    if ( video == NULL ) return;
    subtitulo( NULL );
    pclose( video );
    video = NULL;
    if ( video_ass )
        g_file_set_contents( video_ass, ass->str, -1, NULL );
    if ( video_voz && voz_lista )
        g_file_set_contents( video_voz, voz_lista->str, -1, NULL );
}

/* La frase de un rotulo: si esta sintetizada, se apunta que empieza ahora
   y hasta que cuadro dura (mas una pausa corta antes de la siguiente). */
static void voz_dice( const char *arg )
{
    gchar *sha, *ms_f, *wav, *ms_txt = NULL;

    if ( voz_dir == NULL || video == NULL ) return;
    sha = g_compute_checksum_for_string( G_CHECKSUM_SHA1, arg, -1 );
    ms_f = g_strdup_printf( "%s/%s.ms", voz_dir, sha );
    wav = g_strdup_printf( "%s/%s.wav", voz_dir, sha );
    if ( g_file_get_contents( ms_f, &ms_txt, NULL, NULL ) )
        {
        int ms = atoi( ms_txt );

        voz_hasta = ncuadro + (ms + 450) * fps / 1000;
        g_string_append_printf( voz_lista, "%.3f %s\n", (double) ncuadro / fps, wav );
        }
    else
        g_printerr( "escenas: sin voz para «%s»\n", arg );
    g_free( ms_txt ); g_free( wav ); g_free( ms_f ); g_free( sha );
}

/* Lleva el puntero al centro del rectangulo r (en coordenadas de la ventana
   v, sin recortar). TRUE cuando ya ha llegado: entonces se puede pulsar. */
static gboolean apunta( GtkWidget *v, int x, int y )
{
    int dx, dy;
    double tx, ty;

    if ( video == NULL ) return TRUE;
    origen_recorte( v, &dx, &dy );
    tx = ox + (x - dx) * esc;
    ty = oy + (y - dy) * esc;
    if ( t_anim < 0 )
        {
        ax = px; ay = py; bx = tx; by = ty;
        t_anim = g_get_monotonic_time();
        return FALSE;
        }
    if ( g_get_monotonic_time() - t_anim < 850000 )
        return FALSE;
    t_anim = -1;
    px = tx; py = ty;
    n_clic = ncuadro;

    return TRUE;
}

/* Lo mismo, apuntando al centro de un widget. */
static gboolean apunta_w( GtkWidget *v, GtkWidget *w )
{
    GtkAllocation a;
    int x = 0, y = 0;

    if ( video == NULL ) return TRUE;
    gtk_widget_get_allocation( w, &a );
    gtk_widget_translate_coordinates( w, v, a.width / 2, a.height / 2, &x, &y );

    return apunta( v, x, y );
}

static void video_empieza( const char *f )
{
    gchar *cmd;
    const char *crf = g_getenv( "VIDEO_CRF" ) ? g_getenv( "VIDEO_CRF" ) : "18";

    if ( g_getenv( "FPS" ) ) fps = atoi( g_getenv( "FPS" ) );
    cmd = g_strdup_printf( "ffmpeg -loglevel error -y -f rawvideo -pix_fmt bgra "
                           "-s %dx%d -r %d -i - -c:v libx264 -preset veryfast "
                           "-crf %s -pix_fmt yuv420p '%s'", VW, VH, fps, crf, f );
    video = popen( cmd, "w" );
    g_free( cmd );
    if ( video == NULL ) { g_printerr( "escenas: no puedo lanzar ffmpeg\n" ); return; }
    video_ass = g_strconcat( f, ".ass", NULL );
    voz_dir = g_getenv( "VOZ_DIR" );
    if ( voz_dir )
        {
        video_voz = g_strconcat( f, ".voz", NULL );
        voz_lista = g_string_new( NULL );
        }
    ass = g_string_new(
        "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n"
        "WrapStyle: 0\n\n[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
        "ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
        "MarginL, MarginR, MarginV, Encoding\n"
        "Style: Abajo,Sans,44,&H00FFFFFF,&H00FFFFFF,&H00000000,&H90201A10,0,0,0,0,"
        "100,100,0,0,3,14,0,2,120,120,40,1\n\n"
        "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, "
        "MarginV, Effect, Text\n" );
    g_timeout_add( 1000 / fps, cuadro, NULL );
}

/* Ejecuta la orden; FALSE = aun no se puede (se reintenta hasta TIEMPO). */
static gboolean orden( const char *cmd, const char *arg )
{
    GtkWidget *v = la_ventana(), *w;
    gchar **p = NULL;
    gboolean ok = TRUE;

    if ( strcmp( cmd, "ventana" ) == 0 )
        {
        g_free( titulo ); titulo = g_strdup( arg );
        if ( principal == NULL ) principal = g_strdup( arg );
        return la_ventana() != NULL;
        }
    if ( strcmp( cmd, "espera" ) == 0 )
        return g_get_monotonic_time() - desde >= atoi( arg ) * 1000LL;
    /* Lo que manda el reloj de la voz: ni el rotulo siguiente, ni cerrar
       la ventana de la que se esta hablando, ni el final, hasta que acabe
       la frase que esta sonando. Tampoco la portada siguiente. */
    if ( (strcmp( cmd, "salir" ) == 0 || strcmp( cmd, "rotulo" ) == 0 ||
          strcmp( cmd, "cierra" ) == 0 || strcmp( cmd, "portada" ) == 0)
         && video && ncuadro < voz_hasta )
        return FALSE;
    if ( strcmp( cmd, "salir" ) == 0 )
        { video_fin(); exit( 0 ); }
    if ( strcmp( cmd, "rotulo" ) == 0 )
        {
        if ( video )
            {
            const char *dos = strstr( arg, " || " );
            gchar *se_ve = dos ? g_strndup( arg, dos - arg ) : g_strdup( arg );

            subtitulo( se_ve );
            if ( strcmp( se_ve, "-" ) != 0 ) voz_dice( arg );
            /* Los rotulos dichos sobre una portada la mantienen, hasta el
               «rotulo -» o la portada siguiente. */
            if ( strcmp( se_ve, "-" ) == 0 )
                portada_habla = FALSE;
            else if ( portada_hasta > ncuadro )
                portada_habla = TRUE;
            g_free( se_ve );
            }
        return TRUE;
        }
    if ( strcmp( cmd, "portada" ) == 0 )
        {
        gchar **q = g_strsplit( arg, " | ", 3 );

        if ( video )
            {
            g_free( portada_a ); g_free( portada_b );
            portada_a = g_strdup( q[0] );
            portada_b = q[1] ? g_strdup( q[1] ) : NULL;
            portada_hasta = ncuadro + (q[1] && q[2] ? atoi( q[2] ) : 3000) * fps / 1000;
            portada_habla = FALSE;
            }
        g_strfreev( q );
        return TRUE;
        }
    if ( strcmp( cmd, "principal" ) == 0 )
        { g_free( principal ); principal = g_strdup( arg ); return TRUE; }
    if ( strcmp( cmd, "ventanas" ) == 0 )      /* para escribir guiones */
        {
        GList *l = gtk_window_list_toplevels(), *i;

        for ( i = l; i; i = i->next )
            if ( gtk_widget_get_mapped( i->data ) && gtk_window_get_title( i->data ) )
                g_print( "escenas: ventana «%s»\n", gtk_window_get_title( i->data ) );
        g_list_free( l );
        return TRUE;
        }
    if ( strcmp( cmd, "menu" ) == 0 || strcmp( cmd, "submenu" ) == 0 )
        {
        GtkWidget *m = menu_visible();

        if ( m == NULL || (w = busca( m, es_item, arg )) == NULL ) return FALSE;
        if ( video )
            {
            /* el menu se dibuja en el punto del clic, dentro de la ventana */
            GtkAllocation a;
            int x = 0, y = 0;

            gtk_widget_get_allocation( w, &a );
            gtk_widget_translate_coordinates( w, m, a.width / 3, a.height / 2, &x, &y );
            if ( v && !apunta( v, clic_x + x, clic_y + y ) ) return FALSE;
            }
        if ( strcmp( cmd, "submenu" ) == 0 )
            {
            gtk_menu_shell_select_item( GTK_MENU_SHELL(m), w );
            menu_abierto = gtk_menu_item_get_submenu( GTK_MENU_ITEM(w) );
            return TRUE;
            }
        en_idle( w );
        gtk_menu_shell_deactivate( GTK_MENU_SHELL(m) );
        menu_abierto = NULL;
        return TRUE;
        }
    if ( v == NULL ) return FALSE;

    if ( strcmp( cmd, "boton" ) == 0 )
        {
        GtkAllocation a;

        if ( (w = busca( v, es_boton, arg )) == NULL || !gtk_widget_is_sensitive( w ) )
            return FALSE;
        if ( !apunta_w( v, w ) ) return FALSE;
        gtk_widget_get_allocation( w, &a );
        gtk_widget_translate_coordinates( w, v, 0, a.height, &clic_x, &clic_y );
        en_idle( w );
        }
    else if ( strcmp( cmd, "pestana" ) == 0 )
        {
        int i, n;

        if ( (w = busca( v, es_cuaderno, arg )) == NULL ) return FALSE;
        n = gtk_notebook_get_n_pages( GTK_NOTEBOOK(w) );
        for ( i = 0; i < n; i++ )
            {
            GtkWidget *pg = gtk_notebook_get_nth_page( GTK_NOTEBOOK(w), i );
            GtkWidget *tl = gtk_notebook_get_tab_label( GTK_NOTEBOOK(w), pg );
            const char *t = GTK_IS_LABEL(tl) ? gtk_label_get_text( GTK_LABEL(tl) ) : rotulo( tl );

            if ( t && strcmp( t, arg ) == 0 )
                {
                if ( !apunta_w( v, tl ) ) return FALSE;
                gtk_notebook_set_current_page( GTK_NOTEBOOK(w), i );
                }
            }
        }
    else if ( strcmp( cmd, "fila" ) == 0 || strcmp( cmd, "doble" ) == 0
              || strcmp( cmd, "derecho" ) == 0 )
        {
        tabla_n = 0;
        if ( (w = busca( v, es_tabla_con, arg )) == NULL ) return FALSE;
        if ( video )
            {
            GdkRectangle r;
            int x, y;

            gtk_tree_view_get_cell_area( GTK_TREE_VIEW(w), camino,
                                         gtk_tree_view_get_column( GTK_TREE_VIEW(w), 0 ), &r );
            gtk_tree_view_convert_bin_window_to_widget_coords( GTK_TREE_VIEW(w), r.x + 20,
                                                               r.y + r.height / 2, &x, &y );
            gtk_widget_translate_coordinates( w, v, x, y, &x, &y );
            if ( !apunta( v, x, y ) ) { gtk_tree_path_free( camino ); return FALSE; }
            }
        if ( cmd[0] == 'f' )
            {
            gtk_tree_view_set_cursor( GTK_TREE_VIEW(w), camino, NULL, FALSE );
            gtk_tree_selection_select_path( gtk_tree_view_get_selection( GTK_TREE_VIEW(w) ), camino );
            }
        else if ( cmd[0] == 'd' && cmd[1] == 'o' )
            gtk_tree_view_row_activated( GTK_TREE_VIEW(w), camino,
                                         gtk_tree_view_get_column( GTK_TREE_VIEW(w), 0 ) );
        else
            {
            derecho( GTK_TREE_VIEW(w), camino );
            menu_abierto = NULL;
            }
        gtk_tree_path_free( camino );
        }
    else if ( strcmp( cmd, "entrada" ) == 0 || strcmp( cmd, "combo" ) == 0 )
        {
        p = g_strsplit( arg, " | ", 2 );
        if ( !p[0] || !p[1] ) fallo( "faltan argumentos", arg );
        cuantos = 0;
        if ( cmd[0] == 'e' )
            {
            if ( (w = busca( v, es_entrada_n, p[0] )) == NULL ) ok = FALSE;
            else gtk_entry_set_text( GTK_ENTRY(w), p[1] );
            }
        else
            {
            if ( (w = busca( v, es_combo_n, p[0] )) == NULL ) ok = FALSE;
            else
                {
                GtkTreeModel *m = gtk_combo_box_get_model( GTK_COMBO_BOX(w) );
                GtkTreeIter it;
                gboolean hay = m && gtk_tree_model_get_iter_first( m, &it );

                ok = FALSE;
                for ( ; hay; hay = gtk_tree_model_iter_next( m, &it ) )
                    {
                    gchar *s = NULL;
                    int c, nc = gtk_tree_model_get_n_columns( m );

                    for ( c = 0; c < nc && !ok; c++ )
                        if ( gtk_tree_model_get_column_type( m, c ) == G_TYPE_STRING )
                            {
                            gtk_tree_model_get( m, &it, c, &s, -1 );
                            if ( s && strcmp( s, p[1] ) == 0 )
                                { gtk_combo_box_set_active_iter( GTK_COMBO_BOX(w), &it ); ok = TRUE; }
                            g_free( s ); s = NULL;
                            }
                    if ( ok ) break;
                    }
                }
            }
        }
    else if ( strcmp( cmd, "elige" ) == 0 )
        {
        if ( video )
            {
            /* buscar sin elegir todavia: es_combo_con elige al encontrar */
            GtkWidget *c = busca( v, es_combo_con, arg );

            if ( c == NULL ) return FALSE;
            if ( !apunta_w( v, c ) ) return FALSE;
            }
        else if ( busca( v, es_combo_con, arg ) == NULL ) return FALSE;
        }
    else if ( strcmp( cmd, "campo" ) == 0 )
        {
        GtkWidget *l, *g, *c;
        int x, y, i = 1;

        p = g_strsplit( arg, " | ", -1 );
        if ( !p[0] || !p[1] ) fallo( "faltan argumentos", arg );
        if ( (l = busca( v, es_etiqueta, p[0] )) == NULL ) { g_strfreev( p ); return FALSE; }
        g = gtk_widget_get_parent( l );
        gtk_container_child_get( GTK_CONTAINER(g), l, "left-attach", &x, "top-attach", &y, NULL );
        if ( (c = gtk_grid_get_child_at( GTK_GRID(g), x + 1, y )) == NULL ) fallo( "nada a la derecha de", p[0] );
        if ( !apunta_w( v, c ) ) { g_strfreev( p ); return FALSE; }
        if ( GTK_IS_ENTRY(c) && !GTK_IS_SPIN_BUTTON(c) ) gtk_entry_set_text( GTK_ENTRY(c), p[1] );
        else pon_numeros( c, p, &i );
        }
    else if ( strcmp( cmd, "numero" ) == 0 )
        {
        p = g_strsplit( arg, " | ", 2 );
        if ( !p[0] || !p[1] ) fallo( "faltan argumentos", arg );
        cuantos = 0;
        if ( (w = busca( v, es_numero_n, p[0] )) == NULL ) ok = FALSE;
        else gtk_spin_button_set_value( GTK_SPIN_BUTTON(w), g_ascii_strtod( p[1], NULL ) );
        }
    else if ( strcmp( cmd, "casilla" ) == 0 )
        {
        p = g_strsplit( arg, " | ", 2 );
        if ( !p[0] || !p[1] ) fallo( "faltan argumentos", arg );
        if ( (w = busca( v, es_casilla, p[0] )) == NULL ) ok = FALSE;
        else if ( !apunta_w( v, w ) ) ok = FALSE;
        else gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(w), atoi( p[1] ) );
        }
    else if ( strcmp( cmd, "fichero" ) == 0 )
        {
        if ( !GTK_IS_FILE_CHOOSER(v) ) return FALSE;
        if ( gtk_file_chooser_get_action( GTK_FILE_CHOOSER(v) ) == GTK_FILE_CHOOSER_ACTION_SAVE )
            {
            gchar *d = g_path_get_dirname( arg ), *b = g_path_get_basename( arg );

            gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(v), d );
            gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(v), b );
            g_free( d ); g_free( b );
            }
        else
            gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(v), arg );
        }
    else if ( strcmp( cmd, "foto" ) == 0 )
        {
        cairo_surface_t *s = pinta( v );

        if ( s == NULL ) return FALSE;
        guarda( s, arg );
        cairo_surface_destroy( s );
        }
    else if ( strcmp( cmd, "fotomenu" ) == 0 )
        {
        if ( menu_abierto == NULL ) menu_abierto = menu_visible();
        if ( menu_abierto == NULL ) return FALSE;
        foto_menu( v, arg );
        }
    else if ( strcmp( cmd, "tamano" ) == 0 )
        {
        int W = 0, H = 0;

        if ( sscanf( arg, "%d %d", &W, &H ) != 2 ) fallo( "tamano ANCHO ALTO", arg );
        gtk_window_resize( GTK_WINDOW(v), W, H );
        }
    else if ( strcmp( cmd, "cierra" ) == 0 )
        gtk_window_close( GTK_WINDOW(v) );
    else
        fallo( "orden desconocida", cmd );

    g_strfreev( p );

    return ok;
}

static gboolean tic( gpointer d )
{
    int tiempo = g_getenv( "TIEMPO" ) ? atoi( g_getenv( "TIEMPO" ) ) : 10;

    (void) d;
    while ( il < nl )
        {
        gchar *l = g_strstrip( g_strdup( lineas[il] ) ), *esp;
        const char *arg = "";

        if ( *l == '\0' || *l == '#' ) { g_free( l ); il++; desde = g_get_monotonic_time(); continue; }
        if ( (esp = strchr( l, ' ' )) != NULL ) { *esp = '\0'; arg = g_strstrip( esp + 1 ); }
        if ( orden( l, arg ) )
            {
            g_print( "escenas: %d %s %s\n", il + 1, l, arg );
            il++;
            desde = g_get_monotonic_time();
            g_free( l );
            g_timeout_add( paso_ms, tic, NULL );
            return G_SOURCE_REMOVE;
            }
        /* espera, y lo que espera a la voz, no son «no encuentro». */
        if ( strcmp( l, "espera" ) != 0
             && !( (strcmp( l, "rotulo" ) == 0 || strcmp( l, "salir" ) == 0 ||
                    strcmp( l, "cierra" ) == 0) && video )
             && g_get_monotonic_time() - desde > tiempo * 1000000LL )
            fallo( "no encuentro", lineas[il] );
        g_free( l );
        return G_SOURCE_CONTINUE;       /* se reintenta en el proximo tic */
        }
    g_printerr( "escenas: el guion se acabo sin «salir»\n" );
    video_fin();
    exit( 0 );
}

G_MODULE_EXPORT void gtk_module_init( gint *argc, gchar ***argv )
{
    const char *f = g_getenv( "ESCENA" );
    gchar *txt = NULL;

    (void) argc; (void) argv;
    /* Los hijos que lance este programa heredan el entorno: el guion es
       solo para el primero. */
    g_unsetenv( "GTK_MODULES" );
    if ( f == NULL || !g_file_get_contents( f, &txt, NULL, NULL ) )
        return;
    g_unsetenv( "ESCENA" );
    if ( g_getenv( "PASO_MS" ) ) paso_ms = atoi( g_getenv( "PASO_MS" ) );
    if ( g_getenv( "VIDEO" ) ) video_empieza( g_getenv( "VIDEO" ) );
    lineas = g_strsplit( txt, "\n", -1 );
    nl = g_strv_length( lineas );
    g_free( txt );
    desde = g_get_monotonic_time();
    g_timeout_add( 1500, tic, NULL );
}
