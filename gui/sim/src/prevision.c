/*
 * prevision.c -- la pagina 7: LA PREVISION, LA EVALUACION, LA RESPUESTA AL
 * IMPULSO Y LA DESCOMPOSICION (nodos N5 y N6 de sima: forecast, evaluate,
 * impulse_response, variance_decomposition).
 *
 * HAY DOS COSAS DISTINTAS Y LA PAGINA NO LAS MEZCLA (como lib/outfcst en
 * drtran_gui):
 *
 *   la PREVISION de cada serie en su nivel, con la banda del 95 %, que es
 *   TEORICA: lo que el modelo implica;
 *
 *   la EVALUACION fuera de muestra (-estwin N): los parametros estimados una
 *   vez sobre las N primeras observaciones y clavados mientras el origen
 *   rueda. Es la unica respuesta empirica, y la vara de medir es el sistema
 *   diagonal --los univariantes-- con la misma ventana: ¿prevé mejor el
 *   sistema que las series solas? (sima evaluate).
 *
 * drvarma preve en la pasada en que estima, asi que prever es volver a
 * correr EL MISMO modelo, a <modelo>_prev.*. Con evaluacion, dos corridas
 * mas con -estwin --que preve desde el final de la ventana, no de la
 * muestra, y por eso no sirve para la prevision--: el sistema a
 * <modelo>_eval.* y el diagonal a <modelo>_diag.*. Ninguna toca la
 * estimacion de la pagina 5.
 *
 * La respuesta al impulso y la descomposicion ya estan en el .out de la
 * estimacion (lib/outvarma): se ensenan sin correr nada.
 *
 * CON UN VEC (S10c), lo mismo con drvec: -f H a <modelo>_prev, y con
 * evaluacion -f H -estwin E-1 (E-1 filas de nabla Y: las mismas
 * observaciones que las E primeras de los univariantes) a <modelo>_eval; los
 * univariantes, el diagonal de drvarma con -estwin E. Los ORIGENES se
 * cuentan hacia atras desde el final --drvec numera filas, drvarma da
 * fechas-- y se comparan solo los comunes, con el valor real de drvec
 * (sima, evaluate_vec). La IRF y la FEVD del VEC, en niveles, son las
 * secciones del .out de la estimacion.
 */

/* Antes que GTK: main.h de drvarma define TRUE y FALSE. */
#include "fue_pre_reader.h"
#include "sim.h"

#include <math.h>
#include <string.h>
#include <glib/gstdio.h>

#include "fugdraw.h"
#include "fugplot.h"
#include "fufplot.h"
#include "outfcst.h"
#include "ofgraf.h"
#include "preview.h"

enum { E_SER, E_H, E_N, E_SIS, E_UNI, E_RAZ, E_N_ };

/* --- lanzar ---------------------------------------------------------------- */

static gboolean pulso( gpointer d )
{
    Sim *m = d;
    if (!m->pv.corriendo || !m->avance) return G_SOURCE_REMOVE;
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(m->avance) );
    return G_SOURCE_CONTINUE;
}

static VarmaOut *lee( const char *base )
{
    gchar *out = g_strdup_printf( "%s.out", base );
    VarmaOut *v = g_new0( VarmaOut, 1 );
    if (ov_parse_file( out, v ) != 0) { g_free( v ); v = NULL; }
    g_free( out );
    return v;
}

static gboolean lanza( Sim *m, int paso );

static const char *sufijo( int paso ) { return paso == 1 ? "" : paso == 2 ? "_eval" : "_diag"; }

/* Lo que deja un paso del VEC: la prevision (1), la evaluacion (2) o el
 * diagonal con su .recursive (3). TRUE si se leyo.                     */
static gboolean lee_vec( Sim *m, const char *base )
{
    PagPrev *P = &m->pv;
    gchar *f;
    gboolean ok = FALSE;

    if (P->paso == 1) {
        if (!P->fvec) P->fvec = g_new0( OvecPrev, OVEC_MAX_SER );
        f = g_strdup_printf( "%s.forecast", base );
        P->nfvec = ovec_forecast_file( f, P->fvec, OVEC_MAX_SER );
        ok = P->nfvec > 0;
    } else if (P->paso == 2) {
        VecOut *o = g_new0( VecOut, 1 );
        gchar *out = g_strdup_printf( "%s.out", base );
        ovec_recursive_free( &P->rvec );
        f = g_strdup_printf( "%s.recursive", base );
        ok = ovec_recursive_file( f, &P->rvec ) == 0;
        if (ovec_parse_file( out, o ) == 0) P->vec_nobs_raw = o->nobs_raw;
        g_free( o ); g_free( out );
    } else {
        g_free( P->vd ); P->vd = lee( base );
        ov_recursive_free( &P->rdiag );
        f = g_strdup_printf( "%s.recursive", base );
        ok = ov_recursive_file( f, &P->rdiag ) == 0;
    }
    g_free( f );
    return ok;
}

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagPrev *P = &m->pv;
    gchar *base = g_strdup_printf( "%s%s", P->base, sufijo( P->paso ) );
    VarmaOut *v = NULL;
    gboolean leido;

    P->trabajo = NULL;
    if (P->es_vec)
        leido = lee_vec( m, base );
    else {
        v = lee( base );
        leido = v != NULL;
        if (P->paso == 1)      { g_free( P->v );  P->v = v; }
        else if (P->paso == 2) { g_free( P->ve ); P->ve = v; }
        else                   { g_free( P->vd ); P->vd = v; }
    }
    g_free( base );

    /* Con evaluacion, el sistema con -estwin y despues el diagonal. */
    if (P->paso < 3 && r->status == 0 && leido && m->ventana && P->c_eval &&
        gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) ) && lanza( m, P->paso + 1 ))
        return;

    P->corriendo = FALSE;
    if (!m->ventana || !P->b_calc) return;
    gtk_widget_hide( m->avance );
    P->version = m->pe.version;
    sim_refresca( m );
    if (r->status != 0)
        sim_estado( m, "%s salió con %d al prever: %s", P->es_vec && P->paso < 3 ? "drvec" : "drvarma",
                    r->status, r->message );
    else
        sim_estado( m, "Previsión lista%s.", P->vd ? ", y evaluada contra los univariantes" : "" );
}

