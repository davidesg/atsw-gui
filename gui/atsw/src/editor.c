/*
 * editor.c -- el editor del .inp, desde la rejilla de la madre.
 *
 * LA OTRA PUERTA A LA MISMA ITERACION. fue_gui especifica por formulario, y
 * el formulario solo puede expresar lo que tiene widgets; el fichero expresa
 * todo lo que el motor lee -- varios factores del mismo tipo, los de
 * frecuencia fija, las deterministas a mano. Quien sabe leer un .inp no tenia
 * por donde entrar.
 *
 * docs/DISENO-editor.md cerro la fase 2 con esto implementado DENTRO de
 * fue_gui, y dejo escrito lo que faltaba:
 *
 *     "Hoy el editor trabaja sobre <area de trabajo>/<modelo>.{inp,pre}.
 *      Abrir uno cualquiera es facil, pero choca con lo que la fase 5 acaba
 *      de concluir: la raiz tiene que estar declarada. Cuando exista el
 *      manifiesto del proyecto, el selector abre dentro de el."
 *
 * El manifiesto ya existe. Asi que aqui el sujeto no es un fichero: es un
 * NODO del proyecto, y por eso este editor puede hacer lo que aquel no podia
 * -- cuando lo que vas a pisar es un modelo ya estimado, DERIVAR uno nuevo en
 * vez de borrar el registro de lo que se estimo.
 *
 * Las tres reglas de la fase 2 siguen enteras:
 *
 *   1. Se VALIDA al guardar, por la puerta del motor (inp_check_fue, el mismo
 *      fichero que compila el motor). Un guardado fallido NO toca el fichero
 *      que habia y dice su linea: guardar algo que el motor no podria leer es
 *      dejar el sistema mintiendo.
 *   2. La TERNA se avisa, no se borra. Un .out junto a un .inp que ya no
 *      describe es la situacion en que "los errores tipicos se leen del .out"
 *      se rompe sin darse cuenta.
 *   3. UN SOLO DUEÑO en cada momento: el texto mientras se edita, el fichero
 *      en cuanto se guarda.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "engine.h"
#include "preview.h"
#include "rutas.h"
#include "inpcheck.h"
#include "outfile.h"

#include "atsw.h"

void       barra_pub( Atsw *a, const char *s );
gchar     *atsw_programa( const char *programa );

typedef struct {
    Atsw      *a;
    char       serie[PR_ID];
    char       muestra[PR_ID];
    char       id[PR_ID];

    GtkWidget *win;
    GtkWidget *libro;       /* Editor | Salida                             */
    GtkWidget *texto;       /* el .inp                                     */
    GtkWidget *salida;      /* el .out, el informe                         */
    GtkWidget *consola;     /* LO QUE PASA: la orden y lo que el motor dice */
    GtkWidget *estado;
    GtkWidget *b_guardar;
    GtkWidget *b_estimar;
    GtkWidget *b_graficos;

    EngineJob *job;         /* la corrida en curso, o NULL                 */
} Editor;


/* LOS EDITORES ABIERTOS.
 *
 * Existe para una cosa sola: al DERIVAR hay que cerrar el padre, guardado.
 * Derivar es una transicion, no una bifurcacion de la atencion --el hijo
 * salio del .inp del padre TAL COMO ESTABA--, y dejarlo abierto y editable
 * invita a seguir tocandolo, con lo que la procedencia del hijo pasa a ser
 * mentira: dice que salio de un fichero que ya no es ese.
 *
 * Una lista y no un mapa: son dos o tres ventanas.                     */
static GSList *g_abiertos;


/* ------------------------------------------------------------------------ */
/* Cosas pequeñas                                                            */
/* ------------------------------------------------------------------------ */

static void di( Editor *E, const char *fmt, ... ) G_GNUC_PRINTF( 2, 3 );

static void di( Editor *E, const char *fmt, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, fmt );
    s = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    gtk_label_set_text( GTK_LABEL(E->estado), s );
    gtk_widget_set_tooltip_text( E->estado, s );
    g_free( s );
}

