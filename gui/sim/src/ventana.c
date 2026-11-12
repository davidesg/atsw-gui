/*
 * ventana.c -- la ventana de sim_gui: la barra, las siete paginas y la barra
 * de estado.
 *
 * LA BARRA ES LA DE fue_gui y drtran_gui (lib/barra): Estimar · Ver modelo ▾
 * │ Diagnosis ▾ · Prever ▾, con el mismo icono y el mismo atajo. El alumno
 * sube de fue a drtran y de drtran aqui, y lo comun esta en el mismo sitio.
 * Lo que todavia no se puede hacer se apaga, y el globo dice que falta.
 *
 * Cada pagina pide por su nombre lo que le falta y dice quien lo fabrica:
 * la regla de mtram (DISENO-sim.md §3.1).
 */

#include <stdarg.h>
#include <string.h>

#include "sim.h"
#include "barra.h"
#include "sitio.h"

void sim_veredicto( GtkWidget *w, const char *color, const char *fmt, ... )
{
    va_list ap;
    gchar  *t, *esc, *mk;

    va_start( ap, fmt );
    t = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    esc = g_markup_escape_text( t, -1 );
    mk  = g_strdup_printf( "<span foreground=\"%s\">●</span>  %s", color, esc );
    gtk_label_set_markup( GTK_LABEL(w), mk );
    g_free( mk ); g_free( esc ); g_free( t );
}

void sim_estado( Sim *m, const char *fmt, ... )
{
    va_list ap;
    gchar  *t;

    va_start( ap, fmt );
    t = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    if (m->estado) gtk_label_set_text( GTK_LABEL(m->estado), t );
    g_free( t );
}

/* Los criterios de parada de qnewtopt.c, en castellano. */
const char *sim_criterio( const char *c )
{
    if (strstr( c, "gradient" )) return "gradiente escalado bajo la tolerancia";
    if (strstr( c, "distance" )) return "los dos últimos pasos casi coinciden";
    if (strstr( c, "lower point" )) return "el último paso no encontró un punto más bajo";
    if (strstr( c, "iteration limit" )) return "límite de iteraciones";
    if (strstr( c, "maximum length" )) return "cinco pasos seguidos de longitud máxima";
    return c;
}

GtkWidget *sim_mono( const char *txt )
{
    GtkWidget *l = gtk_label_new( txt );
    PangoAttrList *a = pango_attr_list_new();

    pango_attr_list_insert( a, pango_attr_family_new( "monospace" ) );
    gtk_label_set_attributes( GTK_LABEL(l), a );
    pango_attr_list_unref( a );
    gtk_label_set_selectable( GTK_LABEL(l), TRUE );
    gtk_widget_set_halign( l, GTK_ALIGN_START );
    return l;
}

gchar *sim_hermano( const char *programa )
{
    static const char *const sitio[] = {     /* relativos a gui/sim/          */
        "../../engines/drvarma/bin/%s",      /* el arbol de compilacion        */
        "../../engines/drvec/bin/%s",
        "./%s", NULL                         /* el paquete: todo junto         */
    };
    const char *e = !strcmp( programa, "drvarma" ) ? g_getenv( "SIM_DRVARMA" )
                  : !strcmp( programa, "drvec" )   ? g_getenv( "SIM_DRVEC" ) : NULL;

    /* LA PRUEBA lo sustituye, y sin respaldo: con uno que no esta, no hay
     * motor (asi se prueba el aviso).                                  */
    if (e && *e)
        return g_file_test( e, G_FILE_TEST_IS_EXECUTABLE ) ? g_strdup( e ) : NULL;
    return sitio_busca( programa, sitio );
}


/* ------------------------------------------------------------------------ */
/* La barra y la barra de estado                                             */
/* ------------------------------------------------------------------------ */

static void on_estimar( GtkWidget *w, Sim *m ) { (void) w; estima_lanzar( m ); }
static void on_ver( GtkWidget *w, Sim *m ) { (void) w; if (m->sis.n > 0) pagina_ver( m, 0 ); }

static void on_ver_serie( GtkMenuItem *mi, Sim *m )
{
    pagina_ver( m, GPOINTER_TO_INT(g_object_get_data( G_OBJECT(mi), "serie" )) );
}

/* EL MENU DE «Ver modelo»: una entrada por serie, en el orden del sistema
 * (el de Cholesky). Se hace al abrirlo: depende de las series.          */
