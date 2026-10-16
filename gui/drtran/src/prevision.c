/*
 * prevision.c -- la pantalla de prevision y evaluacion.
 *
 * LA DISTINCION QUE ESTA PANTALLA EXISTE PARA NO DEJAR CONFUNDIR:
 *
 *   las bandas de la prevision son TEORICAS -- dicen lo que el modelo implica
 *   la evaluacion fuera de muestra es EMPIRICA -- dice lo que pasa de verdad
 *
 * El motor lo escribe con todas las letras en su informe: «las varianzas que el
 * modelo da son TEORICAS: dicen lo que el modelo implica, no lo que ocurre
 * fuera de muestra, donde la incertidumbre de los parametros y el cambio
 * estructural tienen su parte». Un modelo puede dar bandas estrechas y fallar;
 * las bandas no son una promesa.
 *
 * Y DE AHI SALE LO QUE ESTA PANTALLA TIENE Y TASTE NO PODIA TENER. TASTE
 * guardaba UNOS residuos (TASTECTV.PAS:475), asi que estimar un segundo modelo
 * borraba el primero: no habia con que comparar. Aqui la evaluacion de cada
 * corrida se GUARDA, y se pueden poner dos al lado.
 *
 * Eso es lo unico que decide empiricamente si un modelo predice mejor que otro.
 * No el ajuste, no la verosimilitud, no las bandas: el error fuera de muestra,
 * horizonte por horizonte, con los parametros CLAVADOS mientras el origen rueda.
 *
 * LAS DOS VENTANAS SON INDEPENDIENTES, y el motor lo dice: la ventana de
 * ESTIMACION (-estwin E) fija de donde salen los parametros; el ORIGEN (-O g)
 * dice desde donde se preve. Comparar dos modelos exige el MISMO origen, y por
 * eso el origen esta a la vista y no escondido.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "gui.h"
#include "barra.h"
#include "previewhost.h"
#include "preview.h"
#include "outfcst.h"
#include "fugplot.h"

enum { P_H, P_N, P_MAE, P_RMSE, P_MAPE, P_COMP, P_N_COLS };

/* ------------------------------------------------------------------------ */

static void refresca_tabla( Mtram *m )
{
    Prev         *P  = &m->prev;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(P->lista) ) );
    GtkTreeIter   it;
    char          b[64], c[64];
    int           i;

    gtk_list_store_clear( st );
    if (!P->vale || !P->f.tiene_ev) return;

    for (i = 0; i < P->f.ev.nh; i++) {
        const OfHoriz *h = &P->f.ev.h[i];

        snprintf( c, sizeof c, "%s", "" );

        /* Contra la guardada, si hay: la diferencia RELATIVA del RMSE, que es
         * lo que se mira para decidir. Se dice el signo en palabras porque
         * «-3%» se lee mal en una tabla de errores.                      */
        if (P->tiene_ref && i < P->ref.nh && P->ref.h[i].rmse > 0.0) {
            double d = 100.0 * ( h->rmse - P->ref.h[i].rmse ) / P->ref.h[i].rmse;

            snprintf( c, sizeof c, "%+.1f%%  %s", d,
                      fabs( d ) < 0.05 ? "igual" : d < 0 ? "MEJOR" : "peor" );
        }

        snprintf( b, sizeof b, "%d", h->h );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            P_H,    b,
            P_N,    h->n,
            P_MAE,  g_strdup_printf( "%.6f", h->mae ),
            P_RMSE, g_strdup_printf( "%.6f", h->rmse ),
            P_MAPE, g_strdup_printf( "%.4f", h->mape ),
            P_COMP, c,
            -1 );
    }
}

