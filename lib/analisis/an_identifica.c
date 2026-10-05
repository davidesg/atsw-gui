/*
 * an_identifica.c -- the identifier's window: art proposes, the analyst
 * chooses (docs/ESTUDIO-identificador.md).
 *
 * THE WINDOW DOES NOT DECIDE. It runs the art engine (engines/art), shows the
 * empirical ACF/PACF of the series it identified on, with the band, and the
 * ranked candidates. Selecting one draws ITS theoretical correlogram over the
 * empirical one: that is the school's way of identifying, made visible. The
 * first candidate is marked "proposed", not selected-for-you.
 *
 * DERIVE, NEVER OVERWRITE, NEVER ESTIMATE (AUDITORIA-art.md §5, and the rule
 * of an_sugerir): the chosen candidate goes into a CHILD .inp -- with the
 * transformation it was identified on at E2, on top of the base model's .pre
 * at E3 -- through pr_deriva, judged by inp_check_fue, and opened. fue
 * estimates it when the analyst says so.
 *
 * Four points of the process (the study's E1-E4):
 *   an_identifica_datos     E1, the data: the series with its transformation
 *                           editable in the window, and the tests (seasonal F,
 *                           ADF/KPSS) shown with what they say.
 *   an_identifica_serie     E2, the identification graphs: a series with the
 *                           lambda, d, D being viewed (the mother's vistazo).
 *   an_identifica_residuos  E3/E4, a model's residuals, read from its .out
 *                           (harmonics already modelled: not removed again).
 *                           What art proposes there is what is MISSING, and
 *                           deriving ADDS it as one more factor (id_anade_arma).
 *
 * The engine is a subprocess (lib/engine), found next to the program
 * (lib/sitio). Its result is the line file DATA_art.cand (lib/artcand).
 */

#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <glib/gstdio.h>

#include "analisis.h"
#include "artcand.h"
#include "engine.h"
#include "inpcheck.h"
#include "inpdet.h"
#include "outfile.h"
#include "sitio.h"

enum { C_RANK, C_ORDEN, C_TIPO, C_SIM, C_PESO, C_AICC, C_NOTA, C_IDX, C_N };
enum { F_TODOS, F_REGULAR, F_ESTACIONAL };

typedef struct {
    AnHost     h;
    char       serie[PR_ID], muestra[PR_ID], id[PR_ID];
    int        punto;               /* 2 or 3 (E2, E3)                      */
    double     lam;
    int        d, D, freq;
    ArtCand   *c;
    int        sel;                 /* index into c->cand, -1 none          */
    int        solo_editor;

    GtkWidget *win, *area, *vista, *filtro, *l_estado, *con_que, *l_cab;
    GtkWidget *c_lam, *s_d, *s_D;   /* E1: the transformation, editable     */
    GtkListStore *store;
    char       que[128];
    double    *x;                   /* E1: the series, to identify again    */
    int        n, per, anio;
} Id;

/* ---- the engine ------------------------------------------------------ */

static gchar *programa_art( void )
{
    static const char *const sitio[] = {     /* relative to the program   */
        "../../engines/art/bin/%s",          /* gui/atsw/atsw_gui         */
        "../../../engines/art/bin/%s",       /* gui/fue/bin/fue_gui       */
        "../art/bin/%s", "./%s", NULL
    };
    return sitio_busca( "art", sitio );
}

/* The series, as a lib/datos file the engine reads: "# freq", "# start". */
static int escribe_datos( const char *ruta, const double *x, int n,
                          int freq, int per, int anio )
{
    FILE *f = g_fopen( ruta, "w" );
    int   i;

    if ( !f ) return 1;
    fprintf( f, "# freq %d\n", freq > 0 ? freq : 1 );
    if ( anio > 0 ) fprintf( f, "# start %d/%d\n", per > 0 ? per : 1, anio );
    for ( i = 0; i < n; i++ ) fprintf( f, "%.10g\n", x[i] );
    fclose( f );
    return 0;
}

