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

static AtRes *hueco( Atsw *a, const char *serie, const char *muestra,
                     const char *id )
{
    int i;

    /* La huella es de (serie, muestra, id): con m01 en dos hojas, la
       cache tiene que distinguirlos o una enseñaria los numeros de la
       otra.                                                          */
    for ( i = 0; i < a->nr; i++ )
        if ( !strcmp( a->r[i].serie, serie ) &&
             !strcmp( a->r[i].muestra, muestra ) &&
             !strcmp( a->r[i].id, id ) )
            return &a->r[i];
    if ( a->nr >= AT_MAX_RES ) return NULL;

    memset( &a->r[a->nr], 0, sizeof a->r[0] );
    snprintf( a->r[a->nr].serie, PR_ID, "%s", serie );
    snprintf( a->r[a->nr].muestra, PR_ID, "%s", muestra );
    snprintf( a->r[a->nr].id, PR_ID, "%s", id );
    return &a->r[a->nr++];
}

const AtRes *atsw_resultado( Atsw *a, const char *serie, const char *muestra,
                             const char *id )
{
    AtRes      *r = hueco( a, serie, muestra, id );
    char        path[PR_RUTA];
    GStatBuf    st;

    if ( r == NULL ) return NULL;
    if ( pr_ruta( a->p, serie, muestra, id, ".out", path, sizeof path ) != 0 )
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

        /* EL DICTAMEN, resumido en una línea. Se calcula aquí, con la
           misma huella que lo demás: si el .out no se ha movido, no se
           vuelve a juzgar.                                           */
        {
        Convergence c;
        Dictamen    d;
        int         i;
        char       *p = r->dx;
        size_t      n = sizeof r->dx;

        if ( !convergence_of( path, &c ) ) memset( &c, 0, sizeof c );
        dx_dictamen( &o, &c, NULL, 0, &d );   /* la rejilla sólo usa el peor */
        convergence_clear( &c );

        r->peor = d.peor;
        for ( i = 0; i < d.n; i++ )
            {
            int esc = snprintf( p, n, "\n  %-16s %s", d.l[i].titulo,
                                dx_estado_es( d.l[i].estado ) );

            if ( esc < 0 || (size_t) esc >= n ) break;
            p += esc; n -= (size_t) esc;
            }
        }
        }
    }
    return r;
}

/* ------------------------------------------------------------------------ */

/* LA HOJA QUE ESTA DELANTE. "" es la completa. */
/* LA HOJA QUE ESTA DELANTE, DE UN CAMPO Y NO DEL WIDGET.
 *
 * Preguntarselo al cuaderno no valia: "switch-page" es RUN_LAST, asi que el
 * manejador de la aplicacion corre ANTES del que cambia la pagina de verdad
 * y gtk_notebook_get_current_page todavia contesta LA ANTERIOR. El repintado
 * salia con la muestra de antes, y al cambiar otra vez salia con la de antes
 * de esa: la lista parecia ir un paso por detras, o "lenta".
 *
 * Asi que el numero de hoja se GUARDA cuando se sabe --lo trae la señal-- en
 * vez de deducirlo del widget en un momento en que el widget miente.     */
const char *atsw_muestra_actual( Atsw *a )
{
    if ( a == NULL || a->hoja_actual < 0 || a->hoja_actual >= a->nhojas )
        return "";
    return a->hoja_mu[a->hoja_actual];
}

const char *atsw_modelo_por_defecto_en( const Proyecto *p, const char *serie,
                                        const char *muestra )
{
    const char *eleg;
    const char *ultimo = "";
    int         i, maxv = -1;

    if ( p == NULL || serie == NULL || !*serie ) return "";
    if ( muestra == NULL ) muestra = "";

    /* El ELEGIDO manda, pero SOLO si esta en esta muestra: el de otra
       ventana no es candidato aqui, porque no se compara con esto.   */
    eleg = pr_elegido( p, serie, muestra );
    if ( eleg && *eleg ) return eleg;

    for ( i = 0; i < p->nm; i++ )
        if ( !strcmp( p->m[i].serie, serie ) &&
             !strcmp( p->m[i].muestra, muestra ) && p->m[i].version > maxv )
            { maxv = p->m[i].version; ultimo = p->m[i].id; }

    return ultimo;
}