static void refresca_texto( Mtram *m )
{
    Prev    *P = &m->prev;
    GString *t = g_string_new( NULL );

    if (!P->vale) {
        gtk_label_set_text( GTK_LABEL(P->texto),
            "Pulsa «Calcular»: la previsión sale de la misma ejecución del\n"
            "motor que la estimación, y el botón la pide él solo." );
        g_string_free( t, TRUE );
        return;
    }

    /* --- la prevision, que es TEORICA ---------------------------------- */
    if (P->f.ns > 0) {
        const OfSerie *s = &P->f.s[0];
        int            i;

        /* La salida del modelo es la primera serie cargada. */
        if (m->c.n > 0 && m->c.s[0]->ts.name) {
            const OfSerie *q = of_serie( &P->f, m->c.s[0]->ts.name );

            if (q) s = q;
        }

        g_string_append_printf( t,
            "%s — origen %s, %d periodo%s.  Nivel y variación anual, con "
            "su desviación típica:\n\n",
            s->nombre, s->origen, s->lead, s->lead == 1 ? "" : "s" );

        for (i = s->nf - s->nprev; i < s->nf; i++)
            g_string_append_printf( t,
                "   %-9s %12.2f  ± %8.2f      %+10.2f  ± %8.2f\n",
                s->f[i].fecha, s->f[i].nivel, s->f[i].sd_nivel,
                s->f[i].var_anu, s->f[i].sd_anu );

        g_string_append( t,
            "\nEsas desviaciones son TEÓRICAS: dicen lo que el modelo implica, "
            "no lo que pasa\nfuera de muestra, donde la incertidumbre de los "
            "parámetros y el cambio estructural\ntienen su parte. Un modelo "
            "puede dar bandas estrechas y fallar." );

        if (P->f.ns > 1)
            g_string_append_printf( t,
                "\n\n(y otras %d series: prever una salida exige prever sus "
                "entradas, así que el error\nde la salida suma su propia "
                "innovación y las de todo lo que tiene aguas arriba)",
                P->f.ns - 1 );
    }

    /* --- la descomposicion, y su negativa razonada --------------------- */
    if (P->f.decomp == 0)
        g_string_append( t,
            "\n\nLa descomposición de la varianza del error NO se da, y el "
            "motor explica por qué:\nΣ no es diagonal, y con innovaciones "
            "correlacionadas la descomposición no es única\n— hay que darle la "
            "parte común a alguien, y eso exige una ordenación (Cholesky).\n"
            "Es el problema del VAR. El motor lo evita mientras Σ sea diagonal "
            "y lo DECLARA\ncuando no lo es. Es un resultado, no un hueco." );

    /* --- la evaluacion, que es EMPIRICA -------------------------------- */
    if (P->f.tiene_ev)
        g_string_append_printf( t,
            "\n\nEvaluación fuera de muestra: %s, %d orígenes (observación %d "
            "a la %d).\nLos parámetros se estimaron UNA vez y se mantuvieron "
            "fijos mientras el origen rodaba.\nEsto es lo único que decide "
            "empíricamente si un modelo predice mejor que otro.",
            P->f.ev.salida, P->f.ev.origenes, P->f.ev.desde, P->f.ev.hasta );
    else
        g_string_append( t,
            "\n\nSin evaluación fuera de muestra. Marca «Evaluar», da una "
            "ventana de estimación y pulsa\n«Calcular»: las bandas de arriba no "
            "se pueden contrastar con nada mientras no la haya." );

    if (P->tiene_ref)
        g_string_append_printf( t,
            "\n\nComparando con la guardada (%s). La columna de la derecha es "
            "la diferencia\nrelativa del RMSE por horizonte.", P->ref_que );

    gtk_label_set_text( GTK_LABEL(P->texto), t->str );
    g_string_free( t, TRUE );
}

void prevision_refresca( Mtram *m )
{
    refresca_tabla( m );
    refresca_texto( m );

    /* El boton se apaga mientras el motor corre: no hay dos corridas a la vez,
     * y un boton que se puede pulsar y no hace nada miente.             */
    if (m->prev.b_calc)
        gtk_widget_set_sensitive( m->prev.b_calc, !m->est.corriendo );
}

