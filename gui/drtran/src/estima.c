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

    /* Lo que se MANTIENE del .pre en vez de reestimarlo al juntar. Es de
     * aqui: dice COMO SE ESTIMA, no que es el modelo -- igual que el cast. */
    if (m->est.fix_N) g_ptr_array_add( a, g_strdup( "-N" ) );
    if (m->est.fix_X) g_ptr_array_add( a, g_strdup( "-X" ) );
    if (m->est.fix_D) g_ptr_array_add( a, g_strdup( "-D" ) );
    if (m->est.fix_E) g_ptr_array_add( a, g_strdup( "-E" ) );
    if (m->est.fix_M) g_ptr_array_add( a, g_strdup( "-M" ) );

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

/* El titular del desenlace: UNA linea. Lo largo va al panel. */
static void cuenta_desenlace( Mtram *m, const EngineResult *r )
{
    Estima      *E = &m->est;
    VerdictInfo  v;
    GString     *t = g_string_new( NULL );
    const char  *color = MT_AMBAR;

    verdict_parse( r->output, &v );
    E->v = v;

    switch (v.ver) {

    case VER_GRADIENTE:
    case VER_PARAMETRO:
        color = MT_VERDE;
        g_string_append_printf( t, "CONVERGE por el %s",
            v.ver == VER_GRADIENTE ? "gradiente" : "parámetro" );
        break;

    /* El que hay que entender: suena a fracaso y es que el .pre YA ERA el
     * optimo, o sea la invariante del contrato.                        */
    case VER_SIN_MEJORA:
        color = MT_VERDE;
        g_string_append( t, "Se paró sin mejorar — el .pre YA ERA el óptimo" );
        break;

    case VER_ITERACIONES:
        color = MT_ROJO;
        g_string_append( t, "NO CONVERGE: agotó el límite de iteraciones" );
        break;

    case VER_PASOS:
        color = MT_ROJO;
        g_string_append( t, "NO CONVERGE: cinco pasos de longitud máxima" );
        break;

    case VER_OTRO:
        g_string_append( t, "Desenlace no reconocido" );
        break;

    case VER_NADA:
        color = MT_ROJO;
        g_string_append( t, r->status == ENGINE_SIGNAL
            ? "Detenido" : "No llegó a estimar" );
        break;
    }

    if (v.iters >= 0) {
        if (v.maxits > 0)
            g_string_append_printf( t, " · %d iteraciones de %d", v.iters, v.maxits );
        else
            g_string_append_printf( t, " · %d iteraciones", v.iters );
    }
    if (v.tiene_logl)
        g_string_append_printf( t, " · logL %.6f", v.logl );
    if (v.ifault)
        g_string_append_printf( t, " · ifault %d: las D.T. no son de fiar",
                                v.ifault );
    if (r->status != 0 && r->status != ENGINE_SIGNAL)
        g_string_append_printf( t, " · el motor salió con %d", r->status );

    mtram_verdicto( E->ver_fin, color, "%s", t->str );
    g_string_free( t, TRUE );
}

