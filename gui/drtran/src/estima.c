/*
 * estima.c -- la pantalla de estimacion.
 *
 * Aqui pasa por primera vez algo que en las cuatro anteriores no pasaba: SE
 * LANZA EL MOTOR. Todo lo demas --leer .pre, la CCF, la red, los slots-- lo
 * hace el GUI llamando al codigo del motor dentro de su propio proceso.
 * Estimar no: estimar es drtran, entero, como programa.
 *
 * Y de ahi salen las tres decisiones de esta pantalla.
 *
 * 1. LA ORDEN SE VE. Lo que se va a ejecutar se enseña, completo y
 *    seleccionable, antes de ejecutarlo. No es un adorno: es lo que permite
 *    reproducir en un terminal exactamente lo que el GUI hizo, pegarlo en un
 *    guion, o mandarlo con un informe de error. Un GUI que esconde la orden
 *    convierte cada duda en una arqueologia.
 *
 * 2. LOS ARTEFACTOS SE ESCRIBEN. El motor lee la red y las restricciones de
 *    FICHEROS (-n, -c), asi que lo que hay en las pantallas Red y Modelo tiene
 *    que estar en disco antes de lanzar. Se escriben solos, en el directorio
 *    de trabajo, y se dice donde. Es la regla del diseño: cada pantalla
 *    fabrica lo que la siguiente pide.
 *
 * 3. EL DESENLACE TIENE NOMBRE. TASTE distinguia tres (MRQEST.PAS:372-376) y
 *    era de lo mejor de su diseño. drtran tiene cinco mas el ifault, y esta
 *    pantalla no los pierde: los lee con lib/verdict.
 *
 *    El que hay que entender es el tercero. "STOPPED AT A POINT WITH NO
 *    IMPROVEMENT" suena a fracaso y es lo que sale cuando el .pre YA ERA el
 *    optimo -- que es exactamente la invariante del contrato. Esta pantalla lo
 *    cuenta como exito y explica por que.
 *
 * LO QUE NO SE INVENTA. TASTE dejaba tocar el numero de iteraciones, la
 * tolerancia y la longitud de paso en su formulario (TFEST.PAS:295-300).
 * drtran NO expone esos: maxits = 500, gradtol = steptol = 1e-7, clavados en
 * drtran.c:3256-3257. Asi que aqui se ENSEÑAN como lo que son --el criterio de
 * parada que rige-- y no se ofrece un control que no existe. Inventar una
 * casilla que no llega a ningun sitio seria peor que no tenerla.
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "gui.h"
#include "previewhost.h"
#include "preview.h"
#include "engine.h"
#include "verdict.h"
#include "netfile.h"
#include "slots.h"

/* ------------------------------------------------------------------------ */
/* El directorio de trabajo y los artefactos                                 */
/* ------------------------------------------------------------------------ */

static gchar *trabajo( void )
{
    gchar *d = g_build_filename( g_get_user_cache_dir(), "mtram", NULL );

    g_mkdir_with_parents( d, 0700 );
    return d;
}

/* Escribe la red y las restricciones que hay en las pantallas anteriores.
 * Devuelve TRUE si pudo; deja las rutas en *dag y *cns (nuevas, o NULL si esa
 * parte no hace falta).                                                    */