/* La llama la estimacion al acabar, con SU .out. */
void prevision_desde( Mtram *m, const char *path )
{
    Prev *P = &m->prev;

    P->vale = FALSE;
    if (path && of_parse_file( path, &P->f ) == 0) P->vale = TRUE;
    prevision_refresca( m );
}

/* ------------------------------------------------------------------------ */

/* Guardar la evaluacion de esta corrida para comparar con la siguiente. Es
 * justo lo que TASTE no podia: alli estimar otra vez borraba la anterior. */
static void on_guardar_ref( GtkButton *b, Mtram *m )
{
    Prev *P = &m->prev;

    if (!P->vale || !P->f.tiene_ev) {
        preview_show_status( m, "No hay evaluación que guardar: marca "
                                "«Evaluar», da una ventana y pulsa «Calcular»." );
        return;
    }
    P->ref = P->f.ev;
    P->tiene_ref = TRUE;
    snprintf( P->ref_que, sizeof P->ref_que, "%d enlaces, %d orígenes",
              m->red.n, P->f.ev.origenes );
    preview_show_status( m, "Guardada. Cambia el modelo, vuelve a pulsar "
                            "«Calcular» y la tabla dirá si mejora." );
    prevision_refresca( m );
}

static void on_olvidar_ref( GtkButton *b, Mtram *m )
{
    m->prev.tiene_ref = FALSE;
    prevision_refresca( m );
}

/* CALCULAR DESDE AQUI.
 *
 * La prevision no se calcula aparte: sale de la MISMA corrida del motor que la
 * estimacion, porque drtran la hace en la misma pasada. Pero eso obligaba a
 * marcar la casilla aqui, irse a la pagina 5, pulsar «Estimar» y volver -- y
 * la casilla no dice en ninguna parte que haya que hacer ese viaje.
 *
 * Asi que el boton lo hace entero: marca lo que haga falta y lanza. Es la
 * regla del diseño, la de TASTE: cada pantalla fabrica lo que pide, y no manda
 * al analista a otra pagina a buscar el interruptor.                     */
static void on_calcular( GtkButton *b, Mtram *m )
{
    Prev *P = &m->prev;

    if (m->est.corriendo) {
        preview_show_status( m, "El motor ya está corriendo." );
        return;
    }

    /* Si no hay nada marcado, marcar «Prever» es lo unico que el boton puede
     * querer decir. No se pregunta: se hace, y se ve marcado.          */
    if (!P->prever && !P->evaluar)
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(P->c_prever), TRUE );

    if (P->evaluar && P->ventana <= 0) {
        preview_show_status( m, "La evaluación necesita una ventana de "
            "estimación: ponla en «ventana», o desmarca «Evaluar»." );
        return;
    }

    if (estima_lanzar( m )) {
        prevision_refresca( m );        /* apaga el boton mientras corre */
        preview_show_status( m, "Estimando con %s… la tabla se llena al "
            "acabar; la consola está en Estimación.",
            P->evaluar ? "previsión y evaluación" : "previsión" );
    }
}

/* ------------------------------------------------------------------------ */
/* Para la barra (docs/DISENO-barra-drtran.md)                               */
/* ------------------------------------------------------------------------ */

void prevision_calcular( Mtram *m )
{
    gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_PREV );
    on_calcular( NULL, m );
}

/* EL GRAFICO DE LA PREVISION (H4 de docs/DISENO-barra-drtran.md), con
 * fugdraw: lo que se ve es lo que va al papel (regla 7 de mtram). Dos
 * paneles, el nivel y la variacion anual, como los da la tabla, con las
 * bandas de +-2 d.t. -- y la nota de que son TEORICAS, en el propio dibujo:
 * un grafico se recorta y se pega sin la pantalla que lo explicaba.     */
static gchar *latin1( const char *s )
{
    gchar *r = g_convert_with_fallback( s, -1, "ISO-8859-1", "UTF-8", "?",
                                        NULL, NULL, NULL );

    return r ? r : g_strdup( s );
}

