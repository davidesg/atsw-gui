/* test_gui.c -- sim_gui de verdad, conducido desde el codigo.
 *
 *   test_gui <datos> <trabajo> <fake_drvarma>
 *
 *   datos         lib/outvarma/datos (los .pre de Jenkins y Alavi)
 *   trabajo       un directorio vacio; dentro va la cache del programa
 *   fake_drvarma  un drvarma cuya puerta falla (sale con 5)
 *
 * Se levanta la ventana (sin ensenarla), se pulsan SUS botones --buscados por
 * su rotulo-- y se mira lo que el usuario veria: la barra de estado, los
 * veredictos, las filas de las listas y los ficheros que quedan. El motor es
 * el drvarma del PATH: pasar la puerta es lanzarlo.
 *
 * Sin servidor grafico no falla: dice "no hay ..." y sale con 0. Cada fallo
 * es una linea que empieza por "FAIL:".
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "sim.h"
#include "previewhost.h"
#include "preview.h"

int sim_main( int argc, char **argv );

static int fails = 0;

static void check( int ok, const char *what, const char *saw )
{
    if (ok) return;
    fails++;
    printf( "FAIL: %s; se vio: \"%s\"\n", what, saw ? saw : "(nada)" );
    fflush( stdout );
}

static void pump( int ms )
{
    gint64 until = g_get_monotonic_time() + (gint64) ms * 1000;
    while (g_get_monotonic_time() < until) {
        while (gtk_events_pending()) gtk_main_iteration_do( FALSE );
        g_usleep( 2000 );
    }
}

static int contiene( const char *s, const char *t ) { return s && t && strstr( s, t ) != NULL; }
static const char *etiqueta( GtkWidget *l ) { return l ? gtk_label_get_text( GTK_LABEL(l) ) : NULL; }

typedef struct { const char *txt; GtkWidget *hallado; } Busca;

static void busca_rec( GtkWidget *w, gpointer data )
{
    Busca *b = data;
    if (GTK_IS_BUTTON(w) && !b->hallado) {
        const char *l = gtk_button_get_label( GTK_BUTTON(w) );
        if (l && strcmp( l, b->txt ) == 0) b->hallado = w;
    }
    if (GTK_IS_CONTAINER(w)) gtk_container_forall( GTK_CONTAINER(w), busca_rec, data );
}

static GtkWidget *boton( GtkWidget *raiz, const char *txt )
{
    Busca b = { txt, NULL };
    busca_rec( raiz, &b );
    if (!b.hallado) check( 0, "no encuentro el boton", txt );
    return b.hallado;
}

static void pulsa( GtkWidget *b ) { if (b) gtk_button_clicked( GTK_BUTTON(b) ); pump( 30 ); }

static int filas( GtkWidget *tv )
{
    return gtk_tree_model_iter_n_children( gtk_tree_view_get_model( GTK_TREE_VIEW(tv) ), NULL );
}

/* El texto de la columna c de la fila i. g_free. */
static gchar *celda( GtkWidget *tv, int i, int c )
{
    GtkTreeModel *m = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter it;
    gchar *s = NULL;
    if (gtk_tree_model_iter_nth_child( m, &it, NULL, i )) gtk_tree_model_get( m, &it, c, &s, -1 );
    return s;
}

static void marca( GtkWidget *tv, int i )
{
    GtkTreePath *p = gtk_tree_path_new_from_indices( i, -1 );
    gtk_tree_selection_select_path( gtk_tree_view_get_selection( GTK_TREE_VIEW(tv) ), p );
    gtk_tree_path_free( p );
}

static gboolean espera( Sim *m, int segundos )
{
    gint64 hasta = g_get_monotonic_time() + (gint64) segundos * G_USEC_PER_SEC;
    pump( 20 );
    while ((m->pp.corriendo || m->pr.corriendo || m->pe.corriendo || m->pv.corriendo || m->pd.corriendo) &&
           g_get_monotonic_time() < hasta) pump( 20 );
    pump( 50 );
    return !m->pp.corriendo && !m->pr.corriendo && !m->pe.corriendo && !m->pv.corriendo && !m->pd.corriendo;
}

/* Las filas de la evaluacion con Diebold-Mariano (columna 6), y en cuantas
   su signo casa con el cociente (columna 5): DM > 0 si y solo si el
   sistema tiene el RMSE menor, porque los dos salen de los mismos pares. */
static int filas_con_dm( GtkWidget *tv, int *casan )
{
    GtkTreeModel *tm = gtk_tree_view_get_model( GTK_TREE_VIEW(tv) );
    GtkTreeIter it;
    int n = 0;
    gboolean hay = gtk_tree_model_get_iter_first( tm, &it );

    *casan = 0;
    while (hay) {
        gchar *ra = NULL, *dm = NULL;
        gtk_tree_model_get( tm, &it, 5, &ra, 6, &dm, -1 );
        if (dm && *dm) {
            n++;
            if (ra && *ra && ( ( atof( dm ) > 0 ) == ( atof( ra ) < 1 ) )) (*casan)++;
        }
        g_free( ra ); g_free( dm );
        hay = gtk_tree_model_iter_next( tm, &it );
    }
    return n;
}

static int filas_de( GtkWidget *tv ) { return filas( tv ); }

static void copia( const char *de, const char *a )
{
    gchar *c; gsize n;
    if (g_file_get_contents( de, &c, &n, NULL )) { g_file_set_contents( a, c, n, NULL ); g_free( c ); }
}

/* Copia de con la primera aparicion de viejo cambiada por nuevo. */
static void retoca( const char *de, const char *a, const char *viejo, const char *nuevo )
{
    gchar *c, *p; gsize n;
    GString *g;
    if (!g_file_get_contents( de, &c, &n, NULL )) return;
    g = g_string_new( c );
    p = strstr( g->str, viejo );
    if (p) { gssize at = p - g->str; g_string_erase( g, at, strlen( viejo ) ); g_string_insert( g, at, nuevo ); }
    g_file_set_contents( a, g->str, -1, NULL );
    g_string_free( g, TRUE ); g_free( c );
}

static int corre_main( int n, ... )
{
    char *av[8];
    va_list ap;
    int i;
    va_start( ap, n );
    av[0] = "sim_gui";
    for (i = 1; i <= n; i++) av[i] = va_arg( ap, char * );
    va_end( ap );
    av[n + 1] = NULL;
    return sim_main( n + 1, av );
}

static Sim *ventana_nueva( GtkApplication *app )
{
    Sim *m = g_new0( Sim, 1 );
    sim_ventana_new( app, m );
    pump( 50 );
    return m;
}

static gchar *sha( const char *f )
{
    gchar *c, *h; gsize n;
    if (!g_file_get_contents( f, &c, &n, NULL )) return g_strdup( "" );
    h = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) c, n );
    g_free( c );
    return h;
}

/* Un proyecto con un sistema (C1, drvarma) y un caso MT (C2, drtran), con
 * los dos .pre donde la madre los pondria. */