/* paso 1: el modelo de la pagina 5 con -forecast; 2: el mismo con -estwin;
 * 3: el diagonal (0 0 -diagcov) con la misma ventana.                    */
static gboolean lanza( Sim *m, int paso )
{
    PagPrev *P = &m->pv;
    const char *av[SIS_MAX_SER + 32];
    char buf[2048], hor[16], win[16];
    gchar *salida, *dir;
    SisModelo mo = m->mod;
    int k;

    snprintf( hor, sizeof hor, "%d", gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_hor) ) );
    snprintf( win, sizeof win, "%d", gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_win) ) );
    if (paso == 3) { mo.p = mo.q = 0; mo.diagcov = 1; }
    salida = g_strdup_printf( "%s%s", P->base, sufijo( paso ) );
    if (P->es_vec && paso < 3) {
        /* drvec: -f H [-estwin E-1] delante de «-name NOMBRE»; E-1 filas de
           nabla Y son las E primeras observaciones de los univariantes. */
        gchar *nom = g_path_get_basename( salida );
        char w1[16];
        snprintf( w1, sizeof w1, "%d", atoi( win ) - 1 );
        k = sis_orden_vec( &m->sis, &m->mod, nom, buf, sizeof buf, av, G_N_ELEMENTS(av) - 6 );
        if (k < 2) { g_free( nom ); g_free( salida ); return FALSE; }
        k -= 2;
        av[k++] = "-f";
        av[k++] = hor;
        if (paso > 1) { av[k++] = "-estwin"; av[k++] = w1; }
        av[k++] = "-name";
        av[k++] = nom;
        av[k] = NULL;
        dir = g_path_get_dirname( salida );
        P->paso = paso;
        P->trabajo = engine_start( dir, "drvec", av, NULL, NULL, on_done, m );
        g_free( dir ); g_free( salida ); g_free( nom );
        return P->trabajo != NULL;
    }
    k = sis_orden_modelo( &m->sis, &mo, salida, buf, sizeof buf, av, G_N_ELEMENTS(av) - 6 );
    if (k < 2) { g_free( salida ); return FALSE; }
    /* -forecast H [-estwin N], delante de «-o SALIDA» (los dos ultimos). */
    k -= 2;
    av[k++] = "-forecast";
    av[k++] = hor;
    if (paso > 1) { av[k++] = "-estwin"; av[k++] = win; }
    av[k++] = "-o";
    av[k++] = salida;
    av[k] = NULL;

    dir = g_path_get_dirname( salida );
    P->paso = paso;
    P->trabajo = engine_start( dir, "drvarma", av, NULL, NULL, on_done, m );
    g_free( dir );
    g_free( salida );
    return P->trabajo != NULL;
}

gboolean prevision_calcular( Sim *m )
{
    PagPrev *P = &m->pv;
    gchar *mot, *nom;

    if (P->corriendo || m->pe.corriendo || m->pp.corriendo) return FALSE;
    P->es_vec = m->pe.vec && m->pe.vec->hay_vec;
    if (!P->es_vec && ( !m->pe.v || m->pe.v->fallo )) {
        sim_estado( m, "Antes, un sistema estimado («5 · Estimación»)." );
        return FALSE;
    }
    if (!( mot = sim_hermano( P->es_vec ? "drvec" : "drvarma" ) )) {
        sim_estado( m, "No pude lanzar %s: ni al lado de sim_gui ni en el PATH.",
                    P->es_vec ? "drvec" : "drvarma" );
        return FALSE;
    }
    g_free( mot );
    if (m->libro) gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_PREV );
    g_free( P->base );
    nom = g_strdup_printf( "%s_prev", m->pe.corrida[0] ? m->pe.corrida : "modelo" );
    P->base = sim_artefacto( m, nom );
    g_free( nom );
    g_free( P->v ); P->v = NULL;
    g_free( P->ve ); P->ve = NULL;
    g_free( P->vd ); P->vd = NULL;
    P->nfvec = 0;
    ovec_recursive_free( &P->rvec );
    ov_recursive_free( &P->rdiag );
    if (!lanza( m, 1 )) { sim_estado( m, "No pude lanzar el motor." ); return FALSE; }
    P->corriendo = TRUE;
    if (m->avance) { gtk_widget_show( m->avance ); g_timeout_add( 120, pulso, m ); }
    sim_estado( m, "%s prevé%s…", P->es_vec ? "drvec" : "drvarma",
                gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) )
                ? " y evalúa (después, los univariantes con la misma ventana)" : "" );
    sim_refresca( m );
    return TRUE;
}

static void on_calc( GtkButton *b, Sim *m ) { (void) b; prevision_calcular( m ); }

/* --- el grafico ------------------------------------------------------------ */

static gchar *latin1( const char *s )
{
    gchar *r = g_convert_with_fallback( s, -1, "ISO-8859-1", "UTF-8", "?", NULL, NULL, NULL );
    return r ? r : g_strdup( s );
}

/* Dos paneles, como drtran_gui: el nivel y la variacion anual, con el tramo
 * observado delante. La variacion lleva su d.t. del .out. En el nivel, con
 * logaritmos la banda del .out es asimetrica y fugdraw solo la dibuja
 * simetrica: no se dibuja, y la nota manda a la tabla.                 */