static gboolean artefactos( Mtram *m, gchar **dag, gchar **cns, char *why,
                            size_t size )
{
    const char *nom[NET_MAX_SER + 1];
    gchar      *d = trabajo();
    int         i;

    *dag = *cns = NULL;

    for (i = 1; i <= m->c.n; i++) nom[i] = m->c.s[i - 1]->ts.name;
    nom[0] = NULL;

    if (m->red.n > 0) {
        *dag = g_build_filename( d, "red.dag", NULL );
        if (net_write( *dag, nom, m->red.lnk, m->red.n,
                       "escrito por mtram desde la pestaña Red" ) != 0) {
            snprintf( why, size, "no pude escribir %s", *dag );
            g_free( d ); return FALSE;
        }
    }

    if (m->mod.vale) {
        char b[256];
        int  dice = 0;

        for (i = 1; i <= m->mod.st.n; i++)
            if (slots_line( &m->mod.st, i, b, sizeof b )) dice++;

        if (dice) {
            *cns = g_build_filename( d, "modelo.cns", NULL );
            if (cns_write( *cns, &m->mod.st,
                           "escrito por mtram desde la pestaña Modelo" ) < 0) {
                snprintf( why, size, "no pude escribir %s", *cns );
                g_free( d ); return FALSE;
            }
        }
    }

    g_free( d );
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* La orden                                                                  */
/* ------------------------------------------------------------------------ */

/* Arma el argv. Devuelve un vector nuevo terminado en NULL; liberar con
 * g_strfreev. out recibe la ruta del .out (nueva).                         */
static gchar **arma_argv( Mtram *m, const gchar *dag, const gchar *cns,
                          gchar **out )
{
    GPtrArray *a = g_ptr_array_new();
    gchar     *d = trabajo();
    int        i;

    /* Los .pre, la salida primero: es el orden que fija los indices. */
    for (i = 0; i < m->c.n; i++)
        g_ptr_array_add( a, g_strdup( m->c.s[i]->path ) );

    *out = g_build_filename( d, "modelo.out", NULL );
    g_ptr_array_add( a, g_strdup( "-o" ) );
    g_ptr_array_add( a, g_strdup( *out ) );

    if (dag) { g_ptr_array_add( a, g_strdup( "-n" ) );
               g_ptr_array_add( a, g_strdup( dag ) ); }
    if (cns) { g_ptr_array_add( a, g_strdup( "-c" ) );
               g_ptr_array_add( a, g_strdup( cns ) ); }

    /* La prevision y la evaluacion las pide la pestaña Prevision; van en la
     * MISMA corrida porque el motor las hace en la misma pasada.       */
    if (m->prev.prever || m->prev.evaluar) {
        g_ptr_array_add( a, g_strdup( "-f" ) );
        g_ptr_array_add( a, g_strdup_printf( "%d", m->prev.horizonte ) );
    }
    if (m->prev.evaluar && m->prev.ventana > 0) {
        gchar *csv = g_build_filename( d, "evaluacion.csv", NULL );

        g_ptr_array_add( a, g_strdup( "-estwin" ) );
        g_ptr_array_add( a, g_strdup_printf( "%d", m->prev.ventana ) );
        g_ptr_array_add( a, g_strdup( "-C" ) );
        g_ptr_array_add( a, csv );
    }

    /* Lo que la pagina Modelo dice que se MANTIENE del .pre en vez de
     * reestimarlo al juntar. Va aqui porque es una opcion del motor, pero
     * se decide alli, que es donde se ve lo que cuesta.                */
    if (m->mod.fix_N) g_ptr_array_add( a, g_strdup( "-N" ) );
    if (m->mod.fix_X) g_ptr_array_add( a, g_strdup( "-X" ) );
    if (m->mod.fix_D) g_ptr_array_add( a, g_strdup( "-D" ) );
    if (m->mod.fix_E) g_ptr_array_add( a, g_strdup( "-E" ) );
    if (m->mod.fix_M) g_ptr_array_add( a, g_strdup( "-M" ) );

    if (m->est.diagonal)  g_ptr_array_add( a, g_strdup( "-0" ) );
    if (m->est.cast_resta) g_ptr_array_add( a, g_strdup( "-S" ) );
    if (m->est.traza)     g_ptr_array_add( a, g_strdup( "-v" ) );

    g_ptr_array_add( a, NULL );
    g_free( d );
    return (gchar **) g_ptr_array_free( a, FALSE );
}

/* La orden en una linea, para que se pueda copiar y pegar en un terminal. */
static gchar *orden_texto( Mtram *m )
{
    gchar  *dag = NULL, *cns = NULL, *out = NULL, **argv, *s;
    GString *t;
    char    why[256];
    int     i;

    if (m->c.n < 1) return g_strdup( "" );

    /* Sin escribir nada: solo para enseñar la forma. Se usan los nombres que
     * tendran cuando se escriban de verdad al pulsar Estimar.            */
    {
    gchar *d = trabajo();

    if (m->red.n > 0)  dag = g_build_filename( d, "red.dag", NULL );
    if (m->mod.vale) {
        char b[256];
        int  dice = 0;

        for (i = 1; i <= m->mod.st.n; i++)
            if (slots_line( &m->mod.st, i, b, sizeof b )) dice++;
        if (dice) cns = g_build_filename( d, "modelo.cns", NULL );
    }
    g_free( d );
    }
    (void) why;

    argv = arma_argv( m, dag, cns, &out );

    t = g_string_new( "drtran" );
    for (i = 0; argv[i]; i++) {
        gchar *base = g_path_get_basename( argv[i] );

        /* Los nombres largos se acortan para que la linea se lea; la orden que
         * se EJECUTA lleva las rutas enteras.                            */
        g_string_append_printf( t, " %s",
            g_str_has_prefix( argv[i], "-" ) ? argv[i] : base );
        g_free( base );
    }

    s = g_string_free( t, FALSE );
    g_strfreev( argv );
    g_free( out ); g_free( dag ); g_free( cns );
    return s;
}

/* ------------------------------------------------------------------------ */
/* QUE drtran se va a lanzar                                                 */
/*                                                                           */
/* No es un detalle. El motor se lanza por el PATH, y un binario instalado    */
/* hace meses se ejecuta igual de callado que el recien compilado: los        */
/* resultados serian de OTRO programa y nada lo diria. Asi que se enseña cual */
/* es y de cuando, y que el analista juzgue.                                  */
/* ------------------------------------------------------------------------ */

static gchar *quien_es_drtran( void )
{
    gchar     *ruta = g_find_program_in_path( "drtran" );
    GStatBuf   st;
    GDateTime *t;
    gchar     *cuando, *s;

    if (!ruta)
        return g_strdup( "drtran NO está en el PATH. «make install» en "
                         "engines/drtran, o añade su bin/ al PATH." );

    if (g_stat( ruta, &st ) != 0) {
        s = g_strdup_printf( "%s", ruta );
        g_free( ruta );
        return s;
    }

    t = g_date_time_new_from_unix_local( (gint64) st.st_mtime );
    cuando = g_date_time_format( t, "%d/%m/%Y" );
    s = g_strdup_printf( "%s   (del %s)", ruta, cuando );

    g_free( cuando );
    g_date_time_unref( t );
    g_free( ruta );
    return s;
}

/* ------------------------------------------------------------------------ */
/* El desenlace                                                              */
/* ------------------------------------------------------------------------ */

static void cuenta_desenlace( Mtram *m, const EngineResult *r )
{
    Estima      *E = &m->est;
    VerdictInfo  v;
    GString     *t = g_string_new( NULL );

    verdict_parse( r->output, &v );
    E->v = v;

    switch (v.ver) {

    case VER_GRADIENTE:
    case VER_PARAMETRO:
        g_string_append_printf( t, "CONVERGE, por el %s.\n",
            v.ver == VER_GRADIENTE ? "gradiente" : "parámetro" );
        break;

    case VER_SIN_MEJORA:
        g_string_append( t,
            "Se paró sin mejorar — y eso aquí es una BUENA noticia.\n\n"
            "El último paso no encontró ningún punto mejor, que es justo lo "
            "que ocurre cuando\nse arranca YA EN el óptimo. Es la invariante "
            "del contrato: corre el motor sobre\nun .pre y los números no se "
            "mueven. El titular del motor suena a fracaso;\nlo que describe "
            "es que no había nada que mejorar.\n" );
        break;

    case VER_ITERACIONES:
        g_string_append( t,
            "NO CONVERGE: agotó el límite de iteraciones.\n\n"
            "Las estimaciones que hay en el .out son el último punto "
            "visitado, no un óptimo.\nMirar: ¿hay parámetros corriéndose a un "
            "extremo? ¿dos cosas explicando lo mismo\n(un enlace "
            "contemporáneo y su covarianza libre)? ¿sobra estructura?\n" );
        break;

    case VER_PASOS:
        g_string_append( t,
            "NO CONVERGE: cinco pasos seguidos de longitud máxima.\n\n"
            "El optimizador se está yendo por una cresta casi plana: la "
            "verosimilitud apenas\nmejora mientras los parámetros corren. "
            "Suele ser un problema de identificación,\nno de optimizador.\n" );
        break;

    case VER_OTRO:
        g_string_append( t, "El motor dio un desenlace que no reconozco.\n" );
        break;

    case VER_NADA:
        g_string_append( t,
            "No llegó a estimar: no hay veredicto del optimizador.\n"
            "Lo que dijo el motor está abajo, entero.\n" );
        break;
    }

    if (v.frase[0])
        g_string_append_printf( t, "\n   %s\n", v.frase );
    if (v.iters >= 0) {
        if (v.maxits > 0)
            g_string_append_printf( t, "   %d iteraciones de %d.\n",
                                    v.iters, v.maxits );
        else
            g_string_append_printf( t, "   %d iteraciones.\n", v.iters );
    }
    if (v.tiene_logl)
        g_string_append_printf( t, "   log-verosimilitud = %.6f\n", v.logl );

    if (v.ifault)
        g_string_append_printf( t,
            "\nOJO: ifault = %d. El evaluador de la verosimilitud se quejó, "
            "así que las\ndesviaciones típicas que salgan no son de fiar.\n",
            v.ifault );

    if (r->status != 0)
        g_string_append_printf( t, "\n(el motor salió con estado %d)\n",
                                r->status );

    gtk_label_set_text( GTK_LABEL(E->desenlace), t->str );
    g_string_free( t, TRUE );
}

/* ------------------------------------------------------------------------ */
/* Lanzar                                                                    */
/* ------------------------------------------------------------------------ */

static void on_done( const EngineResult *r, gpointer data )
{
    Mtram  *m = data;
    Estima *E = &m->est;

    E->corriendo = FALSE;
    gtk_widget_set_sensitive( E->boton, TRUE );

    /* Lo que dijo por pantalla, entero. */
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->salida) ),
                              r->output ? r->output : "", -1 );

    cuenta_desenlace( m, r );

    /* Los residuos son DE ESTA CORRIDA: la diagnosis se alimenta aqui y no
     * de un sitio global. Es la correccion a lo unico verdaderamente malo
     * del diseño de TASTE -- una sola ranura RESIDUOS, que hacia imposible
     * comparar dos modelos.                                            */
    diagnosis_desde( m, E->out_path );
    prevision_desde( m, E->out_path );

    if (r->status == ENGINE_NORUN)
        preview_show_status( m, "No pude lanzar drtran. ¿Está en el PATH? "
                                "(make install en engines/drtran)" );
    else
        preview_show_status( m, "%s", r->message ? r->message : "" );
}