/* Y la explicacion larga, a su panel. */
static void on_desenlace( GtkButton *b, Mtram *m )
{
    const VerdictInfo *v = &m->est.v;
    GString           *t = g_string_new( NULL );

    switch (v->ver) {

    case VER_GRADIENTE:
    case VER_PARAMETRO:
        g_string_append_printf( t,
            "CONVERGE, por el criterio del %s.\n\n"
            "El motor tiene dos criterios de parada y los distingue: el del\n"
            "gradiente y el del parámetro. Los dos son convergencia.",
            v->ver == VER_GRADIENTE ? "gradiente" : "parámetro" );
        break;

    case VER_SIN_MEJORA:
        g_string_append( t,
            "Se paró sin mejorar — y eso aquí es una BUENA noticia.\n\n"
            "El último paso no encontró ningún punto mejor, que es justo lo\n"
            "que ocurre cuando se arranca YA EN el óptimo. Es la invariante\n"
            "del contrato: corre el motor sobre un .pre y los números no se\n"
            "mueven.\n\n"
            "El titular del motor suena a fracaso —«STOPPED AT A POINT WITH\n"
            "NO IMPROVEMENT»— y lo que describe es que no había nada que\n"
            "mejorar. Confundirlo con un fallo es confundir el éxito con el\n"
            "fracaso." );
        break;

    case VER_ITERACIONES:
        g_string_append( t,
            "NO CONVERGE: agotó el límite de iteraciones.\n\n"
            "Las estimaciones del .out son el último punto visitado, NO un\n"
            "óptimo.\n\n"
            "Qué mirar:\n"
            "   ¿hay parámetros corriéndose a un extremo?\n"
            "   ¿dos cosas explicando lo mismo — un enlace contemporáneo y\n"
            "    su covarianza libre? (pestaña Modelo, «Avisos…»)\n"
            "   ¿sobra estructura?" );
        break;

    case VER_PASOS:
        g_string_append( t,
            "NO CONVERGE: cinco pasos seguidos de longitud máxima.\n\n"
            "El optimizador se está yendo por una cresta casi plana: la\n"
            "verosimilitud apenas mejora mientras los parámetros corren.\n"
            "Suele ser un problema de IDENTIFICACIÓN, no de optimizador." );
        break;

    case VER_NADA:
        g_string_append( t,
            "No hay veredicto del optimizador: no llegó a estimar, o se\n"
            "detuvo antes. Lo que dijo el motor está en la caja de abajo,\n"
            "entero." );
        break;

    default:
        g_string_append( t, "El motor dio un desenlace que no reconozco." );
        break;
    }

    if (v->frase[0])
        g_string_append_printf( t, "\n\nLo que escribió el motor:\n   %s",
                                v->frase );

    g_string_append( t,
        "\n\n——\nCriterio de parada del motor: 500 iteraciones como máximo,\n"
        "tolerancias 1e-7 en gradiente y en paso. NO son ajustables desde la\n"
        "línea de órdenes (drtran.c:3256-3257), así que no se ofrece una\n"
        "casilla que no llegaría a ningún sitio." );

    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
    g_string_free( t, TRUE );
}

/* ------------------------------------------------------------------------ */
/* Lanzar                                                                    */
/* ------------------------------------------------------------------------ */

/* La barra va EN PULSO, no con porcentaje: no hay forma de saber el avance.
 * drtran no imprime nada por iteracion --con -v la salida crece en UNA linea,
 * 36 frente a 35-- asi que un porcentaje seria inventado. El pulso dice
 * "trabajando" sin fingir que sabe cuanto queda.                         */
static void carga_informe( Mtram *m );

static gboolean late( gpointer d )
{
    Mtram *m = d;

    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(m->est.barra) );
    return G_SOURCE_CONTINUE;
}

static void para_pulso( Mtram *m )
{
    if (m->est.pulso) { g_source_remove( m->est.pulso ); m->est.pulso = 0; }
    gtk_widget_hide( m->est.barra );
}

/* La salida, SEGUN LLEGA. */
static void on_salida( const char *txt, gsize len, gpointer data )
{
    Mtram         *m = data;
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->est.salida) );
    GtkTextIter    fin;

    gtk_text_buffer_get_end_iter( b, &fin );
    gtk_text_buffer_insert( b, &fin, txt, (gint) len );

    /* que se vea lo ultimo */
    gtk_text_buffer_get_end_iter( b, &fin );
    gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW(m->est.salida), &fin,
                                  0.0, FALSE, 0.0, 0.0 );
}