static gchar *texto_de( GtkWidget *tv )
{
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(tv) );
    GtkTextIter    i, f;

    gtk_text_buffer_get_bounds( b, &i, &f );
    return gtk_text_buffer_get_text( b, &i, &f, FALSE );
}

static void pon_texto( GtkWidget *tv, const char *s )
{
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(tv) ),
                              s ? s : "", -1 );
}

/* Añade al final Y SE QUEDA MIRANDO EL FINAL. Una consola que no sigue a lo
 * que escribe obliga a arrastrar la barra en cada corrida.              */
static void anade( GtkWidget *tv, const char *s, gsize n )
{
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(tv) );
    GtkTextIter    f;
    GtkTextMark   *m;

    gtk_text_buffer_get_end_iter( b, &f );
    gtk_text_buffer_insert( b, &f, s, (gint) n );

    gtk_text_buffer_get_end_iter( b, &f );
    m = gtk_text_buffer_create_mark( b, NULL, &f, FALSE );
    gtk_text_view_scroll_mark_onscreen( GTK_TEXT_VIEW(tv), m );
    gtk_text_buffer_delete_mark( b, m );
}

/* LA CONSOLA ES UN REGISTRO, no una ventana de una corrida: lo de antes no
 * se borra. Ver la orden anterior al lado de la de ahora es lo que deja
 * entender que cambio entre las dos.                                    */
static void consola( Editor *E, const char *fmt, ... ) G_GNUC_PRINTF( 2, 3 );

static void consola( Editor *E, const char *fmt, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, fmt );
    s = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    anade( E->consola, s, strlen( s ) );
    g_free( s );
}

static void titula( Editor *E )
{
    gchar *t = g_strdup_printf( "%s / %s.inp — editor de ATSW",
                                E->serie, E->id );

    gtk_window_set_title( GTK_WINDOW(E->win), t );
    g_free( t );
}

/* El .out del nodo, si lo hay, en el panel de abajo. */
static void trae_out( Editor *E )
{
    char   path[PR_RUTA];
    gchar *c = NULL;
    gsize  n = 0;

    if ( pr_ruta( E->a->p, E->serie, E->muestra, E->id, ".out", path, sizeof path ) == 0 &&
         g_file_get_contents( path, &c, &n, NULL ) )
        { pon_texto( E->salida, c ); g_free( c ); }
    else
        pon_texto( E->salida, "" );
}

static gboolean trae_inp( Editor *E )
{
    char   path[PR_RUTA];
    gchar *c = NULL;
    gsize  n = 0;

    if ( pr_ruta( E->a->p, E->serie, E->muestra, E->id, ".inp", path, sizeof path ) != 0 ||
         !g_file_get_contents( path, &c, &n, NULL ) )
        { di( E, "No pude leer el .inp de %s/%s.", E->serie, E->id );
          return FALSE; }

    pon_texto( E->texto, c );
    g_free( c );
    gtk_text_buffer_set_modified(
        gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->texto) ), FALSE );
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/* Guardar                                                                   */
/* ------------------------------------------------------------------------ */

/* LA TERNA: se NOMBRA lo que deja de describir al .inp, y no se borra.
 * Borrar trabajo ajeno no es cosa de un editor.                         */
static gchar *terna_rancia( Editor *E )
{
    static const char *ext[] = { ".pre", ".out", NULL };
    GString           *s = g_string_new( NULL );
    int                i;

    for ( i = 0; ext[i]; i++ )
        {
        char f[PR_RUTA];

        if ( pr_ruta( E->a->p, E->serie, E->muestra, E->id, ext[i], f, sizeof f ) == 0 &&
             g_file_test( f, G_FILE_TEST_EXISTS ) )
            g_string_append_printf( s, "%s%s", s->len ? " y el " : "",
                                    ext[i] + 1 );
        }
    return g_string_free( s, s->len == 0 );
}

