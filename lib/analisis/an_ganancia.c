/*
 * an_ganancia.c -- la ganancia de las intervenciones: ¿se queda o revierte?
 *
 * POR QUE NO ESTA EN LA DIAGNOSIS, que es donde yo la había puesto.
 *
 * Lo corrigió el analista y tiene razón: la diagnosis JUZGA LO QUE HAY --los
 * residuos son blancos o no lo son, la media es cero o no lo es-- y esto
 * INTERROGA UNA HIPOTESIS sobre la naturaleza del suceso. «¿Este incidente
 * dejó algo para siempre, o revirtió?» no es un veredicto sobre el modelo:
 * es una pregunta sobre el mundo que el modelo permite contestar.
 *
 * En la diagnosis se queda el resumen --una línea que dice cuántas hay y si
 * alguna tiene ganancia nula--, que es lo que allí cabe. El detalle es esto.
 *
 * LO QUE SE ENSEÑA, y por qué cada cosa:
 *
 *   la intervención con su FECHA    el .out sólo dice «deterministic
 *                                   variable 12», y contar líneas en el
 *                                   .inp para saber de cuál te hablan no es
 *                                   trabajo del analista
 *   cada omega con su error y su t  para poder verificar el ω(1) a mano
 *   omega(1), su error y el Wald    la respuesta
 *   la lectura                      permanente / transitorio, y sólo para
 *                                   los escalones, que es donde significa eso
 *
 * Y LA SUMA DE LOS OMEGAS, a propósito, al lado de ω(1): son distintas --el
 * convenio de Box-Jenkins hace que los retardos resten-- y verlas juntas es
 * lo que impide el error. Sobre IPC_ES m03 la suma da 0,0365 y la ganancia
 * 0,0134.
 */

#include <string.h>
#include <math.h>

#include "outfile.h"
#include "dictamen.h"          /* chisq_cola */
#include "intervencion.h"
#include "eqtran.h"
#include "tabla.h"

#include "analisis.h"

#define GA_MAX  32

typedef struct {
    char   linea[AN_LDET];     /* «step 3 2022»                          */
    int    det;                /* su número, 1-based                     */
    int    nom;                /* cuántos omegas                         */
    double om[16], et[16];
    int    libre[16];
    IvGanancia g;
    double p;                  /* del Wald; -1 si no hay                 */
    int    es_escalon;
} Fila;

typedef struct {
    AnHost h;
    char   serie[PR_ID], muestra[PR_ID], id[PR_ID];
    FueOut o;
    char   estruct[64];
    Fila   f[GA_MAX];
    int    nf;
} Ga;


/* ¿La línea del .inp empieza por esa palabra? */
static int es( const char *linea, const char *pal )
{
    size_t n = strlen( pal );

    return ( strncmp( linea, pal, n ) == 0 &&
             ( linea[n] == ' ' || linea[n] == '\0' ) );
}

static void arma( Ga *g )
{
    char det[AN_MAX_DET][AN_LDET];
    int  nd, i, k;

    nd = an_deterministas( g->h.p, g->serie, g->muestra, g->id, det, AN_MAX_DET );

    for ( i = 0; i < g->o.ndet_leidos && g->nf < GA_MAX; i++ )
        {
        Fila  *f = &g->f[g->nf];
        int    n = g->o.det_nom[i], i0 = g->o.det_i0[i];
        double V[16*16];
        int    idx[16], nl = 0, a, b;

        if ( n < 1 || n > 16 || i0 < 1 ) continue;

        memset( f, 0, sizeof *f );
        f->det = i + 1;
        f->nom = n;
        if ( i < nd && det[i][0] ) g_snprintf( f->linea, AN_LDET, "%s", det[i] );
        else g_snprintf( f->linea, AN_LDET, "determinista %d", i + 1 );

        /* SOLO LOS SUCESOS. Los armónicos y la tendencia son estructura, no
           incidentes: su «ganancia» no significa nada.                 */
        if ( !es( f->linea, "step" ) && !es( f->linea, "impulse" ) &&
             !es( f->linea, "compimp" ) && !es( f->linea, "ramp" ) )
            continue;
        f->es_escalon = es( f->linea, "step" );

        for ( k = 0; k < n; k++ )
            {
            f->om[k]    = g->o.par[i0 - 1 + k];
            f->et[k]    = g->o.par_et[i0 - 1 + k];
            f->libre[k] = g->o.par_estimado[i0 - 1 + k];
            if ( f->libre[k] ) idx[nl++] = i0 + k;
            }
        for ( a = 0; a < nl; a++ )
            for ( b = 0; b < nl; b++ )
                V[a*16 + b] = fo_cov( &g->o, idx[a], idx[b] );

        f->p = -1.0;
        if ( iv_ganancia( f->om, f->libre, n, V, 16, NULL, 0, 0.0, &f->g ) == 0 &&
             f->g.hay_wald )
            f->p = chisq_cola( f->g.wald, 1 );
        g->nf++;
        }
}