/* Runs art on `datos`; on success fills g->c and returns TRUE. */
static gboolean corre_art( Id *g, const char *datos, gboolean residuos,
                           char *why, size_t n )
{
    gchar       *exe = programa_art();
    gchar       *dir = g_path_get_dirname( datos );
    char         s[16], d[16], D[16], cand[PR_RUTA];
    EngineResult r;
    int          line = 0, rc;

    if ( !exe )
        { g_snprintf( why, n, "No encuentro el motor art (engines/art/bin/art)." );
          g_free( dir ); return FALSE; }

    g_snprintf( s, sizeof s, "%d", g->freq > 0 ? g->freq : 1 );
    g_snprintf( d, sizeof d, "%d", g->d );
    g_snprintf( D, sizeof D, "%d", g->D );
    if ( residuos )
        r = engine_run( dir, exe, datos, "-s", s, "--harmonics", "off",
                        "--no-tests", NULL );
    else if ( g->lam == 0.0 )
        r = engine_run( dir, exe, datos, "-s", s, "-l", "-d", d, "-D", D, NULL );
    else
        r = engine_run( dir, exe, datos, "-s", s, "-d", d, "-D", D, NULL );
    g_free( exe );
    g_free( dir );

    if ( r.status != 0 )
        {
        g_snprintf( why, n, "art no identificó: %s", r.message ? r.message : "?" );
        engine_result_clear( &r );
        return FALSE;
        }
    engine_result_clear( &r );

    {
    gchar *base = g_strndup( datos, strlen( datos ) -
                             ( g_str_has_suffix( datos, ".txt" ) ? 4 : 0 ) );
    g_snprintf( cand, sizeof cand, "%s_art.cand", base );
    g_free( base );
    }
    rc = ac_leer( cand, g->c, &line );
    if ( rc != AC_OK )
        {
        g_snprintf( why, n, rc == AC_ENOFILE ? "art no dejó %s." :
                            rc == AC_ETRUNC ? "%s está cortado." :
                            "%s no se entiende (línea %d).", cand, line );
        return FALSE;
        }
    if ( g->c->ncand == 0 )
        { g_snprintf( why, n, "art no propuso ningún candidato." ); return FALSE; }
    return TRUE;
}

/* ---- saying things --------------------------------------------------- */

static void di( Id *g, const char *fmt, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, fmt );
    s = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    if ( g->l_estado )
        {
        gchar *m = g_markup_printf_escaped( "<small>%s</small>", s );
        gtk_label_set_markup( GTK_LABEL(g->l_estado), m );
        g_free( m );
        }
    if ( g->h.di ) g->h.di( g->h.dueno, s );
    g_message( "identifica: %s", s );
    g_free( s );
}

/* ---- the correlograms ------------------------------------------------ */

/* One panel: the empirical bars, the band, and -- if a candidate is
 * selected -- its theoretical correlogram on top. */
static void panel( cairo_t *cr, double x0, double y0, double w, double h,
                   const char *titulo, const double *emp, const double *teo,
                   int L, double band, int s )
{
    double m = band, ymax;
    int    k;

    for ( k = 0; k < L; k++ )
        {
        if ( fabs( emp[k] ) > m ) m = fabs( emp[k] );
        if ( teo && fabs( teo[k] ) > m ) m = fabs( teo[k] );
        }
    ymax = ceil( m * 1.15 * 10.0 ) / 10.0;
    if ( ymax > 1.0 ) ymax = 1.0;
    if ( ymax < 0.2 ) ymax = 0.2;

#define X( k )  ( x0 + 30.0 + ( (k) - 0.5 ) * ( w - 40.0 ) / (double) L )
#define Y( v )  ( y0 + h / 2.0 - (v) / ymax * ( h / 2.0 - 12.0 ) )

    /* axis, seasonal lags, band */
    cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );
    cairo_set_line_width( cr, 1.0 );
    cairo_move_to( cr, x0 + 30.0, Y( 0.0 ) );
    cairo_line_to( cr, x0 + w - 10.0, Y( 0.0 ) );
    cairo_stroke( cr );
    cairo_move_to( cr, x0 + 4.0, y0 + 14.0 );
    cairo_show_text( cr, titulo );
    if ( s > 1 )
        {
        cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 0.15 );
        for ( k = s; k <= L; k += s )
            {
            cairo_move_to( cr, X( k ), y0 + 6.0 );
            cairo_line_to( cr, X( k ), y0 + h - 6.0 );
            cairo_stroke( cr );
            }
        }
    {
    double dash[] = { 4.0, 3.0 };
    cairo_set_source_rgb( cr, 0.35, 0.35, 0.35 );
    cairo_set_dash( cr, dash, 2, 0.0 );
    cairo_move_to( cr, x0 + 30.0, Y( band ) );  cairo_line_to( cr, x0 + w - 10.0, Y( band ) );
    cairo_move_to( cr, x0 + 30.0, Y( -band ) ); cairo_line_to( cr, x0 + w - 10.0, Y( -band ) );
    cairo_stroke( cr );
    cairo_set_dash( cr, NULL, 0, 0.0 );
    }

    /* the empirical bars */
    cairo_set_source_rgb( cr, 0.30, 0.45, 0.70 );
    cairo_set_line_width( cr, MAX( 2.0, ( w - 40.0 ) / L * 0.45 ) );
    for ( k = 1; k <= L; k++ )
        {
        cairo_move_to( cr, X( k ), Y( 0.0 ) );
        cairo_line_to( cr, X( k ), Y( emp[k - 1] ) );
        }
    cairo_stroke( cr );

    /* the candidate's theoretical correlogram, on top */
    if ( teo )
        {
        cairo_set_source_rgb( cr, 0.80, 0.15, 0.10 );
        cairo_set_line_width( cr, 1.2 );
        for ( k = 1; k <= L; k++ )
            ( k == 1 ? cairo_move_to : cairo_line_to )( cr, X( k ), Y( teo[k - 1] ) );
        cairo_stroke( cr );
        for ( k = 1; k <= L; k++ )
            {
            cairo_arc( cr, X( k ), Y( teo[k - 1] ), 2.2, 0.0, 2.0 * G_PI );
            cairo_fill( cr );
            }
        }

    /* lag labels */
    cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );
    for ( k = ( s > 1 ? s : 5 ); k <= L; k += ( s > 1 ? s : 5 ) )
        {
        char b[8];
        g_snprintf( b, sizeof b, "%d", k );
        cairo_move_to( cr, X( k ) - 4.0, y0 + h - 1.0 );
        cairo_show_text( cr, b );
        }