const char *atsw_modelo_por_defecto( const Proyecto *p, const char *serie )
{
    const char *eleg;
    const char *ultimo = "";
    int         i, maxv = -1;

    if ( p == NULL || serie == NULL || !*serie ) return "";

    eleg = pr_elegido( p, serie, "" );
    if ( eleg && *eleg ) return eleg;

    /* El ULTIMO por version, que es un CAMPO -- no se deduce del nombre. */
    for ( i = 0; i < p->nm; i++ )
        if ( !strcmp( p->m[i].serie, serie ) && p->m[i].version > maxv )
            { maxv = p->m[i].version; ultimo = p->m[i].id; }

    return ultimo;
}

/* ------------------------------------------------------------------------ */
/* RELEER EL MANIFIESTO                                                      */
/*                                                                           */
/* El proyecto vive en un fichero, y la ventana no es su unico dueño: puede  */
/* haber otra madre abierta sobre el mismo, un editor, o --lo que viene-- un */
/* agente trabajando al lado con un MCP sobre lib/proyecto. Los .out ya se   */
/* releen por huella; el manifiesto no, asi que un cambio de fuera no se veia */
/* hasta reabrir.                                                            */
/*                                                                           */
/* SE LEE A OTRO SITIO Y SOLO SE CAMBIA SI SALIO BIEN. Un manifiesto roto por */
/* fuera --a medio escribir, con una clave que no entendemos-- no puede       */
/* tirarse por delante del que tenemos en memoria, que funciona.             */
/* ------------------------------------------------------------------------ */

static void huella_manifiesto( Atsw *a )
{
    GStatBuf st;

    if ( a->hay && g_stat( a->p->path, &st ) == 0 )
        { a->p_tam = (long) st.st_size; a->p_mtime = (long) st.st_mtime; }
    else
        a->p_tam = a->p_mtime = 0;
}

/* GUARDAR ES ESCRIBIR Y APUNTAR LA HUELLA, LAS DOS COSAS.
 *
 * Si solo se escribe, la huella se queda con la del fichero de antes y el
 * siguiente golpe de foco lo relee creyendo que lo cambio otro -- y lo dice.
 * Un aviso falso de "esto ha cambiado fuera de aqui" es peor que no avisar:
 * enseña a no creerse los avisos.                                       */
int atsw_guarda( Atsw *a, PrError *e )
{
    int rc;

    if ( !a->hay || a->p == NULL ) return 1;
    rc = pr_escribir( a->p, a->p->path, e );
    huella_manifiesto( a );
    return rc;
}

gboolean atsw_relee( Atsw *a, char *why, size_t n )
{
    GStatBuf  st;
    Proyecto *nuevo;
    PrError   e;
    char      path[PR_RUTA];

    if ( why && n ) why[0] = '\0';
    if ( !a->hay || a->p == NULL ) return FALSE;
    if ( g_stat( a->p->path, &st ) != 0 ) return FALSE;

    if ( (long) st.st_size == a->p_tam && (long) st.st_mtime == a->p_mtime )
        return FALSE;                       /* no se ha movido */

    snprintf( path, sizeof path, "%s", a->p->path );

    nuevo = g_new0( Proyecto, 1 );
    if ( pr_leer( path, nuevo, &e ) != 0 )
        {
        /* NO SE PISA LO QUE FUNCIONA. Y no se vuelve a avisar hasta que el
           fichero cambie otra vez: si alguien lo dejo a medias, el aviso
           saldria en cada golpe de foco.                               */
        if ( why ) pr_error_es( &e, why, n );
        a->p_tam   = (long) st.st_size;
        a->p_mtime = (long) st.st_mtime;
        g_free( nuevo );
        return FALSE;
        }

    g_free( a->p );
    a->p = nuevo;
    snprintf( a->p->path, sizeof a->p->path, "%s", path );
    a->p_tam   = (long) st.st_size;
    a->p_mtime = (long) st.st_mtime;

    /* La cache de resultados se vacia: sus huellas siguen valiendo, pero un
       modelo que ya no esta no tiene por que seguir ocupando sitio.    */
    a->nr = 0;

    /* La serie marcada, si sigue estando. Y las hojas se rehacen, que las
       muestras pueden haber cambiado -- atsw_hojas vuelve a la que estaba
       si sigue existiendo.                                             */
    if ( a->serie[0] && pr_serie_idx( a->p, a->serie ) < 0 ) a->serie[0] = '\0';
    atsw_hojas( a );
    return TRUE;
}