gboolean prevision_grafico( Sim *m, int i )
{
    PagPrev *P = &m->pv;
    const OvPrev *x = NULL;
    struct Tusmodel tm;
    struct Tseries ts;
    real **dm = NULL;
    int k, j, nh, nf, f, s, es_log;
    double *y[2], *sd[2], rf;
    const char **fe;
    gchar **fechas, *t[2], *nota, *base, *eps, *u;
    FPPanel pn[2];
    FDFig *fig;
    gboolean ok = FALSE;

    if (i < 0 || i >= m->sis.n) return FALSE;

    /* EL DE fuf (fase P6 de docs/TODO.md): la escalera de drvarma y drvec
     * escriben en su .out la tabla de fuf, y de ella sale el mismo grafico
     * que en fue y en drtran_gui (lib/outfcst, of_grafico). Lo de abajo
     * queda para un .out de drvarma sin esa tabla (uno de antes); un VEC
     * sin ella no tiene grafico.                                         */
    if (P->base && ( P->v || P->nfvec )) {
        gchar *out = g_strconcat( P->base, ".out", NULL );
        static Forecast fo;
        const OfSerie  *S;

        if (of_parse_file( out, &fo ) == 0 &&
            ( S = of_serie( &fo, m->sis.s[i].nombre ) ) != NULL) {
            FDFig *fg = of_grafico( S, m->sis.s[i].lambda, 1, m->sis.s[i].freq );

            if (fg) {
                gchar *b = sim_artefacto( m, m->sis.s[i].nombre ), *dir = g_path_get_dirname( b );
                gchar *nm = g_strdup_printf( "prev_%s.eps", m->sis.s[i].nombre );
                gchar *ep = g_build_filename( dir, nm, NULL );

                ok = fd_write_eps( fg, ep ) == 0 && preview_show( m, ep );
                if (!ok) sim_estado( m, "No pude dibujar %s", ep );
                fd_fig_free( fg );
                g_free( ep ); g_free( nm ); g_free( dir ); g_free( b ); g_free( out );
                return ok;
            }
        }
        g_free( out );
    }

    if (!P->v) return FALSE;
    for (k = 0; k < P->v->nprev; k++)
        if (!strcmp( P->v->prev[k].nombre, m->sis.s[i].nombre )) x = &P->v->prev[k];
    if (!x || x->nf < 1) return FALSE;
    memset( &tm, 0, sizeof tm );
    memset( &ts, 0, sizeof ts );
    if (read_fue_pre( m->sis.s[i].ruta, &tm, &ts, &dm ) != 0) return FALSE;

    f = ts.freq > 0 ? ts.freq : 1;
    es_log = fabs( tm.boxlam ) < 1e-8;
    rf = 1.0;                        /* 100*delta/refactor sobre refactor*y */
    nh = ts.nobs < 3 * f + f ? ts.nobs - f : 3 * f > 12 ? 3 * f : 12;
    if (nh > ts.nobs - f) nh = ts.nobs - f;
    if (nh < 1) nh = 1;
    nf = nh + x->nf;
    fe = g_new0( const char *, nf );
    fechas = g_new0( gchar *, nf + 1 );
    for (j = 0; j < 2; j++) { y[j] = g_new0( double, nf ); sd[j] = g_new0( double, nf ); }
    for (j = 0; j < nh; j++) {
        int o = ts.nobs - nh + j + 1;               /* 1..nobs */
        long a = (long) ts.begyear * f + ts.begtime - 1 + o - 1;
        double yt = ts.data[o], ys = ts.data[o - f];
        fechas[j] = f == 1 ? g_strdup_printf( "%ld", a ) : g_strdup_printf( "%ld/%ld", a % f + 1, a / f );
        y[0][j] = yt;
        y[1][j] = es_log ? ( yt > 0 && ys > 0 ? 100.0 * log( yt / ys ) : 0.0 )
                         : 100.0 * ( yt - ys ) * rf;
    }
    for (j = 0; j < x->nf; j++) {
        s = nh + j;
        fechas[s] = g_strdup( f == 1 && !strncmp( x->fecha[j], "1/", 2 ) ? x->fecha[j] + 2 : x->fecha[j] );
        y[0][s] = x->nivel[j];
        /* Con logaritmos la banda del nivel es asimetrica y fugdraw solo
         * sabe dibujarla simetrica (bajaria de cero): no se dibuja, y la
         * nota manda a la tabla. Sin ellos es simetrica y es esta.      */
        sd[0][s] = es_log ? 0.0 : ( x->hi95[j] - x->lo95[j] ) / ( 2 * 1.96 );
        y[1][s] = x->var_anu[j];
        sd[1][s] = x->sd_anu[j];
    }
    for (j = 0; j < nf; j++) fe[j] = fechas[j];

    u = g_strdup_printf( "%s: el nivel, desde %s", x->nombre,
                         f == 1 && !strncmp( x->origen, "1/", 2 ) ? x->origen + 2 : x->origen );
    t[0] = latin1( u ); g_free( u );
    t[1] = latin1( f > 1 ? "La variación anual (%)" : "La variación (%)" );
    nota = latin1( es_log ? "Bandas de ±2 d.t., TEÓRICAS. En el nivel, la del 95 % es asimétrica "
                         "(logaritmos) y no se dibuja: está en la tabla de la previsión."
                       : "Bandas de ±2 d.t., TEÓRICAS: lo que el modelo implica, no lo que "
                         "pasa fuera de muestra." );
    for (j = 0; j < 2; j++) { pn[j].title = t[j]; pn[j].y = y[j]; pn[j].sd = sd[j]; }
    base = sim_artefacto( m, x->nombre );
    fig = fp_forecast_paneles( pn, 2, nf, x->nf, fe, nota, base );
    if (fig) {
        gchar *dir = g_path_get_dirname( base ), *nom = g_strdup_printf( "prev_%s.eps", x->nombre );
        eps = g_build_filename( dir, nom, NULL );
        fd_fig_free( fig );
        ok = g_file_test( eps, G_FILE_TEST_EXISTS ) && preview_show( m, eps );
        if (!ok) sim_estado( m, "No pude dibujar %s", eps );
        g_free( eps ); g_free( dir ); g_free( nom );
    }
    for (j = 0; j < 2; j++) { g_free( y[j] ); g_free( sd[j] ); g_free( t[j] ); }
    g_strfreev( fechas );
    g_free( fe ); g_free( nota ); g_free( base );
    free_fue_pre( &tm, &ts, dm );
    return ok;
}