static void on_done( const EngineResult *r, gpointer data )
{
    Mtram  *m = data;
    Estima *E = &m->est;

    E->corriendo = FALSE;
    E->trabajo   = NULL;
    para_pulso( m );
    gtk_widget_set_sensitive( E->boton, TRUE );
    gtk_widget_set_sensitive( E->b_parar, FALSE );
    gtk_label_set_text( GTK_LABEL(E->titulo),
        r->status == ENGINE_SIGNAL ? "drtran — detenido" : "drtran — terminado" );

    /* La salida ya se fue pintando en vivo; solo se completa si algo falto
     * --por ejemplo lo que fue a stderr, que no pasa por on_salida--.   */
    {
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->salida) );
    GtkTextIter    a, z;
    gchar         *hay;

    gtk_text_buffer_get_bounds( b, &a, &z );
    hay = gtk_text_buffer_get_text( b, &a, &z, FALSE );
    if (r->output && strlen( r->output ) > strlen( hay ))
        gtk_text_buffer_set_text( b, r->output, -1 );
    g_free( hay );
    }

    cuenta_desenlace( m, r );

    /* El informe, en su pestaña, y alli se salta: es el resultado. */
    carga_informe( m );
    if (verdict_ok( &E->v ))
        gtk_notebook_set_current_page( GTK_NOTEBOOK(E->libreta), 1 );

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

    /* Lo que se va a ejecutar, con las rutas ENTERAS. Va en una ENTRADA y no
     * en una etiqueta: de una etiqueta partida en lineas no se copia una
     * orden, y copiarla es justo para lo que esta.                     */
    aviso = g_string_new( "drtran" );
    for (i = 0; argv[i]; i++) g_string_append_printf( aviso, " %s", argv[i] );
    gtk_entry_set_text( GTK_ENTRY(E->orden), aviso->str );
    g_string_free( aviso, TRUE );

    gtk_label_set_text( GTK_LABEL(E->titulo), "drtran — ejecutando…" );
    gtk_label_set_text( GTK_LABEL(E->ver_fin), "" );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(E->libreta), 0 );
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->salida) ), "", -1 );

    E->corriendo = TRUE;
    gtk_widget_set_sensitive( E->boton, FALSE );
    gtk_widget_set_sensitive( E->b_parar, TRUE );
    gtk_widget_show( E->barra );
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(E->barra) );
    E->pulso = g_timeout_add( 120, late, m );

    d = trabajo();
    E->trabajo = engine_start( d, "drtran", (const char * const *) argv,
                               NULL, on_salida, on_done, m );
    if (!E->trabajo) {
        E->corriendo = FALSE;
        para_pulso( m );
        gtk_widget_set_sensitive( E->boton, TRUE );
        gtk_widget_set_sensitive( E->b_parar, FALSE );
        gtk_label_set_text( GTK_LABEL(E->titulo),
                            "drtran — no se pudo lanzar" );
        mtram_verdicto( E->ver_fin, MT_ROJO,
            "No pude lanzar drtran: tiene que estar en el PATH "
            "(«make install» en engines/drtran)" );
    }
    g_free( d );

    g_strfreev( argv );
    g_free( out ); g_free( dag ); g_free( cns );
}

/* El .out, EN SU PESTAÑA.
 *
 * Antes se abria con el visor del sistema, y eso no tiene sentido: el informe
 * del modelo es el resultado del trabajo, no un adjunto. Va en la segunda
 * pestaña de la libreta, al lado de la consola.                         */
static void carga_informe( Mtram *m )
{
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->est.informe) );
    gchar         *txt = NULL;
    gsize          n = 0;

    if (!m->est.out_path ||
        !g_file_get_contents( m->est.out_path, &txt, &n, NULL )) {
        gtk_text_buffer_set_text( b, "", -1 );
        return;
    }
    /* El .out no tiene por que ser UTF-8 valido: los nombres de serie salen
     * del .pre y pueden traer cualquier cosa.                          */
    if (!g_utf8_validate( txt, n, NULL )) {
        gchar *u = g_locale_to_utf8( txt, n, NULL, NULL, NULL );

        if (u) { g_free( txt ); txt = u; }
        else   { gchar *f = g_utf8_make_valid( txt, n ); g_free( txt ); txt = f; }
    }
    gtk_text_buffer_set_text( b, txt, -1 );
    g_free( txt );
}

static void on_parar( GtkButton *b, Mtram *m )
{
    if (!m->est.corriendo || !m->est.trabajo) return;
    gtk_widget_set_sensitive( m->est.b_parar, FALSE );
    gtk_label_set_text( GTK_LABEL(m->est.titulo), "drtran — deteniendo…" );
    engine_stop( m->est.trabajo );
}




/* ------------------------------------------------------------------------ */