gboolean atsw_abre( Atsw *a, const char *path, char *why, size_t n )
{
    PrError   e;
    Proyecto *nuevo;

    if ( why && n ) why[0] = '\0';

    /* SE LEE A OTRO SITIO, como en atsw_relee. pr_leer escribe sobre el
       proyecto que se le da a medida que lee, asi que un manifiesto roto
       leido encima de a->p dejaba el abierto vaciado y apuntando al roto:
       la barra decia «no se abre», la ventana se quedaba sin series, y el
       gesto siguiente guardaba un proyecto vacio encima del fichero roto.
       Lo encontro la prueba de la ventana (tests/test_gui.c).          */
    nuevo = g_new0( Proyecto, 1 );
    if ( pr_leer( path, nuevo, &e ) != 0 )
        {
        if ( e.cod != PR_ENOFILE )
            { if ( why ) pr_error_es( &e, why, n ); g_free( nuevo );
              return FALSE; }
        pr_nuevo( nuevo, "proyecto", "", "." );
        snprintf( nuevo->path, sizeof nuevo->path, "%s", path );
        }
    /* Se copia DENTRO del que habia, no se cambia el puntero: las ventanas
       de analisis abiertas guardan a->p en su anfitrion. Proyecto no lleva
       punteros, asi que la copia es entera.                            */
    if ( a->p ) { *a->p = *nuevo; g_free( nuevo ); }
    else        a->p = nuevo;
    a->hay = TRUE;
    a->nr  = 0;
    a->serie[0] = '\0';
    huella_manifiesto( a );
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
    const char   *mu;
    int           i, j;
    char          marcada[PR_ID];

    /* La marca se GUARDA y se repone: repintar no puede cambiar lo que el
       analista tenia elegido.                                          */
    snprintf( marcada, sizeof marcada, "%s", a->serie );

    a->recolocando = TRUE;
    gtk_list_store_clear( st );
    if ( !a->hay ) { a->recolocando = FALSE; return; }

    mu = atsw_muestra_actual( a );
    for ( i = 0; i < a->p->ns; i++ )
        {
        const char *eleg;
        gchar      *globo;
        int         nm = 0;

        /* LA LISTA SIGUE A LA HOJA. Contar el proyecto entero aqui haria
           que en la hoja "pre-covid" pusiera "3 modelos" señalando modelos
           que no estan ahi: la lista mentiria sobre lo que se ve.     */
        for ( j = 0; j < a->p->nm; j++ )
            if ( !strcmp( a->p->m[j].serie, a->p->s[i].id ) &&
                 !strcmp( a->p->m[j].muestra, mu ) ) nm++;
        eleg = pr_elegido( a->p, a->p->s[i].id, mu );

        globo = serie_globo( &a->p->s[i] );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            S_ID,      a->p->s[i].id,
            /* EL ELEGIDO, A LA VISTA. Hoy esa decision vive en un diccionario
               a pelo repetido en tres guiones de cases/.                 */
            S_ELEGIDO, eleg[0] ? eleg : "—",
            S_NMOD,    nm,
            S_RAZON,   pr_razon_elegido( a->p, a->p->s[i].id, mu ),
            S_GLOBO,   globo,
            -1 );
        g_free( globo );

        if ( marcada[0] && !strcmp( marcada, a->p->s[i].id ) )
            gtk_tree_selection_select_iter(
                gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_series) ), &it );
        }
    a->recolocando = FALSE;
}

/* LO QUE LA HOJA ESCONDE, RECUPERADO EN EL GLOBO.
 *
 * Las pestañas impiden comparar los NUMEROS entre muestras, que es lo que
 * habia que impedir. Pero comparar la ESTRUCTURA si vale --"¿sale el mismo
 * (0,1,1)(0,1,1)12 antes y despues del salto?"-- y eso la pestaña lo tapa.
 * Se recupera aqui: si la misma estructura esta estimada en otra muestra,
 * se dice en cual y con que nombre.                                    */
static const char *cruce( Atsw *a, const PrModelo *m )
{
    static char b[512];
    const AtRes *mio = atsw_resultado( a, m->serie, m->muestra, m->id );
    GString     *g;
    int          i;

    b[0] = '\0';
    if ( mio == NULL || !mio->hay || !mio->estruct[0] ) return b;

    g = g_string_new( NULL );
    for ( i = 0; i < a->p->nm; i++ )
        {
        const PrModelo *o = &a->p->m[i];
        const AtRes    *r;

        if ( strcmp( o->serie, m->serie ) != 0 ) continue;
        if ( strcmp( o->muestra, m->muestra ) == 0 ) continue;  /* otra hoja */
        r = atsw_resultado( a, o->serie, o->muestra, o->id );
        if ( r == NULL || !r->hay ) continue;
        if ( strcmp( r->estruct, mio->estruct ) != 0 ) continue;

        g_string_append_printf( g, "%s%s (%s)", g->len ? ", " : "",
            o->id, o->muestra[0] ? o->muestra : "completa" );
        }

    if ( g->len )
        snprintf( b, sizeof b, "\n\nLa misma estructura está en: %s", g->str );
    g_string_free( g, TRUE );
    return b;
}