static gchar *proyecto( const char *trab, const char *datos )
{
    gchar *raiz = g_build_filename( trab, "proy", NULL );
    gchar *dm = g_build_filename( raiz, "MUSKRAT", "work", NULL );
    gchar *dk = g_build_filename( raiz, "MINK", "work", NULL );
    gchar *pm = g_build_filename( dm, "MUSKRAT_m03.pre", NULL );
    gchar *pk = g_build_filename( dk, "MINK_m02.pre", NULL );
    gchar *s, *hm, *hk, *yaml, *txt;

    g_mkdir_with_parents( dm, 0700 );
    g_mkdir_with_parents( dk, 0700 );
    s = g_build_filename( datos, "MUSKRAT_m03.pre", NULL ); copia( s, pm ); g_free( s );
    s = g_build_filename( datos, "MINK_m02.pre", NULL );    copia( s, pk ); g_free( s );
    hm = sha( pm ); hk = sha( pk );
    yaml = g_build_filename( raiz, "p.yaml", NULL );
    txt = g_strdup_printf(
        "schema_version: 2\nid: prueba\nseries:\n  MUSKRAT:\n  MINK:\n"
        "modelos:\n"
        "  MUSKRAT/m00:\n    version: 0\n    padre: \"\"\n    razon: \"\"\n    rol: datos\n"
        "  MUSKRAT/m03:\n    version: 3\n    padre: m00\n    razon: \"\"\n"
        "  MINK/m00:\n    version: 0\n    padre: \"\"\n    razon: \"\"\n    rol: datos\n"
        "  MINK/m02:\n    version: 2\n    padre: m00\n    razon: \"\"\n"
        "casos:\n"
        "  C1:\n    motor: drvarma\n    razon: \"\"\n    entradas: MUSKRAT/m03@%s MINK/m02@%s\n"
        "  C2:\n    motor: drtran\n    razon: \"\"\n    entradas: MUSKRAT/m03@%s MINK/m02@%s\n",
        hm, hk, hm, hk );
    g_file_set_contents( yaml, txt, -1, NULL );
    g_free( txt ); g_free( hm ); g_free( hk ); g_free( raiz ); g_free( dm ); g_free( dk );
    g_free( pm ); g_free( pk );
    return yaml;
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    Sim *m;
    gchar *trab, *cache, *dd, *mus, *min, *malo, *musx, *mind, *yaml, *s;
    const char *datos, *fake;

    if (argc < 4) { fprintf( stderr, "uso: test_gui <datos> <trabajo> <fake_drvarma>\n" ); return 2; }
    datos = argv[1];
    fake = argv[3];
    trab = g_canonicalize_filename( argv[2], NULL );
    cache = g_build_filename( trab, "cache", NULL );
    g_mkdir_with_parents( cache, 0700 );
    g_setenv( "XDG_CACHE_HOME", cache, TRUE );
    g_unsetenv( "SIM_DRVARMA" );
    g_chdir( trab );

    dd = g_build_filename( trab, "datos", NULL );
    g_mkdir_with_parents( dd, 0700 );
    mus  = g_build_filename( dd, "MUSKRAT_m03.pre", NULL );
    min  = g_build_filename( dd, "MINK_m02.pre", NULL );
    malo = g_build_filename( dd, "malo.pre", NULL );
    musx = g_build_filename( dd, "MUSKX.inp", NULL );
    mind = g_build_filename( dd, "MINKD.inp", NULL );
    s = g_build_filename( datos, "MUSKRAT_m03.pre", NULL ); copia( s, mus ); g_free( s );
    s = g_build_filename( datos, "MINK_m02.pre", NULL );    copia( s, min ); g_free( s );
    g_file_set_contents( malo, "esto no es un .pre\n12\n", -1, NULL );
    /* Un .pre retocado (su primer AR) ya no es un optimo: es un .inp. */
    retoca( mus, musx, "\n0.6522 1\n", "\n0.3000 1\n" );
    /* MINK con d = 1: dos series diferenciadas, la pregunta del rango. */
    retoca( min, mind, "\n0.00 0 0\n", "\n0.00 1 0\n" );

    /* --- las opciones, sin ventana ------------------------------------ */
    yaml = proyecto( trab, datos );
    check( corre_main( 1, "--help" ) == 0, "--help sale con 0", NULL );
    check( corre_main( 2, "--caso", "C1" ) == 2, "--caso sin --proyecto sale con 2", NULL );
    check( corre_main( 4, "--proyecto", yaml, "--modelo", "c01" ) == 2,
           "--modelo sin --caso sale con 2", NULL );
    check( corre_main( 2, "--proyecto", "no_existe.yaml" ) == 3,
           "un proyecto que no existe sale con 3", NULL );
    check( corre_main( 4, "--proyecto", yaml, "--caso", "C9" ) == 3,
           "un caso que no existe sale con 3", NULL );
    check( corre_main( 4, "--proyecto", yaml, "--caso", "C2" ) == 3,
           "un caso MT (drtran) sale con 3: es de drtran_gui", NULL );

    if (!gtk_init_check( &argc, &argv )) {
        printf( "no hay servidor grafico: la prueba de la ventana no se corre\n" );
        return fails ? 1 : 0;
    }
    if (!g_find_program_in_path( "drvarma" ))
        check( 0, "drvarma tiene que estar en el PATH para esta prueba", g_getenv( "PATH" ) );

    /* El buscador de main.c: el drvarma del arbol, o el de SIM_DRVARMA. */
    engine_set_finder( sim_hermano );
    app = gtk_application_new( "org.atsw.sim.test", G_APPLICATION_NON_UNIQUE );
    g_application_register( G_APPLICATION(app), NULL, NULL );

    /* --- al abrir ------------------------------------------------------ */
    m = ventana_nueva( app );
    check( contiene( etiqueta( m->estado ), ".pre" ), "al abrir, la barra pide los .pre", etiqueta( m->estado ) );
    check( gtk_notebook_get_n_pages( GTK_NOTEBOOK(m->libro) ) == 8 &&
           gtk_notebook_get_nth_page( GTK_NOTEBOOK(m->libro), PG_RANGO ) == m->pr.pagina,
           "ocho paginas: las siete y R · Rango", NULL );
    check( !gtk_widget_get_visible( m->pr.pagina ), "sin series, R · Rango no se ve", NULL );
    check( !gtk_widget_is_sensitive( GTK_WIDGET(m->tb_estimar) ) &&
           contiene( gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ), "dos series" ),
           "sin series, Estimar apagado y el globo pide dos series",
           gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ) );
    {
        GtkWidget *p3 = gtk_notebook_get_nth_page( GTK_NOTEBOOK(m->libro), PG_IDENT );
        Busca b = { "", NULL };
        (void) b;
        check( p3 != NULL, "la pagina 3 existe y dice que pide", NULL );
    }

    /* --- 1 · Series ---------------------------------------------------- */
    check( series_anade( m, mus ) && series_anade( m, min ), "los dos .pre entran", etiqueta( m->estado ) );
    check( filas( m->ps.lista ) == 2, "dos filas", NULL );
    s = celda( m->ps.lista, 1, 1 );
    check( !g_strcmp0( s, "MINK" ), "la segunda es MINK", s ); g_free( s );
    s = celda( m->ps.lista, 1, 9 );
    check( contiene( s, "pierde 1" ), "MINK pierde una observación en la ventana común", s ); g_free( s );
    check( contiene( etiqueta( m->ps.ver_ventana ), "1/1851 – 1/1911, 61" ),
           "la ventana común es la de drvarma", etiqueta( m->ps.ver_ventana ) );
    check( contiene( etiqueta( m->ps.ver_orden ), "Cholesky" ), "el orden es el de Cholesky",
           etiqueta( m->ps.ver_orden ) );
    check( !series_anade( m, malo ) && m->sis.n == 2 && contiene( etiqueta( m->estado ), "no es un fichero de fue" ),
           "un fichero que no es de fue se rechaza con motivo", etiqueta( m->estado ) );
    check( !series_anade( m, min ) && contiene( etiqueta( m->estado ), "ya está" ),
           "la misma serie dos veces, no", etiqueta( m->estado ) );
    check( contiene( gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ), "puerta" ),
           "con series y sin puerta, el globo de Estimar pide la puerta",
           gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ) );
    check( !contiene( etiqueta( m->pp.ver_rango ), "rango" ),
           "con una sola serie diferenciada, no se pregunta por el rango", etiqueta( m->pp.ver_rango ) );
    check( !gtk_widget_get_visible( m->pr.pagina ),
           "con MINK (d = 0), R · Rango no se ve: el caso mixto es de sima", NULL );

    /* --- 2 · Puerta, con el drvarma de verdad --------------------------- */
    check( contiene( etiqueta( m->pp.ver_puerta ), "Todavía no" ), "antes, la puerta dice que no se ha pasado",
           etiqueta( m->pp.ver_puerta ) );
    check( contiene( etiqueta( m->pp.orden ), "MUSKRAT_m03.pre MINK_m02.pre 0 0 -diagcov -o puerta" ),
           "la orden de la puerta se ve antes de lanzar", etiqueta( m->pp.orden ) );
    pulsa( boton( m->ventana, "Pasar la puerta" ) );
    check( espera( m, 120 ), "drvarma acaba", NULL );
    check( puerta_estado( m ) == 1, "la puerta pasa", etiqueta( m->pp.ver_puerta ) );
    check( contiene( etiqueta( m->pp.ver_puerta ), "PUERTA SUPERADA" ), "y la pagina lo dice",
           etiqueta( m->pp.ver_puerta ) );
    check( filas( m->pp.lista ) == 2, "la tabla de la puerta, con dos series", NULL );
    s = celda( m->pp.lista, 1, 4 );
    check( contiene( s, "ventana común" ), "MINK se movió por la ventana común", s ); g_free( s );
    s = g_build_filename( cache, SIM_CACHE, "puerta.out", NULL );
    check( g_file_test( s, G_FILE_TEST_EXISTS ), "sin proyecto, la puerta va a la cache", s ); g_free( s );
    check( gtk_widget_is_sensitive( GTK_WIDGET(m->tb_estimar) ),
           "con la puerta, Estimar se enciende", gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ) );
    check( !gtk_widget_is_sensitive( GTK_WIDGET(m->tb_diag) ), "sin estimar, Diagnosis apagado", NULL );
    printf( "puerta          : %s\n", etiqueta( m->pp.ver_puerta ) );

    /* --- 3 · Identificación: la CCF de los residuos, de la puerta ------- */
    check( m->pi.vale, "con la puerta, la identificación tiene sus CCF", etiqueta( m->pi.ver_pide ) );
    check( m->pi.marca[0][1] && m->pi.marca[1][0],
           "propone MUSKRAT ← MINK y MINK ← MUSKRAT (k = 1, Jenkins y Alavi)", NULL );
    check( m->pi.sigma_prop && contiene( etiqueta( m->pi.ver_sigma ), "Σ completa" ),
           "la contemporánea sale de la banda: Σ completa", etiqueta( m->pi.ver_sigma ) );
    check( contiene( etiqueta( m->pi.ver_lectura ), "Retroalimentación" ),
           "y lo lee como retroalimentación", etiqueta( m->pi.ver_lectura ) );
    {
        /* Pulsar la miniatura MINK -> MUSKRAT: la CCF entera, en la ventana
         * de gráficos, y el EPS que va al papel.                        */
        GList *hs = gtk_container_get_children( GTK_CONTAINER(m->pi.rejilla) ), *l;
        GtkWidget *celda = NULL;
        gboolean r = FALSE;
        for (l = hs; l && !celda; l = l->next) {
            /* «a» y «b» van +1, para que 0 sea «no es una miniatura». */
            if (GPOINTER_TO_INT(g_object_get_data( G_OBJECT(l->data), "a" )) == 1 &&
                GPOINTER_TO_INT(g_object_get_data( G_OBJECT(l->data), "b" )) == 2) celda = l->data;
        }
        g_list_free( hs );
        check( celda != NULL, "la miniatura MINK -> MUSKRAT está en la rejilla", NULL );
        if (celda) {
            GdkEvent *e = gdk_event_new( GDK_BUTTON_PRESS );
            g_signal_emit_by_name( celda, "button-press-event", e, &r );
            gdk_event_free( e );
            pump( 100 );
        }
        s = g_build_filename( cache, SIM_CACHE, "ccf_MINK_MUSKRAT.eps", NULL );
        check( r && g_file_test( s, G_FILE_TEST_EXISTS ), "pulsar escribe el EPS de la CCF entera", s );
        g_free( s );
    }
    {
        /* Desmarcar MINK ← MUSKRAT y llevarlo al modelo. */
        GList *hs = gtk_container_get_children( GTK_CONTAINER(m->pi.enlaces) ), *l;
        for (l = hs; l; l = l->next)
            if (contiene( gtk_button_get_label( GTK_BUTTON(l->data) ), "MINK ← MUSKRAT" ))
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(l->data), FALSE );
        g_list_free( hs );
    }
    pulsa( boton( m->ventana, "Al modelo" ) );
    check( m->mod.enlace[0][1] && !m->mod.enlace[1][0] && m->mod.p == 1 && !m->mod.diagcov,
           "Al modelo: un enlace, p = 1, Σ completa", etiqueta( m->estado ) );

    /* --- 4 · Modelo: la orden ------------------------------------------- */
    check( contiene( etiqueta( m->pm.orden ), "MUSKRAT_m03.pre MINK_m02.pre 1 0 -links \"MUSKRAT<-MINK\" -o modelo" ),
           "la orden lleva el enlace", etiqueta( m->pm.orden ) );
    check( contiene( etiqueta( m->pm.ver_forma ), "un solo sentido" ),
           "con un solo sentido, dice que es una red (MT)", etiqueta( m->pm.ver_forma ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pm.s_p), 2 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pm.s_q), 1 );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->pm.c_diag), TRUE );
    pump( 30 );
    check( contiene( etiqueta( m->pm.orden ), " 2 1 -diagcov -links" ), "p, q y Σ diagonal, en la orden",
           etiqueta( m->pm.orden ) );
    check( contiene( etiqueta( m->pm.ver_cuenta ), "1 enlace × (p + q = 3) = 3 coeficientes cruzados;" ),
           "la cuenta de coeficientes cruzados", etiqueta( m->pm.ver_cuenta ) );
    gtk_combo_box_set_active( GTK_COMBO_BOX(m->pm.cb_lik), 2 );
    pump( 30 );
    check( contiene( etiqueta( m->pm.orden ), "-lik both" ), "una opción avanzada, en la orden",
           etiqueta( m->pm.orden ) );
    pulsa( boton( m->ventana, "Todos" ) );
    check( !contiene( etiqueta( m->pm.orden ), "-links" ) && contiene( etiqueta( m->pm.ver_forma ), "retroalimentación" ),
           "Todos: sin -links, con retroalimentación", etiqueta( m->pm.orden ) );
    printf( "modelo          : %s\n", etiqueta( m->pm.orden ) );

    /* --- 5 · Estimación: el AR(2) cruzado de Jenkins y Alavi ------------ */
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pm.s_p), 2 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pm.s_q), 0 );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->pm.c_diag), FALSE );
    gtk_combo_box_set_active( GTK_COMBO_BOX(m->pm.cb_lik), 0 );
    pump( 30 );
    check( contiene( etiqueta( m->pe.orden ), "MUSKRAT_m03.pre MINK_m02.pre 2 0 -o modelo" ),
           "la estimación lanza la orden del modelo", etiqueta( m->pe.orden ) );
    pulsa( m->pe.b_estimar );
    check( espera( m, 300 ), "drvarma estima y acaba", NULL );
    check( contiene( etiqueta( m->pe.ver_fin ), "Convergió" ), "convergió", etiqueta( m->pe.ver_fin ) );
    check( m->pe.v && m->pe.v->npar == 18 && m->pe.v->lr_df == 5 && fabs( m->pe.v->lr - 41.17 ) < 1.0,
           "18 parámetros y LR ~ 41,17 con 5 gl (Jenkins y Alavi)", etiqueta( m->pe.ver_fin ) );
    check( gtk_widget_is_sensitive( GTK_WIDGET(m->tb_diag) ), "con un sistema estimado, Diagnosis se enciende", NULL );
    printf( "estimación      : %s\n", etiqueta( m->pe.ver_fin ) );

    /* --- 6 · Diagnosis --------------------------------------------------- */
    check( contiene( etiqueta( m->pd.ver_lr ), "aporta" ), "la LR: la dinámica cruzada aporta", etiqueta( m->pd.ver_lr ) );
    check( filas_de( m->pd.l_par ) == 18, "la tabla de parámetros, 18 filas", NULL );
    check( filas_de( m->pd.l_res ) == 2, "los residuos de las dos series", NULL );
    check( contiene( etiqueta( m->pd.ver_hosking ), "Hosking" ) && contiene( etiqueta( m->pd.ver_jb ), "Jarque-Bera" ),
           "Hosking y Jarque-Bera", etiqueta( m->pd.ver_hosking ) );
    check( contiene( etiqueta( m->pd.ver_raices ), "Estacionario e invertible" ), "las raíces",
           etiqueta( m->pd.ver_raices ) );
    check( contiene( etiqueta( m->pd.t_sigma ), "Correlaciones" ), "Σ y sus correlaciones", NULL );
    printf( "diagnosis       : %s\n", etiqueta( m->pd.ver_lr ) );

    /* --- Ver modelo: la página de cada serie, con el dibujante de fue ---- */
    check( gtk_widget_is_sensitive( GTK_WIDGET(m->tb_ver) ), "con un sistema estimado, Ver modelo se enciende", NULL );
    check( pagina_cruzados( m, 0 ) > 0 && pagina_cruzados( m, 1 ) > 0,
           "con todos los enlaces, las dos ecuaciones llevan términos cruzados", NULL );
    {
        int i;
        for (i = 0; i < 2; i++) {
            gchar *pdf = pagina_ruta( m, i );
            char why[256];
            check( pagina_serie( m, i, pdf, why, sizeof why ) == 0, "la página del modelo se escribe", why );
            check( g_file_test( pdf, G_FILE_TEST_EXISTS ) && contiene( pdf, i ? "modelo_MINK.pdf" : "modelo_MUSKRAT.pdf" ),
                   "con su nombre, junto al .out", pdf );
            g_free( pdf );
        }
    }
    check( pagina_ver( m, 1 ), "Ver modelo de MINK abre su página", etiqueta( m->estado ) );

    /* --- 7 · Previsión ---------------------------------------------------- */
    check( gtk_widget_is_sensitive( GTK_WIDGET(m->tb_prev) ), "con un sistema estimado, Prever se enciende", NULL );
    {
        gchar *t = NULL;
        GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->pv.t_irf) );
        GtkTextIter i0, i1;
        gtk_text_buffer_get_bounds( b, &i0, &i1 );
        t = gtk_text_buffer_get_text( b, &i0, &i1, FALSE );
        check( contiene( t, "Choque en MUSKRAT" ) && contiene( t, "Choque en MINK" ),
               "la respuesta al impulso, del .out de la estimación, sin correr nada", t );
        g_free( t );
        b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->pv.t_fevd) );
        gtk_text_buffer_get_bounds( b, &i0, &i1 );
        t = gtk_text_buffer_get_text( b, &i0, &i1, FALSE );
        check( contiene( t, "Descomposición" ) && contiene( t, "100.00" ), "y la descomposición", t );
        g_free( t );
    }
    check( prevision_calcular( m ) && espera( m, 300 ), "prever corre", NULL );
    check( m->pv.v && m->pv.v->nprev == 2 && m->pv.v->prev[0].nf == 10 && !m->pv.vd &&
           !strcmp( m->pv.v->prev[0].origen, "1/1911" ),
           "la previsión de las dos series, 10 periodos, sin evaluar", etiqueta( m->pv.ver_pide ) );
    check( contiene( etiqueta( m->pv.ver_pide ), "TEÓRICAS" ), "la página dice que las bandas son teóricas",
           etiqueta( m->pv.ver_pide ) );
    s = g_build_filename( cache, SIM_CACHE, "modelo_prev.forecast", NULL );
    check( g_file_test( s, G_FILE_TEST_EXISTS ), "en su propia corrida (_prev), sin tocar la estimación", s );
    g_free( s );
    check( m->pe.v && m->pe.v->npar == 18, "la estimación de «5» sigue siendo la misma", NULL );
    check( prevision_grafico( m, 0 ), "el gráfico de MUSKRAT se dibuja", etiqueta( m->estado ) );
    s = g_build_filename( cache, SIM_CACHE, "prev_MUSKRAT.eps", NULL );
    check( g_file_test( s, G_FILE_TEST_EXISTS ), "y queda su EPS", s );
    /* P6: ES EL DE fuf, sacado de la tabla de fuf del .out de drvarma */
    {
        gchar *t = NULL;
        check( g_file_get_contents( s, &t, NULL, NULL ) && contiene( t, "(ERR)" ),
               "y es el de fuf: con el panel ERR", s );
        g_free( t );
    }
    g_free( s );
    {
        GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->pv.t_prev) );
        GtkTextIter i0, i1;
        gchar *t;
        gtk_text_buffer_get_bounds( b, &i0, &i1 );
        t = gtk_text_buffer_get_text( b, &i0, &i1, FALSE );
        check( contiene( t, "FORECAST REPORT" ) && contiene( t, "VARIABLE NAME: MINK" )
               && contiene( t, "TRIMEST" ) && contiene( t, "La banda del 95 %" ),
               "la página enseña la tabla de fuf de cada serie, y la banda del 95 %", t );
        g_free( t );
    }
    /* P6: LA IRF DIBUJADA, con las bandas del metodo delta de la escalera */
    check( gtk_widget_get_sensitive( m->pv.b_irf ), "«Ver el gráfico» de la IRF, encendido", NULL );
    check( prevision_grafico_irf( m, 1, 0 ), "la respuesta de MUSKRAT a un choque en MINK se dibuja",
           etiqueta( m->estado ) );
    s = g_build_filename( cache, SIM_CACHE, "irf_MUSKRAT_MINK.eps", NULL );
    {
        gchar *t = NULL;
        check( m->pe.v && m->pe.v->tiene_irf_se, "el .out de drvarma trae los errores típicos de la IRF",
               NULL );
        check( g_file_get_contents( s, &t, NULL, NULL ) && contiene( t, "(Acumulada)" )
               && contiene( t, "todo delta" ), "con su acumulada, y la nota de las bandas (método delta)", s );
        g_free( t );
    }
    /* Y POR BOOTSTRAP: drvarma -irfboot en segundo plano; al acabar, el
       grafico usa sus percentiles y lo dice                             */
    check( gtk_widget_get_sensitive( m->pv.b_boot ), "«Bandas por bootstrap», encendido", NULL );
    check( contiene( etiqueta( m->pv.ver_boot ), "método delta" ), "y la página dice que hoy son las del delta",
           etiqueta( m->pv.ver_boot ) );
    check( prevision_bootstrap( m, 40 ), "el bootstrap arranca", etiqueta( m->estado ) );
    {
        gint64 hasta = g_get_monotonic_time() + 240 * G_USEC_PER_SEC;
        while (m->pv.bt_corriendo && g_get_monotonic_time() < hasta) pump( 20 );
        pump( 50 );
    }
    check( m->pv.bt && m->pv.bt_version == m->pe.version && m->pv.bt->n_rep == 40 &&
           m->pv.bt->nh == m->pe.v->nh_irf, "las bandas del bootstrap se leen: 40 réplicas",
           etiqueta( m->estado ) );
    check( contiene( etiqueta( m->pv.ver_boot ), "bootstrap (40" ), "y la página lo dice",
           etiqueta( m->pv.ver_boot ) );
    check( prevision_grafico_irf( m, 1, 0 ), "la IRF se dibuja con ellas", etiqueta( m->estado ) );
    {
        gchar *t = NULL;
        check( g_file_get_contents( s, &t, NULL, NULL ) && contiene( t, "bootstrap \\(40" ),   /* el EPS escapa el «(» */
               "y la nota dice que son del bootstrap", s );
        g_free( t );
    }
    g_free( s );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->pv.c_eval), TRUE );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pv.s_hor), 4 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pv.s_win), 48 );
    check( prevision_calcular( m ) && espera( m, 600 ), "prever y evaluar corre (el sistema y los univariantes)", NULL );
    check( m->pv.ve && m->pv.ve->nev == 2 && m->pv.vd && m->pv.vd->nev == 2,
           "las dos evaluaciones: la del sistema y la de los univariantes", etiqueta( m->pv.ver_eval ) );
    check( m->pv.v && !strcmp( m->pv.v->prev[0].origen, "1/1911" ),
           "y la previsión sigue siendo desde el final de la muestra, no de la ventana", NULL );
    check( filas_de( m->pv.l_eval ) == 8, "la tabla: 2 series x 4 horizontes", NULL );
    check( contiene( etiqueta( m->pv.ver_eval ), "orígenes" ) && contiene( etiqueta( m->pv.ver_eval ), "de 8" ),
           "el veredicto cuenta en cuántas gana el sistema", etiqueta( m->pv.ver_eval ) );
    {
        int casan, n = filas_con_dm( m->pv.l_eval, &casan );
        gchar *q = g_strdup_printf( "%d filas con DM, %d casan", n, casan );
        check( n == 8 && casan == 8, "Diebold-Mariano en las 8 filas (los pares dan el RMSE de drvarma), "
               "con el signo del cociente", q );
        g_free( q );
    }
    check( contiene( etiqueta( m->pv.ver_eval ), "Diebold-Mariano" ), "y el veredicto lo cuenta",
           etiqueta( m->pv.ver_eval ) );
    printf( "evaluación      : %s\n", etiqueta( m->pv.ver_eval ) );

    /* --- S7: simplificar y «¿es una red?» -------------------------------- */
    check( estructura_simplificar( m, 1.0 ) && espera( m, 300 ), "Simplificar estima de nuevo", etiqueta( m->estado ) );
    check( m->mod.zero[0] && m->pe.v && m->pe.v->npar < 18,
           "con los coeficientes cruzados de |t| < 1 a cero: menos parámetros", m->mod.zero );
    check( contiene( etiqueta( m->pe.orden ), "-zero" ), "la orden lleva -zero", etiqueta( m->pe.orden ) );
    check( contiene( etiqueta( m->pd.ver_simpl ), "antes de simplificar" ) &&
           contiene( etiqueta( m->pd.ver_simpl ), "LR =" ),
           "la diagnosis contrasta la simplificación contra el modelo de antes", etiqueta( m->pd.ver_simpl ) );
    printf( "simplificar     : %s\n", etiqueta( m->pd.ver_simpl ) );
    check( estructura_red( m, 0.05 ) && espera( m, 600 ), "«¿Es una red?» corre", NULL );
    check( contiene( etiqueta( m->pd.t_red ), "MINK no entra en MUSKRAT" ) &&
           contiene( etiqueta( m->pd.t_red ), "orden" ),
           "los pares y los órdenes triangulares, con su LR", etiqueta( m->pd.t_red ) );
    check( contiene( etiqueta( m->pd.ver_red ), "SIMULTÁNEO" ),
           "con retroalimentación en los dos sentidos, ningún orden se sostiene", etiqueta( m->pd.ver_red ) );
    printf( "estructura      : %s\n", etiqueta( m->pd.ver_red ) );
    pulsa( boton( m->ventana, "Quitar los ceros" ) );
    check( !m->mod.zero[0], "«Quitar los ceros» deja libres todos los coeficientes", m->mod.zero );

    /* --- la pared MA: no se presenta como un óptimo ---------------------- */
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pm.s_p), 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(m->pm.s_q), 1 );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->pm.c_diag), TRUE );
    pulsa( boton( m->ventana, "Ninguno" ) );
    m->mod.enlace[0][1] = 1;
    modelo_refresca( m );
    check( contiene( etiqueta( m->pm.orden ), "1 1 -diagcov -links \"MUSKRAT<-MINK\"" ), "el modelo de la pared",
           etiqueta( m->pm.orden ) );
    check( estima_lanzar( m ) && espera( m, 300 ), "estima", NULL );
    check( contiene( etiqueta( m->pe.ver_fin ), "PARED MA" ) && contiene( etiqueta( m->pe.ver_fin ), "NO es un óptimo" ),
           "parado en la pared MA: lo dice y no lo presenta como óptimo", etiqueta( m->pe.ver_fin ) );
    check( contiene( etiqueta( m->pd.ver_optim ), "pared MA" ), "y la diagnosis lo recuerda", etiqueta( m->pd.ver_optim ) );
    check( pagina_cruzados( m, 0 ) > 0 && pagina_cruzados( m, 1 ) == 0,
           "con solo MUSKRAT ← MINK, la ecuación de MINK es la de fue, sin cruzados", NULL );

    /* --- una estimación que falla (el motor falso sale con 3) ------------ */
    g_setenv( "SIM_DRVARMA", fake, TRUE );
    g_setenv( "SIM_FALSO", "estima", TRUE );
    check( estima_lanzar( m ) && espera( m, 30 ), "el motor falso estima", NULL );
    check( contiene( etiqueta( m->pe.ver_fin ), "FALLÓ" ), "una estimación fallida se dice en rojo",
           etiqueta( m->pe.ver_fin ) );
    check( !gtk_widget_is_sensitive( GTK_WIDGET(m->tb_diag) ) && contiene( etiqueta( m->pd.ver_pide ), "falló" ),
           "y no hay diagnosis que enseñar", etiqueta( m->pd.ver_pide ) );
    g_unsetenv( "SIM_FALSO" );
    g_unsetenv( "SIM_DRVARMA" );

    /* Cambiar el orden invalida la puerta: es otro sistema. */
    marca( m->ps.lista, 0 );
    pulsa( boton( m->ventana, "Bajar" ) );
    check( !strcmp( m->sis.s[0].nombre, "MINK" ), "Bajar mueve MUSKRAT al segundo puesto", m->sis.s[0].nombre );
    check( puerta_estado( m ) == 0 && contiene( etiqueta( m->pp.ver_puerta ), "cambió" ),
           "con otro orden, la puerta hay que volver a pasarla", etiqueta( m->pp.ver_puerta ) );
    check( !m->pi.vale && contiene( etiqueta( m->pi.ver_pide ), "puerta" ),
           "y la identificación vuelve a pedir la puerta", etiqueta( m->pi.ver_pide ) );

    /* --- un .pre que no es un optimo ------------------------------------ */
    marca( m->ps.lista, 1 );
    pulsa( boton( m->ventana, "Quitar" ) );
    check( m->sis.n == 1, "Quitar saca la serie marcada", NULL );
    check( series_anade( m, musx ), "el .inp retocado entra", etiqueta( m->estado ) );
    check( puerta_pasar( m ) && espera( m, 120 ), "la puerta corre", NULL );
    check( puerta_estado( m ) == 1, "la puerta pasa igual", etiqueta( m->pp.ver_puerta ) );
    check( contiene( etiqueta( m->pp.ver_notas ), "no un óptimo" ),
           "y la nota dice que MUSKRAT no es un óptimo", etiqueta( m->pp.ver_notas ) );

    /* --- dos diferenciadas: la pregunta del rango ----------------------- */
    sis_quita( &m->sis, 0 );
    check( series_anade( m, mind ), "MINK con d = 1 entra", etiqueta( m->estado ) );
    check( contiene( etiqueta( m->pp.ver_rango ), "R · Rango" ),
           "con dos diferenciadas, la puerta manda a R · Rango", etiqueta( m->pp.ver_rango ) );
    check( gtk_widget_get_visible( m->pr.pagina ), "y R · Rango se ve", NULL );

    /* --- una puerta que falla (el motor falso sale con 5) --------------- */
    g_setenv( "SIM_DRVARMA", fake, TRUE );
    check( puerta_pasar( m ) && espera( m, 30 ), "el motor falso corre", NULL );
    check( puerta_estado( m ) == -1 && contiene( etiqueta( m->pp.ver_puerta ), "FALLA" ),
           "una puerta que falla se dice en rojo", etiqueta( m->pp.ver_puerta ) );
    check( contiene( etiqueta( m->estado ), "FALLA" ), "y en la barra de estado", etiqueta( m->estado ) );
    check( !gtk_widget_is_sensitive( GTK_WIDGET(m->tb_estimar) ) &&
           contiene( gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ), "puerta" ),
           "con la puerta fallida, Estimar sigue pidiendo la puerta",
           gtk_widget_get_tooltip_text( GTK_WIDGET(m->tb_estimar) ) );

    /* --- el motor que no esta ------------------------------------------- */
    g_setenv( "SIM_DRVARMA", "/no/existe/drvarma", TRUE );
    check( !puerta_pasar( m ), "sin motor, la puerta no arranca", NULL );
    check( contiene( etiqueta( m->pp.ver_puerta ), "No pude lanzar drvarma" ),
           "y la pagina lo dice", etiqueta( m->pp.ver_puerta ) );
    g_unsetenv( "SIM_DRVARMA" );
    gtk_widget_destroy( m->ventana );

    /* --- con proyecto: el sistema C1 ------------------------------------ */
    {
        Sim *p = g_new0( Sim, 1 );
        char why[1024];

        check( sim_proyecto_abre( p, yaml, why, sizeof why ), "el proyecto se abre", why );
        snprintf( p->caso, sizeof p->caso, "C1" );
        sim_ventana_new( app, p );
        check( sim_caso_carga( p, why, sizeof why ) && !why[0], "el sistema C1 se carga", why );
        check( p->sis.n == 2 && !strcmp( p->sis.s[0].nombre, "MUSKRAT" ),
               "con sus dos series, en su orden", NULL );
        check( !gtk_widget_get_visible( p->ps.b_abrir ),
               "con un sistema, las series se cambian en la madre (sin Abrir)", NULL );
        check( contiene( gtk_window_get_title( GTK_WINDOW(p->ventana) ), "C1" ),
               "el título dice el sistema", gtk_window_get_title( GTK_WINDOW(p->ventana) ) );
        check( puerta_pasar( p ) && espera( p, 120 ) && puerta_estado( p ) == 1,
               "la puerta del sistema pasa", etiqueta( p->pp.ver_puerta ) );
        /* Dos estimaciones: dos modelos del sistema, el segundo hijo del primero. */
        p->mod.p = 1; p->mod.q = 0;
        check( estima_lanzar( p ) && espera( p, 300 ) && p->pe.v && !strcmp( p->pe.corrida, "c00" ),
               "la primera estimación es el modelo c00 del sistema", p->pe.corrida );
        s = g_strdup( p->pe.out );
        check( contiene( s, "_casos" ) && contiene( s, "C1_c00.out" ) && g_file_test( s, G_FILE_TEST_EXISTS ),
               "con su .out en el directorio del sistema", s );
        p->mod.p = 2;
        check( estima_lanzar( p ) && espera( p, 300 ) && !strcmp( p->pe.corrida, "c01" ),
               "la segunda es c01", p->pe.corrida );
        check( g_file_test( s, G_FILE_TEST_EXISTS ), "y no pisa a c00", s );
        g_free( s );
        check( pr_corrida_ver( p->proy, "C1", "c01" ) &&
               !g_strcmp0( pr_corrida_ver( p->proy, "C1", "c01" )->padre, "c00" ),
               "c01 cuelga de c00 en el linaje", NULL );
        {
            Proyecto *disco = g_new0( Proyecto, 1 );
            PrError e;
            check( pr_leer( yaml, disco, &e ) == 0 && disco->nco == 2, "el manifiesto guardado tiene las dos", NULL );
            g_free( disco );
        }
        /* P5: AL CUADERNO, con proyecto y caso: la página del modelo c01,
         * desde su ventana, con el caso, la corrida y su informe.        */
        {
            gchar *pdf = pagina_ruta( p, 0 ), *dir = g_path_get_dirname( p->proy->path );
            gchar *md = g_build_filename( dir, "cuaderno", "cuaderno.md", NULL ), *t = NULL;
            char   w2[512];

            check( preview_cuaderno_puede( p ), "con proyecto y caso, hay «Al cuaderno»", NULL );
            check( pagina_serie( p, 0, pdf, w2, sizeof w2 ) == 0 && preview_show( p, pdf ),
                   "la página del modelo, en su ventana", w2 );
            pump( 50 );
            preview_cuaderno( p, pdf );
            check( g_file_get_contents( md, &t, NULL, NULL )
                   && contiene( t, "El modelo — caso C1 / c01" )
                   && contiene( t, "**Informe:** _casos/C1/" ) && contiene( t, "C1_c01.out" )
                   && contiene( t, "](img/" ) && !contiene( t, "no se pudo guardar" ),
                   "y la entrada: el caso, la corrida, el informe y la imagen", t );
            g_free( t ); g_free( md ); g_free( dir ); g_free( pdf );
        }

        s = sim_artefacto( p, "puerta.out" );
        check( contiene( s, "_casos" ) && contiene( s, "C1_puerta.out" ) &&
               g_file_test( s, G_FILE_TEST_EXISTS ),
               "con proyecto, la puerta va al directorio del sistema", s );
        g_free( s );

        /* «PASAR A MT»: un modelo con un solo sentido (MUSKRAT <- MINK) ya es
         * triangular; es una red, y el sistema da un caso MT hermano con la
         * salida (MUSKRAT, la que recibe) primero.                       */
        {
            char why2[512];
            int a, b;
            for (a = 0; a < 2; a++) for (b = 0; b < 2; b++) p->mod.enlace[a][b] = 0;
            p->mod.enlace[0][1] = 1;
            p->mod.p = 1; p->mod.q = 0; p->mod.zero[0] = 0;
            check( estima_lanzar( p ) && espera( p, 300 ) && p->pe.v, "el modelo de un solo sentido se estima", NULL );
            check( estructura_red( p, 0.05 ) && espera( p, 300 ), "«¿Es una red?» corre", NULL );
            check( contiene( etiqueta( p->pd.ver_red ), "ES UNA RED" ) &&
                   contiene( etiqueta( p->pd.ver_red ), "ya es triangular" ),
                   "un solo sentido: ya es una red", etiqueta( p->pd.ver_red ) );
            check( gtk_widget_get_sensitive( p->pd.b_pasar_mt ), "con proyecto, «Pasar a MT» se enciende", NULL );
            check( estructura_pasar_mt( p, why2, sizeof why2 ), "«Pasar a MT» da de alta el caso", why2 );
            {
                const PrCaso *mt = &p->proy->ca[p->proy->nca - 1];
                check( !strcmp( mt->motor, "drtran" ) && !strcmp( mt->padre, "C1" ) &&
                       !strcmp( mt->en[0].serie, "MUSKRAT" ) && !strcmp( mt->en[1].serie, "MINK" ),
                       "un caso MT hermano, colgado del sistema, con la salida (MUSKRAT) primero",
                       mt->en[0].serie );
            }
        }

        /* CERRAR CON EL MOTOR CORRIENDO: se para, y lo que llega despues no
         * escribe en una ventana que ya no existe.                      */
        check( estima_lanzar( p ), "otra estimación arranca", NULL );
        gtk_widget_destroy( p->ventana );
        check( p->ventana == NULL, "al cerrar, la ventana deja de existir para el resto", NULL );
        check( espera( p, 60 ), "y el motor parado acaba sin romper nada", NULL );

        /* Reabrir con --modelo c00: la puerta y el modelo, sin volver a correr. */
        {
            Sim *r = g_new0( Sim, 1 );
            check( sim_proyecto_abre( r, yaml, why, sizeof why ), "se reabre", why );
            snprintf( r->caso, sizeof r->caso, "C1" );
            snprintf( r->modelo_ini, sizeof r->modelo_ini, "c00" );
            sim_ventana_new( app, r );
            check( sim_caso_carga( r, why, sizeof why ), "el sistema se carga", why );
            check( puerta_estado( r ) == 1, "su puerta, leída del .out, sin volver a pasarla",
                   etiqueta( r->pp.ver_puerta ) );
            check( r->pe.v && !strcmp( r->pe.corrida, "c00" ) && r->mod.p == 1,
                   "y el modelo c00, con su p en «4 · Modelo»", etiqueta( r->pe.ver_fin ) );
            check( contiene( etiqueta( r->l_modelo ), "c00" ), "la barra dice el modelo", etiqueta( r->l_modelo ) );
            gtk_widget_destroy( r->ventana );
        }

        /* Un .pre que cambia despues del alta: el sistema esta desfasado. */
        {
            Sim *q = g_new0( Sim, 1 );
            gchar *pm = g_build_filename( trab, "proy", "MUSKRAT", "work", "MUSKRAT_m03.pre", NULL );
            gchar *c; gsize n;
            g_file_get_contents( pm, &c, &n, NULL );
            s = g_strconcat( c, "\n", NULL );
            g_file_set_contents( pm, s, -1, NULL );
            g_free( s ); g_free( c );
            check( sim_proyecto_abre( q, yaml, why, sizeof why ), "se reabre", why );
            snprintf( q->caso, sizeof q->caso, "C1" );
            sim_ventana_new( app, q );
            check( sim_caso_carga( q, why, sizeof why ) && contiene( why, "DESFASADO" ) && q->desfasado,
                   "un .pre que cambió después del alta: el sistema está desfasado", why );
            gtk_widget_destroy( q->ventana );
            g_free( pm );
        }
    }

    /* --- R · Rango: Johansen y Juselius, con el drvec del arbol ---------- */
    {
        Sim *j = ventana_nueva( app );
        const char *nom[] = { "LRM", "LRY", "IBO", "IDE" };
        gchar *jj = g_build_filename( datos, "..", "..", "outvec", "datos", NULL );
        int i, al;

        for (i = 0; i < 4; i++) {
            gchar *de = g_strdup_printf( "%s/%s_u.pre", jj, nom[i] ), *a = g_build_filename( dd, nom[i], NULL );
            gchar *a2 = g_strdup_printf( "%s_u.pre", a );
            copia( de, a2 );
            check( series_anade( j, a2 ), "una serie de Johansen y Juselius entra", etiqueta( j->estado ) );
            g_free( de ); g_free( a ); g_free( a2 );
        }
        check( gtk_widget_get_visible( j->pr.pagina ), "cuatro I(1): R · Rango se ve", NULL );
        check( contiene( etiqueta( j->pr.ver_propuesta ), "Contrastar el rango" ),
               "antes de contrastar, la pagina dice que hacer", etiqueta( j->pr.ver_propuesta ) );
        /* sin bootstrap (segundos) y el caso 2, el del tutorial de sima */
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(j->pr.s_B), 0 );
        gtk_combo_box_set_active( GTK_COMBO_BOX(j->pr.c_caso), 2 );
        check( contiene( etiqueta( j->pr.orden ), "-case 2 -lrtest -name" ) &&
               !contiene( etiqueta( j->pr.orden ), "-bootstrap" ),
               "la orden: caso 2, -lrtest, sin -bootstrap", etiqueta( j->pr.orden ) );
        check( rango_contrastar( j ) && espera( j, 180 ), "drvec contrasta el rango", etiqueta( j->estado ) );
        check( rango_estado( j ) == 1 && j->pr.v->nrango == 4, "la prueba se lee: cuatro rangos",
               etiqueta( j->estado ) );
        check( filas( j->pr.l_rangos ) == 4 && filas( j->pr.l_pruebas ) == 3,
               "las tablas: 4 rangos y 3 contrastes", NULL );
        s = celda( j->pr.l_pruebas, 0, 1 );
        check( !g_strcmp0( s, "25.91" ), "la LR de r = 0 frente a 1 es la de sima (25.91)", s ); g_free( s );
        check( ovec_rango_propuesto( j->pr.v, &al ) == 0 &&
               contiene( etiqueta( j->pr.ver_propuesta ), "r = 0" ),
               "propone r = 0 (los asintóticos)", etiqueta( j->pr.ver_propuesta ) );
        check( contiene( etiqueta( j->pr.ver_notas ), "Sin bootstrap" ),
               "y avisa de que sin bootstrap la prueba sale pequeña", etiqueta( j->pr.ver_notas ) );
        check( contiene( etiqueta( j->pr.ver_puerta ), "suma" ), "la puerta de drvec, verificada",
               etiqueta( j->pr.ver_puerta ) );
        /* --- S10c: Al modelo y el VEC --------------------------------- */
        check( rango_al_modelo( j ) && j->mod.clase == SIS_VARMA && j->mod.p == 1,
               "Al modelo con r = 0: el VARMA en diferencias, con p − 1 = 1", etiqueta( j->estado ) );
        check( gtk_widget_is_sensitive( j->pm.rb_vec ), "con series I(1), la clase VEC se puede elegir", NULL );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(j->pm.rb_vec), TRUE );
        pump( 50 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(j->pm.s_p), 2 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(j->pm.s_r), 1 );
        gtk_combo_box_set_active( GTK_COMBO_BOX(j->pm.cb_caso), 2 );
        pump( 50 );
        check( j->mod.clase == SIS_VEC && gtk_widget_get_visible( j->pm.caja_vec ) &&
               !gtk_widget_get_visible( j->pm.caja_varma ), "VEC: se ven r y la normalización, no los enlaces", NULL );
        /* normalizar sobre LRM, como sima: de por omision es la ultima (IDE) */
        {
            GList *l = gtk_container_get_children( GTK_CONTAINER(j->pm.y1) ), *q;
            for (q = l; q; q = q->next)
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(q->data),
                    !strcmp( gtk_button_get_label( GTK_BUTTON(q->data) ), "LRM" ) );
            g_list_free( l );
        }
        pump( 50 );
        check( contiene( etiqueta( j->pm.orden ), "drvec LRY_u.pre IBO_u.pre IDE_u.pre LRM_u.pre 2 0 1 "
                         "-case 2 -fdhess -name" ), "la orden del VEC: LRM al final (Y1), r = 1, caso 2",
               etiqueta( j->pm.orden ) );
        check( contiene( etiqueta( j->pm.ver_vec ), "sobre LRM" ) && contiene( etiqueta( j->pm.ver_vec ), "comparten" ),
               "la página dice la normalización y que fija la comparación", etiqueta( j->pm.ver_vec ) );
        check( contiene( gtk_window_get_title( GTK_WINDOW(j->ventana) ), "SIM · VEC" ), "el título dice VEC",
               gtk_window_get_title( GTK_WINDOW(j->ventana) ) );
        check( puerta_pasar( j ) && espera( j, 120 ) && puerta_estado( j ) == 1,
               "la puerta de drvarma pasa con Johansen y Juselius", etiqueta( j->pp.ver_puerta ) );
        check( estima_lanzar( j ) && espera( j, 120 ), "drvec estima el VEC", etiqueta( j->estado ) );
        check( j->pe.vec && j->pe.vec->hay_vec && j->pe.vec->r == 1 && !j->pe.v,
               "el .out se lee como un VEC", etiqueta( j->pe.ver_fin ) );
        check( contiene( etiqueta( j->pe.ver_fin ), "normalizado sobre LRM" ) &&
               contiene( etiqueta( j->pe.ver_fin ), "convergió" ), "la estimación: convergió, sobre LRM",
               etiqueta( j->pe.ver_fin ) );
        check( gtk_widget_get_visible( j->pd.libreta ), "la diagnosis se ve", etiqueta( j->pd.ver_pide ) );
        check( contiene( etiqueta( j->pd.ver_lr ), "LRM = 1.12 LRY − 5.16 IBO + 4.26 IDE" ),
               "la relación de equilibrio es la de sima", etiqueta( j->pd.ver_lr ) );
        check( contiene( etiqueta( j->pd.ver_optim ), "LRM -0.166 (t -2.27): corrige el error" ),
               "α en LRM: −0.17, t −2.27, corrige el error", etiqueta( j->pd.ver_optim ) );
        check( filas( j->pd.l_res ) == 4 && filas( j->pd.l_par ) == 33,
               "los residuos de las cuatro y los 33 parámetros", NULL );
        s = celda( j->pd.l_res, 3, 0 );
        check( !g_strcmp0( s, "ec1" ), "la última fila de residuos es el error de equilibrio (ec1)", s ); g_free( s );
        check( contiene( etiqueta( j->pd.ver_hosking ), "Hosking" ), "Hosking, de la salida de drvec",
               etiqueta( j->pd.ver_hosking ) );
        check( gtk_widget_is_sensitive( GTK_WIDGET(j->tb_diag) ) && gtk_widget_is_sensitive( GTK_WIDGET(j->tb_prev) ) &&
               gtk_widget_is_sensitive( GTK_WIDGET(j->tb_ver) ) &&
               contiene( gtk_widget_get_tooltip_text( GTK_WIDGET(j->tb_ver) ), "α (ec − E)" ),
               "con un VEC: Diagnosis, Prever y Ver modelo (con α (ec − E))",
               gtk_widget_get_tooltip_text( GTK_WIDGET(j->tb_ver) ) );

        /* --- S10e: Ver modelo del VEC ------------------------------------- */
        {
            gchar *nota = pagina_nota( j, 0 );
            double r[OV_MAX_OBS];
            int i, n;
            check( nota && contiene( nota, "ec(t) = N(LRM,t) - 1.1196 N(LRY,t) + 5.1557 N(IBO,t) - 4.2617 N(IDE,t)" ),
                   "la nota dice ec, la relación de sima, normalizada sobre LRM", nota );
            check( nota && contiene( nota, "sin sus deterministas (los de su .pre, fijos)" ),
                   "LRM lleva sus deterministas del .pre, y la nota lo dice", nota );
            g_free( nota );
            check( pagina_cruzados( j, 0 ) > 0 && pagina_cruzados( j, 1 ) > 0,
                   "las ecuaciones del VEC llevan α (ec − E) y los Γ cruzados", NULL );
            /* LRM es la de Y1: el .out trae los residuos de ec1; los suyos se
               reconstruyen, a = a_ec − β₂′ a_Y2, y su sd es la σ del modelo */
            n = pagina_residuos( j, 0, r, OV_MAX_OBS );
            {
                double s = 0, s2 = 0, sd;
                for (i = 0; i < n; i++) s += r[i];
                for (i = 0; i < n; i++) s2 += ( r[i] - s / n ) * ( r[i] - s / n );
                sd = n > 1 ? sqrt( s2 / n ) : 0;
                gchar *v = g_strdup_printf( "n %d, sd %.4f, σ %.4f", n, sd, sqrt( j->pe.vec->sigma[3][3] ) );
                check( n == 54 && fabs( sd - 2.11 ) < 0.02,
                       "los residuos de LRM, reconstruidos desde los de ec1: sd 2.11 (σ 2.13)", v );
                g_free( v );
            }
            for (i = 0; i < 4; i++) {
                gchar *pdf = pagina_ruta( j, i );
                char why[256];
                check( pagina_serie( j, i, pdf, why, sizeof why ) == 0 && g_file_test( pdf, G_FILE_TEST_EXISTS ),
                       "la página del VEC se escribe", why );
                g_free( pdf );
            }
            check( pagina_ver( j, 0 ), "Ver modelo de LRM abre su página", etiqueta( j->estado ) );
        }

        /* --- S10c: la debil exogeneidad (Johansen y Juselius, sima) ------ */
        check( restricciones_weakex( j ), "la débil exogeneidad arranca", etiqueta( j->estado ) );
        {
            gint64 hasta = g_get_monotonic_time() + 240 * G_USEC_PER_SEC;
            while (j->pd.wx_corriendo && g_get_monotonic_time() < hasta) pump( 20 );
            pump( 50 );
        }
        {
            int i, iby = -1, ide = -1, lry = -1;
            for (i = 0; i < j->pd.wx_n; i++) {
                if (!strcmp( j->pd.wx_serie[i], "IBO" )) iby = i;
                if (!strcmp( j->pd.wx_serie[i], "IDE" )) ide = i;
                if (!strcmp( j->pd.wx_serie[i], "LRY" )) lry = i;
            }
            check( j->pd.wx_n == 4 && iby >= 0 && j->pd.wx_ok[iby] && j->pd.wx_lr[iby] < 0.01 &&
                   j->pd.wx_p[iby] > 0.95, "IBO débilmente exógena: LR 0.0003, p 0.99 (sima)", NULL );
            check( ide >= 0 && j->pd.wx_ok[ide] && fabs( j->pd.wx_lr[ide] - 2.07 ) < 0.05 &&
                   fabs( j->pd.wx_p[ide] - 0.15 ) < 0.01, "IDE: LR 2.07, p 0.15 (sima)", NULL );
            check( lry >= 0 && j->pd.wx_ok[lry] && fabs( j->pd.wx_lr[lry] - 4.31 ) < 0.05 &&
                   j->pd.wx_p[lry] < 0.05, "LRY no: LR 4.31, p 0.04 (sima)", NULL );
        }
        check( contiene( etiqueta( j->pd.ver_wx ), "IBO, IDE" ), "la página dice cuáles son débilmente exógenas",
               etiqueta( j->pd.ver_wx ) );
        /* otra normalizacion en «4 · Modelo»: no se contrasta */
        j->mod.y1[0] = 0; j->mod.y1[3] = 1;
        check( !restricciones_weakex( j ) && contiene( etiqueta( j->estado ), "normalización" ),
               "con otra normalización en «4 · Modelo», no contrasta y lo dice", etiqueta( j->estado ) );
        j->mod.y1[0] = 1; j->mod.y1[3] = 0;

        /* --- S10c: la prevision y la evaluacion del VEC ------------------ */
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(j->pv.s_hor), 4 );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(j->pv.c_eval), TRUE );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(j->pv.s_win), 40 );
        check( prevision_calcular( j ), "Calcular arranca con un VEC", etiqueta( j->estado ) );
        {
            gint64 hasta = g_get_monotonic_time() + 240 * G_USEC_PER_SEC;
            while (j->pv.corriendo && g_get_monotonic_time() < hasta) pump( 20 );
            pump( 50 );
        }
        check( j->pv.es_vec && j->pv.nfvec == 4 && j->pv.fvec[0].nf == 4,
               "la previsión del VEC: cuatro series, cuatro periodos", etiqueta( j->estado ) );
        check( contiene( etiqueta( j->pv.ver_pide ), "Previsión del VEC" ), "y la página lo dice",
               etiqueta( j->pv.ver_pide ) );
        /* LA TABLA Y EL GRAFICO DE fuf, tambien con un VEC (fase P6): drvec
           escribe la tabla en su .out, y de ella salen los dos.          */
        {
            GtkTextBuffer *tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(j->pv.t_prev) );
            GtkTextIter a0, a1;
            gchar *tx;

            gtk_text_buffer_get_bounds( tb, &a0, &a1 );
            tx = gtk_text_buffer_get_text( tb, &a0, &a1, FALSE );
            check( contiene( tx, "VARIABLE NAME: LRM" ) && contiene( tx, "TRIMEST" ) &&
                   contiene( tx, "La banda del 95 % del nivel" ),
                   "la tabla de fuf del VEC, y debajo la banda del nivel", tx );
            g_free( tx );
        }
        check( prevision_grafico( j, 3 ), "el gráfico de fuf de una serie del VEC se dibuja",
               etiqueta( j->estado ) );
        check( filas( j->pv.l_eval ) == 16 && contiene( etiqueta( j->pv.ver_eval ), "mismos" ),
               "la evaluación: 4 series × 4 horizontes, sobre los mismos orígenes que los univariantes",
               etiqueta( j->pv.ver_eval ) );
        {
            int casan, n = filas_con_dm( j->pv.l_eval, &casan );
            gchar *q = g_strdup_printf( "%d filas con DM, %d casan", n, casan );
            check( n == 16 && casan == 16, "con un VEC, Diebold-Mariano en las 16 filas, con el signo "
                   "del cociente", q );
            g_free( q );
        }
        check( !gtk_widget_get_visible( j->pv.b_boot ) || !gtk_widget_get_visible(
                   gtk_widget_get_parent( j->pv.b_boot ) ),
               "con un VEC no se ofrece el bootstrap de drvarma", NULL );
        {
            GtkTreeModel *tm = gtk_tree_view_get_model( GTK_TREE_VIEW(j->pv.l_eval) );
            GtkTreeIter it;
            int n0 = -1;
            if (gtk_tree_model_get_iter_first( tm, &it )) gtk_tree_model_get( tm, &it, 2, &n0, -1 );
            gchar *vn = g_strdup_printf( "%d", n0 );
            /* drvec guarda un origen solo si se pueden comprobar los H pasos:
               con E = 40 de 55 y H = 4, del 39 al 50 (sima, evaluate_vec) */
            check( n0 == 12, "12 orígenes (los que comprueban los 4 pasos; E = 40 de 55)", vn );
            g_free( vn );
        }
        {
            GtkTextBuffer *tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(j->pv.t_irf) );
            GtkTextIter a0, a1;
            gchar *tx;
            gtk_text_buffer_get_bounds( tb, &a0, &a1 );
            tx = gtk_text_buffer_get_text( tb, &a0, &a1, FALSE );
            check( contiene( tx, "EN NIVELES" ) && strlen( tx ) > 300, "la IRF del VEC, en niveles", NULL );
            g_free( tx );
        }

        /* otra configuracion: la prueba ya no es de esta */
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(j->pr.s_p), 3 );
        check( rango_estado( j ) == 0 && contiene( etiqueta( j->pr.ver_propuesta ), "cambiaron" ),
               "con otro p, hay que volver a contrastar", etiqueta( j->pr.ver_propuesta ) );
        /* el motor que no esta */
        g_setenv( "SIM_DRVEC", "/no/existe/drvec", TRUE );
        check( !rango_contrastar( j ) && contiene( etiqueta( j->pr.ver_propuesta ), "No pude lanzar drvec" ),
               "sin drvec, la pagina lo dice", etiqueta( j->pr.ver_propuesta ) );
        g_unsetenv( "SIM_DRVEC" );
        gtk_widget_destroy( j->ventana );
        g_free( jj );
    }

    printf( "sim_gui: %s\n", fails ? "hay fallos" : "todo bien" );
    return fails ? 1 : 0;
}