static void on_estimar( GtkButton *b, Mtram *m )
{
    Estima  *E = &m->est;
    gchar   *dag = NULL, *cns = NULL, *out = NULL, **argv, *d;
    char     why[512];
    GString *aviso;
    int      i;

    if (E->corriendo) return;
    if (m->c.n < 2) {
        preview_show_status( m, "Carga al menos dos .pre." );
        return;
    }

    if (!artefactos( m, &dag, &cns, why, sizeof why )) {
        preview_show_status( m, "%s", why );
        return;
    }

    argv = arma_argv( m, dag, cns, &out );
    g_free( E->out_path );
    E->out_path = g_strdup( out );

    /* Lo que se va a ejecutar, con las rutas enteras: esto es lo que hay que
     * poder copiar a un terminal.                                        */
    aviso = g_string_new( "drtran" );
    for (i = 0; argv[i]; i++) g_string_append_printf( aviso, " %s", argv[i] );
    gtk_label_set_text( GTK_LABEL(E->orden), aviso->str );
    g_string_free( aviso, TRUE );

    gtk_label_set_text( GTK_LABEL(E->desenlace), "Estimando…" );
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->salida) ), "", -1 );

    E->corriendo = TRUE;
    gtk_widget_set_sensitive( E->boton, FALSE );

    d = trabajo();
    if (!engine_run_async( d, "drtran", (const char * const *) argv,
                           NULL, on_done, m )) {
        E->corriendo = FALSE;
        gtk_widget_set_sensitive( E->boton, TRUE );
        gtk_label_set_text( GTK_LABEL(E->desenlace),
            "No pude lanzar drtran.\n\nTiene que estar en el PATH: "
            "«make install» en engines/drtran, o poner bin/ en el PATH." );
    }
    g_free( d );

    g_strfreev( argv );
    g_free( out ); g_free( dag ); g_free( cns );
}

