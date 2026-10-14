/*
 * an_formales.c -- la ventana de los contrastes formales del modelo
 * estimado: el formal_tests de art (lib/formales). La abre la diagnosis.
 *
 * Cada contraste es una estimacion mas: el modelo con un parametro fijo en
 * su valor nulo, que fue estima partiendo del optimo. Se corren de uno en
 * uno y SIN BLOQUEAR LA VENTANA, y cada resultado se escribe en cuanto
 * llega.
 *
 * AQUI NO SE DECIDE NADA Y NO SE ENCADENA NADA: cada contraste dice su
 * numero, sus criticos y lo que dice, por separado. El veredicto encadenado
 * de art (describe_formal_tests) es donde viven sus fallos abiertos, y no se
 * porta. Lo que no es un estadistico sale como «no calculable».
 *
 * Suponen un modelo ADECUADO (art BUG-0025): con la diagnosis sin limpiar,
 * la ventana lo dice arriba, y los numeros se leen con esa reserva.
 */

#include <math.h>
#include <string.h>

#include <glib/gstdio.h>

#include "dictamen.h"
#include "engine.h"
#include "formales.h"
#include "outfile.h"
#include "sitio.h"

#include "analisis.h"

typedef struct {
    AnHost     h;
    char       serie[PR_ID], muestra[PR_ID], id[PR_ID];
    FmPlan     pl;
    double     l_libre;
    int        cur;               /* el que esta corriendo; pl.n al acabar */
    gboolean   corriendo, cerrada;
    gchar     *exe, *dir;
    char       aviso[512];
    GtkWidget *win, *l_texto, *l_estado;
} Fm;

static gchar *programa_fue( void )
{
    static const char *const sitio[] = {     /* relativo al programa        */
        "../../engines/fue/bin/%s",          /* gui/atsw/atsw_gui           */
        "../../../engines/fue/bin/%s",       /* gui/fue/bin/fue_gui         */
        "../fue/bin/%s", "./%s", NULL
    };
    return sitio_busca( "fue", sitio );
}

static void criticos( GString *t, const FmPrueba *x, const char *de )
{
    g_string_append_printf( t, "críticos%s%s: %.2f (10 %%), <b>%.2f (5 %%)</b>, %.2f (1 %%)",
                            de ? " de " : "", de ? de : "", x->c10, x->c5, x->c1 );
}

static void un_contraste( GString *t, const Fm *g, const FmPrueba *x, int i )
{
    static const char *const nom[] = { "Simplificación estacional", "Shin-Fuller",
                                       "DCD", "DCD estacional" };
    gchar *h = g_markup_escape_text( x->hipotesis, -1 );

    g_string_append_printf( t, "<b>%s</b>   H<sub>0</sub>: %s\n", nom[x->tipo], h );
    g_free( h );
    if ( x->tipo == FM_SF )
        g_string_append_printf( t, "    raíz real positiva dominante del AR: φ̂ = %.4f\n", x->coef );
    else if ( x->tipo == FM_DCD )
        g_string_append_printf( t, "    θ̂ = %.4f\n", x->coef );
    else if ( x->tipo == FM_DCD_S )
        g_string_append_printf( t, "    Θ̂ = %.4f\n", x->coef );

    if ( !x->hecho )
        {
        g_string_append( t, i == g->cur && g->corriendo ? "    <i>estimando el modelo restringido…</i>\n\n"
                                                         : "    <i>en cola</i>\n\n" );
        return;
        }
    if ( !x->calculable )
        {
        gchar *m = g_markup_escape_text( x->motivo, -1 );

        g_string_append_printf( t, "    <b>No calculable</b>: %s.\n\n", m );
        g_free( m );
        return;
        }

    g_string_append_printf( t, "    L = %.4f,  L<sub>0</sub> = %.4f.   ", x->l_libre, x->l_restr );
    switch ( x->tipo )
        {
        case FM_ESTACIONAL:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b> con %d g.l., ",
                                    x->est, x->gl );
            if ( x->p < 1e-4 ) g_string_append( t, "p &lt; 0.0001" );
            else               g_string_append_printf( t, "p = %.4f", x->p );
            g_string_append( t, "\n    " );
            criticos( t, x, "la χ²" );
            g_string_append( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: los armónicos son, en conjunto, significativos.\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: los armónicos se podrían quitar en bloque.\n\n" );
            break;
        case FM_SF:
            g_string_append_printf( t, "Φ = L − L<sub>0</sub> = <b>%.3f</b>  (no 2(L − L<sub>0</sub>))\n    ",
                                    x->est );
            {
            gchar *de = g_strdup_printf( "Shin-Fuller (1998) para n = %d", g->pl.nobs );

            criticos( t, x, de );
            g_free( de );
            }
            if ( x->al_otro_lado )
                g_string_append_printf( t, "\n    → <b>No rechaza H<sub>0</sub></b>: φ̂ = %.4f está "
                    "por encima de ρ, así que Φ = 0 por construcción: los datos son al menos tan "
                    "integrados como la nula.%s\n\n", x->coef,
                    g->pl.d >= 2 ? " Con d = 2 no se propone otra diferencia." : "" );
            else if ( x->rechaza )
                g_string_append_printf( t, "\n    → <b>Rechaza H<sub>0</sub></b>: el AR es "
                    "estacionario; con d = %d basta.\n\n", g->pl.d );
            else
                g_string_append_printf( t, "\n    → <b>No rechaza H<sub>0</sub></b>: el AR puede "
                    "tener una raíz unitaria%s.\n\n", g->pl.d >= 2
                    ? "; con d = 2 no se propone otra diferencia"
                    : ": la alternativa es una diferencia regular más" );
            break;
        case FM_DCD:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b>\n    ", x->est );
            criticos( t, x, "Davis-Dunsmuir" );
            g_string_append( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: el MA es invertible; la diferencia regular no sobra.\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: θ está en la frontera de la "
                  "invertibilidad: con una diferencia regular menos bastaría.\n\n" );
            break;
        case FM_DCD_S:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b>\n    ", x->est );
            {
            gchar *de = g_strdup_printf( "Davis, Chen y Dunsmuir para s = %d", g->pl.s );

            criticos( t, x, de );
            g_free( de );
            }
            g_string_append( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: el MA anual es invertible; la diferencia "
                  "estacional es genuina (estacionalidad estocástica).\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: Θ está en la frontera: la diferencia "
                  "estacional sobra y la estacionalidad sería determinista.\n\n" );
            break;
        }
}

