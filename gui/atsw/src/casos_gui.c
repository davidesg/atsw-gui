/*
 * casos_gui.c -- los casos en la madre. Ver docs/DISENO-casos.md §4.
 *
 * LO QUE FALTABA. La madre llevaba bien lo univariante y para drtran tenia un
 * boton que lanzaba drtran_gui con el proyecto y nada mas: ni que series, ni
 * en que orden, ni que corrida. Lo que se cruzaba no quedaba escrito en
 * ninguna parte que la madre supiera leer.
 *
 * Un CASO es un conjunto FIJO y ORDENADO de series, cada una con el modelo
 * con que entra y el sha256 de su .pre en el momento del alta. Esto es la
 * mitad que se ve: la seccion CASOS de la izquierda, la vista del caso a la
 * derecha --sus entradas, sus corridas con su linaje-- y los gestos.
 *
 * LO QUE SE ESTIMO SE LEE DEL .out, como en la rejilla: la logL de cada
 * corrida sale de lib/outdiag, el mismo lector que la Diagnosis de
 * drtran_gui. El manifiesto guarda decisiones; los numeros son del registro.
 *
 * Y LOS DOS DESFASES NO SE MEZCLAN (§2.4): un .pre que cambio es una alarma;
 * una serie con otro elegido hoy es una nota. Las reglas viven en ventana.c,
 * que tambien las usa para el veredicto de abajo.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "atsw.h"
#include "inpfile.h"
#include "dates.h"

void barra_pub( Atsw *a, const char *s );

/* ------------------------------------------------------------------------ */
/* LA VENTANA COMUN                                                          */
/*                                                                           */
/* La regla de fuepre_check_alignment (lib/fuepre), que es la que drtran      */
/* aplica con salida 4, mas la suya propia de antes: el mismo numero de       */
/* observaciones. Un caso que el motor va a rechazar no tiene que existir.   */
/*                                                                           */
/* NO SE ENLAZA lib/fuepre: arrastra el lector entero del .pre --con sus      */
/* deterministas y su motor-- para leer cuatro numeros de la cabecera. La     */
/* madre ya lleva el lector del .inp de fug (son el mismo formato) y          */
/* lib/dates, que es de donde fuepre saca ObsToDate. La regla es la misma;    */
/* el lector, el que ya estaba.                                              */
/* ------------------------------------------------------------------------ */

int atsw_ventana_comun( const char *const pre[], int n, char *why, size_t nw )
{
    InpFile *f;
    gchar  **nom;
    int      i, rc = 0;

    if ( why && nw ) why[0] = '\0';
    if ( n < 1 ) return 0;

    f   = g_new0( InpFile, n );
    nom = g_new0( gchar *, n + 1 );
    for ( i = 0; i < n; i++ )
        {
        char err[256];

        nom[i] = g_path_get_basename( pre[i] );
        if ( inp_read( pre[i], &f[i], err, sizeof err ) != 0 )
            {
            if ( why ) snprintf( why, nw, "no pude leer %s: %s", nom[i], err );
            rc = 1;
            goto fin;
            }
        }

    for ( i = 1; i < n && !rc; i++ )
        {
        if ( f[i].freq != f[0].freq )
            {
            if ( why ) snprintf( why, nw, "%s es de frecuencia %d y %s de %d: "
                                 "no se pueden modelizar juntas. Rehazlas a la "
                                 "misma frecuencia.", nom[i], f[i].freq, nom[0],
                                 f[0].freq );
            rc = 2;
            }
        /* Sin fechas no hay calendario que comparar. */
        else if ( f[i].numbering != f[0].numbering )
            {
            if ( why ) snprintf( why, nw, "%s tiene fechas y %s no: no hay "
                                 "calendario sobre el que alinearlas.",
                                 f[i].numbering ? nom[0] : nom[i],
                                 f[i].numbering ? nom[i] : nom[0] );
            rc = 3;
            }
        }

    /* LA FECHA FINAL. El cast conjunto alinea por el final y recorta a la
       mas corta: con finales distintos casaria observaciones de años
       distintos sin decir nada (BUG-2).                               */
    if ( !rc && !f[0].numbering )
        {
        int a0, p0;

        ObsToDate( f[0].begyear, f[0].begtime, f[0].nobs, f[0].freq, &a0, &p0 );
        for ( i = 1; i < n && !rc; i++ )
            {
            int ai, pi;

            ObsToDate( f[i].begyear, f[i].begtime, f[i].nobs, f[i].freq,
                       &ai, &pi );
            if ( ai != a0 || pi != p0 )
                {
                if ( why ) snprintf( why, nw, "las series NO acaban en la misma "
                                     "fecha: %s acaba en %02d/%d y %s en "
                                     "%02d/%d. drtran alinea por el final y "
                                     "casaría observaciones de años "
                                     "distintos. Usa una muestra con un final "
                                     "común.", nom[0], p0, a0, nom[i], pi, ai );
                rc = 4;
                }
            }
        }

    /* Y EL NUMERO DE OBSERVACIONES, que drtran exige aparte (drtran.c,
       salida 4): misma fecha final con distinto comienzo tampoco entra. */
    for ( i = 1; i < n && !rc; i++ )
        if ( f[i].nobs != f[0].nobs )
            {
            if ( why ) snprintf( why, nw, "%s tiene %d observaciones y %s %d: "
                                 "drtran exige el mismo número.", nom[0],
                                 f[0].nobs, nom[i], f[i].nobs );
            rc = 5;
            }

fin:
    for ( i = 0; i < n; i++ ) inp_free( &f[i] );
    g_free( f );
    g_strfreev( nom );
    return rc;
}

/* ------------------------------------------------------------------------ */
/* QUE SE PUEDE CRUZAR                                                       */
/* ------------------------------------------------------------------------ */

/* Los modelos de esa serie en esa muestra que pueden entrar en un caso:
   ESTIMADOS --con .pre, que es lo que drtran lee-- y que no son los datos.
   Devuelve cuantos.                                                     */
static int candidatos( const Proyecto *p, const char *serie,
                       const char *muestra, char ids[][PR_ID], int max )
{
    int i, n = 0;

    for ( i = 0; i < p->nm && n < max; i++ )
        {
        const PrModelo *m = &p->m[i];
        char            ruta[PR_RUTA];

        if ( strcmp( m->serie, serie ) || strcmp( m->muestra, muestra ) ) continue;
        if ( m->rol == PR_DATOS ) continue;
        if ( pr_ruta( p, serie, muestra, m->id, ".pre", ruta, sizeof ruta ) != 0 ||
             !g_file_test( ruta, G_FILE_TEST_EXISTS ) ) continue;
        snprintf( ids[n++], PR_ID, "%s", m->id );
        }
    return n;
}