static void menu_ver( GtkMenuToolButton *b, Sim *m )
{
    GtkWidget *menu = gtk_menu_tool_button_get_menu( b );
    GList *l = gtk_container_get_children( GTK_CONTAINER(menu) ), *q;
    int i;

    for (q = l; q; q = q->next) gtk_widget_destroy( q->data );
    g_list_free( l );
    for (i = 0; i < m->sis.n; i++) {
        gchar *t = g_strdup_printf( "La ecuación de %s", m->sis.s[i].nombre );
        GtkWidget *mi = at_entrada( menu, t, NULL, NULL, GTK_WIDGET(b), G_CALLBACK(on_ver_serie), m );
        g_object_set_data( G_OBJECT(mi), "serie", GINT_TO_POINTER(i) );
        g_free( t );
    }
    gtk_widget_show_all( menu );
}

static void on_prever( GtkWidget *w, Sim *m ) { (void) w; prevision_calcular( m ); }

static void on_diag( GtkWidget *w, Sim *m )
{
    (void) w;
    gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_DIAG );
}

static gboolean sal( GtkAccelGroup *g, GObject *o, guint k, GdkModifierType mo, gpointer d )
{
    Sim *m = d;
    (void) g; (void) o; (void) k; (void) mo;
    gtk_window_close( GTK_WINDOW(m->ventana) );
    return TRUE;
}

static void barra_new( Sim *m, GtkWidget *raiz )
{
    GtkWidget     *barra = gtk_toolbar_new();
    GtkAccelGroup *atajos = gtk_accel_group_new();

    gtk_toolbar_set_style( GTK_TOOLBAR(barra), GTK_TOOLBAR_BOTH );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );
    gtk_window_add_accel_group( GTK_WINDOW(m->ventana), atajos );

    m->tb_estimar = at_boton( barra, "Estimar", "estimar", "media-playback-start",
                              "", G_CALLBACK(on_estimar), m );
    at_atajo( atajos, GDK_KEY_e, m->tb_estimar );
    m->tb_ver = at_partido( barra, "Ver modelo", "ver-modelo", "x-office-document",
                            gtk_menu_new(), G_CALLBACK(on_ver), m );
    g_signal_connect( m->tb_ver, "show-menu", G_CALLBACK(menu_ver), m );
    gtk_toolbar_insert( GTK_TOOLBAR(barra), gtk_separator_tool_item_new(), -1 );
    m->tb_diag = at_partido( barra, "Diagnosis", "diagnosis", "dialog-information",
                             gtk_menu_new(), G_CALLBACK(on_diag), m );
    at_atajo( atajos, GDK_KEY_d, m->tb_diag );
    m->tb_prev = at_partido( barra, "Prever", "prever", "go-jump",
                             gtk_menu_new(), G_CALLBACK(on_prever), m );

    gtk_accel_group_connect( atajos, GDK_KEY_q, GDK_CONTROL_MASK, 0,
                             g_cclosure_new( G_CALLBACK(sal), m, NULL ) );
}


static void enciende( GtkToolItem *b, gboolean si, const char *globo_si, const char *globo_no )
{
    gtk_widget_set_sensitive( GTK_WIDGET(b), si );
    gtk_widget_set_tooltip_text( GTK_WIDGET(b), si ? globo_si : globo_no );
}