static void on_grafico( GtkButton *b, Sim *m )
{
    prevision_grafico( m, GPOINTER_TO_INT(g_object_get_data( G_OBJECT(b), "serie" )) );
}

/* LA IRF DIBUJADA (fase P6), con el estilo del grafico de la prevision de
 * fuf: arriba la respuesta de resp a un choque de una desviacion en la
 * innovacion de choque (Cholesky, en el orden del sistema), en impulsos;
 * abajo su acumulada. Con las bandas de +-2 d.t. del metodo delta que la
 * escalera de drvarma escribe en su .out; un .out de antes no las trae, y
 * el dibujo lo dice.                                                    */
gboolean prevision_grafico_irf( Sim *m, int choque, int resp )
{
    const VarmaOut *v = m->pe.v;
    double *nu, *cu, *nsd = NULL, *csd = NULL;
    gchar  *t1, *t2, *u, *nota, *b, *dir, *nm, *ep;
    FDFig  *fg;
    int     k, n;
    gboolean ok = FALSE;

    if (!v || v->fallo || v->nh_irf < 2 || choque < 0 || resp < 0 ||
        choque >= m->sis.n || resp >= m->sis.n) return FALSE;
    n  = v->nh_irf;
    nu = g_new( double, n );
    cu = g_new( double, n );
    for (k = 0; k < n; k++) {
        nu[k] = v->irf[choque][k][resp];
        cu[k] = nu[k] + ( k ? cu[k - 1] : 0.0 );
    }
    if (v->tiene_irf_se) {
        nsd = g_new( double, n );
        csd = g_new( double, n );
        for (k = 0; k < n; k++) {
            nsd[k] = v->irf_se[choque][k][resp];
            csd[k] = v->irfac_se[choque][k][resp];
        }
    }
    u = g_strdup_printf( "Impulso: %s <- %s", m->sis.s[resp].nombre, m->sis.s[choque].nombre );
    t1 = latin1( u ); g_free( u );
    t2 = latin1( "Acumulada" );
    nota = latin1( nsd ? "Choque de una d.t. (Cholesky, en el orden del sistema).\n"
                         "Bandas de ±2 d.t., método delta: simétricas y TEÓRICAS."
                       : "Choque de una d.t. (Cholesky, en el orden del sistema).\n"
                         "Sin bandas: este .out de drvarma no trae sus errores típicos." );
    fg = fp_response( nu, nsd, cu, csd, n, 0.0, 0, t1, t2, nota );
    if (fg) {
        b   = sim_artefacto( m, m->sis.s[resp].nombre );
        dir = g_path_get_dirname( b );
        nm  = g_strdup_printf( "irf_%s_%s.eps", m->sis.s[resp].nombre, m->sis.s[choque].nombre );
        ep  = g_build_filename( dir, nm, NULL );
        ok  = fd_write_eps( fg, ep ) == 0 && preview_show( m, ep );
        if (!ok) sim_estado( m, "No pude dibujar %s", ep );
        fd_fig_free( fg );
        g_free( ep ); g_free( nm ); g_free( dir ); g_free( b );
    }
    g_free( nu ); g_free( cu ); g_free( nsd ); g_free( csd );
    g_free( t1 ); g_free( t2 ); g_free( nota );
    return ok;
}

static void on_ver_irf( GtkButton *bt, Sim *m )
{
    PagPrev *P = &m->pv;

    if (!prevision_grafico_irf( m, gtk_combo_box_get_active( GTK_COMBO_BOX(P->c_choque) ),
                                gtk_combo_box_get_active( GTK_COMBO_BOX(P->c_resp) ) ))
        sim_estado( m, "No hay respuesta al impulso que dibujar: estima el sistema." );
}

/* Las series en los dos desplegables, y el boton encendido si hay IRF. */
static void irf_controles( Sim *m )
{
    PagPrev  *P = &m->pv;
    gboolean  hay = m->pe.v && !m->pe.v->fallo && m->pe.v->nh_irf >= 2 &&
                    !( m->pe.vec && m->pe.vec->hay_vec );
    int       i, a0, b0;

    if (!P->c_choque) return;
    a0 = gtk_combo_box_get_active( GTK_COMBO_BOX(P->c_choque) );
    b0 = gtk_combo_box_get_active( GTK_COMBO_BOX(P->c_resp) );
    gtk_combo_box_text_remove_all( GTK_COMBO_BOX_TEXT(P->c_choque) );
    gtk_combo_box_text_remove_all( GTK_COMBO_BOX_TEXT(P->c_resp) );
    for (i = 0; i < m->sis.n; i++) {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(P->c_choque), m->sis.s[i].nombre );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(P->c_resp), m->sis.s[i].nombre );
    }
    if (m->sis.n > 0) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(P->c_choque), a0 >= 0 && a0 < m->sis.n ? a0 : 0 );
        gtk_combo_box_set_active( GTK_COMBO_BOX(P->c_resp),
                                  b0 >= 0 && b0 < m->sis.n ? b0 : ( m->sis.n > 1 ? 1 : 0 ) );
    }
    gtk_widget_set_sensitive( P->b_irf, hay );
    gtk_widget_set_tooltip_text( P->b_irf, hay
        ? "La respuesta y su acumulada, dibujadas, con bandas de ±2 d.t. (método delta)."
        : m->pe.vec && m->pe.vec->hay_vec ? "La IRF del VEC, en niveles, está en la tabla de abajo."
                                          : "Estima el sistema primero." );
}

/* --- las tablas ------------------------------------------------------------ */

/* LA TABLA DE fuf de cada serie, la del .out tal cual (fase P6): la misma
 * que en fue y en drtran_gui. La escriben drvarma y drvec. FALSE si el .out
 * no la trae (uno de antes).                                            */
