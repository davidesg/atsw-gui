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
    while ((m->pp.corriendo || m->pe.corriendo) && g_get_monotonic_time() < hasta) pump( 20 );
    pump( 50 );
    return !m->pp.corriendo && !m->pe.corriendo;
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
    check( gtk_notebook_get_n_pages( GTK_NOTEBOOK(m->libro) ) == 7, "siete paginas", NULL );
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
    check( contiene( etiqueta( m->pp.ver_rango ), "rango" ),
           "con dos diferenciadas, se pregunta por el rango", etiqueta( m->pp.ver_rango ) );

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
        s = sim_artefacto( p, "puerta.out" );
        check( contiene( s, "_casos" ) && contiene( s, "C1_puerta.out" ) &&
               g_file_test( s, G_FILE_TEST_EXISTS ),
               "con proyecto, la puerta va al directorio del sistema", s );
        g_free( s );

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

    printf( "sim_gui: %s\n", fails ? "hay fallos" : "todo bien" );
    return fails ? 1 : 0;
}
