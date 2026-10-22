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

#include <glib/gstdio.h>

#include "raices.h"
#include "outfile.h"
#include "inpcheck.h"

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

/* COMO EL ROOT DE LA UCM: primero las raíces, luego los factores agrupados
 * --los reales juntos, los complejos juntos--, cada grupo en su tabla y con
 * cuántos son. Antes salían intercalados en el orden de las raíces, con
 * cada par en una línea de texto corrido, y no se podían comparar.
 * Dentro de cada grupo, por módulo: el más cercano a la unidad primero. */
static void un_factor( GString *t, const RzAr *a, int k, int s )
{
    int i, fijos = 0, nr = 0, nc = 0, n;

    for ( i = 0; i < a->orden; i++ ) if ( a->idx[i] == 0 ) fijos++;
    for ( i = 0; i < a->nf; i++ ) if ( a->f[i].complejo ) nc++; else nr++;
    g_string_append_printf( t, "<b>Factor AR %d</b>, orden %d%s:  ", k + 1, a->orden,
                            fijos ? ( fijos == 1 ? ", con un coeficiente fijo"
                                                 : ", con coeficientes fijos" ) : "" );
    polinomio( t, a );
    g_string_append( t, "\n" );
    if ( a->nf == 0 )
        { g_string_append( t, "    Sin raíces: todos los coeficientes valen cero.\n\n" ); return; }

    g_string_append( t, "<tt>\n    RAÍZ       REAL       IMAG     MÓDULO\n" );
    for ( i = 0; i < a->orden && ( a->zre[i] != 0.0 || a->zim[i] != 0.0 ); i++ )
        g_string_append_printf( t, "    %4d  %9.5f  %9.5f  %9.5f\n", i + 1,
                                a->zre[i], a->zim[i], hypot( a->zre[i], a->zim[i] ) );

    if ( nr > 0 )
        {
        g_string_append_printf( t, "\n</tt><b>Factores reales</b> (1 − a B): %d<tt>\n"
                                "    FACTOR          a       RAÍZ\n", nr );
        for ( i = 0, n = 0; i < a->nf; i++ )
            {
            const RzFactor *f = &a->f[i];

            if ( f->complejo ) continue;
            g_string_append_printf( t, "    %6d  %9.5f  %9.4f%s\n", ++n, f->a1, 1.0 / f->a1,
                                    f->modulo < 1.0 + 0.05 ? "   <b>cerca de la unidad</b>" : "" );
            }
        }
    if ( nc > 0 )
        {
        int hay_et = 0;

        /* las columnas de los ± solo si alguno los tiene (un AR(2) entero) */
        for ( i = 0; i < a->nf; i++ ) hay_et |= a->f[i].complejo && a->f[i].con_et;
        g_string_append_printf( t, "\n</tt><b>Factores complejos</b> (1 − a<sub>1</sub> B − "
                                "a<sub>2</sub> B<sup>2</sup>): %d<tt>\n", nc );
        g_string_append_printf( t, "    %6s  %9s  %9s  %7s%s  %6s%s  %8s%s  %8s\n",
                                "FACTOR", "a1", "a2", "d", hay_et ? "        " : "", "FREC",
                                s > 1 ? "  ARMÓN." : "", "PERIODO", hay_et ? "        " : "",
                                "VIDA M." );
        for ( i = 0, n = 0; i < a->nf; i++ )
            {
            const RzFactor *f = &a->f[i];
            char            vm[16] = "";

            if ( !f->complejo ) continue;
            if ( isfinite( f->vida_media ) ) g_snprintf( vm, sizeof vm, "%.1f", f->vida_media );
            g_string_append_printf( t, "    %6d  %9.5f  %9.5f  %7.3f", ++n, f->a1, f->a2, f->r );
            if ( hay_et )
                {
                if ( f->con_et ) g_string_append_printf( t, " ± %5.3f", f->r_et );
                else g_string_append( t, "        " );
                }
            g_string_append_printf( t, "  %6.4f", f->omega / ( 2.0 * M_PI ) );
            if ( s > 1 ) g_string_append_printf( t, "  %6.2f", f->armonico );
            g_string_append_printf( t, "  %8.2f", f->periodo );
            if ( hay_et )
                {
                if ( f->con_et && isfinite( f->periodo_et ) )
                    g_string_append_printf( t, " ± %5.2f", f->periodo_et );
                else g_string_append( t, "        " );
                }
            g_string_append_printf( t, "  %8s", vm );
            if ( f->estacional || f->casi_unitaria )
                g_string_append_printf( t, "   <b>%s%s%s</b>",
                    f->estacional ? "estacional" : "",
                    f->estacional && f->casi_unitaria ? ", " : "",
                    f->casi_unitaria ? "casi unitario" : "" );
            g_string_append( t, "\n" );
            }
        }
    g_string_append( t, "</tt>\n" );
}