static gboolean tablas_fuf( Sim *m, GString *g )
{
    PagPrev *P = &m->pv;
    gchar *out;
    static char tb[65536];
    gsize len0 = g->len;
    int k;

    if (!P->base) return FALSE;
    out = g_strconcat( P->base, ".out", NULL );
    for (k = 0; k < m->sis.n; k++)
        if (of_tabla_file( out, m->sis.s[k].nombre, tb, sizeof tb ) == 0) {
            if (!g_utf8_validate( tb, -1, NULL )) {
                gchar *u = g_convert_with_fallback( tb, -1, "UTF-8", "ISO-8859-1", "?",
                                                    NULL, NULL, NULL );
                g_string_append( g, u ? u : "" );
                g_free( u );
            } else
                g_string_append( g, tb );
            g_string_append( g, "\n\n" );
        }
    g_free( out );
    return g->len > len0;
}

static void texto_prev_vec( Sim *m )
{
    PagPrev *P = &m->pv;
    GString *g = g_string_new( NULL );
    int k, j;

    /* La tabla de fuf, que drvec escribe desde la fase P6: sus desviaciones
     * de la variacion salen del mismo mapa que la banda del nivel. Debajo,
     * la banda del 95 % del .forecast, que esa tabla no lleva.           */
    if (tablas_fuf( m, g ))
        g_string_append( g, "Las desviaciones son TEÓRICAS: lo que el modelo implica, no lo que "
                            "pasa fuera de muestra.\n\nLa banda del 95 % del nivel:\n\n" );
    for (k = 0; k < P->nfvec; k++) {
        const OvecPrev *x = &P->fvec[k];
        double lam = 1;
        int i;

        /* drvec deshace el Box-Cox de cada serie: con λ = 0 el nivel es la
           mediana, no la media; con λ = 1 no hay nada que deshacer */
        for (i = 0; i < m->sis.n; i++)
            if (!strcmp( m->sis.s[i].nombre, x->nombre )) lam = m->sis.s[i].lambda;
        g_string_append_printf( g, "%s (banda del 95 %%, teórica%s):\n", x->nombre,
                                lam == 0 ? "; en logaritmos, el nivel es la mediana" :
                                lam != 1 ? "; con Box-Cox, el nivel es la mediana" : "" );
        g_string_append_printf( g, "  %-10s %14s %14s %14s %10s\n", "fecha", "nivel", "inferior",
                                "superior", "d.t." );
        for (j = 0; j < x->nf; j++)
            g_string_append_printf( g, "  %-10s %14.4f %14.4f %14.4f %10.5f\n", x->fecha[j],
                                    x->nivel[j], x->lo[j], x->hi[j], x->se[j] );
        g_string_append_c( g, '\n' );
    }
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_prev) ), g->str, -1 );
    g_string_free( g, TRUE );
}

static void texto_prev( Sim *m )
{
    PagPrev *P = &m->pv;
    GString *g = g_string_new( NULL );
    int k, j;

    if (P->es_vec) { g_string_free( g, TRUE ); texto_prev_vec( m ); return; }

    /* LA TABLA DE fuf, la del .out de drvarma tal cual (fase P6): la misma
     * que en fue y en drtran_gui. Debajo, la banda del 95 % del nivel, que
     * esa tabla no lleva y el bloque de siempre del .out si.            */
    if (P->v && tablas_fuf( m, g ))
        g_string_append( g, "Las desviaciones son TEÓRICAS: lo que el modelo implica, no lo que "
                            "pasa fuera de muestra.\n\nLa banda del 95 % del nivel:\n\n" );

    for (k = 0; P->v && k < P->v->nprev; k++) {
        const OvPrev *x = &P->v->prev[k];
        g_string_append_printf( g, "%s, desde %s (banda del 95 %%, teórica):\n", x->nombre, x->origen );
        g_string_append_printf( g, "  %-10s %14s %14s %14s %9s %8s\n", "fecha", "nivel", "inferior",
                                "superior", "var.%", "d.t." );
        for (j = 0; j < x->nf; j++)
            g_string_append_printf( g, "  %-10s %14.4f %14.4f %14.4f %9.3f %8.3f\n", x->fecha[j],
                                    x->nivel[j], x->lo95[j], x->hi95[j], x->var_anu[j], x->sd_anu[j] );
        g_string_append_c( g, '\n' );
    }
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_prev) ), g->str, -1 );
    g_string_free( g, TRUE );
}

/* La evaluacion del VEC: drvec y el diagonal de drvarma sobre los MISMOS
 * origenes, contados hacia atras desde el final (sima, evaluate_vec): el de
 * drvec es la fila de nabla Y, nobs_raw - (origen + 1); el de drvarma, una
 * fecha. El real es el de drvec.                                        */