static void on_ver_out( GtkButton *b, Mtram *m )
{
    if (!m->est.out_path) {
        preview_show_status( m, "Todavía no hay resultados." );
        return;
    }
    preview_open_external( m, m->est.out_path );
}

static void on_cambio( GtkToggleButton *b, Mtram *m )
{
    Estima *E = &m->est;

    E->diagonal   = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(E->c_diag) );
    E->cast_resta = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(E->c_resta) );
    E->traza      = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(E->c_traza) );
    estima_refresca( m );
}

/* ------------------------------------------------------------------------ */

void estima_refresca( Mtram *m )
{
    Estima *E = &m->est;
    gchar  *s;

    if (E->corriendo) return;

    s = orden_texto( m );
    gtk_label_set_text( GTK_LABEL(E->orden), s );
    g_free( s );

    /* Qué se va a estimar, dicho antes de estimarlo. */
    {
    GString *t = g_string_new( NULL );

    if (m->c.n < 2)
        g_string_append( t, "Carga al menos dos .pre en la pestaña Series." );
    else {
        int libres = m->mod.vale ? slots_nfree( &m->mod.st ) : 0;

        g_string_append_printf( t,
            "%d series, %d enlace%s, %d parámetro%s libre%s.",
            m->c.n, m->red.n, m->red.n == 1 ? "" : "s",
            libres, libres == 1 ? "" : "s", libres == 1 ? "" : "s" );

        if (E->diagonal)
            g_string_append( t,
                "\n\nDIAGONAL (-0): sin transferencia. Estima los univariantes "
                "conjuntamente, y\ntiene que reproducir fue corrido sobre cada "
                "serie por separado — es la\nhomologación, y el primer paso "
                "del método." );
        else if (m->red.n == 0)
            g_string_append( t,
                "\n\nSin enlaces no hay transferencia que estimar. Define la "
                "red, o marca Diagonal." );

        g_string_append( t,
            "\n\nCriterio de parada del motor: 500 iteraciones como máximo, "
            "tolerancias 1e-7\nen gradiente y en paso. No son ajustables desde "
            "la línea de órdenes." );

        {
        gchar *quien = quien_es_drtran();

        g_string_append_printf( t, "\n\nSe va a lanzar:  %s", quien );
        g_free( quien );
        }
    }
    gtk_label_set_text( GTK_LABEL(E->que), t->str );
    g_string_free( t, TRUE );
    }
}