/* ------------------------------------------------------------------------ */

enum { GC_QUE, GC_FLT, GC_G1, GC_ET, GC_W, GC_P, GC_LEE, GC_COLOR, GC_N };

/* LA FLT COMO LA ESCRIBE fue: el signo delante, el numero positivo y la
 * desviacion tipica DEBAJO, alineada. Es la misma forma del papel y del
 * grafico, y se arma con el mismo vocabulario --EqItem-- para que no se
 * separen nunca.
 *
 * Y NO ES SOLO ESTETICA. Escrita asi, LA GANANCIA ES LA SUMA DE LO QUE SE
 * VE: el signo esta en la linea. Con los valores crudos del .out no lo es
 * --los retardos restan-- y yo llegue a poner una columna «Σω» al lado de
 * la ganancia enseñando esa diferencia como si fuera un hecho del modelo.
 * No lo es: era un artefacto de escribirlos sin su signo. Lo corrigio el
 * analista.                                                            */
static void flt_txt( const Fila *f, char *b, size_t n )
{
    EqItem  it[64];
    EqLink  L;
    char    a1[256], a2[256], fecha[64];
    int     k, ni;

    b[0] = '\0';

    /* La entrada es el suceso: su tipo y su fecha, que es lo que se lee. */
    g_snprintf( fecha, sizeof fecha, "%s", f->linea );

    memset( &L, 0, sizeof L );
    L.entrada  = fecha;
    L.s        = f->nom - 1;
    L.r        = 0;
    L.omega    = f->om;
    L.omega_se = f->et;

    /* Un omega FIJO no tiene error: se pasa a cero y el escritor no le
       pone nada debajo, que es lo cierto.                             */
    {
    static double et2[16];

    for ( k = 0; k < f->nom && k < 16; k++ ) et2[k] = f->libre[k] ? f->et[k] : 0.0;
    L.omega_se = et2;
    }

    ni = eqtran_items( it, 64, &L, 1 );
    eq_items_texto_et( a1, a2, sizeof a1, it, ni );
    g_snprintf( b, n, "%s\n%s", a1, a2 );
}

static const char *lectura_de( const Fila *f, char *b, size_t n )
{
    if ( !f->g.hay_ganancia )
        { g_snprintf( b, n, "δ(1) = 0: el modelo no admite ganancia" ); return b; }
    if ( f->p < 0.0 )
        { g_snprintf( b, n, "un solo ω libre: su t ya lo dice" ); return b; }
    if ( !f->es_escalon )
        { g_snprintf( b, n, "efecto acumulado; «permanente» sólo se lee en un escalón" );
          return b; }
    g_snprintf( b, n, f->p < 0.05 ? "PERMANENTE" : "transitorio" );
    return b;
}

static void llena( Ga *g, GtkListStore *st )
{
    GtkTreeIter it;
    int         i;

    for ( i = 0; i < g->nf; i++ )
        {
        const Fila *f = &g->f[i];
        char flt[768], lee[128], s2[32], s3[32], s4[32], s5[32];
        const char *color = "#57606a";

        flt_txt( f, flt, sizeof flt );
        lectura_de( f, lee, sizeof lee );

        g_snprintf( s2, sizeof s2, "%+.6f", f->g.omega_1 );
        if ( f->g.hay_wald )
            {
            g_snprintf( s3, sizeof s3, "%.6f", f->g.et );
            g_snprintf( s4, sizeof s4, "%.2f", f->g.wald );
            g_snprintf( s5, sizeof s5, "%.3f", f->p );
            color = f->es_escalon ? ( f->p < 0.05 ? "#1a7f37" : "#9a6700" )
                                  : "#57606a";
            }
        else { s3[0] = s4[0] = s5[0] = '\0'; }

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            GC_QUE, f->linea, GC_FLT, flt, GC_G1, s2,
            GC_ET, s3, GC_W, s4, GC_P, s5, GC_LEE, lee, GC_COLOR, color, -1 );
        }
}