static void evaluacion_vec( Sim *m )
{
    PagPrev *P = &m->pv;
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(P->l_eval) ));
    const SisSerie *s0 = &m->sis.s[0];
    long fin = (long) s0->fin_anio * s0->freq + s0->fin_per - 1;
    GHashTable *real = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, g_free );
    int i, j, k, H = P->rvec.horizonte, gana = 0, total = 0, origenes;
    GHashTable *og = g_hash_table_new( g_direct_hash, g_direct_equal );

    gtk_list_store_clear( st );
    if (P->rvec.nf == 0 || P->rdiag.nf == 0) {
        sim_veredicto( P->ver_eval, SIM_AMBAR, P->nfvec ? "Sin evaluación: marca «Evaluar fuera de "
                       "muestra» y calcula." : "" );
        g_hash_table_destroy( real ); g_hash_table_destroy( og );
        return;
    }
    /* los reales de drvec, por serie, horizonte y origen hacia atras */
    for (k = 0; k < P->rvec.nf; k++) {
        const OvecRecFila *x = &P->rvec.f[k];
        int atras = P->vec_nobs_raw - ( x->origen + 1 );
        double *a = g_new( double, 1 );
        *a = x->real;
        g_hash_table_insert( real, g_strdup_printf( "%s|%d|%d", x->serie, x->h, atras ), a );
        g_hash_table_add( og, GINT_TO_POINTER(atras + 1) );
    }
    origenes = (int) g_hash_table_size( og );
    for (i = 0; i < m->sis.n; i++)
        for (j = 1; j <= H; j++) {
            double sv = 0, su = 0;
            int n = 0;
            GtkTreeIter it;
            gchar *a, *b, *c, *h;
            for (k = 0; k < P->rdiag.nf; k++) {
                const OvRecFila *d = &P->rdiag.f[k];
                int per, anio, l;
                long o;
                gchar *key;
                const double *re;
                if (d->h != j || strcmp( d->serie, m->sis.s[i].nombre )) continue;
                if (sscanf( d->origen, "%d/%d", &per, &anio ) != 2) continue;
                o = (long) anio * s0->freq + per - 1;
                key = g_strdup_printf( "%s|%d|%ld", d->serie, j, fin - o );
                re = g_hash_table_lookup( real, key );
                g_free( key );
                if (!re) continue;
                /* el error de drvec en ese mismo origen */
                for (l = 0; l < P->rvec.nf; l++) {
                    const OvecRecFila *x = &P->rvec.f[l];
                    if (x->h == j && !strcmp( x->serie, d->serie ) &&
                        P->vec_nobs_raw - ( x->origen + 1 ) == fin - o) {
                        sv += x->error * x->error;
                        su += ( *re - d->nivel ) * ( *re - d->nivel );
                        n++;
                        break;
                    }
                }
            }
            if (n == 0) continue;
            sv = sqrt( sv / n ); su = sqrt( su / n );
            total++; gana += sv < su;
            h = g_strdup_printf( "%d", n );
            a = g_strdup_printf( "%.6g", sv ); b = g_strdup_printf( "%.6g", su );
            c = su > 0 ? g_strdup_printf( "%.3f", sv / su ) : g_strdup( "" );
            gtk_list_store_append( st, &it );
            gtk_list_store_set( st, &it, E_SER, m->sis.s[i].nombre, E_H, j, E_N, n, E_SIS, a, E_UNI, b,
                                E_RAZ, c, -1 );
            g_free( h ); g_free( a ); g_free( b ); g_free( c );
        }
    if (total)
        sim_veredicto( P->ver_eval, gana * 2 > total ? SIM_VERDE : SIM_AMBAR,
                       "Fuera de muestra, el VEC y los univariantes sobre los mismos %d orígenes (los "
                       "de drvec: los que comprueban los %d pasos): el VEC prevé mejor en %d de %d "
                       "series y horizontes (RMSE VEC / univariantes < 1).", origenes, H, gana, total );
    else
        sim_veredicto( P->ver_eval, SIM_AMBAR, "La evaluación del VEC y la de los univariantes no "
                       "comparten orígenes: revisa la ventana." );
    g_hash_table_destroy( real );
    g_hash_table_destroy( og );
}

/* La evaluacion: el RMSE del sistema y el de los univariantes, por serie y
 * horizonte, y cuantas veces gana el sistema.                            */
static void evaluacion( Sim *m )
{
    if (m->pv.es_vec) { evaluacion_vec( m ); return; }
    {
    PagPrev *P = &m->pv;
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(P->l_eval) ));
    int k, j, gana = 0, total = 0;

    gtk_list_store_clear( st );
    if (!P->ve || P->ve->nev == 0) {
        gtk_label_set_text( GTK_LABEL(P->ver_eval), P->v ? "Sin evaluación: marca «Evaluar fuera de "
                            "muestra» y calcula." : "" );
        return;
    }
    for (k = 0; k < P->ve->nev; k++) {
        const OvEvalSerie *e = &P->ve->ev[k], *d = NULL;
        for (j = 0; P->vd && j < P->vd->nev; j++)
            if (!strcmp( P->vd->ev[j].nombre, e->nombre )) d = &P->vd->ev[j];
        for (j = 0; j < e->nh; j++) {
            GtkTreeIter it;
            const OvHoriz *h = &e->h[j], *hd = d && j < d->nh ? &d->h[j] : NULL;
            gchar *a = g_strdup_printf( "%.6g", h->rmse );
            gchar *b = hd ? g_strdup_printf( "%.6g", hd->rmse ) : g_strdup( "" );
            gchar *c = hd && hd->rmse > 0 ? g_strdup_printf( "%.3f", h->rmse / hd->rmse ) : g_strdup( "" );
            if (hd) { total++; gana += h->rmse < hd->rmse; }
            gtk_list_store_append( st, &it );
            gtk_list_store_set( st, &it, E_SER, e->nombre, E_H, h->h, E_N, h->n, E_SIS, a, E_UNI, b,
                                E_RAZ, c, -1 );
            g_free( a ); g_free( b ); g_free( c );
        }
    }
    if (total)
        sim_veredicto( P->ver_eval, gana * 2 > total ? SIM_VERDE : SIM_AMBAR,
                       "Fuera de muestra (%d orígenes; n, los que tienen dato con que comparar), "
                       "el sistema prevé mejor que los univariantes en %d de %d series y "
                       "horizontes (RMSE sistema / univariantes < 1).",
                       P->ve->ev_origenes, gana, total );
    else
        sim_veredicto( P->ver_eval, SIM_AMBAR, "La evaluación del sistema, sin la de los univariantes "
                       "con la que compararla." );
}
}

/* La IRF y la FEVD, del .out de la estimacion: no se corre nada. Las del
 * VEC son las de drvec, EN NIVELES (sus secciones del .out).            */