static gboolean esta( char ids[][PR_ID], int n, const char *id )
{
    int i;

    if ( id == NULL || !*id ) return FALSE;
    for ( i = 0; i < n; i++ ) if ( !strcmp( ids[i], id ) ) return TRUE;
    return FALSE;
}

/* EL MODELO POR DEFECTO de una serie en el dialogo: el ELEGIDO de hoy si se
   puede cruzar; si no, con el que entraba en el caso de partida (derivar);
   si no, el ultimo estimado. Es solo el punto de partida: cualquiera de los
   estimados se puede escoger.                                          */
static const char *por_defecto( const Proyecto *p, const char *serie,
                                const char *muestra, const char *del_caso,
                                char ids[][PR_ID], int n )
{
    const char *hoy = pr_elegido( p, serie, muestra );

    if ( esta( ids, n, hoy ) ) return hoy;
    if ( esta( ids, n, del_caso ) ) return del_caso;
    return n ? ids[n - 1] : "";
}

/* ------------------------------------------------------------------------ */
/* «NUEVO CASO…» Y «DERIVAR CASO…»                                           */
/*                                                                           */
/* El mismo dialogo. Derivar es el alta con las entradas del caso de partida  */
/* ya puestas --en su orden, con el elegido de hoy y los .pre de hoy-- y la   */
/* muestra fija: es la salida natural de los dos desfases.                   */
/* ------------------------------------------------------------------------ */

typedef struct {
    char       serie[PR_ID];
    GtkWidget *chk, *combo, *sube, *baja;
    int        pos;                 /* la fila, 0..n-1: EL ORDEN del caso   */
} NcFila;

typedef struct {
    Atsw      *a;
    GtkWidget *d, *rej, *mu, *titulo, *razon, *aviso, *vacio;
    NcFila     f[PR_MAX_SERIE];
    int        n;
    char       muestra[PR_ID];
    char       de[PR_ID];           /* derivar: el caso de partida, o ""   */
} Nc;

static void nc_coloca( Nc *x, NcFila *f )
{
    GtkWidget *w[4];
    int        k;

    w[0] = f->chk; w[1] = f->combo; w[2] = f->sube; w[3] = f->baja;
    for ( k = 0; k < 4; k++ )
        gtk_container_child_set( GTK_CONTAINER(x->rej), w[k],
                                 "top-attach", f->pos + 1, NULL );
}

/* SUBIR O BAJAR UNA SERIE. El orden es PARTE DE LA IDENTIDAD del caso: es
   el con que el .cns indexa las posiciones y el de Cholesky de drvarma. */
static void nc_mueve( GtkButton *b, Nc *x )
{
    int      k   = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(b), "fila" ) );
    int      dir = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(b), "dir" ) );
    int      j, destino = x->f[k].pos + dir;

    for ( j = 0; j < x->n; j++ )
        if ( x->f[j].pos == destino )
            {
            x->f[j].pos = x->f[k].pos;
            x->f[k].pos = destino;
            nc_coloca( x, &x->f[j] );
            nc_coloca( x, &x->f[k] );
            return;
            }
}

static GtkWidget *nc_boton( Nc *x, const char *txt, const char *nombre,
                            const char *serie, int fila, int dir )
{
    GtkWidget *b = gtk_button_new_with_label( txt );
    gchar     *nm = g_strdup_printf( "%s-%s", nombre, serie );

    gtk_widget_set_name( b, nm );
    g_free( nm );
    g_object_set_data( G_OBJECT(b), "fila", GINT_TO_POINTER(fila) );
    g_object_set_data( G_OBJECT(b), "dir",  GINT_TO_POINTER(dir) );
    g_signal_connect( b, "clicked", G_CALLBACK(nc_mueve), x );
    return b;
}

/* Las filas, una por serie que tenga algo que cruzar EN ESTA MUESTRA. Al
   derivar, las del caso van primero, en su orden, y marcadas.         */
static void nc_filas( Nc *x )
{
    const Proyecto *p = x->a->p;
    const PrCaso   *c = x->de[0] ? pr_caso_ver( p, x->de ) : NULL;
    const char     *orden[PR_MAX_SERIE];
    int             i, j, no = 0;

    for ( i = 0; i < x->n; i++ )
        {
        gtk_widget_destroy( x->f[i].chk );
        gtk_widget_destroy( x->f[i].combo );
        gtk_widget_destroy( x->f[i].sube );
        gtk_widget_destroy( x->f[i].baja );
        }
    x->n = 0;

    if ( c )
        for ( i = 0; i < c->nen && no < PR_MAX_SERIE; i++ )
            orden[no++] = c->en[i].serie;
    for ( i = 0; i < p->ns && no < PR_MAX_SERIE; i++ )
        {
        gboolean ya = FALSE;

        for ( j = 0; j < no; j++ ) if ( !strcmp( orden[j], p->s[i].id ) ) ya = TRUE;
        if ( !ya ) orden[no++] = p->s[i].id;
        }

    for ( i = 0; i < no; i++ )
        {
        char        ids[PR_MAX_MODELO][PR_ID];
        const char *del_caso = NULL, *def, *hoy;
        NcFila     *f;
        gchar      *nm;
        int         nc, k;

        nc = candidatos( p, orden[i], x->muestra, ids, PR_MAX_MODELO );
        if ( nc == 0 ) continue;

        if ( c )
            for ( k = 0; k < c->nen; k++ )
                if ( !strcmp( c->en[k].serie, orden[i] ) ) del_caso = c->en[k].modelo;

        f = &x->f[x->n];
        snprintf( f->serie, PR_ID, "%s", orden[i] );
        f->pos = x->n;

        f->chk = gtk_check_button_new_with_label( orden[i] );
        nm = g_strdup_printf( "incluye-%s", orden[i] );
        gtk_widget_set_name( f->chk, nm );
        g_free( nm );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(f->chk), del_caso != NULL );
        gtk_widget_set_tooltip_text( f->chk, pr_serie_titulo( p, orden[i] ) );

        /* LOS ESTIMADOS DE LA SERIE, y por defecto el elegido. Se puede
           escoger otro a proposito: art aconseja a menudo un modelo
           distinto para lo multivariante que para prever.          */
        hoy = pr_elegido( p, orden[i], x->muestra );
        f->combo = gtk_combo_box_text_new();
        nm = g_strdup_printf( "modelo-%s", orden[i] );
        gtk_widget_set_name( f->combo, nm );
        g_free( nm );
        for ( k = 0; k < nc; k++ )
            {
            gchar *et = !strcmp( ids[k], hoy )
                      ? g_strdup_printf( "%s  ★ elegido", ids[k] )
                      : g_strdup( ids[k] );

            gtk_combo_box_text_append( GTK_COMBO_BOX_TEXT(f->combo), ids[k], et );
            g_free( et );
            }
        def = por_defecto( p, orden[i], x->muestra, del_caso, ids, nc );
        gtk_combo_box_set_active_id( GTK_COMBO_BOX(f->combo), def );

        f->sube = nc_boton( x, "↑", "sube", orden[i], x->n, -1 );
        f->baja = nc_boton( x, "↓", "baja", orden[i], x->n, +1 );

        gtk_grid_attach( GTK_GRID(x->rej), f->chk,   0, f->pos + 1, 1, 1 );
        gtk_grid_attach( GTK_GRID(x->rej), f->combo, 1, f->pos + 1, 1, 1 );
        gtk_grid_attach( GTK_GRID(x->rej), f->sube,  2, f->pos + 1, 1, 1 );
        gtk_grid_attach( GTK_GRID(x->rej), f->baja,  3, f->pos + 1, 1, 1 );
        x->n++;
        }

    gtk_widget_set_visible( x->vacio, x->n == 0 );
    gtk_widget_show_all( x->rej );
}