#undef X
#undef Y
}

static gboolean pinta( GtkWidget *w, cairo_t *cr, Id *g )
{
    int    W = gtk_widget_get_allocated_width( w );
    int    H = gtk_widget_get_allocated_height( w );
    const  AcCand *k = ( g->sel >= 0 && g->sel < g->c->ncand &&
                         g->c->cand[g->sel].scored ) ? &g->c->cand[g->sel] : NULL;

    cairo_set_source_rgb( cr, 1.0, 1.0, 1.0 );
    cairo_paint( cr );
    cairo_set_font_size( cr, 11.0 );
    panel( cr, 0.0, 0.0, W, H / 2.0, "ACF", g->c->acf, k ? k->tacf : NULL,
           g->c->lags, g->c->band, g->c->s );
    panel( cr, 0.0, H / 2.0, W, H / 2.0, "PACF", g->c->pacf, k ? k->tpacf : NULL,
           g->c->lags, g->c->band, g->c->s );
    return FALSE;
}

/* ---- the candidates -------------------------------------------------- */

static int pasa_filtro( Id *g, const AcCand *k )
{
    int f = gtk_combo_box_get_active( GTK_COMBO_BOX(g->filtro) );
    const AcCand *r = ( g->sel >= 0 ) ? &g->c->cand[g->sel] : &g->c->cand[0];

    if ( f == F_REGULAR )    return k->p == r->p && k->q == r->q;
    if ( f == F_ESTACIONAL ) return k->P == r->P && k->Q == r->Q;
    return 1;
}

static void llena( Id *g )
{
    GtkTreeIter it;
    int         i;

    gtk_list_store_clear( g->store );
    for ( i = 0; i < g->c->ncand; i++ )
        {
        const AcCand *k = &g->c->cand[i];
        char orden[32], tipo[48], sim[16], peso[16], aicc[24];

        if ( !pasa_filtro( g, k ) ) continue;
        g_snprintf( orden, sizeof orden, "(%d,%d)(%d,%d)", k->p, k->q, k->P, k->Q );
        ac_kind( k, tipo, sizeof tipo );
        if ( k->scored )
            {
            g_snprintf( sim, sizeof sim, "%.3f", k->sim );
            g_snprintf( peso, sizeof peso, "%.3f", k->weight );
            g_snprintf( aicc, sizeof aicc, "%.1f", k->aicc );
            }
        else
            { g_snprintf( sim, sizeof sim, "—" ); g_snprintf( peso, sizeof peso, "—" );
              g_snprintf( aicc, sizeof aicc, "no puntúa" ); }
        gtk_list_store_append( g->store, &it );
        gtk_list_store_set( g->store, &it, C_RANK, i + 1, C_ORDEN, orden, C_TIPO, tipo,
                            C_SIM, sim, C_PESO, peso, C_AICC, aicc,
                            C_NOTA, k->proposed ? "propuesto" : "", C_IDX, i, -1 );
        }
}

static void on_sel( GtkTreeSelection *s, Id *g )
{
    GtkTreeModel *m;
    GtkTreeIter   it;

    if ( gtk_tree_selection_get_selected( s, &m, &it ) )
        gtk_tree_model_get( m, &it, C_IDX, &g->sel, -1 );
    gtk_widget_queue_draw( g->area );
}

static void on_filtro( GtkComboBox *c, Id *g )
{
    (void) c;
    llena( g );
}

/* ---- deriving -------------------------------------------------------- */

