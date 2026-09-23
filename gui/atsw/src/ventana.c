/*
 * ventana.c -- la ventana de la madre. Ver atsw.h.
 */

#include <string.h>
#include <sys/stat.h>

#include <glib/gstdio.h>

#include "outfile.h"

#include "atsw.h"

/* Un veredicto: una linea de altura fija con su punto de color. Es la regla
 * R3 del diseño, la misma que en las siete paginas de drtran_gui.        */
#define AT_VERDE  "#1a7f37"
#define AT_AMBAR  "#9a6700"
#define AT_ROJO   "#b3261e"

static void verdicto( GtkWidget *w, const char *color, const char *fmt, ... )
     G_GNUC_PRINTF( 3, 4 );

static void verdicto( GtkWidget *w, const char *color, const char *fmt, ... )
{
    va_list ap;
    gchar  *t, *esc, *mk;

    va_start( ap, fmt );
    t = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    esc = g_markup_escape_text( t, -1 );
    mk  = g_strdup_printf( "<span foreground=\"%s\">●</span>  %s", color, esc );
    gtk_label_set_markup( GTK_LABEL(w), mk );
    g_free( mk ); g_free( esc ); g_free( t );
}

/* ------------------------------------------------------------------------ */
/* LOS RESULTADOS, DEL .out Y CON SU HUELLA                                  */
/*                                                                           */
/* Cachear sigma y Q en el manifiesto seria mas rapido y PODRIA MENTIR: si    */
/* alguien reestima por fuera --y puede, porque los motores son programas--   */
/* el numero guardado seguiria diciendo lo de antes. El manifiesto guarda     */
/* LINAJE y RAZON, que son decisiones; los resultados son del .out, que es el */
/* registro.                                                                 */
/*                                                                           */
/* La huella es tamaño + fecha, no un SHA: lo que tiene que cazar es que el   */
/* fichero haya cambiado desde que se leyo, y para eso basta.                */
/* ------------------------------------------------------------------------ */

static AtRes *hueco( Atsw *a, const char *serie, const char *id )
{
    int i;

    for ( i = 0; i < a->nr; i++ )
        if ( !strcmp( a->r[i].serie, serie ) && !strcmp( a->r[i].id, id ) )
            return &a->r[i];
    if ( a->nr >= AT_MAX_RES ) return NULL;

    memset( &a->r[a->nr], 0, sizeof a->r[0] );
    snprintf( a->r[a->nr].serie, PR_ID, "%s", serie );
    snprintf( a->r[a->nr].id, PR_ID, "%s", id );
    return &a->r[a->nr++];
}

const AtRes *atsw_resultado( Atsw *a, const char *serie, const char *id )
{
    AtRes      *r = hueco( a, serie, id );
    char        path[PR_RUTA];
    GStatBuf    st;

    if ( r == NULL ) return NULL;
    if ( pr_ruta( a->p, serie, id, ".out", path, sizeof path ) != 0 )
        { r->hay = FALSE; return r; }

    if ( g_stat( path, &st ) != 0 )
        {
        /* No esta: puede que no se haya estimado aun. No es un error, es un
           hecho, y la rejilla lo enseña como tal.                        */
        r->hay = FALSE;
        r->tam = r->mtime = 0;
        return r;
        }

    /* Si la huella no ha cambiado, lo leido sigue valiendo. */
    if ( r->hay && r->tam == (long) st.st_size && r->mtime == (long) st.st_mtime )
        return r;

    r->tam   = (long) st.st_size;
    r->mtime = (long) st.st_mtime;
    r->hay   = FALSE;

    /* EL LECTOR DE fue, no el de drtran. Son dos formatos: aquel trae el
       portmanteau de Hosking --multivariante-- y este el Ljung-Box de la
       ACF de los residuos. Darle el .out de fue al otro no falla, no
       encuentra nada, que es por lo que "Hosking" salia siempre vacia. */
    {
    FueOut o;

    if ( fueout_read( path, &o ) )
        {
        r->hay  = TRUE;
        r->sd   = o.sd;
        r->npar = o.npar;
        r->q    = o.tiene_lb ? o.lb_q : 0.0;
        r->qdf  = o.tiene_lb ? o.lb_df : 0;
        r->qp   = o.tiene_lb ? o.lb_p : -1.0;
        r->jb   = o.tiene_jb ? o.jb : 0.0;
        r->jbp  = o.tiene_jb ? o.jb_p : -1.0;
        r->skew = o.skew;
        r->kurt = o.kurt;
        fueout_estructura( &o, r->estruct, sizeof r->estruct );
        }
    }
    return r;
}