static void nc_muestra( GtkComboBox *cb, Nc *x )
{
    int i = gtk_combo_box_get_active( cb );

    snprintf( x->muestra, sizeof x->muestra, "%s",
              i <= 0 ? "" : x->a->p->mu[i - 1].id );
    nc_filas( x );
}

/* ACEPTAR: las marcadas, en su orden; la ventana comun; los hashes; el
   alta. TRUE si el caso quedo hecho; si no, el motivo en why y el
   dialogo sigue abierto para arreglarlo.                              */
static gboolean nc_acepta( Nc *x, char *why, size_t n )
{
    Atsw       *a = x->a;
    PrEntrada   en[PR_MAX_ENTRADA];
    char        pre[PR_MAX_ENTRADA][PR_RUTA];
    const char *pv[PR_MAX_ENTRADA];
    char        id[PR_ID], w[900];
    const char *ya, *titulo, *razon;
    PrError     e;
    GString    *lista;
    int         pos, i, ne = 0;

    for ( pos = 0; pos < x->n; pos++ )
        for ( i = 0; i < x->n; i++ )
            {
            const char *mod;

            if ( x->f[i].pos != pos ) continue;
            if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(x->f[i].chk) ) )
                continue;
            if ( ne >= PR_MAX_ENTRADA )
                { snprintf( why, n, "No caben más de %d series en un caso.",
                            PR_MAX_ENTRADA ); return FALSE; }
            mod = gtk_combo_box_get_active_id( GTK_COMBO_BOX(x->f[i].combo) );
            if ( mod == NULL ) continue;
            memset( &en[ne], 0, sizeof en[ne] );
            snprintf( en[ne].serie,  PR_ID, "%s", x->f[i].serie );
            snprintf( en[ne].modelo, PR_ID, "%s", mod );
            pr_ruta( a->p, en[ne].serie, x->muestra, mod, ".pre",
                     pre[ne], sizeof pre[ne] );
            pv[ne] = pre[ne];
            ne++;
            }

    if ( ne < 2 )
        { snprintf( why, n, "Un caso cruza al menos dos series: marca las que "
                    "entran." ); return FALSE; }

    if ( atsw_ventana_comun( pv, ne, w, sizeof w ) != 0 )
        { snprintf( why, n, "No es un caso: %s", w ); return FALSE; }

    /* LA IDENTIDAD, POR CONTENIDO: el sha256 de cada .pre ahora. */
    for ( i = 0; i < ne; i++ )
        {
        gchar *h = atsw_sha_de( pre[i] );

        snprintf( en[i].sha, PR_SHA, "%s", h ? h : "" );
        g_free( h );
        }

    /* EL MISMO CASO DOS VECES NO: mismas series, mismo orden, mismos .pre. */
    ya = pr_caso_de_entradas( a->p, en, ne, x->muestra );
    if ( ya[0] )
        { snprintf( why, n, "Ese caso ya existe: es %s, con las mismas series, "
                    "en el mismo orden y con los mismos .pre.", ya );
          return FALSE; }

    titulo = gtk_entry_get_text( GTK_ENTRY(x->titulo) );
    razon  = gtk_entry_get_text( GTK_ENTRY(x->razon) );

    if ( x->de[0] )
        {
        int k;

        if ( pr_caso_deriva( a->p, x->de, en, ne, id, sizeof id, &e ) != 0 )
            { pr_error_es( &e, why, n ); return FALSE; }
        /* El titulo se hereda; si el analista lo cambio, manda el suyo. */
        k = pr_caso_idx( a->p, id );
        if ( k >= 0 && *titulo )
            snprintf( a->p->ca[k].titulo, PR_TEXTO, "%s", titulo );
        if ( *razon ) pr_caso_razon( a->p, id, razon, &e );
        }
    else if ( pr_caso_add( a->p, en, ne, x->muestra, "drtran", titulo, razon,
                           id, sizeof id, &e ) != 0 )
        { pr_error_es( &e, why, n ); return FALSE; }

    if ( atsw_guarda( a, &e ) != 0 )
        {
        char m[512];

        pr_error_es( &e, m, sizeof m );
        snprintf( why, n, "El caso %s está, pero no pude guardar el proyecto: "
                  "%s", id, m );
        barra_pub( a, why );
        }
    else
        {
        lista = g_string_new( NULL );
        for ( i = 0; i < ne; i++ )
            g_string_append_printf( lista, "%s%s/%s", i ? ", " : "",
                                    en[i].serie, en[i].modelo );
        if ( x->de[0] )
            snprintf( why, n, "Caso %s, derivado de %s: %s. «Abrir en "
                      "drtran» para estimarlo.", id, x->de, lista->str );
        else
            snprintf( why, n, "Caso %s dado de alta: %s. «Abrir en drtran» "
                      "para estimarlo.", id, lista->str );
        g_string_free( lista, TRUE );
        barra_pub( a, why );
        }

    snprintf( a->caso, sizeof a->caso, "%s", id );
    a->viendo_caso = TRUE;
    return TRUE;
}