static AnHerramienta herramienta_de( Id *g )
{
    if ( g->solo_editor ) return AN_CON_EDITOR;
    return gtk_combo_box_get_active( GTK_COMBO_BOX(g->con_que) ) == 1
           ? AN_CON_EDITOR : AN_CON_FUE;
}

/* The parent's file: its .pre if there is one (the optimum), else its .inp. */
static int origen_de( Id *g, char *out, size_t n )
{
    char pre[PR_RUTA];

    if ( pr_ruta( g->h.p, g->serie, g->muestra, g->id, ".pre", pre, sizeof pre ) == 0 &&
         g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { g_snprintf( out, n, "%s", pre ); return 1; }
    if ( pr_ruta( g->h.p, g->serie, g->muestra, g->id, ".inp", out, n ) != 0 ) return 0;
    return g_file_test( out, G_FILE_TEST_EXISTS ) ? 2 : 0;
}

static void on_derivar( GtkButton *b, Id *g )
{
    char     origen[PR_RUTA], destino[PR_RUTA], porque[512], msg[512], nuevo[PR_ID];
    char     tipo[48];
    PrError  e;
    gchar   *dir, *razon, *tmp = NULL;
    const AcCand *k;
    int      cual, rc;

    (void) b;
    if ( g->sel < 0 ) { di( g, "Elige antes un candidato de la lista." ); return; }
    k = &g->c->cand[g->sel];

    cual = origen_de( g, origen, sizeof origen );
    if ( !cual ) { di( g, "No encuentro el fichero de «%s».", g->id ); return; }

    if ( pr_deriva( g->h.p, g->serie, g->muestra, g->id, nuevo, sizeof nuevo,
                    destino, sizeof destino, &e ) != 0 )
        { pr_error_es( &e, msg, sizeof msg ); di( g, "%s", msg ); return; }
    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    /* E2: the child carries the transformation it was identified on. */
    if ( g->punto <= 2 )
        {
        tmp = g_strdup_printf( "%s.trans", destino );
        rc = id_pon_transformacion( origen, tmp, g->lam, g->d, g->D, porque, sizeof porque );
        if ( rc == 0 )
            rc = id_pon_arma( tmp, destino, k->p, k->phi, k->q, k->theta,
                              k->P, k->Phi, k->Q, k->Theta, porque, sizeof porque );
        g_unlink( tmp );
        g_free( tmp );
        }
    else       /* E3/E4: what is missing is ADDED to what the model has */
        rc = id_anade_arma( origen, destino, k->p, k->phi, k->q, k->theta,
                            k->P, k->Phi, k->Q, k->Theta, porque, sizeof porque );
    if ( rc != 0 )
        {
        pr_borra( g->h.p, g->serie, g->muestra, nuevo, &e );
        di( g, "%s", porque );
        return;
        }

    /* THE JUDGE IS THE ENGINE: a .inp fue rejects is undone. */
    if ( inp_check_fue( destino, msg, sizeof msg ) != 0 )
        {
        g_unlink( destino );
        pr_borra( g->h.p, g->serie, g->muestra, nuevo, &e );
        di( g, "Lo que salió no lo acepta fue: %s", msg );
        return;
        }

    ac_kind( k, tipo, sizeof tipo );
    razon = g_strdup_printf(
        "Identificado con art en %s: (%d,%d)(%d,%d) %s, similitud %.3f, peso %.3f; %s. "
        "%s%s.",
        g->punto == 1 ? "los datos" :
        g->punto == 2 ? "los gráficos de identificación" : "los residuos del modelo",
        k->p, k->q, k->P, k->Q, tipo, k->sim, k->weight,
        k->proposed ? "el propuesto" : "elegido frente al propuesto",
        g->punto <= 2 ? "Con la transformación identificada. Derivado de "
                      : "Añadido como un factor más a lo que ya tenía. Derivado de ",
        g->id );
    pr_razon( g->h.p, g->serie, g->muestra, nuevo, razon, &e );
    g_free( razon );
    pr_pon_herramienta( g->h.p, herramienta_de( g ) == AN_CON_EDITOR );
    if ( g->h.guarda && g->h.guarda( g->h.dueno ) != 0 )
        di( g, "El .inp está en %s, pero no pude guardar el proyecto.", nuevo );
    if ( g->h.refresca ) g->h.refresca( g->h.dueno );

    {
    AnHost host = g->h;
    char   serie[PR_ID], muestra[PR_ID], padre[PR_ID];
    AnHerramienta con = herramienta_de( g );
    gchar *aviso;

    g_snprintf( serie, sizeof serie, "%s", g->serie );
    g_snprintf( muestra, sizeof muestra, "%s", g->muestra );
    g_snprintf( padre, sizeof padre, "%s", g->id );
    aviso = g_strdup_printf( "%s nace de %s con (%d,%d)(%d,%d). Estímalo para ver "
                             "si la identificación se sostiene.", nuevo, padre,
                             k->p, k->q, k->P, k->Q );
    gtk_widget_destroy( g->win );
    if ( host.cierra && cual ) host.cierra( host.dueno, serie, muestra, padre );
    if ( host.abre ) host.abre( host.dueno, serie, muestra, nuevo, con );
    an_di( &host, "%s", aviso );
    g_free( aviso );
    }
}

/* ---- the window ------------------------------------------------------ */

static void on_cerrar( GtkWidget *w, Id *g )
{
    (void) w;
    g_free( g->c );
    g_free( g->x );
    g_free( g );
}

static void columna( GtkWidget *vista, const char *titulo, int col, float x )
{
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c;

    g_object_set( r, "xalign", x, NULL );
    c = gtk_tree_view_column_new_with_attributes( titulo, r, "text", col, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(vista), c );
}

/* What the tests say, said: the data's state at this transformation, not
 * what to do (the analyst decides). */
static void cabecera( Id *g )
{
    GString *cab = g_string_new( "" );
    const ArtCand *c = g->c;

    g_string_append_printf( cab, "<b>%s</b>  —  ", g->que );
    if ( g->punto <= 2 )
        g_string_append_printf( cab, "%s, d = %d, D = %d, s = %d",
                                g->lam == 0.0 ? "logaritmos" : "niveles", g->d, g->D, g->freq );
    else
        g_string_append_printf( cab, "residuos del modelo, sin volver a quitar armónicos. "
                                "Lo que se propone es lo que FALTA: derivar lo AÑADE como "
                                "un factor más a lo que el modelo ya tiene" );
    g_string_append_printf( cab, ".  %d observaciones, %d retardos, banda ±%.3f.",
                            c->n_used, c->lags, c->band );
    if ( c->has_seasonal )
        {
        g_string_append_printf( cab, "\n<b>Estacionalidad</b> (F HAC): F = %.2f, p = %.4f. ",
                                c->seasonal_F, c->seasonal_p );
        g_string_append( cab, c->seasonal_detected
            ? "Hay un patrón estacional determinista: con D = 0 se retiran los armónicos "
              "antes de identificar (la ruta determinista); con D = 1 la estacionalidad "
              "se trata como estocástica."
            : "Sin patrón estacional determinista." );
        }
    if ( c->has_unit_root )
        {
        g_string_append_printf( cab, "\n<b>Raíz unitaria</b> con d = %d: ADF %.3f (p = %.4f), "
                                "KPSS %.3f (p = %.4f). ", g->d, c->adf_stat, c->adf_p,
                                c->kpss_stat, c->kpss_p );
        if ( c->adf_p < 0.05 && c->kpss_p > 0.05 )
            g_string_append( cab, "ADF rechaza la raíz unitaria y KPSS no rechaza la "
                                  "estacionariedad: así la serie parece estacionaria." );
        else if ( c->adf_p >= 0.05 && c->kpss_p <= 0.05 )
            g_string_append_printf( cab, "ADF no rechaza la raíz unitaria y KPSS rechaza la "
                                         "estacionariedad: queda una raíz unitaria; mira "
                                         "d = %d.", g->d + 1 );
        else
            g_string_append( cab, "Los dos contrastes no dicen lo mismo: decide con el "
                                  "correlograma." );
        }
    else if ( g->punto == 1 )
        g_string_append( cab, "\nLos contrastes de raíz unitaria se hacen con d ≤ 1 y D = 0." );
    gtk_label_set_markup( GTK_LABEL(g->l_cab), cab->str );
    g_string_free( cab, TRUE );
}

static gboolean corre_art( Id *g, const char *datos, gboolean residuos, char *why, size_t n );
static int escribe_datos( const char *ruta, const double *x, int n, int freq, int per, int anio );

/* E1: art again, with the transformation in the controls. */
static void on_reidentificar( GtkButton *b, Id *g )
{
    gchar *base, *datos;
    char   why[512];

    (void) b;
    g->lam = gtk_combo_box_get_active( GTK_COMBO_BOX(g->c_lam) ) == 0 ? 0.0 : 1.0;
    g->d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(g->s_d) );
    g->D = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(g->s_D) );
    if ( g->lam == 0.0 )
        {
        int i;
        for ( i = 0; i < g->n; i++ )
            if ( !( g->x[i] > 0.0 ) )
                { di( g, "La serie tiene valores no positivos: en logaritmos no." ); return; }
        }
    base = an_fichero( "identifica", g->serie, g->muestra, g->id );
    datos = g_strdup_printf( "%s.txt", base );
    g_free( base );
    if ( escribe_datos( datos, g->x, g->n, g->freq, g->per, g->anio ) != 0 ||
         !corre_art( g, datos, FALSE, why, sizeof why ) )
        { di( g, "%s", why ); g_free( datos ); return; }
    g_free( datos );
    g->sel = -1;
    cabecera( g );
    llena( g );
    gtk_widget_queue_draw( g->area );
    di( g, "Identificado de nuevo: %s, d = %d, D = %d.", g->lam == 0.0 ? "logaritmos" : "niveles",
        g->d, g->D );
}