/* --- derivar el modelo factorizado -------------------------------------- */

typedef struct {
    AnHost     h;
    char       serie[PR_ID], muestra[PR_ID], id[PR_ID];
    int        s;
    GtkWidget *win;
} Fa;

/* El fichero del padre: su .pre si lo hay (el óptimo), si no su .inp. */
static int origen_de( const Fa *g, char *out, size_t n )
{
    char pre[PR_RUTA];

    if ( pr_ruta( g->h.p, g->serie, g->muestra, g->id, ".pre", pre, sizeof pre ) == 0 &&
         g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { g_snprintf( out, n, "%s", pre ); return 1; }
    if ( pr_ruta( g->h.p, g->serie, g->muestra, g->id, ".inp", out, n ) != 0 ) return 0;
    return g_file_test( out, G_FILE_TEST_EXISTS ) ? 2 : 0;
}

/* EL MODELO FACTORIZADO, como hijo del modelo (pr_deriva), juzgado por fue
 * y abierto para estimarlo: lo que hacen el identificador y el MEG al
 * derivar. Lo escribe lib/raices (rz_deriva_factorizado). */
static void on_derivar( GtkButton *b, Fa *g )
{
    char          origen[PR_RUTA], destino[PR_RUTA], nuevo[PR_ID], msg[512], why[256];
    PrError       e;
    gchar        *dir, *razon, *aviso;
    int           cual, partidos = 0, fijos = 0;
    AnHerramienta con;

    (void) b;
    cual = origen_de( g, origen, sizeof origen );
    if ( !cual ) { an_di( &g->h, "No encuentro el fichero de «%s».", g->id ); return; }
    if ( pr_deriva( g->h.p, g->serie, g->muestra, g->id, nuevo, sizeof nuevo,
                    destino, sizeof destino, &e ) != 0 )
        { pr_error_es( &e, msg, sizeof msg ); an_di( &g->h, "%s", msg ); return; }
    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
    if ( rz_deriva_factorizado( origen, destino, g->s, &partidos, &fijos, why, sizeof why ) != 0 )
        {
        pr_borra( g->h.p, g->serie, g->muestra, nuevo, &e );
        an_di( &g->h, "%s", why );
        return;
        }
    /* EL JUEZ ES EL MOTOR: un .inp que fue no acepta se deshace */
    if ( inp_check_fue( destino, msg, sizeof msg ) != 0 )
        {
        g_unlink( destino );
        pr_borra( g->h.p, g->serie, g->muestra, nuevo, &e );
        an_di( &g->h, "Lo que salió no lo acepta fue: %s", msg );
        return;
        }
    razon = g_strdup_printf( "Modelo factorizado: %s del AR regular partido%s en sus factores "
        "reales (1 − a B) y sus pares complejos (1 − a1 B − a2 B²), libres y sembrados con la "
        "factorización de %s.%s", partidos == 1 ? "el factor" : "los factores",
        partidos == 1 ? "" : "s", cual == 1 ? "su óptimo" : "su especificación",
        fijos ? " Tenía coeficientes fijos: partido, esa restricción ya no está." : "" );
    pr_razon( g->h.p, g->serie, g->muestra, nuevo, razon, &e );
    g_free( razon );
    con = ( g->h.puede && !( g->h.puede & AN_PUEDE_FUE ) ) ? AN_CON_EDITOR : AN_CON_FUE;
    pr_pon_herramienta( g->h.p, con == AN_CON_EDITOR );
    if ( g->h.guarda && g->h.guarda( g->h.dueno ) != 0 )
        an_di( &g->h, "El .inp está en %s, pero no pude guardar el proyecto.", nuevo );
    if ( g->h.refresca ) g->h.refresca( g->h.dueno );
    {
    AnHost host = g->h;
    char   serie[PR_ID], muestra[PR_ID], padre[PR_ID];

    g_snprintf( serie, sizeof serie, "%s", g->serie );
    g_snprintf( muestra, sizeof muestra, "%s", g->muestra );
    g_snprintf( padre, sizeof padre, "%s", g->id );
    aviso = g_strdup_printf( "%s nace de %s con el AR factorizado. Estímalo: %scada par sale "
                             "con su ± y se puede fijar o quitar uno a uno.", nuevo, padre,
                             fijos ? "" : "la verosimilitud tiene que ser la misma, y " );
    gtk_widget_destroy( g->win );
    if ( host.cierra && cual ) host.cierra( host.dueno, serie, muestra, padre );
    if ( host.abre ) host.abre( host.dueno, serie, muestra, nuevo, con );
    an_di( &host, "%s", aviso );
    g_free( aviso );
    }
}

static void on_cerrar( GtkWidget *w, Fa *g )
{
    (void) w;
    g_free( g );
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
    int          k, sin_et, s, partible = 0;

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
        g_string_append( t, "<small>«Derivar el modelo factorizado» escribe un hijo con cada "
            "factor partido así, libres y sembrados con estos números: estimado, cada par sale "
            "con su ± y se puede fijar o quitar uno a uno. ");
        g_string_append( t, "Un par complejo con el periodo en s/k (un armónico "
            "entero) y d cerca de 1 es un <b>AR estacional escondido</b> dentro de un AR(p) "
            "libre: candidato a un factor de frecuencia fija. Parecerlo no es serlo: se "
            "contrasta, y eso no lo hace esta ventana. Los ± salen por el método delta y solo para un AR(2) estimado entero; "
            "en un orden mayor los pares salen de las raíces y no tienen covarianza "
            "propia.</small>" );
        }
    for ( k = 0; k < f->nar; k++ ) partible |= f->ar[k].nf > 1;
    g_free( f );

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( h->padre ) gtk_window_set_transient_for( GTK_WINDOW(win), h->padre );
    gtk_window_set_default_size( GTK_WINDOW(win), 980, 560 );
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

    /* derivar: solo si el anfitrion sabe abrir el hijo y guardar */
    if ( h->abre && h->guarda )
        {
        GtkWidget *barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
        GtkWidget *b = gtk_button_new_with_label( "Derivar el modelo factorizado…" );
        Fa        *g = g_new0( Fa, 1 );

        g->h = *h;
        g_snprintf( g->serie, sizeof g->serie, "%s", serie );
        g_snprintf( g->muestra, sizeof g->muestra, "%s", muestra ? muestra : "" );
        g_snprintf( g->id, sizeof g->id, "%s", id );
        g->s   = s;
        g->win = win;
        gtk_widget_set_sensitive( b, partible );
        gtk_widget_set_tooltip_text( b, partible
            ? "Un hijo de este modelo con cada factor AR partido en sus factores reales "
              "(1 − a B) y sus pares complejos (1 − a1 B − a2 B²), libres y sembrados con "
              "la factorización, para estimarlo."
            : "Cada factor AR ya es irreducible: el modelo ya está factorizado del todo." );
        g_signal_connect( b, "clicked", G_CALLBACK(on_derivar), g );
        g_signal_connect( win, "destroy", G_CALLBACK(on_cerrar), g );
        gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );
        }

    gtk_widget_show_all( win );
}