gchar *prevision_grafico( Mtram *m, int k )
{
    Prev          *P = &m->prev;
    const OfSerie *S;
    FPPanel        pn[2];
    double        *y[2], *sd[2];
    const char   **fe;
    gchar         *t[2], *nota, *dir, *base, *eps = NULL, *u;
    FDFig         *fig;
    int            i, freq;

    if (!P->vale || k < 0 || k >= P->f.ns || P->f.s[k].nprev < 1) return NULL;
    S    = &P->f.s[k];
    freq = (m->c.n > 0) ? m->c.s[0]->ts.freq : 1;

    fe = g_new0( const char *, S->nf );
    for (i = 0; i < 2; i++) {
        y[i]  = g_new0( double, S->nf );
        sd[i] = g_new0( double, S->nf );
    }
    for (i = 0; i < S->nf; i++) {
        fe[i]    = S->f[i].fecha;
        y[0][i]  = S->f[i].nivel;    sd[0][i] = S->f[i].sd_nivel;
        y[1][i]  = S->f[i].var_anu;  sd[1][i] = S->f[i].sd_anu;
    }
    u = g_strdup_printf( "%s: el nivel, desde %s", S->nombre, S->origen );
    t[0] = latin1( u ); g_free( u );
    t[1] = latin1( freq > 1 ? "La variación anual" : "La variación" );
    nota = latin1( "Bandas de ±2 d.t., TEÓRICAS: lo que el modelo implica, no lo "
                   "que pasa fuera de muestra." );
    for (i = 0; i < 2; i++) {
        pn[i].title = t[i];
        pn[i].y     = y[i];
        pn[i].sd    = sd[i];
    }

    dir = g_build_filename( g_get_user_cache_dir(), GUI_CACHE, NULL );
    g_mkdir_with_parents( dir, 0700 );
    base = g_build_filename( dir, S->nombre, NULL );
    fig  = fp_forecast( pn, 2, S->nf, S->nprev, fe, nota, base );
    if (fig) {
        gchar *nom = g_strdup_printf( "prev_%s.eps", S->nombre );

        eps = g_build_filename( dir, nom, NULL );
        g_free( nom );
        fd_fig_free( fig );
        if (!g_file_test( eps, G_FILE_TEST_EXISTS )) { g_free( eps ); eps = NULL; }
    }

    for (i = 0; i < 2; i++) { g_free( y[i] ); g_free( sd[i] ); g_free( t[i] ); }
    g_free( fe ); g_free( nota ); g_free( base ); g_free( dir );
    return eps;
}

static void on_grafico_mi( GtkMenuItem *mi, Mtram *m )
{
    int    k   = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(mi), "serie" ) );
    gchar *eps = prevision_grafico( m, k );

    if (!eps) {
        preview_show_status( m, "No pude dibujar la previsión." );
        return;
    }
    if (!preview_show( m, eps ))
        preview_show_status( m, "No pude enseñar %s", eps );
    g_free( eps );
}

/* La salida primero, como en Ver modelo: es la serie del modelo. */
static int salida_en( const Mtram *m, const Forecast *f )
{
    int k;

    if (m->c.n > 0 && m->c.s[0]->ts.name)
        for (k = 0; k < f->ns; k++)
            if (!strcmp( f->s[k].nombre, m->c.s[0]->ts.name )) return k;
    return 0;
}

static void on_calcular_mi( GtkMenuItem *mi, Mtram *m ) { prevision_calcular( m ); }
static void on_guardar_mi ( GtkMenuItem *mi, Mtram *m ) { on_guardar_ref( NULL, m ); }
static void on_olvidar_mi ( GtkMenuItem *mi, Mtram *m ) { on_olvidar_ref( NULL, m ); }
static void on_pestana_mi ( GtkMenuItem *mi, Mtram *m )
{
    gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_PREV );
}