/* GUARDAR UN .inp, VALIDANDO ANTES POR LA PUERTA DEL MOTOR.
 *
 * Fuera del Editor a proposito: esta es LA regla del editor -- un guardado
 * fallido no puede costar trabajo -- y en un widget no se podria probar.
 *
 * Guardar algo que el motor no podria leer es dejar el sistema mintiendo, y
 * la leccion de la fase 1 es que una propiedad que solo se sostiene si todos
 * se acuerdan es una costumbre: lo que la convierte en propiedad es que la
 * operacion prohibida falle RUIDOSAMENTE.
 *
 * 0 si pudo; si no, el fichero que habia sigue intacto y why dice por que.  */
int atsw_guarda_inp( const char *destino, const char *txt,
                     char *why, size_t n )
{
    char tmp[PR_RUTA + 16], msg[512] = "";

    if ( why && n ) why[0] = '\0';
    if ( destino == NULL || !*destino || txt == NULL )
        { if ( why ) g_snprintf( why, n, "No hay dónde escribir." ); return 1; }

    /* Primero a un temporal AL LADO, que es donde el motor lo leeria: un
       .inp se lee con rutas relativas a su propio sitio.               */
    g_snprintf( tmp, sizeof tmp, "%s.editando", destino );
    if ( !g_file_set_contents( tmp, txt, -1, NULL ) )
        { if ( why ) g_snprintf( why, n, "No pude escribir el temporal." );
          return 1; }

    if ( inp_check_fue( tmp, msg, sizeof msg ) != 0 )
        {
        /* NO SE TOCA LO QUE HABIA, y se dice el motivo CON SU LINEA, que es
           lo que deja corregir sin volver a empezar.                   */
        g_unlink( tmp );
        if ( why ) g_snprintf( why, n, "No lo guardo — %s",
                               msg[0] ? msg : "no es un .inp válido" );
        return 1;
        }

    if ( g_rename( tmp, destino ) != 0 )
        { g_unlink( tmp );
          if ( why ) g_snprintf( why, n, "No pude escribir %s.", destino );
          return 1; }
    return 0;
}

static int escribe( Editor *E, const char *id, char *why, size_t n )
{
    char   destino[PR_RUTA];
    gchar *txt;
    int    rc;

    if ( pr_ruta( E->a->p, E->serie, E->muestra, id, ".inp", destino, sizeof destino ) != 0 )
        { g_snprintf( why, n, "No pude componer la ruta." ); return 1; }

    txt = texto_de( E->texto );
    rc  = atsw_guarda_inp( destino, txt, why, n );
    g_free( txt );
    return rc;
}

/* UN NODO NUEVO COLGADO DE ESTE, con su directorio hecho. 0 si pudo.
 * Lo usan las dos salidas de guarda(): la del nodo estimado y la del nodo
 * con hijos. Estaba escrito dos veces y una de ellas iba a quedarse atras. */
static int deriva_nodo( Editor *E, char *id_out, size_t nid )
{
    PrError e;
    char    nuevo[PR_ID], ruta[PR_RUTA], *dir;

    if ( pr_deriva( E->a->p, E->serie, E->muestra, E->id, nuevo, sizeof nuevo,
                    ruta, sizeof ruta, &e ) != 0 )
        { char w[512]; pr_error_es( &e, w, sizeof w ); di( E, "%s", w );
          return 1; }

    dir = g_path_get_dirname( ruta );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
    g_snprintf( id_out, nid, "%s", nuevo );
    return 0;
}