/* UNA HOJA: los modelos de ESTA serie EN ESTA muestra. */
static void pinta_hoja( Atsw *a, GtkWidget *vista, const char *muestra )
{
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(vista) ) );
    GtkTreeIter   it;
    const char   *eleg, *porde;
    gchar        *marca;
    int           i;

    /* La marca se GUARDA y se repone: repintar no puede cambiar lo que el
       analista tenia elegido.                                          */
    marca = atsw_marcada( vista, M_ID );

    gtk_list_store_clear( st );
    if ( !a->hay || a->serie[0] == '\0' ) { g_free( marca ); return; }

    eleg  = pr_elegido( a->p, a->serie, muestra );
    porde = atsw_modelo_por_defecto_en( a->p, a->serie, muestra );

    for ( i = 0; i < a->p->nm; i++ )
        {
        const PrModelo *m = &a->p->m[i];
        const AtRes    *r;
        char            sd[32], q[48], pv[16];
        gchar          *globo;

        if ( strcmp( m->serie, a->serie ) != 0 ) continue;
        if ( strcmp( m->muestra, muestra ) != 0 ) continue;

        r = atsw_resultado( a, m->serie, m->muestra, m->id );

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
            /* EL DICTAMEN EN EL GLOBO: la rejilla ya enseña Q y p; esto
               dice QUE SIGNIFICAN JUNTOS, que es otra cosa.          */
            globo = g_strdup_printf(
                "%s · %d parámetro%s\n%s\n"
                "Ljung-Box Q(%d) = %.2f, p = %.4f\n"
                "Jarque-Bera = %.1f, p = %.4f  (asimetría %.2f, curtosis %.2f)"
                "%s\n\nDoble clic para abrirlo en fue.",
                r->estruct[0] ? r->estruct : "sin estructura",
                r->npar, r->npar == 1 ? "" : "s", r->dx,
                r->qdf, r->q, r->qp, r->jb, r->jbp, r->skew, r->kurt,
                cruce( a, m ) );
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

        /* Se repone la marca; y si no habia, se marca el de por defecto
         * DE ESTA HOJA, para que los botones tengan a que apuntar.    */
        if ( marca ? !strcmp( marca, m->id ) : !strcmp( porde, m->id ) )
            gtk_tree_selection_select_iter(
                gtk_tree_view_get_selection( GTK_TREE_VIEW(vista) ), &it );
        }
    g_free( marca );
}

/* SOLO LA HOJA QUE SE VE.
 *
 * Pintarlas todas en cada refresco es trabajo que nadie mira: una hoja que
 * no esta delante no necesita estar al dia, necesita estarlo CUANDO SE MIRE
 * -- y cambiar de hoja repinta. Con varias muestras, pintarlas todas
 * multiplicaba por el numero de hojas un refresco que ademas ocurre al
 * volver el foco a la ventana.                                         */
static void pinta_modelos( Atsw *a )
{
    if ( a->hoja_actual < 0 || a->hoja_actual >= a->nhojas ) return;

    a->recolocando = TRUE;
    pinta_hoja( a, a->hoja[a->hoja_actual], a->hoja_mu[a->hoja_actual] );
    a->recolocando = FALSE;
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
        r = atsw_resultado( a, a->p->m[i].serie, a->p->m[i].muestra,
                            a->p->m[i].id );
        if ( r && r->hay ) estimados++;
        }
    for ( i = 0; i < a->p->nm; i++ )
        if ( a->p->m[i].elegido ) elegidos++;

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
        a->hay && a->serie[0] && pr_datos_de( a->p, a->serie, atsw_muestra_actual( a ) )[0] );
    gtk_widget_set_sensitive( a->b_iterar, a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_elegir, a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_razon,  a->hay && a->serie[0] != '\0' );
    gtk_widget_set_sensitive( a->b_drtran, a->hay );
    gtk_widget_set_sensitive( a->b_fue,    a->hay );
    gtk_widget_set_sensitive( a->b_fug,    a->hay );
}

/* ------------------------------------------------------------------------ */
/* LAS HOJAS DEL CUADERNO                                                    */
/*                                                                           */
/* Una por muestra, y la primera es SIEMPRE la completa: no se crea, no se   */
/* borra y no se renombra, porque es lo que entro. Un proyecto que nunca     */
/* trunque nada vive entero en ella y no se entera de que hay mas.           */
/*                                                                           */
/* Cada hoja tiene su propia rejilla porque un widget no puede tener dos     */
/* padres. a->l_modelos apunta a la de la hoja visible, asi que todo lo que  */
/* actua sobre "el modelo marcado" sigue leyendo de un solo sitio.           */
/* ------------------------------------------------------------------------ */