/* ------------------------------------------------------------------------ */

const char *atsw_modelo_por_defecto( const Proyecto *p, const char *serie )
{
    const char *eleg;
    const char *ultimo = "";
    int         i, maxv = -1;

    if ( p == NULL || serie == NULL || !*serie ) return "";

    eleg = pr_elegido( p, serie );
    if ( eleg && *eleg ) return eleg;

    /* El ULTIMO por version, que es un CAMPO -- no se deduce del nombre. */
    for ( i = 0; i < p->nm; i++ )
        if ( !strcmp( p->m[i].serie, serie ) && p->m[i].version > maxv )
            { maxv = p->m[i].version; ultimo = p->m[i].id; }

    return ultimo;
}

gboolean atsw_abre( Atsw *a, const char *path, char *why, size_t n )
{
    PrError e;

    if ( why && n ) why[0] = '\0';
    if ( !a->p ) a->p = g_new0( Proyecto, 1 );

    if ( pr_leer( path, a->p, &e ) != 0 )
        {
        if ( e.cod != PR_ENOFILE )
            { if ( why ) pr_error_es( &e, why, n ); return FALSE; }
        pr_nuevo( a->p, "proyecto", "", "." );
        snprintf( a->p->path, sizeof a->p->path, "%s", path );
        }
    a->hay = TRUE;
    a->nr  = 0;
    a->serie[0] = '\0';
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Pintar                                                                    */
/* ------------------------------------------------------------------------ */

/* LO QUE SE SABE DE LA SERIE, y SOLO lo que se sabe.
 *
 * Un campo vacio no sale: "Fuente: (sin declarar)" repetido seis veces es
 * ruido, y ademas es mentira por insinuacion -- parece que falta algo que
 * habria que poner, cuando la mayoria de las series no necesitan las seis.
 * Si no hay nada, se dice que no hay nada y se dice donde ponerlo.    */
static gchar *serie_globo( const PrSerie *s )
{
    GString *g = g_string_new( NULL );

    if ( s->descripcion[0] ) g_string_append_printf( g, "%s\n", s->descripcion );
    if ( s->unidades[0] )    g_string_append_printf( g, "Unidades: %s\n", s->unidades );
    if ( s->fuente[0] )      g_string_append_printf( g, "Fuente: %s\n", s->fuente );
    if ( s->url[0] )         g_string_append_printf( g, "%s\n", s->url );
    if ( s->bajada[0] )      g_string_append_printf( g, "Bajada: %s\n", s->bajada );
    if ( s->notas[0] )       g_string_append_printf( g, "\n%s\n", s->notas );

    if ( g->len == 0 )
        g_string_append( g, "De esta serie no consta nada más que su clave.\n"
                            "Botón derecho → «Editar la serie…»." );
    else
        g_string_append( g, "\nBotón derecho → «Editar la serie…»." );

    return g_string_free( g, FALSE );
}

static void pinta_series( Atsw *a )
{
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(a->l_series) ) );
    GtkTreeIter   it;
    int           i, j;
    char          marcada[PR_ID];

    /* La marca se GUARDA y se repone: repintar no puede cambiar lo que el
       analista tenia elegido.                                          */
    snprintf( marcada, sizeof marcada, "%s", a->serie );

    a->recolocando = TRUE;
    gtk_list_store_clear( st );
    if ( !a->hay ) { a->recolocando = FALSE; return; }

    for ( i = 0; i < a->p->ns; i++ )
        {
        gchar *globo;
        int    nm = 0;

        for ( j = 0; j < a->p->nm; j++ )
            if ( !strcmp( a->p->m[j].serie, a->p->s[i].id ) ) nm++;

        globo = serie_globo( &a->p->s[i] );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            S_ID,      a->p->s[i].id,
            /* EL ELEGIDO, A LA VISTA. Hoy esa decision vive en un diccionario
               a pelo repetido en tres guiones de cases/.                 */
            S_ELEGIDO, a->p->s[i].elegido[0] ? a->p->s[i].elegido : "—",
            S_NMOD,    nm,
            S_RAZON,   a->p->s[i].razon,
            S_GLOBO,   globo,
            -1 );
        g_free( globo );

        if ( marcada[0] && !strcmp( marcada, a->p->s[i].id ) )
            gtk_tree_selection_select_iter(
                gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_series) ), &it );
        }
    a->recolocando = FALSE;
}