/* 0 si guardo; deja en id_out el nodo donde acabo (puede ser otro). */
static int guarda( Editor *E, char *id_out, size_t nid )
{
    char     why[512];
    gchar   *rancia;
    gboolean estimado;

    g_snprintf( id_out, nid, "%s", E->id );

    /* ¿ESTE NODO YA ESTABA ESTIMADO? Entonces guardar encima borra el
       registro de lo que se estimo: el .out de al lado pasaria a describir
       otra cosa. Con un manifiesto delante hay una salida mejor que avisar,
       y es DERIVAR -- pero la decision es del analista.                */
    rancia   = terna_rancia( E );
    estimado = ( rancia != NULL );

    /* DOS RAZONES PARA NO GUARDAR AQUI, Y NO SON LA MISMA.
     *
     * Estimado: el .out de al lado pasaria a describir otra cosa. Grave,
     * pero es un registro que el analista puede decidir tirar -- asi que se
     * ofrece derivar y se deja elegir.
     *
     * CON HIJOS: el hijo salio de este .inp TAL COMO ESTABA. Cambiarlo no
     * borra nada: deja un linaje que dice una cosa y unos ficheros que
     * dicen otra, y eso no se ve hasta que alguien intenta rehacer el
     * camino meses despues. Ahi no hay eleccion que ofrecer. Es la unica
     * puerta de este programa que se cierra del todo, y se cierra porque lo
     * que impide no es una molestia: es escribir algo falso.
     *
     * pr_borra ya se negaba por lo mismo --dejaria hijos colgando-- y decia
     * CUAL cuelga. Aqui igual.                                         */
    {
    char hijos[8][PR_ID];
    int  nh = pr_hijos( E->a->p, E->serie, E->muestra, E->id, hijos, 8 );

    if ( nh > 0 )
        {
        GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(E->win),
                GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                "De «%s» ya cuelga %s%s.", E->id, hijos[0],
                ( nh > 1 ) ? " (y algún otro)" : "" );
        int r;

        gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d),
            "%s salió de este .inp TAL COMO ESTABA. Si lo cambias, su "
            "procedencia deja de ser cierta: el linaje diría que viene de un "
            "fichero que ya no existe.\n\nLo editado va a un modelo nuevo, "
            "colgado de éste.", hijos[0] );
        gtk_dialog_add_buttons( GTK_DIALOG(d),
            "Derivar un modelo nuevo", 1,
            "Cancelar",                GTK_RESPONSE_CANCEL, NULL );
        gtk_dialog_set_default_response( GTK_DIALOG(d), 1 );
        r = gtk_dialog_run( GTK_DIALOG(d) );
        gtk_widget_destroy( d );
        g_free( rancia );

        if ( r != 1 ) { di( E, "Sin guardar." ); return 1; }
        if ( deriva_nodo( E, id_out, nid ) != 0 ) return 1;
        }
    else if ( estimado )
        {
        GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(E->win),
                GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                "«%s» ya está estimado.", E->id );
        int r;

        gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d),
            "Si guardas aquí, el %s de al lado queda describiendo otra cosa: "
            "no se borra, pero deja de valer.\n\nDerivar deja %s como está y "
            "pone lo editado en un modelo nuevo, colgado de él.", rancia,
            E->id );
        gtk_dialog_add_buttons( GTK_DIALOG(d),
            "Derivar un modelo nuevo", 1,
            "Guardar aquí",            2,
            "Cancelar",                GTK_RESPONSE_CANCEL, NULL );
        gtk_dialog_set_default_response( GTK_DIALOG(d), 1 );
        r = gtk_dialog_run( GTK_DIALOG(d) );
        gtk_widget_destroy( d );
        g_free( rancia );

        if ( r == GTK_RESPONSE_CANCEL || r == GTK_RESPONSE_DELETE_EVENT )
            { di( E, "Sin guardar." ); return 1; }
        if ( r == 1 && deriva_nodo( E, id_out, nid ) != 0 ) return 1;
        }
    else
        g_free( rancia );
    }

    if ( escribe( E, id_out, why, sizeof why ) != 0 )
        {
        /* Si se habia derivado un nodo para nada, se queda vacio en el
           manifiesto. Se deshace: un modelo sin .inp no es nada.      */
        if ( strcmp( id_out, E->id ) != 0 )
            { PrError e; pr_borra( E->a->p, E->serie, E->muestra, id_out, &e );
              g_snprintf( id_out, nid, "%s", E->id ); }
        di( E, "%s", why );
        return 1;
        }

    gtk_text_buffer_set_modified(
        gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->texto) ), FALSE );

    if ( strcmp( id_out, E->id ) != 0 )
        {
        PrError e;

        atsw_guarda( E->a, &e );
        g_snprintf( E->id, sizeof E->id, "%s", id_out );
        titula( E );
        trae_out( E );      /* el nuevo no tiene .out: el panel se vacia */
        di( E, "Guardado en %s, derivado de su padre. Ponle su razón en la "
               "madre.", id_out );
        }
    else
        di( E, "Guardado en %s.inp.", id_out );

    atsw_refresca( E->a );
    return 0;
}