void estima_refresca( Mtram *m )
{
    Estima *E = &m->est;
    gchar  *s;

    if (E->corriendo) return;

    s = orden_texto( m );
    gtk_entry_set_text( GTK_ENTRY(E->orden), s );
    g_free( s );

    /* QUE drtran se va a lanzar, y DE CUANDO. La version no sirve para esto:
     * DRTRAN_VERSION es "1.0", una constante que no cambia nunca y que no
     * distingue el binario de julio del de hoy. La FECHA si.           */
    s = quien_es_drtran();
    gtk_label_set_text( GTK_LABEL(E->motor), s );
    gtk_widget_set_tooltip_text( E->motor, s );
    g_free( s );

    /* Que se va a estimar: UNA linea. */
    if (m->c.n < 2)
        mtram_verdicto( E->ver_que, MT_AMBAR,
            "Carga al menos dos .pre en la pestaña Series." );
    else {
        int libres = m->mod.vale ? slots_nfree( &m->mod.st ) : 0;

        if (E->diagonal)
            mtram_verdicto( E->ver_que, MT_AMBAR,
                "DIAGONAL (-0): sin transferencia · %d series · %d libres · "
                "es la homologación con fue y tiene que reproducirlo serie a "
                "serie", m->c.n, libres );
        else if (m->red.n == 0)
            mtram_verdicto( E->ver_que, MT_AMBAR,
                "%d series y NINGÚN enlace: no hay transferencia que estimar "
                "· define la red, o marca Diagonal", m->c.n );
        else
            mtram_verdicto( E->ver_que, MT_VERDE,
                "%d series · %d enlace%s · %d parámetros libres · cast %s",
                m->c.n, m->red.n, m->red.n == 1 ? "" : "s", libres,
                E->cast_resta ? "−S por resta" : "−V empotrado" );
    }
}

/* ------------------------------------------------------------------------ */
/* UN SOLO sitio para las opciones                                           */
/*                                                                           */
/* El cast y lo que se mantiene del .pre son la misma clase de cosa: dicen    */
/* COMO SE ESTIMA. Tenerlas en dos botones distintos --"Opciones" y           */
/* "Mantener"-- las presentaba como si fueran sistemas separados, y no lo     */
/* son. Un solo dialogo, con sus dos bloques.                                 */
/* ------------------------------------------------------------------------ */

static GtkWidget *bloque( GtkWidget *caja, const char *titulo )
{
    GtkWidget *l = gtk_label_new( NULL );
    gchar     *mk = g_strdup_printf( "<b>%s</b>", titulo );

    gtk_label_set_markup( GTK_LABEL(l), mk );
    g_free( mk );
    gtk_widget_set_halign( l, GTK_ALIGN_START );
    gtk_widget_set_margin_top( l, 6 );
    gtk_container_add( GTK_CONTAINER(caja), l );
    return l;
}

