/*
 * diagnosis.c -- la pantalla de diagnosis.
 *
 * DIAGNOSTICAR ES IDENTIFICAR OTRA VEZ. Es el hallazgo del estudio de TASTE, y
 * alli estaba dicho con la estructura del codigo: DiagnTFI (TFEST.PAS:55-62) es
 * literalmente la misma secuencia que la identificacion univariante
 * (USID.PAS:242-251) con 'RESIDUOS' en lugar de la serie.
 *
 * Con una correccion obligada. TASTE tenia UNA SOLA ranura de residuos
 * (TASTECTV.PAS:475): estimar un segundo modelo pisaba los del primero, asi que
 * no se podian comparar dos modelos. Aqui los residuos son del .out de LA
 * CORRIDA, y cada corrida tiene el suyo.
 *
 * QUE HACE ESTA PANTALLA, Y QUE NO. No calcula nada. El motor ya hace toda la
 * diagnosis y la hace bien: por serie, media, desviacion, asimetria, curtosis,
 * el histograma con el PORCENTAJE OBSERVADO CONTRA EL ESPERADO, la ACF con el
 * Ljung-Box escalonado y la PACF; luego Hosking y Jarque-Bera multivariantes; y
 * por cada enlace, la CCF entre el ruido estimado y la entrada preblanqueada,
 * con su veredicto.
 *
 * Lo que faltaba es que 2000 lineas de .out se conviertan en algo con lo que se
 * pueda DECIDIR. Eso es todo lo que hay aqui.
 *
 * Y LO QUE SE PONE DELANTE ES EL VEREDICTO POR ENLACE, porque es lo accionable
 * y porque son DOS diagnosticos opuestos que se arreglan de forma opuesta:
 *
 *     significativo en k >= 0  ->  falta estructura en LA TRANSFERENCIA
 *                                  se arregla cambiando (b, r, s)
 *     significativo en k <  0  ->  RETROALIMENTACION: la entrada no es exogena
 *                                  se arregla quitando el enlace, o subiendo
 *                                  al VARMA simultaneo
 *
 * Confundirlos es reespecificar durante horas lo que no tenia arreglo en este
 * escalon.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "gui.h"
#include "previewhost.h"
#include "outdiag.h"

enum { D_QUE, D_NOMBRE, D_CONTRASTE, D_P, D_VEREDICTO, D_N };

/* ------------------------------------------------------------------------ */

static const char *si_no( int v, const char *si, const char *no )
{
    return v ? si : no;
}

static void refresca_lista( Mtram *m )
{
    Diag         *D  = &m->dia;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(D->lista) ) );
    GtkTreeIter   it;
    char          b[128], p[32];
    int           i;

    gtk_list_store_clear( st );
    if (!D->vale) return;

    /* --- los enlaces primero: es lo que se decide --------------------- */
    for (i = 0; i < D->d.ne; i++) {
        const OdEnlace *e = &D->d.e[i];

        snprintf( b, sizeof b, "Q(%d) = %.4f", e->transfer.df, e->transfer.q );
        snprintf( p, sizeof p, "%.4f", e->transfer.p );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            D_QUE,       "transferencia",
            D_NOMBRE,    e->entrada,
            D_CONTRASTE, b,
            D_P,         p,
            D_VEREDICTO, si_no( e->adecuado, "adecuada",
                                "FALTA ESTRUCTURA — cambia (b, r, s)" ),
            -1 );

        snprintf( b, sizeof b, "Q(%d) = %.4f", e->exogen.df, e->exogen.q );
        snprintf( p, sizeof p, "%.4f", e->exogen.p );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            D_QUE,       "exogeneidad",
            D_NOMBRE,    e->entrada,
            D_CONTRASTE, b,
            D_P,         p,
            D_VEREDICTO, si_no( e->exogeno, "exógena",
                                "RETROALIMENTACIÓN — el enlace sobra, o toca VARMA" ),
            -1 );
    }

    /* --- y los residuos de cada ecuacion ------------------------------ */
    for (i = 0; i < D->d.ns; i++) {
        const OdSerie *s = &D->d.s[i];
        const char    *nom = i < m->c.n && m->c.s[i]->ts.name
                             ? m->c.s[i]->ts.name : s->nombre;

        snprintf( b, sizeof b, "L-B Q(%d) = %.2f", s->lb.df, s->lb.q );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            D_QUE,       "residuos",
            D_NOMBRE,    nom,
            D_CONTRASTE, b,
            D_P,         "",
            D_VEREDICTO, "",
            -1 );

        if (s->tiene_hist) {
            snprintf( b, sizeof b, "%.2f%% vs %.2f%%  y  %.2f%% vs %.2f%%",
                      s->fuera1, s->esp1, s->fuera2, s->esp2 );
            gtk_list_store_append( st, &it );
            gtk_list_store_set( st, &it,
                D_QUE,       "  normalidad",
                D_NOMBRE,    nom,
                D_CONTRASTE, b,
                D_P,         "",
                D_VEREDICTO, "fuera de ±1 y ±2, observado contra esperado",
                -1 );
        }
    }
}