static void ventana( Id *g, const char *que )
{
    GtkWidget *caja, *l, *sw, *barra, *b;
    GString   *cab = g_string_new( "" );

    g_snprintf( g->que, sizeof g->que, "%s", que );
    gchar     *t;
    unsigned   puede = g->h.puede ? g->h.puede : ( AN_PUEDE_FUE | AN_PUEDE_EDITOR );
    int        i;

    g->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    t = g_strdup_printf( "Identificación — %s / %s", g->serie, g->id );
    gtk_window_set_title( GTK_WINDOW(g->win), t );
    g_free( t );
    if ( g->h.padre ) gtk_window_set_transient_for( GTK_WINDOW(g->win), g->h.padre );
    gtk_window_set_default_size( GTK_WINDOW(g->win), 820, 760 );

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );
    gtk_container_add( GTK_CONTAINER(g->win), caja );

    /* E1: the transformation, editable, and identify again */
    if ( g->punto == 1 )
        {
        GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 ), *bi;

        gtk_box_pack_start( GTK_BOX(fila), gtk_label_new( "Transformación:" ), FALSE, FALSE, 0 );
        g->c_lam = gtk_combo_box_text_new();
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->c_lam), "logaritmos (λ = 0)" );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->c_lam), "niveles (λ = 1)" );
        gtk_combo_box_set_active( GTK_COMBO_BOX(g->c_lam), g->lam == 0.0 ? 0 : 1 );
        gtk_box_pack_start( GTK_BOX(fila), g->c_lam, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(fila), gtk_label_new( "  d" ), FALSE, FALSE, 0 );
        g->s_d = gtk_spin_button_new_with_range( 0, 2, 1 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(g->s_d), g->d );
        gtk_box_pack_start( GTK_BOX(fila), g->s_d, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(fila), gtk_label_new( "  D" ), FALSE, FALSE, 0 );
        g->s_D = gtk_spin_button_new_with_range( 0, 1, 1 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(g->s_D), g->D );
        gtk_widget_set_sensitive( g->s_D, g->freq > 1 );
        gtk_box_pack_start( GTK_BOX(fila), g->s_D, FALSE, FALSE, 0 );
        bi = gtk_button_new_with_label( "Volver a identificar" );
        gtk_widget_set_tooltip_text( bi,
            "art otra vez con esta transformación: los contrastes, el "
            "correlograma y los candidatos se rehacen aquí mismo." );
        g_signal_connect( bi, "clicked", G_CALLBACK(on_reidentificar), g );
        gtk_box_pack_start( GTK_BOX(fila), bi, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );
        }

    /* where we are, what is fixed, and what the tests say */
    g->l_cab = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_cab), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_cab), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), g->l_cab, FALSE, FALSE, 0 );
    cabecera( g );
    g_string_free( cab, TRUE );

    /* the correlograms */
    g->area = gtk_drawing_area_new();
    gtk_widget_set_size_request( g->area, 760, 380 );
    g_signal_connect( g->area, "draw", G_CALLBACK(pinta), g );
    gtk_box_pack_start( GTK_BOX(caja), g->area, TRUE, TRUE, 0 );
    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l),
        "<small>Barras: la ACF y la PACF de la serie identificada. En rojo, la "
        "teórica del candidato elegido en la lista. Discontinua: la banda.</small>" );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );

    /* the partial filter */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(barra), gtk_label_new( "Mostrar:" ), FALSE, FALSE, 0 );
    g->filtro = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->filtro), "todos los candidatos" );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->filtro),
                                    "los de la misma parte regular que el elegido" );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->filtro),
                                    "los de la misma parte estacional que el elegido" );
    gtk_combo_box_set_active( GTK_COMBO_BOX(g->filtro), F_TODOS );
    gtk_widget_set_tooltip_text( g->filtro,
        "Identificación parcial: fija una parte del elegido y mira sólo los "
        "candidatos que la comparten. Filtra la evidencia; no la inventa." );
    g_signal_connect( g->filtro, "changed", G_CALLBACK(on_filtro), g );
    gtk_box_pack_start( GTK_BOX(barra), g->filtro, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    /* the candidates */
    g->store = gtk_list_store_new( C_N, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING,
                                   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                   G_TYPE_STRING, G_TYPE_INT );
    g->vista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(g->store) );
    g_object_unref( g->store );
    columna( g->vista, "#", C_RANK, 1.0 );
    columna( g->vista, "(p,q)(P,Q)", C_ORDEN, 0.0 );
    columna( g->vista, "modelo", C_TIPO, 0.0 );
    columna( g->vista, "similitud", C_SIM, 1.0 );
    columna( g->vista, "peso Akaike", C_PESO, 1.0 );
    columna( g->vista, "AICc (CSS)", C_AICC, 1.0 );
    columna( g->vista, "", C_NOTA, 0.0 );
    gtk_widget_set_tooltip_text( g->vista,
        "Ordenados por similitud de patrón (opción B). El peso de Akaike es de un "
        "AICc condicional: comparable entre estos candidatos y nada más." );
    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(g->vista) ), "changed",
                      G_CALLBACK(on_sel), g );
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_widget_set_size_request( sw, -1, 200 );
    gtk_container_add( GTK_CONTAINER(sw), g->vista );
    gtk_box_pack_start( GTK_BOX(caja), sw, TRUE, TRUE, 0 );
    llena( g );

    /* what the engine said */
    if ( g->c->nmsg )
        {
        GString *m = g_string_new( "<small>" );
        for ( i = 0; i < g->c->nmsg; i++ )
            {
            gchar *e = g_markup_escape_text( g->c->msg[i], -1 );
            g_string_append_printf( m, "%s%s", i ? "\n" : "", e );
            g_free( e );
            }
        g_string_append( m, "</small>" );
        l = gtk_label_new( NULL );
        gtk_label_set_markup( GTK_LABEL(l), m->str );
        gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
        gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );
        g_string_free( m, TRUE );
        }

    g->l_estado = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(g->l_estado),
        "<small>La propuesta es evidencia, no un veredicto: elige un candidato, "
        "mira su correlograma teórico sobre el empírico y decide.</small>" );
    gtk_label_set_xalign( GTK_LABEL(g->l_estado), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_estado), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), g->l_estado, FALSE, FALSE, 0 );

    /* derive: offered only if the host can save AND open (an_sugerir) */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );
    if ( g->h.abre && g->h.guarda )
        {
        g->con_que = gtk_combo_box_text_new();
        if ( puede & AN_PUEDE_FUE )
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->con_que), "abrir en fue_gui" );
        if ( puede & AN_PUEDE_EDITOR )
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(g->con_que), "abrir en el editor" );
        g->solo_editor = !( puede & AN_PUEDE_FUE );
        gtk_combo_box_set_active( GTK_COMBO_BOX(g->con_que), 0 );
        gtk_box_pack_end( GTK_BOX(barra), g->con_que, FALSE, FALSE, 0 );
        b = gtk_button_new_with_label( "Derivar modelo con el candidato elegido" );
        gtk_widget_set_tooltip_text( b,
            "Crea un modelo NUEVO, hijo de éste, con los órdenes del candidato y "
            "sus coeficientes como semillas, y lo abre. No lo estima." );
        g_signal_connect( b, "clicked", G_CALLBACK(on_derivar), g );
        gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );
        }
    else
        {
        l = gtk_label_new( NULL );
        gtk_label_set_markup( GTK_LABEL(l),
            "<small>Desde aquí sólo se <b>mira</b>: derivar toca el manifiesto, "
            "y eso se hace desde la madre.</small>" );
        gtk_box_pack_end( GTK_BOX(barra), l, FALSE, FALSE, 0 );
        }

    g_signal_connect( g->win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( g->win );
}