static void pinta( Fm *g )
{
    GString *t = g_string_new( "" );
    int      i, k;

    if ( g->aviso[0] )
        g_string_append_printf( t, "<span foreground=\"#9a6700\"><b>%s</b></span>\n\n", g->aviso );
    for ( i = 0; i < g->pl.n; i++ ) un_contraste( t, g, &g->pl.t[i], i );
    if ( g->pl.n == 0 )
        g_string_append( t, "Este modelo no admite ninguno de los contrastes formales que hay.\n\n" );
    for ( k = 0; k < 4; k++ )
        if ( g->pl.no_aplica[k][0] )
            {
            gchar *m = g_markup_escape_text( g->pl.no_aplica[k], -1 );

            g_string_append_printf( t, "<small>%s</small>\n", m );
            g_free( m );
            }
    {
    gchar *d = g_markup_escape_text( g->dir, -1 );

    g_string_append_printf( t, "\n<small>Cada contraste es una estimación más: el modelo con el "
        "parámetro fijo en su valor nulo, que fue reestima partiendo del óptimo. Los .inp y los "
        ".out de los restringidos quedan en %s. Shin-Fuller usa Φ = L − L<sub>0</sub> y no el "
        "doble; el DCD, 2(L − L<sub>0</sub>) con su propia ley, que no es la χ².</small>", d );
    g_free( d );
    }
    gtk_label_set_markup( GTK_LABEL(g->l_texto), t->str );
    g_string_free( t, TRUE );
}

static void siguiente( Fm *g );

static void acabo( const EngineResult *r, gpointer data )
{
    Fm *g = data;

    g->corriendo = FALSE;
    if ( g->cerrada )
        { g_free( g->exe ); g_free( g->dir ); g_free( g ); return; }
    /* SOLO EL 0: con un 3 fue no pudo estimar y deja los valores
       iniciales, y su logelf no es el maximo de nada.                  */
    fm_resultado( &g->pl.t[g->cur], g->l_libre, g->pl.nobs, g->pl.s, r->status == 0 );
    g->cur++;
    siguiente( g );
}

static void siguiente( Fm *g )
{
    if ( g->cur < g->pl.n && g->exe )
        {
        const char *argv[2] = { g->pl.t[g->cur].base, NULL };
        gchar      *viejo = g_strdup( g->pl.t[g->cur].inp ), *e;

        /* el .out de una vuelta anterior, fuera: el que se lea sera este */
        strcpy( viejo + strlen( viejo ) - 4, ".out" );
        g_unlink( viejo );
        g_free( viejo );
        e = g_strdup_printf( "Contraste %d de %d: estimando el modelo restringido…",
                                         g->cur + 1, g->pl.n );

        gtk_label_set_text( GTK_LABEL(g->l_estado), e );
        g_free( e );
        g->corriendo = engine_run_async( g->dir, g->exe, argv, NULL, acabo, g );
        if ( !g->corriendo )
            {
            fm_resultado( &g->pl.t[g->cur], g->l_libre, g->pl.nobs, g->pl.s, FALSE );
            g_snprintf( g->pl.t[g->cur].motivo, sizeof g->pl.t[g->cur].motivo,
                        "no pude lanzar %s", g->exe );
            g->cur++;
            siguiente( g );
            return;
            }
        }
    else
        gtk_label_set_text( GTK_LABEL(g->l_estado), g->exe
            ? "Hecho. Cada contraste dice lo suyo; qué hacer con ello lo decides tú."
            : "No encuentro «fue»: ni al lado de esta ventana ni en el PATH." );
    pinta( g );
}

