/*
 * an_factores.c -- la ventana de la factorizacion del AR del modelo
 * estimado. La abre la diagnosis («Factorización del AR…»).
 *
 * AQUI NO SE DECIDE NADA, como en la diagnosis: los numeros salen de
 * lib/raices y esto los escribe con lo que dicen. Que hacer con ello es
 * del analista.
 *
 * Es el ar_factorization de art: las raices de cada factor AR regular, y de
 * cada par complejo su amortiguamiento, su frecuencia y su periodo. Un par
 * con el periodo en s/k y el amortiguamiento cerca de 1 es un AR estacional
 * escondido en un AR(p) libre.
 *
 * NO ES UN CONTRASTE FORMAL, y en art tampoco esta en formal_tests: describe
 * el operador estimado para IDENTIFICAR. Si el par escondido lo es de verdad
 * se contrasta despues (el MEG, con los contrastes formales de verdad, que
 * tendran su propia ventana: docs/TODO.md).
 */

#include <math.h>
#include <string.h>

#include "raices.h"
#include "outfile.h"

#include "analisis.h"

/* «1 − 1.321 B + 0.432 B²», con el signo del fichero: P(B) = 1 − Σ c_j B^j */
static void polinomio( GString *t, const RzAr *a )
{
    int j;

    g_string_append( t, "1" );
    for ( j = 0; j < a->orden; j++ )
        {
        double v = -a->c[j];

        if ( v == 0.0 ) continue;
        g_string_append_printf( t, " %s %.4f B", v < 0 ? "−" : "+", fabs( v ) );
        if ( j > 0 ) g_string_append_printf( t, "<sup>%d</sup>", j + 1 );
        }
}

static void un_factor( GString *t, const RzAr *a, int k, int s )
{
    int i, fijos = 0;

    for ( i = 0; i < a->orden; i++ ) if ( a->idx[i] == 0 ) fijos++;
    g_string_append_printf( t, "<b>Factor AR %d</b>, orden %d%s:  ", k + 1, a->orden,
                            fijos ? ( fijos == 1 ? ", con un coeficiente fijo"
                                                 : ", con coeficientes fijos" ) : "" );
    polinomio( t, a );
    g_string_append( t, "\n" );
    if ( a->nf == 0 )
        { g_string_append( t, "    Sin raíces: todos los coeficientes valen cero.\n\n" ); return; }

    g_string_append( t, "<tt>    raíz       real       imag     módulo\n" );
    for ( i = 0; i < a->orden && ( a->zre[i] != 0.0 || a->zim[i] != 0.0 ); i++ )
        g_string_append_printf( t, "    %3d   %9.5f  %9.5f  %9.5f\n", i + 1,
                                a->zre[i], a->zim[i], hypot( a->zre[i], a->zim[i] ) );
    g_string_append( t, "</tt>" );

    for ( i = 0; i < a->nf; i++ )
        {
        const RzFactor *f = &a->f[i];

        if ( !f->complejo )
            {
            g_string_append_printf( t, "    (1 %s %.5f B): real, raíz %.4f%s\n",
                                    f->a1 < 0 ? "+" : "−", fabs( f->a1 ), 1.0 / f->a1,
                                    f->modulo < 1.0 + 0.05 ? " — <b>cerca de la unidad</b>" : "" );
            continue;
            }
        g_string_append_printf( t, "    (1 %s %.5f B + %.5f B<sup>2</sup>): complejo,  d = %.3f",
                                f->a1 < 0 ? "+" : "−", fabs( f->a1 ), -f->a2, f->r );
        if ( f->con_et ) g_string_append_printf( t, " ± %.3f", f->r_et );
        g_string_append_printf( t, ",  frecuencia %.4f", f->omega / ( 2.0 * M_PI ) );
        if ( s > 1 ) g_string_append_printf( t, " (armónico %.2f de s = %d)", f->armonico, s );
        g_string_append_printf( t, ",  periodo %.2f", f->periodo );
        if ( f->con_et && isfinite( f->periodo_et ) )
            g_string_append_printf( t, " ± %.2f", f->periodo_et );
        if ( isfinite( f->vida_media ) )
            g_string_append_printf( t, ",  vida media %.1f", f->vida_media );
        if ( f->estacional || f->casi_unitaria )
            g_string_append_printf( t, "  — <b>%s%s%s</b>",
                f->estacional ? "en una frecuencia estacional" : "",
                f->estacional && f->casi_unitaria ? ", " : "",
                f->casi_unitaria ? "casi unitario" : "" );
        g_string_append( t, "\n" );
        }
    g_string_append( t, "\n" );
}