void prevision_menu( Mtram *m, GtkWidget *menu, GtkWidget *ref )
{
    Prev      *P = &m->prev;
    GtkWidget *mi;

    at_entrada( menu, "Calcular la previsión", "prever", "go-jump", ref,
                G_CALLBACK(on_calcular_mi), m );
    if (P->vale && P->f.ns > 0) {
        int k0 = salida_en( m, &P->f ), j;

        for (j = -1; j < P->f.ns; j++) {
            int    k = (j < 0) ? k0 : j;
            gchar *r;

            if (j >= 0 && j == k0) continue;
            r = (j < 0) ? g_strdup_printf( "Ver el gráfico de la previsión: %s, "
                                           "la salida", P->f.s[k].nombre )
                        : g_strdup_printf( "El gráfico de %s", P->f.s[k].nombre );
            mi = at_entrada( menu, r, NULL, NULL, ref,
                             G_CALLBACK(on_grafico_mi), m );
            g_object_set_data( G_OBJECT(mi), "serie", GINT_TO_POINTER(k) );
            g_free( r );
        }
    } else {
        mi = gtk_menu_item_new_with_label( "Ver el gráfico de la previsión" );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
        gtk_widget_set_sensitive( mi, FALSE );
        gtk_widget_set_tooltip_text( mi, "Todavía no hay previsión: «Calcular "
                                         "la previsión» la hace." );
    }
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
    mi = at_entrada( menu, "Guardar esta evaluación", NULL, NULL, ref,
                     G_CALLBACK(on_guardar_mi), m );
    if (!P->vale || !P->f.tiene_ev) {
        gtk_widget_set_sensitive( mi, FALSE );
        gtk_widget_set_tooltip_text( mi, "No hay evaluación: marca «Evaluar» "
                                         "en «7 · Previsión» y calcula." );
    }
    mi = at_entrada( menu, "Olvidarla", NULL, NULL, ref,
                     G_CALLBACK(on_olvidar_mi), m );
    if (!P->tiene_ref) {
        gtk_widget_set_sensitive( mi, FALSE );
        gtk_widget_set_tooltip_text( mi, "No hay ninguna evaluación guardada." );
    }
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
    at_entrada( menu, "Ir a la pestaña de la previsión", NULL, NULL, ref,
                G_CALLBACK(on_pestana_mi), m );
}

static void on_cambio( GtkWidget *w, Mtram *m )
{
    Prev *P = &m->prev;

    P->prever  = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_prever) );
    P->evaluar = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) );
    P->horizonte = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_hor) );
    P->ventana   = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_win) );

    gtk_widget_set_sensitive( P->s_hor, P->prever || P->evaluar );
    gtk_widget_set_sensitive( P->s_win, P->evaluar );

    estima_refresca( m );          /* la orden cambia: se ve al momento */
    prevision_refresca( m );
}

/* ------------------------------------------------------------------------ */

static void columna( GtkWidget *tv, const char *titulo, int col )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                               titulo, r, "text", col, NULL );

    gtk_tree_view_column_set_resizable( c, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), c );
}

