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
#include <glib/gstdio.h>

#include "gui.h"
#include "barra.h"
#include "previewhost.h"
#include "preview.h"
#include "outfcst.h"
#include "fufplot.h"
#include "engine.h"

enum { P_H, P_N, P_MAE, P_RMSE, P_MAPE, P_UNI, P_RATIO, P_DM, P_COMP, P_N_COLS };

/* ------------------------------------------------------------------------ */

static void refresca_tabla( Mtram *m )
{
    Prev         *P  = &m->prev;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(P->lista) ) );
    GtkTreeIter   it;
    char          b[64], c[64], uni[64], rat[64], dmt[64];
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

        /* FRENTE AL UNIVARIANTE, con la misma ventana: su RMSE, el cociente y
         * Diebold-Mariano. Las estrellas, como forecast_compare.py.     */
        {
        const FcHoriz *u = NULL;
        int            j;

        uni[0] = rat[0] = dmt[0] = 0;
        if (P->hay_comp)
            for (j = 0; j < P->comp.nh; j++)
                if (P->comp.h[j].h == h->h) u = &P->comp.h[j];
        if (u) {
            snprintf( uni, sizeof uni, "%.6f", u->rmse_a );
            snprintf( rat, sizeof rat, "%.3f  %s", u->ratio,
                      fabs( u->ratio - 1.0 ) < 0.0005 ? "igual"
                      : u->ratio < 1.0 ? "MEJOR" : "peor" );
            if (u->dm_ok)
                snprintf( dmt, sizeof dmt, "%+.2f  p = %.4f%s", u->dm, u->p,
                          u->p < 0.05 ? " **" : u->p < 0.10 ? " *" : "" );
            else
                snprintf( dmt, sizeof dmt, "—" );
        } else if (P->uni_corriendo)
            snprintf( uni, sizeof uni, "calculando…" );
        }

        snprintf( b, sizeof b, "%d", h->h );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            P_H,    b,
            P_N,    h->n,
            P_MAE,  g_strdup_printf( "%.6f", h->mae ),
            P_RMSE, g_strdup_printf( "%.6f", h->rmse ),
            P_MAPE, g_strdup_printf( "%.4f", h->mape ),
            P_UNI,  uni,
            P_RATIO, rat,
            P_DM,   dmt,
            P_COMP, c,
            -1 );
    }
}

static void refresca_texto( Mtram *m )
{
    Prev    *P = &m->prev;
    GString *t = g_string_new( NULL );

    if (!P->vale) {
        gtk_widget_hide( P->tabla );
        gtk_label_set_text( GTK_LABEL(P->texto),
            "Pulsa «Calcular»: la previsión sale de la misma ejecución del\n"
            "motor que la estimación, y el botón la pide él solo." );
        g_string_free( t, TRUE );
        return;
    }

    /* --- la prevision, que es TEORICA ---------------------------------- */
    /* LA TABLA ES LA DEL .out, TAL CUAL: la misma que lee el analista en el
     * informe, como fue_gui ensena el .out de fuf. Rehacerla aqui con los
     * numeros leidos seria una segunda tabla que podria no cuadrar.     */
    if (P->f.ns > 0) {
        const OfSerie *s = &P->f.s[0];
        static char    tb[65536];

        /* La salida del modelo es la primera serie cargada. */
        if (m->c.n > 0 && m->c.s[0]->ts.name) {
            const OfSerie *q = of_serie( &P->f, m->c.s[0]->ts.name );

            if (q) s = q;
        }

        if (P->out && of_tabla_file( P->out, s->nombre, tb, sizeof tb ) == 0) {
            /* fuf escribe Latin-1; drtran, ASCII: solo se convierte lo que
             * no es UTF-8 ya.                                          */
            gchar *u = g_utf8_validate( tb, -1, NULL ) ? g_strdup( tb )
                     : g_convert_with_fallback( tb, -1, "UTF-8", "ISO-8859-1",
                                                "?", NULL, NULL, NULL );
            gchar *e = g_markup_escape_text( u ? u : tb, -1 );
            gchar *mk = g_strdup_printf( "<tt>%s</tt>", e );

            gtk_label_set_markup( GTK_LABEL(P->tabla), mk );
            gtk_widget_show( P->tabla );
            g_free( mk ); g_free( e ); g_free( u );
        } else
            gtk_widget_hide( P->tabla );

        g_string_append_printf( t,
            "%s — origen %s, %d periodo%s.  ", s->nombre, s->origen,
            s->lead, s->lead == 1 ? "" : "s" );
        g_string_append( t,
            "Las desviaciones de la tabla son TEÓRICAS: dicen lo que el modelo "
            "implica, no lo que pasa\nfuera de muestra, donde la incertidumbre "
            "de los parámetros y el cambio estructural\ntienen su parte. Un "
            "modelo puede dar bandas estrechas y fallar." );

        if (P->f.ns > 1)
            g_string_append_printf( t,
                "\n\n(y otras %d series: prever una salida exige prever sus "
                "entradas, así que el error\nde la salida suma su propia "
                "innovación y las de todo lo que tiene aguas arriba. Sus "
                "tablas\nestán en el .out y sus gráficos en «Prever ▾»)",
                P->f.ns - 1 );
    } else
        gtk_widget_hide( P->tabla );

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

    if (P->f.tiene_ev) {
        if (P->hay_comp)
            g_string_append( t,
                "\n\nFrente al univariante —el diagonal, con la MISMA ventana y "
                "los mismos orígenes—: el cociente es\nRMSE del modelo / RMSE "
                "del univariante, y menos de 1 dice que la transferencia prevé "
                "mejor.\nDiebold-Mariano contrasta si la diferencia es real: "
                "DM > 0 a favor del modelo (HLN, t de N−1;\n** al 5 %, * al "
                "10 %). A horizonte largo se apoya en pocos datos "
                "independientes: los errores\nde h pasos solapan h−1." );
        else if (P->uni_corriendo)
            g_string_append( t,
                "\n\nEvaluando el univariante con la misma ventana, para "
                "comparar…" );
        else if (P->evaluar)
            g_string_append( t,
                "\n\nNo hay comparación con el univariante: su evaluación no "
                "llegó a hacerse (mira la barra de estado)." );
    }

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
        gtk_widget_set_sensitive( m->prev.b_calc,
                                  !m->est.corriendo && !m->prev.uni_corriendo );
}