static void on_opciones( GtkButton *bt, Mtram *m )
{
    Estima    *E = &m->est;
    GtkWidget *d, *caja, *c_resta, *c_traza, *c[5], *av;
    static const struct { const char *txt, *tip; } OP[5] = {
      { "-N   el ARMA del ruido de la SALIDA",
        "Los phi y theta de la primera serie: se mantienen los del .pre." },
      { "-X   el ARMA de las ENTRADAS",
        "Los phi y theta de las demás series." },
      { "-D   los deterministas de la SALIDA", "" },
      { "-E   los deterministas de las ENTRADAS", "" },
      { "-M   las medias",
        "Una media que el .pre ya declara FIJA lo está de todos modos; esto "
        "clava además las que estaban libres." },
    };
    gboolean *campo[5];
    int       i;

    campo[0] = &E->fix_N;  campo[1] = &E->fix_X;  campo[2] = &E->fix_D;
    campo[3] = &E->fix_E;  campo[4] = &E->fix_M;

    d = gtk_dialog_new_with_buttons( "Cómo se estima",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL,
            "_Aceptar",  GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    gtk_box_set_spacing( GTK_BOX(caja), 4 );

    /* --- el cast --- */
    bloque( caja, "El cast" );

    c_resta = gtk_check_button_new_with_label( "-S   por resta" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c_resta), E->cast_resta );
    gtk_container_add( GTK_CONTAINER(caja), c_resta );

    av = gtk_label_new(
        "Por omisión el cast es EMPOTRADO (-V) y la verosimilitud es la\n"
        "exacta. El de resta construye el ruido fuera del motor y en t=1\n"
        "necesita valores de la entrada que no existen: los pone a cero.\n\n"
        "OJO: el motor DESPACHA SOLO al cast por resta cuando los operadores\n"
        "∇ de dos series son incompatibles — lo dice al empezar, en la\n"
        "consola. Así que esta casilla puede quedar contradicha, y no es un\n"
        "fallo: es que el empotrado no puede representar esa relación de\n"
        "niveles (BUG-8)." );
    gtk_widget_set_halign( av, GTK_ALIGN_START );
    gtk_container_add( GTK_CONTAINER(caja), av );

    /* --- lo que se mantiene del .pre --- */
    bloque( caja, "Qué se mantiene del .pre" );

    av = gtk_label_new(
        "Al juntar varios univariantes, sus parámetros pueden dejarse correr\n"
        "— y se mueven, porque ahora hay covarianzas — o clavarse en lo que\n"
        "fue dijo. Marcar es MANTENER. Lo que el .pre ya declare FIJO lo está\n"
        "de todos modos: esto sólo añade.\n"
        "La pestaña Modelo enseña lo que cuesta cada casilla." );
    gtk_widget_set_halign( av, GTK_ALIGN_START );
    gtk_container_add( GTK_CONTAINER(caja), av );

    for (i = 0; i < 5; i++) {
        c[i] = gtk_check_button_new_with_label( OP[i].txt );
        if (OP[i].tip[0]) gtk_widget_set_tooltip_text( c[i], OP[i].tip );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c[i]), *campo[i] );
        gtk_container_add( GTK_CONTAINER(caja), c[i] );
    }

    /* --- la traza --- */
    bloque( caja, "Traza" );
    c_traza = gtk_check_button_new_with_label( "-v   traza del optimizador" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c_traza), E->traza );
    gtk_widget_set_tooltip_text( c_traza,
        "Añade poco: drtran no imprime por iteración. De ahí que la barra "
        "vaya en pulso y no con porcentaje." );
    gtk_container_add( GTK_CONTAINER(caja), c_traza );

    gtk_widget_show_all( d );
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        E->cast_resta = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(c_resta) );
        E->traza      = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(c_traza) );
        for (i = 0; i < 5; i++)
            *campo[i] = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(c[i]) );
        gtk_widget_destroy( d );
        mtram_refresca( m );          /* la cuenta de Modelo cambia */
        return;
    }
    gtk_widget_destroy( d );
}

static void on_diagonal( GtkToggleButton *b, Mtram *m )
{
    m->est.diagonal = gtk_toggle_button_get_active( b );
    estima_refresca( m );
}