GtkWidget *prevision_pagina_new( Mtram *m )
{
    Prev         *P = &m->prev;
    GtkWidget    *caja, *barra, *b, *sc, *marco, *vb;
    GtkListStore *st;

    P->b_calc = NULL;
    P->vale = P->tiene_ref = FALSE;
    P->prever = P->evaluar = FALSE;
    P->horizonte = 8;
    P->ventana   = 0;
    P->ref_que[0] = 0;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    /* EL BOTON VA PRIMERO, porque es lo que se viene a hacer aqui. Lo demas
     * de la barra son sus ajustes.                                      */
    P->b_calc = gtk_button_new_with_label( "Calcular" );
    gtk_widget_set_tooltip_text( P->b_calc,
        "Lanza drtran pidiéndole la previsión. Sale de la MISMA ejecución que "
        "la estimación —el motor la hace en la misma pasada—, así que no hay "
        "que ir a la página de Estimación a pulsar nada." );
    g_signal_connect( P->b_calc, "clicked", G_CALLBACK(on_calcular), m );
    gtk_box_pack_start( GTK_BOX(barra), P->b_calc, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    P->c_prever = gtk_check_button_new_with_label( "Prever (-f)" );
    gtk_widget_set_tooltip_text( P->c_prever,
        "Prever una salida exige prever sus entradas: el error de la salida "
        "suma su propia innovación y todas las de aguas arriba, cada una "
        "propagada por la ν(B) que atraviesa." );
    g_signal_connect( P->c_prever, "toggled", G_CALLBACK(on_cambio), m );
    gtk_box_pack_start( GTK_BOX(barra), P->c_prever, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_label_new( "periodos" ),
                        FALSE, FALSE, 0 );
    P->s_hor = gtk_spin_button_new_with_range( 1, 60, 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(P->s_hor), 8 );
    g_signal_connect( P->s_hor, "value-changed", G_CALLBACK(on_cambio), m );
    gtk_box_pack_start( GTK_BOX(barra), P->s_hor, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    P->c_eval = gtk_check_button_new_with_label( "Evaluar fuera de muestra (-C)" );
    gtk_widget_set_tooltip_text( P->c_eval,
        "Estima UNA vez sobre la ventana, clava los parámetros y rueda el "
        "origen comparando cada previsión con lo que de verdad pasó. Es lo "
        "único que decide empíricamente entre dos modelos." );
    g_signal_connect( P->c_eval, "toggled", G_CALLBACK(on_cambio), m );
    gtk_box_pack_start( GTK_BOX(barra), P->c_eval, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_label_new( "ventana" ),
                        FALSE, FALSE, 0 );
    P->s_win = gtk_spin_button_new_with_range( 0, 10000, 1 );
    gtk_widget_set_tooltip_text( P->s_win,
        "La ventana de ESTIMACIÓN: de dónde salen los parámetros. Es "
        "independiente del origen de la previsión, y comparar dos modelos "
        "exige la misma en los dos." );
    g_signal_connect( P->s_win, "value-changed", G_CALLBACK(on_cambio), m );
    gtk_box_pack_start( GTK_BOX(barra), P->s_win, FALSE, FALSE, 0 );

    gtk_widget_set_sensitive( P->s_hor, FALSE );
    gtk_widget_set_sensitive( P->s_win, FALSE );

    /* --- la comparacion --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Guardar esta evaluación" );
    gtk_widget_set_tooltip_text( b,
        "Para comparar con la siguiente. TASTE no podía: tenía una sola "
        "ranura de residuos y estimar otra vez borraba la anterior." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_guardar_ref), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Olvidarla" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_olvidar_ref), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    /* --- la tabla de la evaluacion --- */
    st = gtk_list_store_new( P_N_COLS, G_TYPE_STRING, G_TYPE_INT,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING );
    P->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( P->lista, "Horizonte", P_H );
    columna( P->lista, "n",         P_N );
    columna( P->lista, "MAE",       P_MAE );
    columna( P->lista, "RMSE",      P_RMSE );
    columna( P->lista, "MAPE %",    P_MAPE );
    columna( P->lista, "vs la guardada", P_COMP );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), P->lista );
    marco = gtk_frame_new( "Error fuera de muestra, por horizonte" );
    gtk_container_add( GTK_CONTAINER(marco), sc );
    gtk_box_pack_start( GTK_BOX(caja), marco, TRUE, TRUE, 0 );

    /* --- la prevision --- */
    marco = gtk_frame_new( "La previsión" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    P->texto = gtk_label_new( "" );
    gtk_widget_set_halign( P->texto, GTK_ALIGN_START );
    gtk_label_set_selectable( GTK_LABEL(P->texto), TRUE );
    gtk_label_set_line_wrap( GTK_LABEL(P->texto), TRUE );
    gtk_container_add( GTK_CONTAINER(vb), P->texto );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), vb );
    gtk_container_add( GTK_CONTAINER(marco), sc );
    gtk_widget_set_size_request( marco, -1, 240 );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    prevision_refresca( m );
    return caja;
}