static void irf_fevd( Sim *m )
{
    if (m->pe.vec && m->pe.vec->hay_vec && m->pe.out) {
        char *a = ovec_seccion_file( m->pe.out, "Impulse Response of the Levels" );
        char *b = ovec_seccion_file( m->pe.out, "Forecast Error Variance Decomposition, in Levels" );
        /* drvec responde en w = factor de escala * BoxCox(z), las unidades
           en que estima, y su .out no lo dice: se dice aqui */
        gchar *ta = g_strdup_printf( "La respuesta al impulso del VEC, EN NIVELES (de drvec, tal cual).\n"
                                     "Va en las unidades en que estima drvec: cada serie, transformada con\n"
                                     "su Box-Cox y multiplicada por el factor de escala de su .pre. Con un\n"
                                     "factor 100, un 1 aquí es 0.01 en la serie.\n%s",
                                     a ? a : "(el .out no la trae)\n" );
        gchar *tb = g_strdup_printf( "La descomposición de la varianza del VEC, EN NIVELES (de drvec, "
                                     "tal cual):\n%s", b ? b : "(el .out no la trae)\n" );
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->pv.t_irf) ), ta, -1 );
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->pv.t_fevd) ), tb, -1 );
        free( a ); free( b ); g_free( ta ); g_free( tb );
        return;
    }
    {
    PagPrev *P = &m->pv;
    const VarmaOut *v = m->pe.v;
    GString *g = g_string_new( NULL ), *h = g_string_new( NULL );
    int a, b, k, n = m->sis.n;

    if (v && !v->fallo && v->nh_irf > 0) {
        g_string_append( g, "Respuesta al impulso ortogonalizada (Cholesky, en el orden del sistema):\n"
                            "un choque de una desviación típica en la innovación de cada serie.\n\n" );
        for (a = 0; a < n; a++) {
            g_string_append_printf( g, "Choque en %s:\n  %3s", m->sis.s[a].nombre, "h" );
            for (b = 0; b < n; b++) g_string_append_printf( g, " %12s", m->sis.s[b].nombre );
            g_string_append_c( g, '\n' );
            for (k = 0; k < v->nh_irf; k++) {
                g_string_append_printf( g, "  %3d", k );
                for (b = 0; b < n; b++) g_string_append_printf( g, " %12.4f", v->irf[a][k][b] );
                g_string_append_c( g, '\n' );
            }
            g_string_append_c( g, '\n' );
        }
    }
    if (v && !v->fallo && v->nh_fevd > 0) {
        g_string_append( h, "Descomposición de la varianza del error de previsión (%, Cholesky):\n"
                            "de qué innovación viene el error de prever cada serie.\n\n" );
        for (a = 0; a < n; a++) {
            g_string_append_printf( h, "%s:\n  %3s", m->sis.s[a].nombre, "h" );
            for (b = 0; b < n; b++) g_string_append_printf( h, " %10s", m->sis.s[b].nombre );
            g_string_append_c( h, '\n' );
            for (k = 0; k < v->nh_fevd; k++) {
                g_string_append_printf( h, "  %3d", k + 1 );
                for (b = 0; b < n; b++) g_string_append_printf( h, " %10.2f", v->fevd[a][k][b] );
                g_string_append_c( h, '\n' );
            }
            g_string_append_c( h, '\n' );
        }
    }
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_irf) ), g->str, -1 );
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_fevd) ), h->str, -1 );
    g_string_free( g, TRUE );
    g_string_free( h, TRUE );
}
}

void prevision_refresca( Sim *m )
{
    PagPrev *P = &m->pv;
    GList *l, *q;
    int i;

    if (!P->b_calc) return;
    {
        gboolean vec = m->pe.vec && m->pe.vec->hay_vec;
        gtk_widget_set_sensitive( P->b_calc, !P->corriendo && !m->pe.corriendo &&
                                  ( vec || ( m->pe.v && !m->pe.v->fallo ) ) );
        if (!P->corriendo) P->es_vec = vec;
    }
    gtk_widget_set_sensitive( P->s_win, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) ) );

    /* Una prevision de otra estimacion ya no es la de esta. */
    if (( P->v || P->nfvec ) && P->version != m->pe.version && !P->corriendo) {
        g_free( P->v ); P->v = NULL;
        g_free( P->ve ); P->ve = NULL;
        g_free( P->vd ); P->vd = NULL;
        P->nfvec = 0;
        ovec_recursive_free( &P->rvec );
        ov_recursive_free( &P->rdiag );
    }
    if (( ( m->pe.v && m->pe.v->nobs > 0 ) || ( m->pe.vec && m->pe.vec->nobs_raw > 0 ) ) && !P->corriendo) {
        int nobs = m->pe.vec ? m->pe.vec->nobs_raw : m->pe.v->nobs;
        int w = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_win) );
        gtk_spin_button_set_range( GTK_SPIN_BUTTON(P->s_win), 10, nobs > 11 ? nobs - 1 : 11 );
        if (w <= 10 || w >= nobs) gtk_spin_button_set_value( GTK_SPIN_BUTTON(P->s_win), (int)( 0.75 * nobs ) );
    }

    if (P->corriendo)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "%s está previendo…", P->es_vec ? "drvec" : "drvarma" );
    else if (P->es_vec && !P->nfvec)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "La respuesta al impulso y la descomposición del VEC, en "
                       "niveles, ya están (abajo). La previsión: «Calcular»." );
    else if (P->es_vec)
        sim_veredicto( P->ver_pide, SIM_VERDE, "Previsión del VEC%s%s, %d periodos, en el nivel de cada "
                       "serie. Las bandas son TEÓRICAS; lo empírico es la evaluación.",
                       m->pe.corrida[0] ? " " : "", m->pe.corrida, P->fvec[0].nf );
    else if (!m->pe.v || m->pe.v->fallo)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "Pide un sistema estimado («5 · Estimación»)." );
    else if (!P->v)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "La respuesta al impulso y la descomposición ya están "
                       "(abajo). La previsión: «Calcular»." );
    else
        sim_veredicto( P->ver_pide, SIM_VERDE, "Previsión %s%s, %d periodos. Las bandas son TEÓRICAS; "
                       "lo empírico es la evaluación.", m->pe.corrida[0] ? "del modelo " : "del sistema",
                       m->pe.corrida, P->v->horizonte );

    texto_prev( m );
    evaluacion( m );
    irf_fevd( m );
    irf_controles( m );

    l = gtk_container_get_children( GTK_CONTAINER(P->graficos) );
    for (q = l; q; q = q->next) gtk_widget_destroy( q->data );
    g_list_free( l );
    /* Con un VEC, el grafico sale de la tabla de fuf del .out de drvec
       (fase P6): los botones, en cuanto hay prevision.                 */
    for (i = 0; ( P->v || ( P->es_vec && P->nfvec ) ) && i < m->sis.n; i++) {
        gchar *t = g_strdup_printf( "Gráfico de %s", m->sis.s[i].nombre );
        GtkWidget *b = gtk_button_new_with_label( t );
        g_object_set_data( G_OBJECT(b), "serie", GINT_TO_POINTER(i) );
        g_signal_connect( b, "clicked", G_CALLBACK(on_grafico), m );
        gtk_box_pack_start( GTK_BOX(P->graficos), b, FALSE, FALSE, 0 );
        g_free( t );
    }
    gtk_widget_show_all( P->graficos );
}