static void pinta_modelos( Atsw *a )
{
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(a->l_modelos) ) );
    GtkTreeIter   it;
    const char   *eleg, *porde;
    int           i;

    gtk_list_store_clear( st );
    if ( !a->hay || a->serie[0] == '\0' ) return;

    eleg  = pr_elegido( a->p, a->serie );
    porde = atsw_modelo_por_defecto( a->p, a->serie );

    for ( i = 0; i < a->p->nm; i++ )
        {
        const PrModelo *m = &a->p->m[i];
        const AtRes    *r;
        char            sd[32], q[48], pv[16];
        gchar          *globo;

        if ( strcmp( m->serie, a->serie ) != 0 ) continue;

        r = atsw_resultado( a, m->serie, m->id );

        if ( r && r->hay )
            {
            snprintf( sd, sizeof sd, "%.4f", r->sd );
            if ( r->qdf > 0 )
                { snprintf( q, sizeof q, "%.1f (%d)", r->q, r->qdf );
                  snprintf( pv, sizeof pv, "%.3f", r->qp ); }
            else
                { snprintf( q, sizeof q, "—" ); snprintf( pv, sizeof pv, "—" ); }

            /* EL GLOBO: lo que no decide entre modelos pero se pregunta
               del elegido. Asi la normalidad esta sin robar una columna. */
            globo = g_strdup_printf(
                "%s · %d parámetro%s\n\n"
                "Ljung-Box Q(%d) = %.2f, p = %.4f\n"
                "Jarque-Bera = %.1f, p = %.4f  (asimetría %.2f, curtosis %.2f)"
                "\n\nDoble clic para abrirlo en fue.",
                r->estruct[0] ? r->estruct : "sin estructura",
                r->npar, r->npar == 1 ? "" : "s",
                r->qdf, r->q, r->qp, r->jb, r->jbp, r->skew, r->kurt );
            }
        else
            {
            snprintf( sd, sizeof sd, "—" );
            snprintf( q,  sizeof q,  "—" );
            snprintf( pv, sizeof pv, "—" );
            globo = g_strdup( m->rol == PR_DATOS
                ? "Los datos de la serie. No se estiman: de aquí cuelga todo."
                : "Sin estimar todavía. Doble clic para abrirlo en fue." );
            }

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            M_ID,       m->id,
            M_PADRE,    m->padre[0] ? m->padre : "—",
            M_ESTRUCT,  ( r && r->hay && r->estruct[0] ) ? r->estruct
                        : ( m->rol == PR_DATOS ? "los datos" : "—" ),
            M_SD,       sd,
            M_Q,        q,
            M_P,        pv,
            /* "SIN RAZON" SE VE COMO SIN RAZON. Nunca se infiere ni se
               rellena: es la regla de la huella vacia del guion.        */
            M_RAZON,    m->rol == PR_DATOS
                        ? "tal como entraron — no se editan"
                        : ( m->razon[0] ? m->razon : "(sin razón)" ),
            M_ESTRELLA, ( eleg && !strcmp( eleg, m->id ) ) ? "★" : "",
            M_GLOBO,    globo,
            -1 );
        g_free( globo );

        /* SE MARCA EL DE POR DEFECTO, para que los botones tengan a que
         * apuntar sin exigir un segundo click. El analista puede marcar
         * otro, claro.                                                */
        if ( !strcmp( porde, m->id ) )
            gtk_tree_selection_select_iter(
                gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_modelos) ), &it );
        }
}