/* --- frente al univariante (fase P3) -------------------------------------- */

static void on_uni_done( const EngineResult *r, gpointer data )
{
    Mtram *m = data;
    Prev  *P = &m->prev;

    P->uni_corriendo = FALSE;
    P->uni_job = NULL;
    P->hay_comp = r->status == 0 && P->csv_uni && P->csv_modelo &&
                  fc_compara_csv( P->csv_uni, P->csv_modelo, &P->comp ) == 0;
    if (P->hay_comp)
        preview_show_status( m, "Comparado con el univariante: %d orígenes "
                                "comunes, %d horizontes.",
                             P->comp.origenes, P->comp.nh );
    else
        preview_show_status( m, "La evaluación del univariante no salió: %s",
                             r->message ? r->message : "" );
    prevision_refresca( m );
    estima_refresca( m );
    mtram_barra_refresca( m );
}

/* EL UNIVARIANTE CON LA MISMA VENTANA: una corrida aparte, a sus propios
 * ficheros (_uni_eval.*). No da de alta una corrida en el proyecto ni toca
 * la estimacion ni la diagnosis: es una vara de medir, como el diagonal de
 * sim_gui (gui/sim/src/prevision.c).                                     */
static void lanza_univariante( Mtram *m )
{
    Prev   *P = &m->prev;
    gchar  *out = mtram_artefacto( m, "_uni_eval.out" ), **argv, *d;

    g_free( P->csv_uni );
    P->csv_uni = mtram_artefacto( m, "_uni_eval.csv" );
    g_remove( P->csv_uni );
    argv = estima_argv_univariante( m, out, P->csv_uni );
    d    = estima_trabajo();
    P->uni_job = engine_start( d, "drtran", (const char * const *) argv,
                               NULL, NULL, on_uni_done, m );
    P->uni_corriendo = P->uni_job != NULL;
    if (P->uni_corriendo)
        preview_show_status( m, "Evaluando el univariante con la misma "
                                "ventana (%d), para comparar…", P->ventana );
    else
        preview_show_status( m, "No pude lanzar drtran para el univariante." );
    g_strfreev( argv );
    g_free( out ); g_free( d );
}

