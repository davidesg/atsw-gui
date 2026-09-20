/*
 * ventana.c -- la ventana de la madre. Ver atsw.h.
 */

#include <string.h>
#include <sys/stat.h>

#include <glib/gstdio.h>

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
    Diagnosis  *d;

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

    d = g_new0( Diagnosis, 1 );
    if ( od_parse_file( path, d ) == 0 )
        {
        r->hay        = TRUE;
        r->sd         = ( d->ns > 0 ) ? d->s[0].sd : 0.0;
        r->logl       = d->logl;
        r->tiene_logl = d->tiene_logl;
        r->hq         = d->hosking.q;
        r->hdf        = d->hosking.df;
        r->hp         = d->hosking.p;
        r->blanco     = d->hosking.hay ? d->hosking_blanco : TRUE;
        }
    g_free( d );
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
        int nm = 0;

        for ( j = 0; j < a->p->nm; j++ )
            if ( !strcmp( a->p->m[j].serie, a->p->s[i].id ) ) nm++;

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            S_ID,      a->p->s[i].id,
            /* EL ELEGIDO, A LA VISTA. Hoy esa decision vive en un diccionario
               a pelo repetido en tres guiones de cases/.                 */
            S_ELEGIDO, a->p->s[i].elegido[0] ? a->p->s[i].elegido : "—",
            S_NMOD,    nm,
            S_RAZON,   a->p->s[i].razon,
            -1 );

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
        char            sd[32], q[48];

        if ( strcmp( m->serie, a->serie ) != 0 ) continue;

        r = atsw_resultado( a, m->serie, m->id );

        if ( r && r->hay )
            {
            snprintf( sd, sizeof sd, "%.4f", r->sd );
            if ( r->hdf > 0 )
                snprintf( q, sizeof q, "P(%d) = %.1f, p = %.4f",
                          r->hdf, r->hq, r->hp );
            else
                snprintf( q, sizeof q, "—" );
            }
        else
            {
            /* SIN .out NO SE INVENTA NADA. Que un modelo este declarado no
               quiere decir que se haya estimado.                         */
            snprintf( sd, sizeof sd, "—" );
            snprintf( q, sizeof q, "sin estimar" );
            }

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            M_ID,       m->id,
            M_VER,      m->version,
            M_PADRE,    m->padre[0] ? m->padre : "—",
            M_SD,       sd,
            M_Q,        q,
            M_BLANCO,   ( r && r->hay ) ? ( r->blanco ? "sí" : "NO" ) : "—",
            /* "SIN RAZON" SE VE COMO SIN RAZON. Nunca se infiere ni se
               rellena: es la regla de la huella vacia del guion.        */
            /* LOS DATOS SE DICEN. Un nodo sin razon y sin estimar podria
             * parecer un modelo a medias, y no lo es: es la raiz, y no se
             * edita.                                                   */
            M_RAZON,    m->rol == PR_DATOS
                        ? "los datos, tal como entraron — no se editan"
                        : ( m->razon[0] ? m->razon : "(sin razón)" ),
            M_ESTRELLA, ( eleg && !strcmp( eleg, m->id ) ) ? "★" : "",
            -1 );

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

    gtk_widget_set_sensitive( a->b_iterar, a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_elegir, a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_razon,  a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_drtran, a->hay );
    gtk_widget_set_sensitive( a->b_fue,    a->hay );
    gtk_widget_set_sensitive( a->b_fug,    a->hay );
}