static void on_guardar( GtkButton *b, Editor *E )
{
    char id[PR_ID];

    (void) b;
    guarda( E, id, sizeof id );
}

static void on_recargar( GtkButton *b, Editor *E )
{
    (void) b;
    if ( gtk_text_buffer_get_modified(
             gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->texto) ) ) )
        {
        GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(E->win),
                GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                "¿Tiro lo que has escrito y releo el fichero?" );
        int r = gtk_dialog_run( GTK_DIALOG(d) );

        gtk_widget_destroy( d );
        if ( r != GTK_RESPONSE_OK ) return;
        }
    trae_inp( E );
    trae_out( E );
    di( E, "Releído del disco." );
}


/* ------------------------------------------------------------------------ */
/* Estimar, sin congelar la ventana                                          */
/* ------------------------------------------------------------------------ */

/* EL GRAFICO DE LOS RESIDUOS LO ESCRIBE EL MOTOR, y hay que saber su nombre
 * para encontrarlo: fue lo arma poniendo una "A" DELANTE DEL NOMBRE, no de
 * la ruta -- "Acaso/X.eps" no se podia escribir, y era un aviso, no un
 * error. Por eso el motor usa ruta_componer, y por eso aqui se compone
 * igual en vez de pegar cadenas.
 *
 * Y es JUSTO lo que hace falta: fue lo dibuja con fp_PlotSer_CorrSer, "the
 * same graph as fug -c" -- la serie de residuos con su ACF y su PACF. No
 * hay que calcular nada ni lanzar nada: ya esta escrito desde la ultima
 * estimacion.                                                          */
static int eps_de( Editor *E, char *out, size_t n )
{
    char base[PR_RUTA];

    out[0] = '\0';
    if ( pr_ruta( E->a->p, E->serie, E->muestra, E->id, ".eps", base, sizeof base ) != 0 )
        return 1;
    return ruta_componer( base, "A", NULL, out, n );
}

static gboolean hay_eps( Editor *E )
{
    char f[PR_RUTA];

    return eps_de( E, f, sizeof f ) == 0 &&
           g_file_test( f, G_FILE_TEST_EXISTS );
}

static void on_graficos( GtkButton *b, Editor *E )
{
    char f[PR_RUTA];

    (void) b;
    if ( eps_de( E, f, sizeof f ) != 0 ||
         !g_file_test( f, G_FILE_TEST_EXISTS ) )
        {
        /* SE DICE QUE NO ESTA. El grafico sale de estimar, asi que faltar
           significa una cosa concreta y se puede decir cual.          */
        di( E, "Todavía no hay gráfico de %s: sale al estimar.", E->id );
        return;
        }

    if ( !preview_show( (PreviewApp *) E->a, f ) )
        di( E, "No pude abrir %s.", f );
    else
        {
        di( E, "Residuos, ACF y PACF de %s.", E->id );
        consola( E, "* %s\n", f );
        }
}

static void corriendo( Editor *E, gboolean si )
{
    gtk_widget_set_sensitive( E->b_guardar, !si );
    gtk_widget_set_sensitive( E->b_estimar, !si );
    gtk_widget_set_sensitive( E->b_graficos, !si && hay_eps( E ) );
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->texto), !si );
}

static void sale( const char *txt, gsize n, gpointer d )
{
    anade( ((Editor *) d)->consola, txt, n );
}

static void paso( int it, double obj, gpointer d )
{
    di( (Editor *) d, "Estimando… iteración %d, función %.6g", it, obj );
}