static void barra_refresca( Sim *m )
{
    gboolean corre = m->pe.corriendo || m->pp.corriendo || m->pr.corriendo;
    gboolean hay = m->pe.v && !m->pe.v->fallo && !corre;
    gboolean vec = m->pe.vec && m->pe.vec->hay_vec && !corre;
    const char *falta, *sin = corre ? "El motor está corriendo." : "Todavía no hay sistema estimado: "
                                       "Estimar (Ctrl+E) lo hace.";
    /* CON UN VEC, la diagnosis si; la pagina del modelo y la prevision son
     * todavia del VARMA (la del VEC la hace sima: forecast).            */
    const char *sin_vec = "Con un VEC, la página del modelo todavía no: la ecuación con α β′ "
                          "está en «6 · Diagnosis ▸ Ajuste» y en el .out.";

    if (!m->tb_estimar) return;
    falta = corre ? "El motor ya está corriendo."
          : m->sis.n < 2 ? "Hacen falta al menos dos series: «1 · Series», Abrir…"
          : "Antes, la puerta: «2 · Puerta».";
    enciende( m->tb_estimar, !corre && m->sis.n >= 2 && puerta_estado( m ) == 1,
              m->mod.clase == SIS_VEC ? "Estimar el VEC con drvec (Ctrl+E), con el modelo de «4 · Modelo»"
                                      : "Estimar el sistema con drvarma (Ctrl+E), con el modelo de «4 · Modelo»",
              falta );
    enciende( m->tb_diag, hay || vec, "«6 · Diagnosis» (Ctrl+D)", sin );
    enciende( m->tb_ver, hay, "La página del modelo de la primera serie: sus residuos con la ACF "
              "y la PACF y, debajo, su ecuación como la escribe fue, con los términos cruzados "
              "en la línea del ruido. La flecha: las demás series.", vec ? sin_vec : sin );
    enciende( m->tb_prev, ( hay || vec ) && !m->pv.corriendo, "Calcular la previsión, en «7 · Previsión»",
              sin );
}

static void estado_new( Sim *m, GtkWidget *raiz )
{
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );

    gtk_widget_set_margin_start( fila, 8 );
    gtk_widget_set_margin_end( fila, 8 );
    gtk_widget_set_margin_bottom( fila, 4 );

    m->estado = gtk_label_new( "Abre los .pre de las series, en orden (Abrir…)." );
    gtk_widget_set_halign( m->estado, GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(m->estado), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(fila), m->estado, TRUE, TRUE, 0 );

    m->l_modelo = gtk_label_new( "" );
    gtk_box_pack_end( GTK_BOX(fila), m->l_modelo, FALSE, FALSE, 0 );

    m->avance = gtk_progress_bar_new();
    gtk_widget_set_valign( m->avance, GTK_ALIGN_CENTER );
    gtk_widget_set_size_request( m->avance, 120, -1 );
    gtk_widget_set_no_show_all( m->avance, TRUE );
    gtk_widget_set_tooltip_text( m->avance, "drvarma trabaja." );
    gtk_box_pack_end( GTK_BOX(fila), m->avance, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(raiz), fila, FALSE, FALSE, 0 );
}

/* ------------------------------------------------------------------------ */

void sim_refresca( Sim *m )
{
    gchar *t;

    if (!m->ventana) return;
    series_refresca( m );
    puerta_refresca( m );
    rango_refresca( m );
    identifica_refresca( m );
    modelo_refresca( m );
    estima_refresca( m );
    diagnosis_refresca( m );
    prevision_refresca( m );
    barra_refresca( m );

    if (m->caso[0]) t = g_strdup_printf( "Sistema: %s%s%s%s", m->caso, m->desfasado ? " (desfasado)" : "",
                                         m->pe.corrida[0] ? " · Modelo: " : "", m->pe.corrida );
    else            t = g_strdup( "" );
    gtk_label_set_text( GTK_LABEL(m->l_modelo), t );
    g_free( t );

    {
        const char *cl = m->mod.clase == SIS_VEC ? "VEC" : "VARMA";
        t = m->caso[0] ? g_strdup_printf( "sim_gui — SIM · %s · %s", cl, m->caso )
                       : g_strdup_printf( "sim_gui — SIM · %s", cl );
    }
    gtk_window_set_title( GTK_WINDOW(m->ventana), t );
    g_free( t );
}

static void pestana( Sim *m, GtkWidget *pag, const char *rotulo, const char *globo )
{
    GtkWidget *l = gtk_label_new( rotulo );

    gtk_widget_set_tooltip_text( l, globo );
    gtk_notebook_append_page( GTK_NOTEBOOK(m->libro), pag, l );
}

/* AL CERRAR, el motor que corra se para, y la ventana deja de existir para
 * lo que llegue despues (puerta.c, on_done). Sin esto, el final de una
 * puerta en marcha escribia en widgets ya destruidos.                  */