/* --- la pagina ------------------------------------------------------------- */

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
}

static GtkWidget *texto_new( GtkWidget **tv )
{
    GtkWidget *sw = gtk_scrolled_window_new( NULL, NULL );
    *tv = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(*tv), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(*tv), TRUE );
    gtk_container_add( GTK_CONTAINER(sw), *tv );
    return sw;
}

static void columna( GtkWidget *tv, const char *t, int c, const char *globo )
{
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes( t, r, "text", c, NULL );
    GtkWidget *l = gtk_label_new( t );
    gtk_widget_set_tooltip_text( l, globo );
    gtk_widget_show( l );
    gtk_tree_view_column_set_widget( col, l );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), col );
}

static void on_eval( GtkToggleButton *t, Sim *m ) { (void) t; prevision_refresca( m ); }

GtkWidget *prevision_pagina_new( Sim *m )
{
    PagPrev *P = &m->pv;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *pg, *sw;
    GtkListStore *st;

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    P->ver_pide = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), P->ver_pide, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(fila), gtk_label_new( "Horizonte" ), FALSE, FALSE, 0 );
    P->s_hor = gtk_spin_button_new_with_range( 1, 120, 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(P->s_hor), 10 );
    gtk_box_pack_start( GTK_BOX(fila), P->s_hor, FALSE, FALSE, 0 );
    P->c_eval = gtk_check_button_new_with_label( "Evaluar fuera de muestra, estimando con las primeras" );
    gtk_widget_set_tooltip_text( P->c_eval, "-estwin N: los parámetros se estiman una vez con las N "
                                 "primeras observaciones y se clavan mientras el origen avanza. Después, "
                                 "los univariantes con la misma ventana, para comparar" );
    gtk_box_pack_start( GTK_BOX(fila), P->c_eval, FALSE, FALSE, 0 );
    P->s_win = gtk_spin_button_new_with_range( 10, 2000, 1 );
    gtk_box_pack_start( GTK_BOX(fila), P->s_win, FALSE, FALSE, 0 );
    g_signal_connect( P->c_eval, "toggled", G_CALLBACK(on_eval), m );
    P->b_calc = gtk_button_new_with_label( "Calcular" );
    g_signal_connect( P->b_calc, "clicked", G_CALLBACK(on_calc), m );
    gtk_box_pack_start( GTK_BOX(fila), P->b_calc, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    P->libreta = gtk_notebook_new();
    gtk_box_pack_start( GTK_BOX(caja), P->libreta, TRUE, TRUE, 0 );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 6 );
    P->graficos = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(pg), P->graficos, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), texto_new( &P->t_prev ), TRUE, TRUE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), pg, gtk_label_new( "Previsión" ) );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 6 );
    P->ver_eval = veredicto_new();
    gtk_box_pack_start( GTK_BOX(pg), P->ver_eval, FALSE, FALSE, 0 );
    st = gtk_list_store_new( E_N_, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    P->l_eval = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( P->l_eval, "Serie", E_SER, "" );
    columna( P->l_eval, "h", E_H, "El horizonte" );
    columna( P->l_eval, "n", E_N, "Cuántas previsiones a ese horizonte" );
    columna( P->l_eval, "RMSE sistema", E_SIS, "El del modelo de «5 · Estimación»" );
    columna( P->l_eval, "RMSE univariantes", E_UNI, "El del sistema diagonal, con la misma ventana" );
    columna( P->l_eval, "Cociente", E_RAZ, "Menor que 1: el sistema prevé mejor" );
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER(sw), P->l_eval );
    gtk_box_pack_start( GTK_BOX(pg), sw, TRUE, TRUE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), pg, gtk_label_new( "Evaluación" ) );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 6 );
    {
    GtkWidget *f = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );

    gtk_box_pack_start( GTK_BOX(f), gtk_label_new( "Choque en" ), FALSE, FALSE, 0 );
    P->c_choque = gtk_combo_box_text_new();
    gtk_box_pack_start( GTK_BOX(f), P->c_choque, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(f), gtk_label_new( "respuesta de" ), FALSE, FALSE, 0 );
    P->c_resp = gtk_combo_box_text_new();
    gtk_box_pack_start( GTK_BOX(f), P->c_resp, FALSE, FALSE, 0 );
    P->b_irf = gtk_button_new_with_label( "Ver el gráfico" );
    g_signal_connect( P->b_irf, "clicked", G_CALLBACK(on_ver_irf), m );
    gtk_widget_set_sensitive( P->b_irf, FALSE );
    gtk_box_pack_start( GTK_BOX(f), P->b_irf, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), f, FALSE, FALSE, 0 );
    }
    gtk_box_pack_start( GTK_BOX(pg), texto_new( &P->t_irf ), TRUE, TRUE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), pg, gtk_label_new( "Respuesta al impulso" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), texto_new( &P->t_fevd ),
                              gtk_label_new( "Descomposición" ) );
    return caja;
}