static GtkWidget *hoja_nueva( Atsw *a )
{
    GtkListStore *st = gtk_list_store_new( M_N,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING );
    GtkWidget *v = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );

    g_object_unref( st );
    gtk_tree_view_set_tooltip_column( GTK_TREE_VIEW(v), M_GLOBO );
    atsw_columna( v, "",           M_ESTRELLA );
    atsw_columna( v, "Modelo",     M_ID );
    atsw_columna( v, "Viene de",   M_PADRE );
    atsw_columna( v, "Estructura", M_ESTRUCT );
    atsw_columna( v, "d.t. res.",  M_SD );
    atsw_columna( v, "Q (g.l.)",   M_Q );
    atsw_columna( v, "p",          M_P );
    atsw_columna( v, "Por qué",    M_RAZON );
    gtk_widget_set_tooltip_text( v,
        "Los números salen del .out, no del manifiesto, y se releen cuando el "
        "fichero cambia. Cachearlos podría mentir: si alguien reestima por "
        "fuera, el número guardado seguiría diciendo lo de antes.\n\nEl "
        "manifiesto guarda linaje y razón, que son DECISIONES." );
    g_signal_connect( v, "row-activated", G_CALLBACK(atsw_on_activado), a );
    g_signal_connect( v, "button-press-event", G_CALLBACK(atsw_on_click), a );
    return v;
}

void atsw_hojas( Atsw *a )
{
    const char *antes = atsw_muestra_actual( a );
    char        vuelve[PR_ID];
    int         i, n;

    if ( a->libro == NULL ) return;
    snprintf( vuelve, sizeof vuelve, "%s", antes );

    a->recolocando = TRUE;
    while ( gtk_notebook_get_n_pages( GTK_NOTEBOOK(a->libro) ) > 0 )
        gtk_notebook_remove_page( GTK_NOTEBOOK(a->libro), 0 );

    a->nhojas = 0;
    n = a->hay ? a->p->nmu : 0;
    for ( i = 0; i <= n && a->nhojas <= PR_MAX_MUESTRA; i++ )
        {
        /* i == 0 es la COMPLETA, y por eso va primera y sin declarar. */
        const char *mu = ( i == 0 ) ? "" : a->p->mu[i - 1].id;
        GtkWidget  *v  = hoja_nueva( a );
        GtkWidget  *et = gtk_label_new( ( i == 0 ) ? "Completa" : mu );

        if ( i > 0 )
            {
            const PrMuestra *m = &a->p->mu[i - 1];
            gchar *t = g_strdup_printf( "%s%s%s%s%s",
                m->desde[0] ? "Desde " : "", m->desde,
                m->hasta[0] ? ( m->desde[0] ? ", hasta " : "Hasta " ) : "",
                m->hasta,
                m->razon[0] ? "" : "" );

            if ( m->razon[0] )
                { gchar *u = g_strdup_printf( "%s\n\n%s", t, m->razon );
                  g_free( t ); t = u; }
            gtk_widget_set_tooltip_text( et, t );
            g_free( t );
            }
        else
            gtk_widget_set_tooltip_text( et,
                "La muestra total: los datos tal como entraron. No se crea, "
                "no se borra y no se recorta." );

        gtk_widget_show_all( v );
        gtk_widget_show( et );
        gtk_notebook_append_page( GTK_NOTEBOOK(a->libro), atsw_en_scroll( v ),
                                  et );
        a->hoja[a->nhojas] = v;
        snprintf( a->hoja_mu[a->nhojas], PR_ID, "%s", mu );
        a->nhojas++;
        }

    gtk_widget_show_all( a->libro );

    /* Se vuelve a la hoja que estaba, si sigue estando. */
    for ( i = 0; i < a->nhojas; i++ )
        if ( !strcmp( a->hoja_mu[i], vuelve ) )
            { gtk_notebook_set_current_page( GTK_NOTEBOOK(a->libro), i ); break; }

    a->hoja_actual = gtk_notebook_get_current_page( GTK_NOTEBOOK(a->libro) );
    if ( a->hoja_actual < 0 || a->hoja_actual >= a->nhojas )
        a->hoja_actual = 0;
    a->l_modelos = a->hoja[a->hoja_actual];
    a->recolocando = FALSE;
}