static void refresca_veredicto( Mtram *m )
{
    Diag    *D = &m->dia;
    GString *t = g_string_new( NULL );
    int      mal, nex, i;

    if (!D->vale) {
        gtk_label_set_text( GTK_LABEL(D->veredicto),
            "Estima primero. La diagnosis sale del .out que escribe el motor: "
            "no se recalcula aquí." );
        g_string_free( t, TRUE );
        return;
    }

    mal = od_no_adecuados( &D->d );
    nex = od_no_exogenos( &D->d );

    /* --- el conjunto ------------------------------------------------- */
    if (D->d.hosking.hay)
        g_string_append_printf( t,
            "Hosking al retardo %d:  Q(%d) = %.4f,  p = %.4f  →  %s\n",
            D->d.hosking_lag, D->d.hosking.df, D->d.hosking.q, D->d.hosking.p,
            D->d.hosking_blanco
              ? "los residuos son ruido blanco CONJUNTAMENTE"
              : "los residuos NO son ruido blanco: falta estructura" );

    if (D->d.jb.hay)
        g_string_append_printf( t,
            "Jarque-Bera:  JB(%d) = %.4f,  p = %.4f  →  %s\n",
            D->d.jb.df, D->d.jb.q, D->d.jb.p,
            D->d.jb_normal ? "normalidad no rechazada"
                           : "normalidad RECHAZADA" );

    if (D->d.hosking.hay && D->d.jb.hay && D->d.hosking_blanco && !D->d.jb_normal)
        g_string_append( t,
            "\nLos dos veredictos van al revés, y no es contradictorio: "
            "incorrelación y normalidad\nson cosas distintas. Unos residuos "
            "pueden ser blancos y no gaussianos — atípicos,\nasimetría, colas. "
            "Mira el histograma del .out: si el exceso está en las colas, la\n"
            "sospecha es un atípico, y eso se trata con una intervención, no "
            "reespecificando.\n" );

    /* --- lo accionable ----------------------------------------------- */
    g_string_append_c( t, '\n' );

    if (D->d.ne == 0)
        g_string_append( t, "No hay enlaces que juzgar." );
    else if (!mal && !nex)
        g_string_append_printf( t,
            "Los %d enlaces pasan los dos contrastes.", D->d.ne );
    else {
        if (mal) {
            g_string_append_printf( t,
                "%d enlace%s con FALTA DE ESTRUCTURA:", mal,
                mal == 1 ? "" : "s" );
            for (i = 0; i < D->d.ne; i++)
                if (D->d.e[i].adecuado == 0)
                    g_string_append_printf( t, "  %s (p = %.4f)",
                        D->d.e[i].entrada, D->d.e[i].transfer.p );
            g_string_append( t,
                "\n   La CCF entre el ruido y la entrada preblanqueada tiene "
                "señal en k ≥ 0: lo que\n   ν(B) describe no agota la "
                "relación. Se arregla en (b, r, s) — vuelve a la\n"
                "   pestaña Identificación y mira DÓNDE está el pico.\n" );
        }
        if (nex) {
            g_string_append_printf( t,
                "\n%d enlace%s con RETROALIMENTACIÓN:", nex,
                nex == 1 ? "" : "s" );
            for (i = 0; i < D->d.ne; i++)
                if (D->d.e[i].exogeno == 0)
                    g_string_append_printf( t, "  %s (p = %.4f)",
                        D->d.e[i].entrada, D->d.e[i].exogen.p );
            g_string_append( t,
                "\n   Hay señal en k < 0: la salida antecede a la entrada. "
                "Un modelo de transferencia\n   supone que la entrada no "
                "responde a la salida. Esto NO se arregla con (b, r, s):\n"
                "   o el enlace sobra, o el sistema es simultáneo y el escalón "
                "que toca es drvarma.\n" );
        }
        if (mal && nex)
            g_string_append( t,
                "\nSon diagnósticos OPUESTOS y se arreglan de forma opuesta. "
                "Confundirlos cuesta\nuna tarde de reespecificar lo que no "
                "tenía arreglo en este escalón.\n" );
    }

    if (D->path)
        g_string_append_printf( t, "\n\n(de %s)", D->path );

    gtk_label_set_text( GTK_LABEL(D->veredicto), t->str );
    g_string_free( t, TRUE );
}