static void on_exportar( GtkButton *b, Ga *g )
{
    GtkWidget *d;
    Tabla     *t;
    char       titulo[256];
    int        i;

    (void) b;
    g_snprintf( titulo, sizeof titulo, "Ganancia de las intervenciones de "
                "%s / %s%s%s", g->serie, g->id,
                g->muestra[0] ? ", muestra " : "", g->muestra );
    t = tb_new( titulo );
    if ( t == NULL ) return;

    tb_col( t, "Intervención", NULL, TB_TXT, 0 );
    /* EN DOS COLUMNAS y no en dos lineas: un salto de linea dentro de una
       celda no sobrevive a un CSV. En la pantalla van una debajo de otra. */
    tb_col( t, "ω(B) ξ_t",     NULL, TB_TXT, 0 );
    tb_col( t, "sus et",       NULL, TB_TXT, 0 );
    tb_col( t, "ω(1)",         NULL, TB_TXT, 0 );
    tb_col( t, "et",           NULL, TB_TXT, 0 );
    tb_col( t, "W",            NULL, TB_TXT, 0 );
    tb_col( t, "p",            NULL, TB_TXT, 0 );
    tb_col( t, "Lectura",      NULL, TB_TXT, 0 );

    for ( i = 0; i < g->nf; i++ )
        {
        const Fila *f = &g->f[i];
        char flt[768], lee[128], b2[32], b3[32], b4[32], b5[32];
        char *salto;

        flt_txt( f, flt, sizeof flt );
        salto = strchr( flt, '\n' );
        if ( salto ) *salto++ = '\0'; else salto = flt + strlen( flt );
        lectura_de( f, lee, sizeof lee );
        g_snprintf( b2, sizeof b2, "%+.6f", f->g.omega_1 );
        if ( f->g.hay_wald )
            { g_snprintf( b3, sizeof b3, "%.6f", f->g.et );
              g_snprintf( b4, sizeof b4, "%.2f", f->g.wald );
              g_snprintf( b5, sizeof b5, "%.3f", f->p ); }
        else { b3[0] = b4[0] = b5[0] = '\0'; }

        tb_fila( t );
        tb_pon_txt( t, 0, f->linea );
        tb_pon_txt( t, 1, flt );
        tb_pon_txt( t, 2, salto );
        tb_pon_txt( t, 3, b2 );
        tb_pon_txt( t, 4, b3 );
        tb_pon_txt( t, 5, b4 );
        tb_pon_txt( t, 6, b5 );
        tb_pon_txt( t, 7, lee );
        }

    tb_procedencia( t, "Serie",   g->serie );
    tb_procedencia( t, "Modelo",  g->id );
    tb_procedencia( t, "Muestra", g->muestra[0] ? g->muestra : "completa" );
    if ( g->estruct[0] ) tb_procedencia( t, "Estructura", g->estruct );
    tb_procedencia( t, "Contraste",
        "Wald sobre omega(1), con 1 g.l. Escrito el polinomio con sus "
        "signos, omega(1) es la SUMA de los coeficientes." );

    d = gtk_file_chooser_dialog_new( "Exportar la ganancia", g->h.padre,
            GTK_FILE_CHOOSER_ACTION_SAVE, "Cancelar", GTK_RESPONSE_CANCEL,
            "Guardar", GTK_RESPONSE_ACCEPT, NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
    {
    char sug[256];

    g_snprintf( sug, sizeof sug, "%s_%s_ganancia.txt", g->serie, g->id );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), sug );
    }
    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );

        if ( tb_write( t, p ) == 0 ) an_di( &g->h, "Ganancia en %s.", p );
        else                         an_di( &g->h, "No pude escribirla." );
        g_free( p );
        }
    gtk_widget_destroy( d );
    tb_free( t );
}

static void on_cerrar( GtkWidget *w, Ga *g ) { (void) w; g_free( g ); }


/* ------------------------------------------------------------------------ */