GtkWidget *estima_pagina_new( Mtram *m )
{
    Estima    *E = &m->est;
    GtkWidget *caja, *barra, *b, *marco, *vb, *sc;

    E->corriendo = FALSE;
    E->out_path  = NULL;
    E->diagonal = E->cast_resta = E->traza = FALSE;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    /* --- los botones y las opciones --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    E->boton = gtk_button_new_with_label( "Estimar" );
    gtk_widget_set_tooltip_text( E->boton,
        "Lanza drtran. La red y las restricciones se escriben antes, solas, "
        "en el directorio de trabajo." );
    g_signal_connect( E->boton, "clicked", G_CALLBACK(on_estimar), m );
    gtk_box_pack_start( GTK_BOX(barra), E->boton, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Ver el .out" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_ver_out), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

#define CASILLA(campo, txt, tip) \
    E->campo = gtk_check_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( E->campo, tip ); \
    g_signal_connect( E->campo, "toggled", G_CALLBACK(on_cambio), m ); \
    gtk_box_pack_start( GTK_BOX(barra), E->campo, FALSE, FALSE, 0 );

    CASILLA( c_diag, "Diagonal (-0)",
             "Sin transferencia: los univariantes estimados juntos. Tiene que "
             "reproducir fue serie a serie — es la homologación." )
    CASILLA( c_resta, "Cast por resta (-S)",
             "El cast antiguo. Por omisión se EMPOTRA (-V), que es exacto; "
             "sólo hace falta -S cuando los operadores ∇ son incompatibles, y "
             "en ese caso el motor lo despacha solo." )
    CASILLA( c_traza, "Traza (-v)", "La traza del optimizador." )
#undef CASILLA

    /* --- qué se va a estimar --- */
    marco = gtk_frame_new( "Qué se va a estimar" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    E->que = gtk_label_new( "" );
    gtk_widget_set_halign( E->que, GTK_ALIGN_START );
    gtk_label_set_line_wrap( GTK_LABEL(E->que), TRUE );
    gtk_container_add( GTK_CONTAINER(vb), E->que );
    gtk_container_add( GTK_CONTAINER(marco), vb );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    /* --- la orden, copiable --- */
    marco = gtk_frame_new( "La orden" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    E->orden = gtk_label_new( "" );
    gtk_widget_set_halign( E->orden, GTK_ALIGN_START );
    gtk_label_set_selectable( GTK_LABEL(E->orden), TRUE );
    gtk_label_set_line_wrap( GTK_LABEL(E->orden), TRUE );
    gtk_label_set_line_wrap_mode( GTK_LABEL(E->orden), PANGO_WRAP_WORD_CHAR );
    gtk_widget_set_tooltip_text( E->orden,
        "Se puede copiar y pegar en un terminal: es exactamente lo que mtram "
        "ejecuta." );
    gtk_container_add( GTK_CONTAINER(vb), E->orden );
    gtk_container_add( GTK_CONTAINER(marco), vb );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    /* --- el desenlace --- */
    marco = gtk_frame_new( "Cómo acabó" );
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vb), 6 );
    E->desenlace = gtk_label_new( "Sin estimar todavía." );
    gtk_widget_set_halign( E->desenlace, GTK_ALIGN_START );
    gtk_label_set_selectable( GTK_LABEL(E->desenlace), TRUE );
    gtk_label_set_line_wrap( GTK_LABEL(E->desenlace), TRUE );
    gtk_container_add( GTK_CONTAINER(vb), E->desenlace );
    gtk_container_add( GTK_CONTAINER(marco), vb );
    gtk_box_pack_start( GTK_BOX(caja), marco, FALSE, FALSE, 0 );

    /* --- lo que dijo el motor, entero --- */
    E->salida = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->salida), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(E->salida), TRUE );
    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), E->salida );
    marco = gtk_frame_new( "Lo que dijo el motor" );
    gtk_container_add( GTK_CONTAINER(marco), sc );
    gtk_box_pack_start( GTK_BOX(caja), marco, TRUE, TRUE, 0 );

    return caja;
}