static Id *nuevo_id( const AnHost *h, const char *serie, const char *muestra, const char *id )
{
    Id *g = g_new0( Id, 1 );

    g->h = *h;
    g_snprintf( g->serie, sizeof g->serie, "%s", serie );
    g_snprintf( g->muestra, sizeof g->muestra, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, sizeof g->id, "%s", id );
    g->c = g_new0( ArtCand, 1 );
    g->x = NULL;
    g->sel = -1;
    return g;
}

/* ---- E2: the identification graphs ------------------------------------ */

void an_identifica_serie( const AnHost *h, const char *serie, const char *muestra,
                          const char *id, const AnSerie *s )
{
    Id    *g;
    gchar *base, *datos;
    char   why[512];

    if ( !h || !h->p || !serie || !id || !s || !s->x || s->n < 10 ) return;
    if ( s->lam != 0.0 && s->lam != 1.0 )
        {
        an_di( h, "art identifica en logaritmos (λ = 0) o en niveles (λ = 1); con "
                  "λ = %.2f no. Ponla en 0 o en 1.", s->lam );
        return;
        }
    g = nuevo_id( h, serie, muestra, id );
    g->punto = 2;
    g->lam = s->lam; g->d = s->d; g->D = s->D; g->freq = s->freq;

    base = an_fichero( "identifica", serie, muestra, id );
    datos = g_strdup_printf( "%s.txt", base );
    g_free( base );
    if ( escribe_datos( datos, s->x, s->n, s->freq, s->per, s->anio ) != 0 )
        { an_di( h, "No pude escribir %s.", datos ); g_free( datos ); on_cerrar( NULL, g ); return; }
    if ( !corre_art( g, datos, FALSE, why, sizeof why ) )
        { an_di( h, "%s", why ); g_free( datos ); on_cerrar( NULL, g ); return; }
    g_free( datos );
    ventana( g, s->que && s->que[0] ? s->que : "Gráficos de identificación" );
}