static void pinta_veredictos( Atsw *a )
{
    char sin[32][PR_ID];
    int  n, i, estimados = 0, elegidos = 0, datos = 0;

    if ( !a->hay )
        {
        verdicto( a->ver_cuenta, AT_AMBAR,
                  "No hay proyecto abierto. «Abrir…» o «Nuevo…»." );
        gtk_label_set_text( GTK_LABEL(a->ver_ojo), "" );
        return;
        }

    /* Los DATOS no son un modelo sin estimar: no se estiman. Contarlos
     * entre los pendientes daria un aviso que nunca se puede apagar.  */
    for ( i = 0; i < a->p->nm; i++ )
        {
        const AtRes *r;

        if ( a->p->m[i].rol == PR_DATOS ) { datos++; continue; }
        r = atsw_resultado( a, a->p->m[i].serie, a->p->m[i].id );
        if ( r && r->hay ) estimados++;
        }
    for ( i = 0; i < a->p->ns; i++ )
        if ( a->p->s[i].elegido[0] ) elegidos++;

    verdicto( a->ver_cuenta, a->p->ns ? AT_VERDE : AT_AMBAR,
        "%d serie%s, %d modelo%s, %d estimado%s · %d con el elegido declarado",
        a->p->ns, a->p->ns == 1 ? "" : "s",
        a->p->nm - datos, a->p->nm - datos == 1 ? "" : "s",
        estimados, estimados == 1 ? "" : "s", elegidos );

    /* El segundo ELIGE lo que hay que mirar, en orden de gravedad. */
    n = pr_sin_razon( a->p, sin, 32 );

    if ( a->p->ns && elegidos < a->p->ns )
        verdicto( a->ver_ojo, AT_AMBAR,
            "%d serie%s sin declarar cuál es el modelo elegido: esa decisión "
            "no está en ninguna parte hasta que se declara.",
            a->p->ns - elegidos, a->p->ns - elegidos == 1 ? "" : "s" );
    else if ( n )
        verdicto( a->ver_ojo, AT_AMBAR,
            "%d iteración%s sin razón (%s%s). El linaje está; el porqué, no.",
            n, n == 1 ? "" : "es", sin[0], n > 1 ? ", …" : "" );
    else if ( a->p->nm - datos > estimados )
        verdicto( a->ver_ojo, AT_AMBAR,
            "%d modelo%s declarado%s pero sin estimar.",
            a->p->nm - datos - estimados,
            a->p->nm - datos - estimados == 1 ? "" : "s",
            a->p->nm - datos - estimados == 1 ? "" : "s" );
    else if ( a->p->nm - datos )
        verdicto( a->ver_ojo, AT_VERDE,
            "Todo estimado, con su razón y con su elegido." );
    else
        verdicto( a->ver_ojo, AT_AMBAR,
            datos   ? "Los datos están; no hay ningún modelo todavía. Marca "
                      "una serie y mándala a fue."
          : a->p->ns ? "Hay series, pero ningún fichero: cárgalas con «Datos…»."
                     : "Todavía no hay series: «Datos…»." );
}

void atsw_refresca( Atsw *a )
{
    gchar *t;

    if ( a->hay )
        t = g_strdup_printf( "%s%s%s  —  %s",
                             a->p->id,
                             a->p->titulo[0] ? ": " : "",
                             a->p->titulo, a->p->path );
    else
        t = g_strdup( "(sin proyecto)" );
    gtk_label_set_text( GTK_LABEL(a->l_proy), t );
    g_free( t );

    pinta_series( a );
    pinta_modelos( a );
    pinta_veredictos( a );

    /* Sin datos no hay de donde empezar un modelo, y el boton lo dice
       apagandose en vez de dejar que se pulse y conteste que no.      */
    gtk_widget_set_sensitive( a->b_nuevo,
        a->hay && a->serie[0] && pr_datos_de( a->p, a->serie )[0] );
    gtk_widget_set_sensitive( a->b_iterar, a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_elegir, a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_razon,  a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_drtran, a->hay );
    gtk_widget_set_sensitive( a->b_fue,    a->hay );
    gtk_widget_set_sensitive( a->b_fug,    a->hay );
}