static void acabo( const EngineResult *r, gpointer d )
{
    Editor *E = d;

    corriendo( E, FALSE );
    E->job = NULL;

    /* ENGINE_WROTE_RESULTS incluye el 3: "no pude estimarlo, ahi van los
       valores iniciales". Eso TAMBIEN escribe, y la rejilla tiene que
       enterarse -- lo que no puede es decir que fue bien.             */
    if ( engine_wrote_results( r ) )
        {
        trae_out( E );
        atsw_refresca( E->a );
        /* El informe es lo que se mira despues de estimar: se pone delante
           solo, que es lo que uno iba a hacer con el raton.           */
        gtk_notebook_set_current_page( GTK_NOTEBOOK(E->libro), 1 );
        }
    gtk_widget_set_sensitive( E->b_graficos, hay_eps( E ) );
    consola( E, "\n%s\n\n", r->message ? r->message : "terminó" );
    di( E, "%s", r->message ? r->message : "Terminó." );
}

static void on_estimar( GtkButton *b, Editor *E )
{
    char        id[PR_ID], ruta[PR_RUTA];
    gchar      *exe, *dir, *base;
    const char *argv[2];

    (void) b;
    if ( E->job ) return;

    /* SE GUARDA PRIMERO, Y SI NO SE PUEDE, NO SE CORRE. Estimar lo que hay
       en el disco mientras la pantalla enseña otra cosa es el fallo de dos
       dueños que cerro la fase 2.                                      */
    if ( gtk_text_buffer_get_modified(
             gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->texto) ) ) &&
         guarda( E, id, sizeof id ) != 0 )
        return;

    if ( pr_ruta( E->a->p, E->serie, E->muestra, E->id, ".inp", ruta, sizeof ruta ) != 0 )
        { di( E, "No pude componer la ruta." ); return; }

    exe = atsw_programa( "fue" );
    if ( exe == NULL )
        { di( E, "No encuentro «fue»: ni al lado de esta ventana ni en el "
                 "PATH. ¿Está compilado?" ); return; }

    dir  = g_path_get_dirname( ruta );
    base = g_path_get_basename( ruta );
    if ( strlen( base ) > 4 ) base[strlen( base ) - 4] = '\0';   /* sin .inp */

    argv[0] = base;
    argv[1] = NULL;

    /* LA ORDEN, TAL CUAL, con su directorio: es lo que hace reproducible lo
       que acaba de pasar. Quien quiera repetirlo fuera lo tiene escrito. */
    consola( E, "$ cd %s\n$ %s %s\n", dir, exe, base );

    corriendo( E, TRUE );
    di( E, "Estimando %s…", base );
    E->job = engine_start( dir, exe, argv, paso, sale, acabo, E );
    if ( E->job == NULL )
        { corriendo( E, FALSE ); di( E, "No pude lanzar %s.", exe ); }

    g_free( exe ); g_free( dir ); g_free( base );
}


/* ------------------------------------------------------------------------ */
/* La ventana                                                                */
/* ------------------------------------------------------------------------ */

static gboolean on_cerrar( GtkWidget *w, GdkEvent *ev, Editor *E )
{
    (void) w; (void) ev;

    if ( E->job ) { engine_stop( E->job ); E->job = NULL; }

    if ( gtk_text_buffer_get_modified(
             gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->texto) ) ) )
        {
        GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(E->win),
                GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                "Hay cambios sin guardar en %s.inp.", E->id );
        int r;

        gtk_dialog_add_buttons( GTK_DIALOG(d), "Guardar", 1,
                                "Cerrar sin guardar", 2,
                                "Seguir editando", GTK_RESPONSE_CANCEL, NULL );
        r = gtk_dialog_run( GTK_DIALOG(d) );
        gtk_widget_destroy( d );

        if ( r == GTK_RESPONSE_CANCEL || r == GTK_RESPONSE_DELETE_EVENT )
            return TRUE;
        if ( r == 1 )
            { char id[PR_ID]; if ( guarda( E, id, sizeof id ) != 0 ) return TRUE; }
        }
    return FALSE;
}