void atsw_caso_nuevo( Atsw *a, const char *deriva_de )
{
    Nc          *x;
    const PrCaso *c = NULL;
    GtkWidget   *caja, *l, *rej2;
    gchar       *tit;
    int          i, r;

    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }
    if ( deriva_de && ( c = pr_caso_ver( a->p, deriva_de ) ) == NULL )
        { barra_pub( a, "Marca el caso del que derivar." ); return; }

    x = g_new0( Nc, 1 );
    x->a = a;
    if ( c )
        {
        snprintf( x->de, sizeof x->de, "%s", c->id );
        snprintf( x->muestra, sizeof x->muestra, "%s", c->muestra );
        }
    else
        snprintf( x->muestra, sizeof x->muestra, "%s", atsw_muestra_actual( a ) );

    tit = c ? g_strdup_printf( "Derivar caso de %s", c->id )
            : g_strdup( "Nuevo caso" );
    x->d = gtk_dialog_new_with_buttons( tit, GTK_WINDOW(a->ventana),
               GTK_DIALOG_MODAL, "Cancelar", GTK_RESPONSE_CANCEL,
               c ? "Derivarlo" : "Darlo de alta", GTK_RESPONSE_OK, NULL );
    g_free( tit );
    gtk_dialog_set_default_response( GTK_DIALOG(x->d), GTK_RESPONSE_OK );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(x->d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );
    gtk_box_set_spacing( GTK_BOX(caja), 8 );

    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l), c
        ? "<small>Las mismas series, con las entradas cambiadas: por defecto el\n"
          "modelo elegido HOY y su .pre de hoy. El caso de partida no se toca;\n"
          "el nuevo queda colgado de él.</small>"
        : "<small>Un caso es un conjunto <b>fijo y ordenado</b> de series, cada una\n"
          "con el .pre con que entra. <b>El orden es parte del caso</b>: es el\n"
          "con que el .cns indexa las posiciones. Por defecto, el elegido de\n"
          "cada serie; se puede escoger otro estimado.</small>" );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );

    /* LA MUESTRA: todas las entradas de UNA ventana (§2.2). */
    {
    GtkWidget *h = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    int        act = 0;

    gtk_box_pack_start( GTK_BOX(h), gtk_label_new( "Muestra" ), FALSE, FALSE, 0 );
    x->mu = gtk_combo_box_text_new();
    gtk_widget_set_name( x->mu, "muestra" );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(x->mu), "Completa" );
    for ( i = 0; i < a->p->nmu; i++ )
        {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(x->mu), a->p->mu[i].id );
        if ( !strcmp( a->p->mu[i].id, x->muestra ) ) act = i + 1;
        }
    gtk_combo_box_set_active( GTK_COMBO_BOX(x->mu), act );
    gtk_widget_set_sensitive( x->mu, c == NULL );
    gtk_widget_set_tooltip_text( x->mu, c
        ? "La del caso de partida: derivar no cambia de ventana."
        : "Todas las entradas tienen que haber nacido en la misma muestra. Una "
          "submuestra con «hasta» garantiza un final común." );
    gtk_box_pack_start( GTK_BOX(h), x->mu, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), h, FALSE, FALSE, 0 );
    }

    x->rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(x->rej), 4 );
    gtk_grid_set_column_spacing( GTK_GRID(x->rej), 8 );
    {
    static const char *cab[] = { "Serie", "Entra con (.pre)", "Orden" };

    for ( i = 0; i < 3; i++ )
        {
        GtkWidget *c2 = gtk_label_new( NULL );
        gchar     *mk = g_markup_printf_escaped( "<b>%s</b>", cab[i] );

        gtk_label_set_markup( GTK_LABEL(c2), mk );
        g_free( mk );
        gtk_label_set_xalign( GTK_LABEL(c2), 0.0 );
        gtk_grid_attach( GTK_GRID(x->rej), c2, i, 0, i == 2 ? 2 : 1, 1 );
        }
    }
    gtk_box_pack_start( GTK_BOX(caja), x->rej, FALSE, FALSE, 0 );

    x->vacio = gtk_label_new( "Ninguna serie tiene un modelo estimado (con .pre) "
                              "en esta muestra." );
    gtk_widget_set_no_show_all( x->vacio, TRUE );
    gtk_box_pack_start( GTK_BOX(caja), x->vacio, FALSE, FALSE, 0 );

    rej2 = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej2), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(rej2), 8 );
    x->titulo = atsw_fila( rej2, 0, "Título ", c ? c->titulo : "",
        "Para quien lo lea: «inflación y petróleo». No es la clave." );
    gtk_widget_set_name( x->titulo, "titulo" );
    x->razon = atsw_fila( rej2, 1, "Por qué ", "",
        "Qué se quiere ver cruzando estas series. Se pide, no se exige: sin "
        "razón se verá como sin razón." );
    gtk_widget_set_name( x->razon, "razon" );
    gtk_box_pack_start( GTK_BOX(caja), rej2, FALSE, FALSE, 0 );

    x->aviso = gtk_label_new( "" );
    gtk_label_set_line_wrap( GTK_LABEL(x->aviso), TRUE );
    gtk_label_set_max_width_chars( GTK_LABEL(x->aviso), 70 );
    gtk_label_set_xalign( GTK_LABEL(x->aviso), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), x->aviso, FALSE, FALSE, 0 );

    nc_filas( x );
    g_signal_connect( x->mu, "changed", G_CALLBACK(nc_muestra), x );
    gtk_widget_show_all( x->d );

    /* SI NO CUADRA, SE DICE Y EL DIALOGO SIGUE: lo que el analista marco no
       se pierde por un .pre que acaba en otra fecha.                  */
    for ( ;; )
        {
        char why[1024];

        r = gtk_dialog_run( GTK_DIALOG(x->d) );
        if ( r != GTK_RESPONSE_OK ) break;
        if ( nc_acepta( x, why, sizeof why ) ) break;
        {
        gchar *mk = g_markup_printf_escaped(
            "<span foreground=\"#b3261e\">%s</span>", why );

        gtk_label_set_markup( GTK_LABEL(x->aviso), mk );
        g_free( mk );
        }
        barra_pub( a, why );
        }
    gtk_widget_destroy( x->d );
    g_free( x );
    atsw_refresca( a );
}

/* ------------------------------------------------------------------------ */
/* LA SECCION CASOS Y LA VISTA DEL CASO                                      */
/* ------------------------------------------------------------------------ */

static const PrCaso *caso_marcado( Atsw *a )
{
    if ( !a->hay || !a->viendo_caso || !a->caso[0] ) return NULL;
    return pr_caso_ver( a->p, a->caso );
}

