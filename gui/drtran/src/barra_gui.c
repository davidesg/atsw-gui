/*
 * barra_gui.c -- la barra de drtran_gui y su barra de estado.
 *
 * LA BARRA ES LA DE fue_gui (lib/barra), no una parecida: el alumno sube de
 * fue a drtran, y lo comun --estimar, ver el modelo, diagnosticar, prever--
 * esta en el mismo sitio, con el mismo icono y el mismo atajo. Solo sube a la
 * barra lo que NO depende de la pagina en que se esta: los botones de cada
 * pagina actuan sobre lo que la pagina enseña y se quedan alli. Ver
 * docs/DISENO-barra-drtran.md.
 *
 * APAGAR, CON GLOBO. mtram no grisa nada dentro de las paginas (cada una pide
 * por su nombre lo que le falta); la barra sigue la regla de fue_gui: lo que
 * necesita un modelo estimado se apaga sin el, y el globo dice que falta y
 * quien lo fabrica. Es la decision D2 del diseño.
 */

#include <string.h>

#include "gui.h"
#include "barra.h"
#include "previewhost.h"

#define SIN_MODELO "Todavía no hay modelo estimado: Estimar (Ctrl+E) lo hace."

static void on_estimar( GtkToolButton *b, Mtram *m ) { estima_lanzar( m ); }
static void on_ver( GtkToolButton *b, Mtram *m )     { diagnosis_ver_modelo( m, 0 ); }
static void on_prever( GtkToolButton *b, Mtram *m )  { prevision_calcular( m ); }

static void on_diagnosis( GtkToolButton *b, Mtram *m )
{
    gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_DIAG );
}

/* LOS MENUS SE HACEN AL ABRIRLOS: dependen de la red (una entrada por
 * ecuacion, una CCF por entrada) y de lo que haya estimado.            */
static void vacia( GtkWidget *menu )
{
    GList *l = gtk_container_get_children( GTK_CONTAINER(menu) ), *p;

    for (p = l; p; p = p->next) gtk_widget_destroy( p->data );
    g_list_free( l );
}

static void menu_ver( GtkMenuToolButton *b, Mtram *m )
{
    GtkWidget *menu = gtk_menu_tool_button_get_menu( b );

    vacia( menu );
    diagnosis_menu_modelos( m, menu, GTK_WIDGET(b) );
    gtk_widget_show_all( menu );
}

static void menu_diag( GtkMenuToolButton *b, Mtram *m )
{
    GtkWidget *menu = gtk_menu_tool_button_get_menu( b );

    vacia( menu );
    diagnosis_menu( m, menu, GTK_WIDGET(b) );
    gtk_widget_show_all( menu );
}

static void menu_prev( GtkMenuToolButton *b, Mtram *m )
{
    GtkWidget *menu = gtk_menu_tool_button_get_menu( b );

    vacia( menu );
    prevision_menu( m, menu, GTK_WIDGET(b) );
    gtk_widget_show_all( menu );
}

static GtkToolItem *partido( Mtram *m, GtkWidget *barra, const char *rotulo,
                             const char *icono, const char *respaldo,
                             GCallback cb, GCallback al_abrir )
{
    GtkWidget   *menu = gtk_menu_new();
    GtkToolItem *b = at_partido( barra, rotulo, icono, respaldo, menu, cb, m );

    g_signal_connect( b, "show-menu", al_abrir, m );
    return b;
}

static gboolean sal( GtkAccelGroup *g, GObject *o, guint k, GdkModifierType mo,
                     gpointer d )
{
    Mtram *m = d;

    (void) g; (void) o; (void) k; (void) mo;
    gtk_window_close( GTK_WINDOW(m->ventana_p) );
    return TRUE;
}