GtkWidget *estima_pagina_new( Mtram *m )
{
    Estima    *E = &m->est;
    GtkWidget *caja, *barra, *b, *sc, *vb, *marco;

    E->corriendo = FALSE;
    E->out_path  = NULL;
    E->trabajo   = NULL;
    E->pulso     = 0;
    E->diagonal = E->cast_resta = E->traza = FALSE;
    E->fix_N = E->fix_X = E->fix_D = E->fix_E = E->fix_M = FALSE;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    /* --- los botones --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    E->boton = gtk_button_new_with_label( "Estimar" );
    gtk_widget_set_tooltip_text( E->boton,
        "Lanza drtran. La red y las restricciones se escriben antes, solas, "
        "en el directorio de trabajo." );
    g_signal_connect( E->boton, "clicked", G_CALLBACK(on_estimar), m );
    gtk_box_pack_start( GTK_BOX(barra), E->boton, FALSE, FALSE, 0 );

    E->b_parar = gtk_button_new_with_label( "Detener" );
    gtk_widget_set_tooltip_text( E->b_parar,
        "Para la corrida. Hace falta de verdad con la evaluación fuera de "
        "muestra, que son muchas estimaciones seguidas." );
    gtk_widget_set_sensitive( E->b_parar, FALSE );
    g_signal_connect( E->b_parar, "clicked", G_CALLBACK(on_parar), m );
    gtk_box_pack_start( GTK_BOX(barra), E->b_parar, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    /* Diagonal NO es una opcion: es un MODO. Es la homologacion con fue, el
     * primer paso del metodo, asi que se queda a la vista.             */
    E->c_diag = gtk_check_button_new_with_label( "Diagonal (-0)" );
    gtk_widget_set_tooltip_text( E->c_diag,
        "Sin transferencia: los univariantes estimados juntos. Tiene que "
        "reproducir fue serie a serie — es la homologación, y el primer paso "
        "del método. No es una opción más: es otro modelo." );
    g_signal_connect( E->c_diag, "toggled", G_CALLBACK(on_diagonal), m );
    gtk_box_pack_start( GTK_BOX(barra), E->c_diag, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

#define BOTON(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON( "Opciones…", on_opciones,
           "Cómo se estima: el cast, qué se mantiene del .pre, y la traza." )
    BOTON( "Criterio de parada…", on_desenlace,
           "Qué significa cómo acabó el optimizador, y cuál es su criterio de "
           "parada." )
#undef BOTON

    /* Que binario, y de cuando: a la derecha y pequeño. Es lo unico que caza
     * un drtran instalado hace meses -- la version no, porque es una
     * constante que no cambia.                                        */
    E->motor = gtk_label_new( "" );
    gtk_label_set_ellipsize( GTK_LABEL(E->motor), PANGO_ELLIPSIZE_MIDDLE );
    gtk_widget_set_halign( E->motor, GTK_ALIGN_END );
    gtk_box_pack_end( GTK_BOX(barra), E->motor, FALSE, FALSE, 0 );

    /* --- la orden, EN UNA LINEA Y COPIABLE --- */
    E->orden = gtk_entry_new();
    gtk_editable_set_editable( GTK_EDITABLE(E->orden), FALSE );
    gtk_widget_set_tooltip_text( E->orden,
        "Exactamente lo que mtram ejecuta. Se puede copiar y pegar en un "
        "terminal: Ctrl+A, Ctrl+C." );
    {
    PangoAttrList *al = pango_attr_list_new();

    pango_attr_list_insert( al, pango_attr_family_new( "monospace" ) );
    gtk_entry_set_attributes( GTK_ENTRY(E->orden), al );
    pango_attr_list_unref( al );
    }
    gtk_box_pack_start( GTK_BOX(caja), E->orden, FALSE, FALSE, 0 );

    /* --- lo que dice el motor, EN VIVO, con todo el alto --- */
    vb = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    E->titulo = gtk_label_new( "drtran — sin lanzar" );
    gtk_widget_set_halign( E->titulo, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(vb), E->titulo, FALSE, FALSE, 0 );

    /* En PULSO: no se puede saber el avance, y un porcentaje seria inventado. */
    E->barra = gtk_progress_bar_new();
    gtk_widget_set_valign( E->barra, GTK_ALIGN_CENTER );
    gtk_widget_set_no_show_all( E->barra, TRUE );
    gtk_widget_set_tooltip_text( E->barra,
        "En pulso, no con porcentaje: drtran no informa de su avance, así "
        "que un porcentaje sería inventado." );
    gtk_box_pack_start( GTK_BOX(vb), E->barra, TRUE, TRUE, 0 );

    /* Dos pestañas: la CONSOLA --lo que el motor va diciendo-- y el INFORME
     * del modelo, que es el .out. El informe es el resultado del trabajo y
     * abrirlo con el visor del sistema no tenia sentido: va aqui.      */
    E->libreta = gtk_notebook_new();

    E->salida = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->salida), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(E->salida), TRUE );
    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), E->salida );
    gtk_notebook_append_page( GTK_NOTEBOOK(E->libreta), sc,
                              gtk_label_new( "Consola" ) );

    E->informe = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->informe), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(E->informe), TRUE );
    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), E->informe );
    gtk_notebook_append_page( GTK_NOTEBOOK(E->libreta), sc,
                              gtk_label_new( "Salida del modelo" ) );

    marco = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    gtk_box_pack_start( GTK_BOX(marco), vb, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(marco), E->libreta, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), marco, TRUE, TRUE, 0 );

    /* --- los dos veredictos, altura fija --- */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_widget_set_margin_top( vb, 2 );

    E->ver_fin = gtk_label_new( "" );
    E->ver_que = gtk_label_new( "" );
    gtk_widget_set_halign( E->ver_fin, GTK_ALIGN_START );
    gtk_widget_set_halign( E->ver_que, GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(E->ver_fin), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(E->ver_que), PANGO_ELLIPSIZE_END );

    gtk_box_pack_start( GTK_BOX(vb), E->ver_fin, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), E->ver_que, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), vb, FALSE, FALSE, 0 );

    return caja;
}