static void on_destroy( GtkWidget *w, Sim *m )
{
    (void) w;
    if (m->pp.trabajo) engine_stop( m->pp.trabajo );
    if (m->pr.trabajo) engine_stop( m->pr.trabajo );
    if (m->pd.wx_trabajo) engine_stop( m->pd.wx_trabajo );
    if (m->pr.tic) { g_source_remove( m->pr.tic ); m->pr.tic = 0; }
    if (m->pe.trabajo) engine_stop( m->pe.trabajo );
    if (m->pv.trabajo) engine_stop( m->pv.trabajo );
    if (m->pd.trabajo) engine_stop( m->pd.trabajo );
    memset( &m->ps, 0, sizeof m->ps );
    m->pp.b_pasar = m->pp.orden = m->pp.lista = NULL;
    m->pp.ver_puerta = m->pp.ver_notas = m->pp.ver_rango = NULL;
    m->pr.pagina = m->pr.s_p = m->pr.s_q = m->pr.s_B = m->pr.c_caso = NULL;
    m->pr.b_contrastar = m->pr.b_parar = m->pr.b_al_modelo = m->pr.orden = m->pr.barra = NULL;
    m->pr.l_pruebas = m->pr.l_rangos = m->pr.ver_propuesta = m->pr.ver_puerta = NULL;
    m->pr.ver_notas = NULL;
    memset( &m->pi, 0, sizeof m->pi );
    memset( &m->pm, 0, sizeof m->pm );
    m->pe.b_estimar = m->pe.b_parar = m->pe.orden = m->pe.consola = NULL;
    m->pe.ver_fin = m->pe.ver_donde = NULL;
    m->pd.ver_pide = m->pd.libreta = m->pd.ver_lr = m->pd.ver_hosking = m->pd.ver_jb = NULL;
    m->pd.ver_raices = m->pd.ver_optim = m->pd.l_par = m->pd.l_res = m->pd.t_sigma = NULL;
    m->pd.t_out = m->pd.b_simpl = m->pd.b_red = m->pd.b_pasar_mt = NULL;
    m->pd.ver_simpl = m->pd.ver_red = m->pd.t_red = NULL;
    m->pd.b_weakex = m->pd.ver_wx = m->pd.t_wx = NULL;
    m->pv.s_hor = m->pv.c_eval = m->pv.s_win = m->pv.b_calc = NULL;
    m->pv.ver_pide = m->pv.ver_eval = m->pv.libreta = NULL;
    m->pv.t_prev = m->pv.l_eval = m->pv.t_irf = m->pv.t_fevd = m->pv.graficos = NULL;
    m->ventana = m->libro = m->estado = m->l_modelo = m->avance = NULL;
    m->tb_estimar = m->tb_ver = m->tb_diag = m->tb_prev = NULL;
}

GtkWidget *sim_ventana_new( GtkApplication *app, Sim *m )
{
    GtkWidget *raiz;

    m->ventana = gtk_application_window_new( app );
    g_signal_connect( m->ventana, "destroy", G_CALLBACK(on_destroy), m );
    gtk_window_set_default_size( GTK_WINDOW(m->ventana), 1000, 680 );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_add( GTK_CONTAINER(m->ventana), raiz );
    barra_new( m, raiz );

    m->libro = gtk_notebook_new();
    gtk_box_pack_start( GTK_BOX(raiz), m->libro, TRUE, TRUE, 0 );

    pestana( m, series_pagina_new( m ), "1 · Series",
             "Los ficheros de fue de las series, en el orden del sistema" );
    pestana( m, puerta_pagina_new( m ), "2 · Puerta",
             "La puerta diagonal: el sistema sin dinámica cruzada tiene que "
             "reproducir los univariantes" );
    pestana( m, rango_pagina_new( m ), "R · Rango",
             "El rango de cointegración, con drvec: ¿se mueven juntas a largo plazo? "
             "Solo con series I(1)" );
    pestana( m, identifica_pagina_new( m ), "3 · Identificación",
             "La CCF de los residuos univariantes, par a par, y los enlaces" );
    pestana( m, modelo_pagina_new( m ), "4 · Modelo", "p, q, Σ y los enlaces: la orden de drvarma" );
    pestana( m, estima_pagina_new( m ), "5 · Estimación", "La estimación con drvarma, y cómo acabó" );
    pestana( m, diagnosis_pagina_new( m ), "6 · Diagnosis", "La diagnosis del sistema estimado" );
    pestana( m, prevision_pagina_new( m ), "7 · Previsión",
             "La previsión, la evaluación contra los univariantes, la IRF y la FEVD" );

    estado_new( m, raiz );
    sim_refresca( m );
    return m->ventana;
}