static void on_cerrar( GtkWidget *w, Fm *g )
{
    (void) w;
    /* un fue en marcha llamara a acabo(): ahi se suelta */
    if ( g->corriendo ) { g->cerrada = TRUE; return; }
    g_free( g->exe ); g_free( g->dir ); g_free( g );
}

void an_formales( const AnHost *h, const char *serie, const char *muestra, const char *id )
{
    Fm         *g;
    FueOut     *o;
    Convergence c;
    char        out[PR_RUTA], pre[PR_RUTA], why[256];
    GtkWidget  *raiz, *cab, *sw;

    if ( !h || !h->p || !serie || !*serie || !id || !*id ) return;
    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 ||
         pr_ruta( h->p, serie, muestra, id, ".pre", pre, sizeof pre ) != 0 )
        { an_di( h, "No pude componer la ruta." ); return; }
    if ( !g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { an_di( h, "«%s» no tiene .pre: hay que estimarlo antes de contrastar nada.", id ); return; }

    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) || !o->tiene_logelf )
        { an_di( h, "El .out de «%s» no trae su log-verosimilitud (logelf).", id );
          g_free( o ); return; }

    g = g_new0( Fm, 1 );
    g->h = *h;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );
    g->l_libre = o->logelf;

    /* LA DIAGNOSIS PRIMERO (art BUG-0025): no se impide, se dice. */
    memset( &c, 0, sizeof c );
    if ( !convergence_of( out, &c ) ) memset( &c, 0, sizeof c );
    {
    Dictamen *d = g_new0( Dictamen, 1 );

    dx_dictamen( o, &c, NULL, 0, d );
    if ( d->peor == DX_NO )
        g_snprintf( g->aviso, sizeof g->aviso, "La diagnosis no está limpia. Los contrastes "
                    "formales suponen un modelo adecuado: léelos con esa reserva, o arregla "
                    "antes el modelo." );
    g_free( d );
    }
    convergence_clear( &c );

    g->dir = an_fichero( "formales", serie, muestra, id );
    g_mkdir_with_parents( g->dir, 0700 );
    if ( fm_plan( pre, out, g->dir, o->nobs, o->s > 0 ? o->s : 1, o->d, &g->pl,
                  why, sizeof why ) != 0 )
        { an_di( h, "%s", why ); g_free( o ); g_free( g->dir ); g_free( g ); return; }
    g_free( o );
    g->exe = programa_fue();

    g->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( h->padre ) gtk_window_set_transient_for( GTK_WINDOW(g->win), h->padre );
    gtk_window_set_default_size( GTK_WINDOW(g->win), 860, 600 );
    {
    gchar *ti = g_strdup_printf( "Contrastes formales — %s / %s", serie, id );

    gtk_window_set_title( GTK_WINDOW(g->win), ti );
    g_free( ti );
    }
    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(g->win), raiz );

    cab = gtk_label_new( NULL );
    {
    gchar *m = g_markup_printf_escaped( "<b>%s / %s</b>%s%s   <small>%d observaciones, "
                                        "s = %d, d = %d</small>", serie, id,
                                        ( muestra && *muestra ) ? "   muestra " : "",
                                        ( muestra && *muestra ) ? muestra : "",
                                        g->pl.nobs, g->pl.s, g->pl.d );

    gtk_label_set_markup( GTK_LABEL(cab), m );
    g_free( m );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    g->l_texto = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_texto), 0.0 );
    gtk_label_set_yalign( GTK_LABEL(g->l_texto), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_texto), TRUE );
    gtk_label_set_selectable( GTK_LABEL(g->l_texto), TRUE );
    gtk_widget_set_can_focus( g->l_texto, FALSE );   /* si no, sale todo seleccionado */
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sw), g->l_texto );
    gtk_box_pack_start( GTK_BOX(raiz), sw, TRUE, TRUE, 0 );

    g->l_estado = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_estado), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), g->l_estado, FALSE, FALSE, 0 );

    g_signal_connect( g->win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( g->win );
    siguiente( g );
}