static void on_caso( GtkTreeSelection *sel, Atsw *a )
{
    gchar *s;

    (void) sel;
    if ( a->recolocando ) return;
    s = atsw_marcada( a->l_casos, CA_ID );
    if ( s == NULL ) return;
    snprintf( a->caso, sizeof a->caso, "%s", s );
    g_free( s );
    a->viendo_caso = TRUE;
    atsw_refresca( a );
}

static void on_nuevo_caso( GtkButton *b, Atsw *a )
     { (void) b; atsw_caso_nuevo( a, NULL ); }

static void on_derivar( GtkButton *b, Atsw *a )
{
    (void) b;
    if ( caso_marcado( a ) == NULL ) { barra_pub( a, "Marca un caso." ); return; }
    atsw_caso_nuevo( a, a->caso );
}

void atsw_caso_lanza( Atsw *a, const char *programa )
{
    gchar *corrida;

    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }
    if ( caso_marcado( a ) == NULL ) { barra_pub( a, "Marca un caso." ); return; }

    /* LA CORRIDA, SOLO SI SE MARCO. Sin ella drtran parte de la elegida, o
       de nada: elegir por el analista una de partida seria decidir algo
       que no ha dicho.                                                */
    corrida = atsw_marcada( a->c_corridas, CO_ID );
    atsw_lanza_caso( a, programa, a->caso, corrida );
    g_free( corrida );
}

static void on_c_drtran( GtkButton *b, Atsw *a )
     { (void) b; atsw_caso_lanza( a, "drtran_gui" ); }

static gchar *corrida_marcada( Atsw *a )
{
    gchar *c = caso_marcado( a ) ? atsw_marcada( a->c_corridas, CO_ID ) : NULL;

    if ( c == NULL ) barra_pub( a, "Marca una corrida del caso." );
    return c;
}

/* ELEGIR UNA CORRIDA, con su porque: como el elegido de una serie, uno por
   caso, y la razon se pide sin exigirse.                              */
static void on_c_elegir( GtkButton *b, Atsw *a )
{
    gchar           *co = corrida_marcada( a );
    const PrCorrida *r;
    char             razon[PR_RAZON] = "";
    PrError          e;

    (void) b;
    if ( co == NULL ) return;
    r = pr_corrida_ver( a->p, a->caso, co );
    if ( atsw_pide_texto( a, "La corrida elegida",
            "Por qué ésta y no otra. Se puede dejar en blanco: sin razón se "
            "verá como sin razón, que es mejor que una inventada.",
            r ? r->razon_elegido : "", razon, sizeof razon ) )
        {
        if ( pr_corrida_elige( a->p, a->caso, co, razon, &e ) != 0 ||
             atsw_guarda( a, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Corrida elegida y guardada." );
        atsw_refresca( a );
        }
    g_free( co );
}

static void on_c_razon( GtkButton *b, Atsw *a )
{
    gchar           *co = corrida_marcada( a );
    const PrCorrida *r;
    char             razon[PR_RAZON] = "";
    PrError          e;

    (void) b;
    if ( co == NULL ) return;
    r = pr_corrida_ver( a->p, a->caso, co );
    if ( atsw_pide_texto( a, "El porqué de esta corrida",
            "Qué cambió respecto de la corrida de la que cuelga: un enlace "
            "fuera, otra (b, r, s), una restricción. Se puede poner después, "
            "mirando el .out.", r ? r->razon : "", razon, sizeof razon ) )
        {
        if ( pr_corrida_razon( a->p, a->caso, co, razon, &e ) != 0 ||
             atsw_guarda( a, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, razon[0] ? "Razón guardada."
                                   : "Sin razón: se seguirá viendo como tal." );
        atsw_refresca( a );
        }
    g_free( co );
}

static void on_c_razon_caso( GtkButton *b, Atsw *a )
{
    const PrCaso *c = caso_marcado( a );
    char          razon[PR_RAZON] = "";
    PrError       e;

    (void) b;
    if ( c == NULL ) { barra_pub( a, "Marca un caso." ); return; }
    if ( atsw_pide_texto( a, "El porqué de este caso",
            "Qué se quiere ver cruzando estas series. Se pide, no se exige.",
            c->razon, razon, sizeof razon ) )
        {
        if ( pr_caso_razon( a->p, a->caso, razon, &e ) != 0 ||
             atsw_guarda( a, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, razon[0] ? "Razón guardada."
                                   : "Sin razón: se seguirá viendo como tal." );
        atsw_refresca( a );
        }
}

/* LOS FICHEROS DE UNA CORRIDA, los que drtran_gui escribe con el nombre de
   cortesia. Como con un modelo: o se va entera o no se va -- la siguiente
   corrida puede volver a llamarse igual y se encontraria un .out ajeno. */
static const char *const ext_corrida[] =
    { ".out", ".dag", ".cns", "_res.txt", "_eval.csv", NULL };

static gboolean confirma( Atsw *a, const char *pregunta, const char *detalle )
{
    GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(a->ventana),
                       GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                       GTK_BUTTONS_OK_CANCEL, "%s", pregunta );
    int        r;

    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d), "%s",
                                              detalle );
    r = gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
    return r == GTK_RESPONSE_OK;
}

static void on_c_borrar( GtkButton *b, Atsw *a )
{
    gchar   *co = corrida_marcada( a );
    PrError  e;
    char     why[512], out[PR_RUTA];
    gchar   *q, *det;
    int      i;

    (void) b;
    if ( co == NULL ) return;

    /* Se pregunta A LA LIBRERIA antes que al analista, en una copia en el
       monton (~1,2 MB): si de ella cuelgan otras, la pregunta sobra.  */
    {
    Proyecto *tmp = g_memdup2( a->p, sizeof *a->p );
    int       no  = pr_corrida_borra( tmp, a->caso, co, &e );

    g_free( tmp );
    if ( no != 0 )
        { pr_error_es( &e, why, sizeof why ); barra_pub( a, why );
          g_free( co ); return; }
    }

    pr_corrida_ruta( a->p, a->caso, co, ".out", out, sizeof out );
    q   = g_strdup_printf( "¿Borro la corrida %s de %s?", co, a->caso );
    det = g_strdup_printf( "Se van su nodo del manifiesto Y sus ficheros (%s y "
                           "los que lleven su nombre: .dag, .cns, residuos, "
                           "evaluación). No se puede deshacer.\n\nLos .pre de "
                           "las entradas son de las series y no se tocan.", out );
    if ( !confirma( a, q, det ) )
        { g_free( q ); g_free( det ); g_free( co ); return; }
    g_free( q ); g_free( det );

    for ( i = 0; ext_corrida[i]; i++ )
        {
        char f[PR_RUTA];

        if ( pr_corrida_ruta( a->p, a->caso, co, ext_corrida[i], f, sizeof f ) == 0 )
            g_unlink( f );
        }
    pr_corrida_borra( a->p, a->caso, co, &e );
    if ( atsw_guarda( a, &e ) != 0 )
        barra_pub( a, "Los ficheros se fueron, pero no pude guardar el "
                      "proyecto." );
    else
        {
        gchar *t = g_strdup_printf( "%s: corrida %s borrada, con sus ficheros.",
                                    a->caso, co );

        barra_pub( a, t );
        g_free( t );
        }
    g_free( co );
    atsw_refresca( a );
}

static void on_c_borrar_caso( GtkButton *b, Atsw *a )
{
    PrError  e;
    char     why[512], id[PR_ID];
    gchar   *q;

    (void) b;
    if ( caso_marcado( a ) == NULL ) { barra_pub( a, "Marca un caso." ); return; }
    snprintf( id, sizeof id, "%s", a->caso );

    {
    Proyecto *tmp = g_memdup2( a->p, sizeof *a->p );
    int       no  = pr_caso_borra( tmp, id, &e );

    g_free( tmp );
    if ( no != 0 )
        { pr_error_es( &e, why, sizeof why ); barra_pub( a, why ); return; }
    }

    q = g_strdup_printf( "¿Borro el caso %s?", id );
    if ( !confirma( a, q, "Se va del manifiesto: qué series se cruzaban, en "
                          "qué orden y con qué .pre. No tiene corridas.\n\nLos "
                          ".pre de las entradas son de las series y no se "
                          "tocan." ) )
        { g_free( q ); return; }
    g_free( q );

    pr_caso_borra( a->p, id, &e );
    /* Su carpeta, si quedo vacia: sin corridas no hay nada dentro. */
    {
    char   dir[PR_RUTA];
    gchar *w;

    if ( pr_corrida_ruta( a->p, id, NULL, NULL, dir, sizeof dir ) == 0 )
        {
        w = g_build_filename( dir, "work", NULL );
        g_rmdir( w );
        g_rmdir( dir );
        g_free( w );
        }
    }
    a->caso[0] = '\0';
    a->viendo_caso = FALSE;
    if ( atsw_guarda( a, &e ) != 0 )
        { pr_error_es( &e, why, sizeof why ); barra_pub( a, why ); }
    else
        {
        gchar *t = g_strdup_printf( "Caso %s borrado.", id );

        barra_pub( a, t );
        g_free( t );
        }
    atsw_refresca( a );
}

void atsw_casos_panel( Atsw *a, GtkWidget *izq )
{
    GtkWidget    *h, *l;
    GtkListStore *st;

    h = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l), "<b>Casos</b>" );
    gtk_widget_set_tooltip_text( l,
        "Lo que se cruza de varias series a la vez: cada una con el .pre con "
        "que entra, en un orden que es parte del caso. drtran estima sus "
        "corridas." );
    gtk_box_pack_start( GTK_BOX(h), l, FALSE, FALSE, 4 );
    a->b_nuevo_caso = gtk_button_new_with_label( "Nuevo caso…" );
    gtk_widget_set_tooltip_text( a->b_nuevo_caso,
        "Qué series se cruzan, cada una con qué modelo estimado, y en qué "
        "orden. La ventana común se comprueba antes de aceptar." );
    g_signal_connect( a->b_nuevo_caso, "clicked", G_CALLBACK(on_nuevo_caso), a );
    gtk_box_pack_end( GTK_BOX(h), a->b_nuevo_caso, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(izq), h, FALSE, FALSE, 2 );

    st = gtk_list_store_new( CA_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    a->l_casos = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    atsw_columna( a->l_casos, "Caso",    CA_ID );
    atsw_columna( a->l_casos, "Elegida", CA_ELEGIDA );
    atsw_columna( a->l_casos, "",        CA_MARCA );
    atsw_columna( a->l_casos, "Título",  CA_TITULO );
    gtk_tree_view_set_tooltip_column( GTK_TREE_VIEW(a->l_casos), CA_GLOBO );
    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_casos) ),
                      "changed", G_CALLBACK(on_caso), a );
    gtk_box_pack_start( GTK_BOX(izq), atsw_en_scroll( a->l_casos ), TRUE, TRUE, 0 );
}