/* ---- E3: a base model's residuals ------------------------------------- */

void an_identifica_residuos( const AnHost *h, const char *serie, const char *muestra,
                             const char *id )
{
    Id      *g;
    FueOut  *o;
    char     out[PR_RUTA], why[512];
    gchar   *base, *datos;
    int      i, per = 0, anio = 0, freq = 0;

    if ( !h || !h->p || !serie || !id ) return;
    if ( an_estado( h->p, serie, muestra, id, why, sizeof why ) != AN_LISTO )
        { an_di( h, "%s", why ); return; }
    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 ) return;

    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) || o->nres < 20 )
        { an_di( h, "El .out de %s no trae los residuos.", id ); g_free( o ); return; }

    /* the frequency: the model's, or the largest period in the dates */
    freq = o->s;
    for ( i = 0; i < o->nres; i++ )
        {
        int p = 0, a = 0;
        if ( fo_fecha_parte( o->res_fecha[i], &p, &a ) == 0 && p > freq && p <= 52 ) freq = p;
        }
    if ( freq < 1 ) freq = 1;
    fo_fecha_parte( o->res_fecha[0], &per, &anio );

    g = nuevo_id( h, serie, muestra, id );
    g->punto = 3;
    g->lam = 1.0; g->d = 0; g->D = 0; g->freq = freq;

    base = an_fichero( "identifica_res", serie, muestra, id );
    datos = g_strdup_printf( "%s.txt", base );
    g_free( base );
    if ( escribe_datos( datos, o->res, o->nres, freq, per, anio ) != 0 )
        { an_di( h, "No pude escribir %s.", datos ); g_free( datos ); g_free( o );
          on_cerrar( NULL, g ); return; }
    g_free( o );
    if ( !corre_art( g, datos, TRUE, why, sizeof why ) )
        { an_di( h, "%s", why ); g_free( datos ); on_cerrar( NULL, g ); return; }
    g_free( datos );
    ventana( g, "Residuos del modelo" );
}