void an_ganancia( const AnHost *h, const char *serie, const char *muestra,
                  const char *id )
{
    Ga           *g;
    Convergence   c;
    GtkWidget    *win, *raiz, *cab, *sc, *vista, *barra, *b, *pie;
    GtkListStore *st;
    char          out[PR_RUTA];

    if ( !h || !h->p || !serie || !*serie || !id || !*id ) return;
    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 )
        { an_di( h, "No pude componer la ruta." ); return; }

    g = g_new0( Ga, 1 );
    g->h = *h;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );

    if ( !fueout_read( out, &g->o ) )
        { an_di( h, "«%s» no está estimado.", id ); g_free( g ); return; }
    fueout_estructura( &g->o, g->estruct, sizeof g->estruct );
    if ( !convergence_of( out, &c ) ) memset( &c, 0, sizeof c );

    arma( g );

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( h->padre ) gtk_window_set_transient_for( GTK_WINDOW(win), h->padre );
    gtk_window_set_default_size( GTK_WINDOW(win), 980, 380 );
    {
    gchar *t = g_strdup_printf( "Ganancia de las intervenciones — %s / %s",
                                serie, id );

    gtk_window_set_title( GTK_WINDOW(win), t );
    g_free( t );
    }

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(win), raiz );

    cab = gtk_label_new( NULL );
    {
    gchar *t = g_markup_printf_escaped(
        "<b>%s / %s</b>%s%s   ·   %s   ·   %d intervención%s\n"
        "<small>¿El suceso dejó algo <b>para siempre</b> o revirtió? Lo dice "
        "la <b>ganancia</b>, ω(1). El polinomio va escrito con sus signos "
        "—convenio de Box-Jenkins, el primero suma y los retardos restan— y "
        "así <b>ω(1) es la suma de lo que se ve</b>: se puede comprobar a "
        "ojo.</small>",
        serie, id, ( muestra && *muestra ) ? "   muestra " : "",
        ( muestra && *muestra ) ? muestra : "", g->estruct, g->nf,
        g->nf == 1 ? "" : "es" );

    gtk_label_set_markup( GTK_LABEL(cab), t );
    g_free( t );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(cab), TRUE );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    st = gtk_list_store_new( GC_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    llena( g, st );
    vista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    {
    static const char *cab2[] = { "Intervención", "ω(B) ξ_t", "ω(1)",
                                  "et", "W", "p", "Lectura" };
    int i;

    for ( i = 0; i < 7; i++ )
        {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();

        /* LA FLT EN MONOESPACIADO: la dt va debajo alineada por columnas,
           y con una fuente proporcional la alineacion es mentira.    */
        if ( i == 1 ) g_object_set( r, "family", "monospace", NULL );
        gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(vista), -1,
            cab2[i], r, "text", i, "foreground", GC_COLOR, NULL );
        }
    }

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), vista );
    gtk_box_pack_start( GTK_BOX(raiz), sc, TRUE, TRUE, 0 );

    pie = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(pie), TRUE );
    gtk_label_set_markup( GTK_LABEL(pie), g->nf == 0
        ? "<small>Este modelo no lleva ninguna intervención de suceso. Los "
          "armónicos y la tendencia son estructura, no incidentes: su "
          "«ganancia» no significa nada y por eso no salen.</small>"
        : "<small>Con <b>ganancia nula</b>, L+1 escalones son exactamente L "
          "impulsos de nivel: el suceso <b>revirtió</b>, y sobra un parámetro. "
          "Con ganancia distinta de cero, algo se quedó y <b>gobierna la "
          "previsión</b> de aquí en adelante.\n"
          "Con un solo ω no hay contraste nuevo: la ganancia es el "
          "coeficiente y su t ya está en la tabla. Y ω(1) es la suma de los "
          "coeficientes <b>tal como están escritos</b>: se comprueba a "
          "ojo.</small>" );
    gtk_box_pack_start( GTK_BOX(raiz), pie, FALSE, FALSE, 0 );

    /* SIN COVARIANZA NO HAY CONTRASTE, y se dice arriba del todo. */
    if ( c.iterations == 0 )
        {
        GtkWidget *av = gtk_label_new( NULL );

        gtk_label_set_markup( GTK_LABEL(av),
            "<small>⚠ El motor convergió en <b>cero iteraciones</b>: lo que "
            "imprime como covarianza es la semilla del optimizador, no una "
            "covarianza. Los contrastes de abajo son ficción.</small>" );
        gtk_label_set_line_wrap( GTK_LABEL(av), TRUE );
        gtk_label_set_xalign( GTK_LABEL(av), 0.0 );
        gtk_box_pack_start( GTK_BOX(raiz), av, FALSE, FALSE, 0 );
        }
    convergence_clear( &c );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Exportar…" );
    gtk_widget_set_tooltip_text( b,
        "El formato lo dice la extensión: .csv, .txt o .tex.\n\nLa tabla lleva "
        "dentro de qué modelo salió: una ganancia suelta en un papel no vale "
        "para nada dentro de un mes." );
    gtk_widget_set_sensitive( b, g->nf > 0 );
    g_signal_connect( b, "clicked", G_CALLBACK(on_exportar), g );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    g_signal_connect( win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( win );
}