static GtkWidget *boton_caso( GtkWidget *caja, const char *txt, const char *tip,
                              GCallback cb, Atsw *a )
{
    GtkWidget *b = gtk_button_new_with_label( txt );

    gtk_widget_set_tooltip_text( b, tip );
    g_signal_connect( b, "clicked", cb, a );
    gtk_box_pack_start( GTK_BOX(caja), b, FALSE, FALSE, 0 );
    return b;
}

static GtkWidget *titulo_seccion( const char *mk )
{
    GtkWidget *l = gtk_label_new( NULL );

    gtk_label_set_markup( GTK_LABEL(l), mk );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    return l;
}

GtkWidget *atsw_caso_vista( Atsw *a )
{
    GtkWidget    *v, *b2, *s;
    GtkListStore *le;
    GtkTreeStore *lr;

    v  = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    b2 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );

    a->b_c_drtran = boton_caso( b2, "Abrir en drtran",
        "drtran_gui con este caso: sus series en su orden, comprobando los "
        "hashes. Si hay una corrida marcada, parte de ella; si no, de la "
        "elegida.", G_CALLBACK(on_c_drtran), a );
    gtk_box_pack_start( GTK_BOX(b2), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 4 );
    a->b_c_elegir = boton_caso( b2, "Elegir",
        "Declara que la corrida marcada es LA del caso.",
        G_CALLBACK(on_c_elegir), a );
    a->b_c_razon = boton_caso( b2, "Razón…",
        "El porqué de la corrida marcada. Se puede poner después, mirando el "
        ".out, o no ponerse.", G_CALLBACK(on_c_razon), a );
    a->b_c_borrar = boton_caso( b2, "Borrar corrida…",
        "La corrida marcada y sus ficheros. Se pregunta antes. Una de la que "
        "cuelgue otra no se borra.", G_CALLBACK(on_c_borrar), a );
    gtk_box_pack_start( GTK_BOX(b2), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 4 );
    a->b_c_derivar = boton_caso( b2, "Derivar caso…",
        "Otro caso con las entradas cambiadas --por defecto el elegido y el "
        ".pre de hoy--, colgado de éste. Es la salida de los dos desfases: "
        "este no se reescribe nunca.", G_CALLBACK(on_derivar), a );
    a->b_c_razon_caso = boton_caso( b2, "Razón del caso…",
        "Qué se quiere ver cruzando estas series.",
        G_CALLBACK(on_c_razon_caso), a );
    a->b_c_borrar_caso = boton_caso( b2, "Borrar caso…",
        "Sólo si no tiene corridas: son estimaciones con su .out.",
        G_CALLBACK(on_c_borrar_caso), a );
    gtk_box_pack_start( GTK_BOX(v), b2, FALSE, FALSE, 0 );

    a->c_cabeza = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(a->c_cabeza), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(a->c_cabeza), TRUE );
    gtk_box_pack_start( GTK_BOX(v), a->c_cabeza, FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX(v), titulo_seccion(
        "<b>Entradas</b>  <small>en su orden: el orden es parte del caso</small>" ),
        FALSE, FALSE, 0 );
    le = gtk_list_store_new( EN_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    a->c_entradas = gtk_tree_view_new_with_model( GTK_TREE_MODEL(le) );
    g_object_unref( le );
    atsw_columna( a->c_entradas, "#",           EN_POS );
    atsw_columna( a->c_entradas, "Serie",       EN_SERIE );
    atsw_columna( a->c_entradas, "Modelo",      EN_MODELO );
    atsw_columna( a->c_entradas, ".pre",        EN_PRE );
    atsw_columna( a->c_entradas, "Elegido hoy", EN_HOY );
    atsw_columna( a->c_entradas, "Nota",        EN_NOTA );
    gtk_widget_set_tooltip_text( a->c_entradas,
        ".pre: si el de hoy es el mismo que en el alta (sha256). Si cambió o "
        "no está, lo estimado en el caso ya no corresponde a sus datos.\n\n"
        "Elegido hoy: el modelo elegido de la serie en esta muestra. Si es "
        "otro, es una nota y no una alarma: a menudo es deliberado." );
    s = atsw_en_scroll( a->c_entradas );
    gtk_scrolled_window_set_min_content_height( GTK_SCROLLED_WINDOW(s), 110 );
    gtk_box_pack_start( GTK_BOX(v), s, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(v), titulo_seccion(
        "<b>Corridas</b>  <small>el linaje: cada una cuelga de la que se "
        "cargó</small>" ), FALSE, FALSE, 0 );
    lr = gtk_tree_store_new( CO_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING );
    a->c_corridas = gtk_tree_view_new_with_model( GTK_TREE_MODEL(lr) );
    g_object_unref( lr );
    atsw_columna( a->c_corridas, "Corrida",      CO_ID );
    atsw_columna( a->c_corridas, "",             CO_ESTRELLA );
    atsw_columna( a->c_corridas, "logL",         CO_LOGL );
    atsw_columna( a->c_corridas, "Puerta diag.", CO_PUERTA );
    atsw_columna( a->c_corridas, "Estado",       CO_ESTADO );
    atsw_columna( a->c_corridas, "Por qué",      CO_RAZON );
    gtk_tree_view_set_tooltip_column( GTK_TREE_VIEW(a->c_corridas), CO_GLOBO );
    gtk_box_pack_start( GTK_BOX(v), atsw_en_scroll( a->c_corridas ), TRUE, TRUE, 0 );
    return v;
}