void mtram_barra_new( Mtram *m, GtkWidget *ventana, GtkWidget *raiz )
{
    GtkWidget     *barra = gtk_toolbar_new();
    GtkAccelGroup *atajos = gtk_accel_group_new();

    gtk_toolbar_set_style( GTK_TOOLBAR(barra), GTK_TOOLBAR_BOTH );
    /* EN LA VENTANA ANTES DE LOS BOTONES, como en fue_gui: los iconos toman
     * el color del texto de la barra, y suelta el tema no le da el suyo. */
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );
    gtk_window_add_accel_group( GTK_WINDOW(ventana), atajos );

    m->tb_estimar = at_boton( barra, "Estimar", "estimar", "media-playback-start",
        "Estimar el modelo con drtran (Ctrl+E), desde cualquier página",
        G_CALLBACK(on_estimar), m );
    at_atajo( atajos, GDK_KEY_e, m->tb_estimar );

    m->tb_ver = partido( m, barra, "Ver modelo", "ver-modelo", "x-office-document",
                         G_CALLBACK(on_ver), G_CALLBACK(menu_ver) );

    gtk_toolbar_insert( GTK_TOOLBAR(barra), gtk_separator_tool_item_new(), -1 );

    m->tb_diag = partido( m, barra, "Diagnosis", "diagnosis", "dialog-information",
                          G_CALLBACK(on_diagnosis), G_CALLBACK(menu_diag) );
    at_atajo( atajos, GDK_KEY_d, m->tb_diag );

    m->tb_prev = partido( m, barra, "Prever", "prever", "go-jump",
                          G_CALLBACK(on_prever), G_CALLBACK(menu_prev) );

    /* SALIR, sin boton: la equis de la ventana y Ctrl+Q, como en fue_gui. */
    gtk_accel_group_connect( atajos, GDK_KEY_q, GDK_CONTROL_MASK, 0,
                             g_cclosure_new( G_CALLBACK(sal), m, NULL ) );
}

/* La de fue: el mensaje a la izquierda, recortado; la barra de avance solo
 * mientras se estima; y a la derecha el modelo que hay delante.          */
void mtram_estado_new( Mtram *m, GtkWidget *raiz )
{
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );

    gtk_widget_set_margin_start( fila, 8 );
    gtk_widget_set_margin_end( fila, 8 );
    gtk_widget_set_margin_bottom( fila, 4 );

    m->estado = gtk_label_new( "Añade los .pre. El primero es la salida." );
    gtk_widget_set_halign( m->estado, GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(m->estado), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(fila), m->estado, TRUE, TRUE, 0 );

    m->l_modelo = gtk_label_new( "" );
    gtk_box_pack_end( GTK_BOX(fila), m->l_modelo, FALSE, FALSE, 0 );

    m->avance = gtk_progress_bar_new();
    gtk_widget_set_valign( m->avance, GTK_ALIGN_CENTER );
    gtk_widget_set_size_request( m->avance, 120, -1 );
    gtk_widget_set_no_show_all( m->avance, TRUE );
    gtk_widget_set_tooltip_text( m->avance, "drtran no dice por qué iteración "
                                            "va: la barra solo dice que trabaja." );
    gtk_box_pack_end( GTK_BOX(fila), m->avance, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(raiz), fila, FALSE, FALSE, 0 );
}

static void enciende( GtkToolItem *b, gboolean si, const char *globo_si,
                      const char *globo_no )
{
    gtk_widget_set_sensitive( GTK_WIDGET(b), si );
    gtk_widget_set_tooltip_text( GTK_WIDGET(b), si ? globo_si : globo_no );
}

void mtram_barra_refresca( Mtram *m )
{
    gboolean corre = m->est.corriendo;
    gboolean hay   = m->dia.vale && !corre;
    const char *no = corre ? "El motor está corriendo: espera, o Detener en "
                             "«5 · Estimación»."
                           : SIN_MODELO;
    gchar *s;

    if (!m->tb_estimar) return;           /* la barra todavia no existe */

    enciende( m->tb_estimar, !corre && m->c.n >= 2,
              "Estimar el modelo con drtran (Ctrl+E), desde cualquier página",
              corre ? "El motor ya está corriendo."
                    : "Hacen falta al menos dos .pre: «1 · Series», Abrir…" );
    enciende( m->tb_ver, hay && m->dia.hay_res && m->dia.hay_par,
              "La página de la ecuación que miras en «6 · Diagnosis» (al abrir, "
              "la salida): sus residuos con la ACF y la PACF y, debajo, la "
              "transferencia, el ruido y los deterministas estimados. La flecha: "
              "las demás ecuaciones de la red.",
              no );
    enciende( m->tb_diag, hay,
              "«6 · Diagnosis» (Ctrl+D). La flecha: sus pestañas, los gráficos, "
              "el baseline y exportar.",
              no );
    enciende( m->tb_prev, hay,
              "Calcular la previsión, en «7 · Previsión». La flecha: guardar la "
              "evaluación para compararla con la del modelo siguiente.",
              no );

    /* EL MODELO QUE HAY DELANTE: con proyecto, su nombre en el caso; sin el,
     * el .out, que es lo unico que lo nombra.                           */
    if (m->hay_proy && m->corrida[0])
        s = g_strdup_printf( "Modelo: %s", m->corrida );
    else if (m->est.out_path && m->dia.vale) {
        gchar *b = g_path_get_basename( m->est.out_path );

        s = g_strdup_printf( "Modelo: %s", b );
        g_free( b );
    } else
        s = g_strdup( "" );
    gtk_label_set_text( GTK_LABEL(m->l_modelo), s );
    g_free( s );
}