/* La llama la estimacion al acabar, con SU .out. */
void prevision_desde( Mtram *m, const char *path )
{
    Prev *P = &m->prev;

    P->vale = FALSE;
    P->hay_comp = FALSE;
    g_free( P->out );
    P->out = g_strdup( path );
    if (path && of_parse_file( path, &P->f ) == 0) P->vale = TRUE;

    /* Con evaluacion, la del univariante con la misma ventana. Si el modelo
     * ES el diagonal, no hay nada que comparar.                        */
    if (P->vale && P->f.tiene_ev && P->evaluar && P->ventana > 0 &&
        P->csv_modelo && !m->est.diagonal && !P->uni_corriendo &&
        g_file_test( P->csv_modelo, G_FILE_TEST_EXISTS ))
        lanza_univariante( m );
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

    if (m->est.corriendo || P->uni_corriendo) {
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

    /* Con un modelo estimado y la misma especificacion, la prevision es la
     * de ESE modelo: estima_lanzar no da de alta otro.                */
    m->est.pide_prever = TRUE;
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

/* EL GRAFICO DE LA PREVISION, EL DE fuf (lib/fufplot): el que el analista ya
 * lee en fue_gui, con las mismas marcas y las mismas bandas. Arriba, la
 * variacion anual --la LRC anual en %, si el modelo va en logaritmos-- de las
 * L ultimas observaciones y de las L previsiones, con la banda de UNA
 * desviacion tipica; con lambda < 0, el NIVEL, con la de dos, deshaciendo la
 * transformacion como fuf (forecast_graphic_BC). Debajo, ERR: el error de
 * prevision a un periodo de las observaciones, en impulsos bajo su fecha, con
 * su banda de dos sigma.
 *
 * Todo sale de la tabla del .out, que lleva cada columna en las unidades de
 * fuf. Y sigma tambien: el error a un periodo de la tabla es el de la forma
 * REDUCIDA --el de prever Y sin conocer todavia la innovacion de X-- y su
 * varianza es exactamente la del error de la prevision a un periodo, la
 * desviacion de la primera fila prevista. En fuf, lo mismo: v1 en h = 1 es
 * sigma2. Sin leer una segunda cosa que pudiera no cuadrar con la tabla.  */

/* «4/1993» o «1993»: el periodo y el anio. */
static int fecha_de( const char *s, int *per, int *anio )
{
    if (sscanf( s, "%d/%d", per, anio ) == 2) return 1;
    if (sscanf( s, "%d", anio ) == 1) { *per = 1; return 1; }
    return 0;
}

gchar *prevision_grafico( Mtram *m, int k )
{
    Prev          *P = &m->prev;
    const OfSerie *S;
    const struct Tusmodel *tm = NULL;
    double        *y, *b1, *b2, *err, lam = 1.0, sigma;
    int            i, L, i0, freq = 1, per, anio, nivel;
    gchar         *dir, *eps = NULL, *nom;
    FDFig         *fig;

    if (!P->vale || k < 0 || k >= P->f.ns) return NULL;
    S = &P->f.s[k];
    L = S->nprev;
    i0 = S->nf - 2 * L;                /* la primera de las L observadas  */
    if (L < 2 || i0 < 0 || !fecha_de( S->f[i0].fecha, &per, &anio )) return NULL;

    /* El modelo de ESA serie: su lambda y su frecuencia. */
    for (i = 0; i < m->c.n; i++)
        if (m->c.s[i]->ts.name && !strcmp( m->c.s[i]->ts.name, S->nombre )) {
            tm   = &m->c.s[i]->tm;
            freq = m->c.s[i]->ts.freq;
        }
    if (tm) lam = tm->boxlam;
    nivel = lam < 0.0;                 /* fuf: forecast_graphic_BC       */
    if (freq < 1) freq = 1;

    y   = g_new0( double, 2 * L );
    b1  = g_new0( double, 2 * L );
    b2  = g_new0( double, 2 * L );
    err = g_new0( double, L );
    for (i = 0; i < 2 * L; i++) {
        const OfFila *f = &S->f[i0 + i];
        /* la variacion anual; con datos anuales, la del periodo, que es ella */
        double        v  = (freq > 1) ? f->var_anu : f->var_per;
        double        sd = (freq > 1) ? f->sd_anu  : f->sd_per;

        if (nivel) {
            /* La banda, como fuf: la transformada mas/menos dos desviaciones,
             * deshecha. La tabla da el nivel y la desviacion EN LA ESCALA
             * TRANSFORMADA (/refactor), y el refactor se va: z = (x^l - 1)/l. */
            double z = (pow( f->nivel, lam ) - 1.0) / lam;

            y[i] = f->nivel;
            if (i >= L) {
                b1[i] = pow( (z + 2.0 * f->sd_nivel) * lam + 1.0, 1.0 / lam );
                b2[i] = pow( (z - 2.0 * f->sd_nivel) * lam + 1.0, 1.0 / lam );
            }
        } else {
            y[i] = v;
            if (i >= L) { b1[i] = v + sd; b2[i] = v - sd; }
        }
        if (i < L) err[i] = f->tiene_err ? f->err : 0.0;
    }
    sigma = S->f[i0 + L].sd_nivel;

    fig = fp_forecast( y, b1, b2, err, L, sigma, freq, anio, per,
                       nivel ? "LEVEL"
                             : (tm && fabs( lam ) < 1e-8) ? "LRC anual (%)"
                                                          : "Annual change" );
    if (fig) {
        dir = g_build_filename( g_get_user_cache_dir(), GUI_CACHE, NULL );
        g_mkdir_with_parents( dir, 0700 );
        nom = g_strdup_printf( "prev_%s.eps", S->nombre );
        eps = g_build_filename( dir, nom, NULL );
        if (fd_write_eps( fig, eps ) != 0) { g_free( eps ); eps = NULL; }
        fd_fig_free( fig );
        g_free( nom );
        g_free( dir );
    }

    g_free( y ); g_free( b1 ); g_free( b2 ); g_free( err );
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
    gtk_widget_set_tooltip_text( mi, "Para comparar con la siguiente: la "
        "columna «vs la guardada» de «7 · Previsión». TASTE no podía: tenía "
        "una sola ranura de residuos y estimar otra vez borraba la anterior." );
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
    GtkWidget    *caja, *barra, *sc, *marco, *vb;
    GtkListStore *st;

    P->b_calc = NULL;
    P->out  = NULL;
    P->csv_modelo = P->csv_uni = NULL;
    P->uni_job = NULL;
    P->uni_corriendo = P->hay_comp = FALSE;
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

    P->c_prever = gtk_check_button_new_with_label( "Prever" );
    gtk_widget_set_tooltip_text( P->c_prever,
        "drtran -f. Prever una salida exige prever sus entradas: el error de la salida "
        "suma su propia innovación y todas las de aguas arriba, cada una "
        "propagada por la ν(B) que atraviesa." );
    g_signal_connect( P->c_prever, "toggled", G_CALLBACK(on_cambio), m );
    gtk_box_pack_start( GTK_BOX(barra), P->c_prever, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_label_new( "períodos" ),
                        FALSE, FALSE, 0 );
    P->s_hor = gtk_spin_button_new_with_range( 1, 60, 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(P->s_hor), 8 );
    g_signal_connect( P->s_hor, "value-changed", G_CALLBACK(on_cambio), m );
    gtk_box_pack_start( GTK_BOX(barra), P->s_hor, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    P->c_eval = gtk_check_button_new_with_label( "Evaluar fuera de muestra" );
    gtk_widget_set_tooltip_text( P->c_eval,
        "drtran -C. Estima UNA vez sobre la ventana, clava los parámetros y rueda el "
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

    /* Guardar la evaluacion y olvidarla estan en Prever ▾, como en la
     * pagina 6 lo que no es leer: aqui, calcular y lo que se calcula.   */

    /* --- la tabla de la evaluacion --- */
    st = gtk_list_store_new( P_N_COLS, G_TYPE_STRING, G_TYPE_INT,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING );
    P->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( P->lista, "Horizonte", P_H );
    columna( P->lista, "n",         P_N );
    columna( P->lista, "MAE",       P_MAE );
    columna( P->lista, "RMSE",      P_RMSE );
    columna( P->lista, "MAPE %",    P_MAPE );
    columna( P->lista, "RMSE univariante", P_UNI );
    columna( P->lista, "Cociente",  P_RATIO );
    columna( P->lista, "Diebold-Mariano", P_DM );
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
    P->tabla = gtk_label_new( "" );
    gtk_widget_set_halign( P->tabla, GTK_ALIGN_START );
    gtk_label_set_selectable( GTK_LABEL(P->tabla), TRUE );
    gtk_widget_set_no_show_all( P->tabla, TRUE );
    gtk_box_pack_start( GTK_BOX(vb), P->tabla, FALSE, FALSE, 0 );

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
    gtk_box_pack_start( GTK_BOX(caja), marco, TRUE, TRUE, 0 );

    prevision_refresca( m );
    return caja;
}