void an_factores_ar( const AnHost *h, const char *serie, const char *muestra,
                    const char *id )
{
    FueOut      *o;
    Convergence  c;
    RzFactores  *f;
    GtkWidget   *win, *raiz, *cab, *sw, *l;
    GString     *t;
    char         out[PR_RUTA], why[256];
    int          k, sin_et, s;

    if ( !h || !h->p || !serie || !*serie || !id || !*id ) return;
    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 )
        { an_di( h, "No pude componer la ruta." ); return; }

    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) )
        { an_di( h, "«%s» no está estimado: no hay .out que contrastar.", id ); g_free( o ); return; }
    s = o->s > 0 ? o->s : 1;
    g_free( o );

    /* CERO ITERACIONES: la covarianza es la semilla del optimizador, no el
       hessiano (art BUG-0027). Los ± que saldrian serian ficcion.        */
    memset( &c, 0, sizeof c );
    sin_et = convergence_of( out, &c ) && c.iterations == 0;
    convergence_clear( &c );

    f = g_new0( RzFactores, 1 );
    if ( rz_factores_ar( out, s, sin_et, f, why, sizeof why ) != 0 )
        { an_di( h, "%s", why ); g_free( f ); return; }

    t = g_string_new( "" );
    g_string_append( t, "<big><b>La factorización del AR</b></big>\n"
        "<small>Cada factor AR regular, partido en factores reales (1 − a B) y pares "
        "complejos (1 − a<sub>1</sub> B − a<sub>2</sub> B<sup>2</sup>). De un par: "
        "<b>d</b>, el amortiguamiento (1/|raíz|); la <b>frecuencia</b> en ciclos por "
        "observación; el <b>periodo</b>, en observaciones; la <b>vida media</b>, lo que "
        "tarda la onda en quedarse en la mitad.</small>\n\n" );

    if ( f->nar == 0 )
        g_string_append( t, "El modelo no tiene AR regular: no hay nada que factorizar.\n" );
    for ( k = 0; k < f->nar; k++ ) un_factor( t, &f->ar[k], k, s );

    if ( f->nar > 0 )
        {
        if ( sin_et )
            g_string_append( t, "<b>Sin errores típicos.</b> El optimizador paró en cero "
                "iteraciones (se arrancó en el óptimo): la covarianza del .out es su semilla, "
                "no la del modelo. Los factores son correctos; lo que falta son los ±.\n\n" );
        if ( f->modulos_parecidos )
            g_string_append_printf( t, "<b>Los %d módulos son casi iguales</b> (%.4f–%.4f, "
                "dispersión %.1f %%): es el aspecto que tendría un operador en B<sup>%d</sup>. "
                "<b>Eso es la hipótesis, no el hallazgo</b>: (1 − Θ B<sup>%d</sup>) impone un "
                "amortiguamiento común y clava las frecuencias en 2πk/%d; aquí están libres. "
                "No lo impongas: estima el modelo factorizado para ver cada d con su ± y "
                "contrasta la restricción por razón de verosimilitudes.\n\n",
                f->nmod, f->mod_min, f->mod_max, 100.0 * f->mod_disp, f->nmod, f->nmod, f->nmod );
        g_string_append( t, "<small>Un par complejo con el periodo en s/k (un armónico "
            "entero) y d cerca de 1 es un <b>AR estacional escondido</b> dentro de un AR(p) "
            "libre: candidato a un factor de frecuencia fija. Parecerlo no es serlo: se "
            "contrasta, y eso no lo hace esta ventana. Los ± salen por el método delta y solo para un AR(2) estimado entero; "
            "en un orden mayor los pares salen de las raíces y no tienen covarianza "
            "propia.</small>" );
        }
    g_free( f );

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( h->padre ) gtk_window_set_transient_for( GTK_WINDOW(win), h->padre );
    gtk_window_set_default_size( GTK_WINDOW(win), 820, 520 );
    {
    gchar *ti = g_strdup_printf( "Factorización del AR — %s / %s", serie, id );

    gtk_window_set_title( GTK_WINDOW(win), ti );
    g_free( ti );
    }
    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(win), raiz );

    cab = gtk_label_new( NULL );
    {
    gchar *m = g_markup_printf_escaped( "<b>%s / %s</b>%s%s", serie, id,
                                        ( muestra && *muestra ) ? "   muestra " : "",
                                        ( muestra && *muestra ) ? muestra : "" );

    gtk_label_set_markup( GTK_LABEL(cab), m );
    g_free( m );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l), t->str );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_yalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_label_set_selectable( GTK_LABEL(l), TRUE );
    gtk_widget_set_can_focus( l, FALSE );     /* si no, sale todo seleccionado */
    g_string_free( t, TRUE );

    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sw), l );
    gtk_box_pack_start( GTK_BOX(raiz), sw, TRUE, TRUE, 0 );

    gtk_widget_show_all( win );
}