void diagnosis_refresca( Mtram *m )
{
    refresca_lista( m );
    refresca_veredicto( m );
}

/* Leer el .out de la ultima estimacion. La llama la pantalla de Estimacion
 * cuando el motor acaba: los residuos son DE ESTA CORRIDA.               */
void diagnosis_desde( Mtram *m, const char *path )
{
    Diag *D = &m->dia;

    D->vale = FALSE;
    g_free( D->path );
    D->path = NULL;

    if (path && od_parse_file( path, &D->d ) == 0) {
        D->vale = TRUE;
        D->path = g_strdup( path );
    }
    diagnosis_refresca( m );
}

/* ------------------------------------------------------------------------ */

static void on_releer( GtkButton *b, Mtram *m )
{
    if (!m->est.out_path) {
        preview_show_status( m, "No hay ningún .out todavía: estima primero." );
        return;
    }
    diagnosis_desde( m, m->est.out_path );
    if (!m->dia.vale)
        preview_show_status( m, "En %s no hay diagnosis. ¿Llegó a estimar?",
                             m->est.out_path );
}

static void on_ver_out( GtkButton *b, Mtram *m )
{
    if (!m->dia.path) {
        preview_show_status( m, "No hay .out que enseñar." );
        return;
    }
    /* El .out entero: los graficos de la ACF, la PACF y las CCF estan ahi,
     * con la lectura pegada al dibujo. Esta pantalla resume; no sustituye. */
    preview_open_external( m, m->dia.path );
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

GtkWidget *diagnosis_pagina_new( Mtram *m )
{
    Diag         *D = &m->dia;
    GtkWidget    *caja, *barra, *b, *sc, *marco, *vb;
    GtkListStore *st;

    D->vale = FALSE;
    D->path = NULL;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Releer el .out" );
    gtk_widget_set_tooltip_text( b,
        "La diagnosis sale del .out de la última estimación. Aquí no se "
        "recalcula nada: el motor ya la hace." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_releer), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Ver el .out entero" );
    gtk_widget_set_tooltip_text( b,
        "Los gráficos —la ACF con el Ljung-Box escalonado, la PACF, el "
        "histograma con el porcentaje esperado, las CCF— están ahí, con la "
        "lectura pegada al dibujo. Esta pantalla resume; no sustituye." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_ver_out), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    st = gtk_list_store_new( D_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    D->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( D->lista, "Qué",       D_QUE );
    columna( D->lista, "De",        D_NOMBRE );
    columna( D->lista, "Contraste", D_CONTRASTE );
    columna( D->lista, "p",         D_P );
    columna( D->lista, "Veredicto", D_VEREDICTO );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), D->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    marco = gtk_frame_new( "Qué hay que hacer con esto" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    D->veredicto = gtk_label_new( "Estima primero." );
    gtk_widget_set_halign( D->veredicto, GTK_ALIGN_START );
    gtk_label_set_line_wrap( GTK_LABEL(D->veredicto), TRUE );
    gtk_label_set_selectable( GTK_LABEL(D->veredicto), TRUE );
    gtk_container_add( GTK_CONTAINER(vb), D->veredicto );
    gtk_container_add( GTK_CONTAINER(marco), vb );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    return caja;
}