static void on_destruir( GtkWidget *w, Editor *E )
{
    (void) w;
    g_abiertos = g_slist_remove( g_abiertos, E );
    g_free( E );
}

/* Cerrar lo que haya abierto de ese nodo. Se llama al DERIVAR.
 *
 * Y NO GUARDA, que es lo que hacia y estaba mal. Al llamarse, el hijo ya
 * existe: guardar el padre seria justo la escritura que este programa
 * impide dos funciones mas abajo --un padre con hijos no se edita--, y
 * hacerlo aqui por la puerta de atras es peor que hacerlo de frente.
 *
 * Con cambios sin guardar NO SE CIERRA. Tirar el trabajo del analista sin
 * preguntar no es una opcion, y guardarlo tampoco: se deja la ventana
 * abierta y se dice. La decision es suya, que para eso el .inp del padre
 * sigue siendo suyo hasta que el hijo se estime.
 *
 * Devuelve 0 si cerro todo lo que habia, y 1 si dejo algo abierto.    */
int atsw_editor_cierra( const char *serie, const char *muestra,
                        const char *id )
{
    GSList *l, *copia;
    int     quedan = 0;

    if ( !serie || !id ) return 0;
    copia = g_slist_copy( g_abiertos );      /* destruir modifica la lista */
    for ( l = copia; l; l = l->next )
        {
        Editor *E = (Editor *) l->data;

        if ( strcmp( E->serie, serie ) || strcmp( E->id, id ) ) continue;
        if ( strcmp( E->muestra, muestra ? muestra : "" ) ) continue;

        if ( gtk_text_buffer_get_modified(
                 gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->texto) ) ) )
            {
            di( E, "Se ha derivado de aquí: este .inp queda congelado. "
                   "Tienes cambios sin guardar; decide tú qué hacer con "
                   "ellos." );
            quedan = 1;
            continue;
            }
        gtk_widget_destroy( E->win );
        }
    g_slist_free( copia );
    return quedan;
}

static GtkWidget *monoespaciado( void )
{
    GtkWidget      *tv = gtk_text_view_new();
    GtkCssProvider *css = gtk_css_provider_new();

    /* REJILLA DE VERDAD. Un .inp es posicional en varias secciones: con una
       tipografia proporcional, las columnas dejan de estar donde estan. */
    gtk_css_provider_load_from_data( css,
        "textview { font-family: monospace; font-size: 10pt; }", -1, NULL );
    gtk_style_context_add_provider( gtk_widget_get_style_context( tv ),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
    g_object_unref( css );
    return tv;
}

static GtkWidget *en_scroll( GtkWidget *w )
{
    GtkWidget *s = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(s),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(s), w );
    return s;
}