/* UNA CORRIDA Y LAS QUE CUELGAN DE ELLA. Recursiva, como el linaje de los
   modelos: la cadena puede ser larga pero no es ancha.                */
static void cuelga_corridas( Atsw *a, const PrCaso *c, GtkTreeStore *st,
                             GtkTreeIter *padre, const char *de,
                             gboolean desfasado, const char *marca,
                             GtkTreeIter *marcada, gboolean *hay_marca )
{
    const char *eleg = pr_corrida_elegida( a->p, c->id );
    int         i;

    for ( i = 0; i < a->p->nco; i++ )
        {
        const PrCorrida *r = &a->p->co[i];
        GtkTreeIter      it;
        char             out[PR_RUTA], logl[48] = "—";
        gboolean         hay_out;
        gchar           *globo;

        if ( strcmp( r->caso, c->id ) || strcmp( r->padre, de ) ) continue;

        /* LA logL, DEL .out Y CON EL LECTOR DE drtran_gui. Lo que no esta
           se enseña como «—», no como un cero.                         */
        hay_out = pr_corrida_ruta( a->p, c->id, r->id, ".out", out,
                                   sizeof out ) == 0 &&
                  g_file_test( out, G_FILE_TEST_EXISTS );
        if ( hay_out )
            {
            Diagnosis *d = g_new0( Diagnosis, 1 );

            if ( od_parse_file( out, d ) != -1 && d->tiene_logl )
                g_ascii_formatd( logl, sizeof logl, "%.2f", d->logl );
            g_free( d );
            }

        globo = g_strdup_printf(
            "%s · versión %d%s%s · creada %s\n%s%s%s\n\n"
            "Puerta diagonal: la suma de las logL univariantes contra la "
            "conjunta del diagonal. drtran no la escribe en el .out; la "
            "contrasta la Diagnosis de drtran_gui con su baseline.",
            r->id, r->version, r->padre[0] ? " · cuelga de " : "", r->padre,
            r->creado[0] ? r->creado : "—",
            hay_out ? out : "Sin .out: no se ha estimado, o se borró.",
            r->elegido ? "\n\nLa elegida: " : "",
            r->elegido ? ( r->razon_elegido[0] ? r->razon_elegido
                                               : "(sin razón)" ) : "" );

        gtk_tree_store_append( st, &it, padre );
        gtk_tree_store_set( st, &it,
            CO_ID,       r->id,
            CO_ESTRELLA, ( eleg[0] && !strcmp( eleg, r->id ) ) ? "★" : "",
            CO_RAZON,    r->razon[0] ? r->razon : "(sin razón)",
            CO_LOGL,     logl,
            CO_PUERTA,   "—",
            CO_ESTADO,   desfasado ? "⚠ desfasada"
                                  : ( hay_out ? "" : "sin .out" ),
            CO_GLOBO,    globo,
            -1 );
        g_free( globo );
        if ( marca && !strcmp( marca, r->id ) ) { *marcada = it; *hay_marca = TRUE; }

        cuelga_corridas( a, c, st, &it, r->id, desfasado, marca, marcada,
                         hay_marca );
        }
}