/* ---- E1: the data ------------------------------------------------------ */

void an_identifica_datos( const AnHost *h, const char *serie, const char *muestra,
                          const char *id, const AnSerie *s )
{
    Id    *g;
    gchar *base, *datos;
    char   why[512];
    int    i, positiva = 1;

    if ( !h || !h->p || !serie || !id || !s || !s->x || s->n < 10 ) return;
    for ( i = 0; i < s->n; i++ ) if ( !( s->x[i] > 0.0 ) ) positiva = 0;

    g = nuevo_id( h, serie, muestra, id );
    g->punto = 1;
    /* where to start: the transformation the data node says, made one art
       can take -- log if the file says log and the series allows it */
    g->lam = ( s->lam == 0.0 && positiva ) ? 0.0 : 1.0;
    g->d = s->d < 0 ? 0 : ( s->d > 2 ? 2 : s->d );
    g->D = ( s->freq > 1 && s->D > 0 ) ? 1 : 0;
    g->freq = s->freq;
    g->n = s->n; g->per = s->per; g->anio = s->anio;
    g->x = g_malloc( (gsize) s->n * sizeof *s->x );
    memcpy( g->x, s->x, (size_t) s->n * sizeof *s->x );

    base = an_fichero( "identifica", serie, muestra, id );
    datos = g_strdup_printf( "%s.txt", base );
    g_free( base );
    if ( escribe_datos( datos, g->x, g->n, g->freq, g->per, g->anio ) != 0 ||
         !corre_art( g, datos, FALSE, why, sizeof why ) )
        { an_di( h, "%s", why ); g_free( datos ); on_cerrar( NULL, g ); return; }
    g_free( datos );
    ventana( g, s->que && s->que[0] ? s->que : "Identificación desde los datos" );
}