void atsw_editor( Atsw *a, const char *serie, const char *muestra,
                  const char *id )
{
    Editor    *E;
    GtkWidget *raiz, *barra, *pan, *b;

    if ( !a->hay || serie == NULL || !*serie || id == NULL || !*id ) return;

    /* LOS DATOS NO SE EDITAN. Es la misma regla que en todas partes, dicha
       aqui tambien porque aqui es donde mas a mano esta romperla.     */
    if ( pr_es_datos( a->p, serie, muestra, id ) )
        {
        barra_pub( a, "Los datos no se editan: son la raíz. Empieza un modelo "
                      "con «Modelo nuevo» y edita ése." );
        return;
        }

    E = g_new0( Editor, 1 );
    E->a = a;
    g_snprintf( E->serie, sizeof E->serie, "%s", serie );
    g_snprintf( E->muestra, sizeof E->muestra, "%s", muestra ? muestra : "" );
    g_snprintf( E->id,    sizeof E->id,    "%s", id );

    E->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_default_size( GTK_WINDOW(E->win), 820, 700 );
    gtk_window_set_transient_for( GTK_WINDOW(E->win), GTK_WINDOW(a->ventana) );
    titula( E );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
    gtk_container_add( GTK_CONTAINER(E->win), raiz );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    E->b_guardar = b = gtk_button_new_with_label( "Guardar" );
    gtk_widget_set_tooltip_text( b,
        "Se valida ANTES de escribir, con el mismo comprobador que compila el "
        "motor. Si no vale, el fichero que había no se toca y se dice la "
        "línea." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_guardar), E );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    E->b_estimar = b = gtk_button_new_with_label( "Guardar y estimar" );
    gtk_widget_set_tooltip_text( b,
        "Corre fue sobre este .inp y trae su salida aquí abajo. La ventana no "
        "se congela y se puede cerrar: la corrida se para." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_estimar), E );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    E->b_graficos = b = gtk_button_new_with_label( "Gráficos…" );
    gtk_widget_set_tooltip_text( b,
        "Los residuos con su ACF y su PACF — el mismo gráfico que «fug -c», "
        "dibujado por el motor al estimar.\n\nSi está apagado es que este "
        "modelo no se ha estimado todavía." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_graficos), E );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Releer" );
    gtk_widget_set_tooltip_text( b, "Vuelve a traer el fichero del disco." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_recargar), E );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    E->estado = gtk_label_new( "" );
    gtk_label_set_ellipsize( GTK_LABEL(E->estado), PANGO_ELLIPSIZE_END );
    gtk_widget_set_halign( E->estado, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(barra), E->estado, TRUE, TRUE, 8 );

    /* ARRIBA LO QUE SE MIRA, ABAJO LO QUE PASA.
     *
     * El .inp y el .out son dos vistas del MISMO modelo -- la especificacion
     * y el output -- asi que van en pestañas: se alternan, no se comparan.
     * La consola es otra cosa: es el registro de lo que se ha ejecutado, y
     * tiene que verse A LA VEZ que cualquiera de las dos.              */
    pan = gtk_paned_new( GTK_ORIENTATION_VERTICAL );
    gtk_box_pack_start( GTK_BOX(raiz), pan, TRUE, TRUE, 0 );

    E->libro = gtk_notebook_new();
    gtk_paned_pack1( GTK_PANED(pan), E->libro, TRUE, FALSE );

    E->texto = monoespaciado();
    gtk_notebook_append_page( GTK_NOTEBOOK(E->libro), en_scroll( E->texto ),
                              gtk_label_new( "Especificación" ) );

    E->salida = monoespaciado();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->salida), FALSE );
    gtk_widget_set_tooltip_text( E->salida,
        "El informe de la estimación con su diagnosis. Se lee AQUI: los "
        "errores típicos se leen del .out, nunca de reejecutar un .pre." );
    gtk_notebook_append_page( GTK_NOTEBOOK(E->libro), en_scroll( E->salida ),
                              gtk_label_new( "Output" ) );

    E->consola = monoespaciado();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->consola), FALSE );
    gtk_widget_set_tooltip_text( E->consola,
        "La orden que se ejecuta, con su directorio, y lo que el motor va "
        "diciendo.\n\nNo se borra entre corridas: ver la anterior al lado "
        "de la de ahora es lo que deja entender qué cambió." );
    gtk_paned_pack2( GTK_PANED(pan), en_scroll( E->consola ), FALSE, TRUE );
    gtk_paned_set_position( GTK_PANED(pan), 470 );

    g_signal_connect( E->win, "delete-event", G_CALLBACK(on_cerrar), E );
    g_signal_connect( E->win, "destroy", G_CALLBACK(on_destruir), E );
    g_abiertos = g_slist_prepend( g_abiertos, E );

    if ( !trae_inp( E ) ) { gtk_widget_destroy( E->win ); return; }
    trae_out( E );
    gtk_widget_set_sensitive( E->b_graficos, hay_eps( E ) );
    consola( E, "* %s / %s\n", E->serie, E->id );
    di( E, "%s / %s.inp", E->serie, E->id );

    gtk_widget_show_all( E->win );
}