static void pinta_vista( Atsw *a )
{
    const PrCaso *c = caso_marcado( a );
    GtkListStore *le = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(a->c_entradas) ) );
    GtkTreeStore *lr = GTK_TREE_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(a->c_corridas) ) );
    gchar        *marca = atsw_marcada( a->c_corridas, CO_ID );
    char          des[1024];
    int           i, nd;

    gtk_list_store_clear( le );
    gtk_tree_store_clear( lr );
    if ( c == NULL )
        { gtk_label_set_text( GTK_LABEL(a->c_cabeza), "" ); g_free( marca );
          return; }

    nd = atsw_caso_desfases( a->p, c, des, sizeof des );

    /* LA CABECERA. "Sin razón" se ve como sin razón: nunca se inventa. */
    {
    GString *g = g_string_new( NULL );
    gchar   *t;

    t = g_markup_printf_escaped( "<big><b>%s — %s</b></big>    %s · muestra %s",
                                 c->id, c->titulo[0] ? c->titulo : "(sin título)",
                                 c->motor[0] ? c->motor : "drtran",
                                 c->muestra[0] ? c->muestra : "completa" );
    g_string_append( g, t ); g_free( t );
    t = g_markup_printf_escaped( "\n%s%s%screado %s",
                                 c->padre[0] ? "derivado de " : "", c->padre,
                                 c->padre[0] ? " · " : "",
                                 c->creado[0] ? c->creado : "—" );
    g_string_append( g, t ); g_free( t );
    if ( c->razon[0] )
        t = g_markup_printf_escaped( "\nRazón: %s", c->razon );
    else
        t = g_strdup( "\nRazón: <i>sin razón</i>" );
    g_string_append( g, t ); g_free( t );
    if ( nd )
        {
        t = g_markup_printf_escaped( "\n<span foreground=\"#b3261e\">⚠ "
                                     "desfasado: el .pre de %s después del "
                                     "alta. Lo estimado aquí ya no corresponde "
                                     "a sus datos: «Derivar caso…».</span>",
                                     des );
        g_string_append( g, t ); g_free( t );
        }
    gtk_label_set_markup( GTK_LABEL(a->c_cabeza), g->str );
    g_string_free( g, TRUE );
    }

    for ( i = 0; i < c->nen; i++ )
        {
        static const char *pre[] = { "igual", "cambió", "no está", "sin hash" };
        const char *hoy = pr_elegido( a->p, c->en[i].serie, c->muestra );
        GtkTreeIter it;
        char        pos[16];

        snprintf( pos, sizeof pos, "%d", i + 1 );
        gtk_list_store_append( le, &it );
        gtk_list_store_set( le, &it,
            EN_POS,    pos,
            EN_SERIE,  c->en[i].serie,
            EN_MODELO, c->en[i].modelo,
            EN_PRE,    pre[atsw_entrada_pre( a->p, c, i )],
            EN_HOY,    hoy[0] ? hoy : "—",
            /* UNA NOTA, NO UNA ALARMA: el modelo que se cruza no tiene por
               que ser el de prever (art, mcp_server.py:557).           */
            EN_NOTA,   ( hoy[0] && strcmp( hoy, c->en[i].modelo ) )
                      ? "otro elegido hoy (nota)" : "",
            -1 );
        }

    {
    GtkTreeIter marcada;
    gboolean    hay = FALSE;

    cuelga_corridas( a, c, lr, NULL, "", nd > 0, marca, &marcada, &hay );
    gtk_tree_view_expand_all( GTK_TREE_VIEW(a->c_corridas) );
    if ( hay )
        gtk_tree_selection_select_iter(
            gtk_tree_view_get_selection( GTK_TREE_VIEW(a->c_corridas) ),
            &marcada );
    }
    g_free( marca );
}

void atsw_pinta_casos( Atsw *a )
{
    GtkListStore *st;
    int           i;

    if ( a->l_casos == NULL ) return;
    st = GTK_LIST_STORE( gtk_tree_view_get_model( GTK_TREE_VIEW(a->l_casos) ) );

    /* Un caso que ya no esta --borrado, o un manifiesto releido sin el-- no
       se sigue enseñando.                                              */
    if ( !a->hay || ( a->caso[0] && pr_caso_idx( a->p, a->caso ) < 0 ) )
        { a->caso[0] = '\0'; a->viendo_caso = FALSE; }

    a->recolocando = TRUE;
    gtk_list_store_clear( st );
    for ( i = 0; a->hay && i < a->p->nca; i++ )
        {
        const PrCaso *c = &a->p->ca[i];
        const char   *eleg = pr_corrida_elegida( a->p, c->id );
        char          des[1024], ent[1024] = "", el[64];
        GtkTreeIter   it;
        gchar        *globo;
        int           k, nd;

        nd = atsw_caso_desfases( a->p, c, des, sizeof des );
        for ( k = 0; k < c->nen; k++ )
            {
            size_t l = strlen( ent );

            snprintf( ent + l, sizeof ent - l, "%s%d. %s/%s", k ? "\n" : "",
                      k + 1, c->en[k].serie, c->en[k].modelo );
            }
        globo = g_strdup_printf( "%s · %s · muestra %s\n\n%s\n\n%s%s%s",
                    c->titulo[0] ? c->titulo : "(sin título)",
                    c->motor[0] ? c->motor : "drtran",
                    c->muestra[0] ? c->muestra : "completa", ent,
                    c->razon[0] ? c->razon : "(sin razón)",
                    nd ? "\n\n⚠ Desfasado: " : "", nd ? des : "" );
        if ( eleg[0] ) snprintf( el, sizeof el, "%s ★", eleg );
        else           snprintf( el, sizeof el, "—" );

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            CA_ID,      c->id,
            CA_TITULO,  c->titulo,
            CA_ELEGIDA, el,
            /* EL ⚠ ES SOLO EL DESFASE DE VERDAD. Otro elegido hoy no
               lleva marca aqui: es una nota, y una marca seria alarma. */
            CA_MARCA,   nd ? "⚠" : "",
            CA_GLOBO,   globo,
            -1 );
        g_free( globo );
        if ( a->viendo_caso && !strcmp( a->caso, c->id ) )
            gtk_tree_selection_select_iter(
                gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_casos) ), &it );
        }
    if ( a->viendo_caso )
        gtk_tree_selection_unselect_all(
            gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_series) ) );
    a->recolocando = FALSE;

    pinta_vista( a );

    if ( a->pila )
        {
        GtkWidget *hijo = gtk_stack_get_child_by_name( GTK_STACK(a->pila),
                              a->viendo_caso ? "caso" : "modelos" );

        /* Antes del primer show_all los hijos no estan visibles, y la pila
           no deja poner delante a uno que no lo este.                 */
        if ( hijo && gtk_widget_get_visible( hijo ) )
            gtk_stack_set_visible_child( GTK_STACK(a->pila), hijo );
        }
    gtk_widget_set_sensitive( a->b_nuevo_caso, a->hay );
}
