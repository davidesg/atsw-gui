/*
 * proyecto.c -- el proyecto de la madre y el sistema que se abre.
 *
 * Un sistema es un CASO del manifiesto con motor drvarma (o drvec): la misma
 * lista de entradas SERIE/modelo@sha256, en orden, que un caso MT
 * (docs/DISENO-casos.md §2.3; DISENO-sim.md §5). Los .pre de entrada no se
 * copian: son los de las series, y el hash dice si siguen siendo los mismos.
 */

#include <string.h>
#include <glib/gstdio.h>

#include "sim.h"
#include "previewhost.h"
#include "preview.h"
#include "cuaderno.h"

gboolean sim_proyecto_abre( Sim *m, const char *path_dado, char *why, size_t n )
{
    PrError e;
    gchar  *path;

    if (why && n) why[0] = '\0';
    if (!m->proy) m->proy = g_new0( Proyecto, 1 );

    /* LA RUTA, ABSOLUTA y con '/' (como en drtran_gui): las de los casos se
     * resuelven contra el directorio del manifiesto.                    */
    path = g_canonicalize_filename( path_dado, NULL );
#ifdef G_OS_WIN32
    g_strdelimit( path, "\\", '/' );
#endif
    if (pr_leer( path, m->proy, &e ) != 0) {
        /* Un manifiesto que no esta o esta roto NO se empieza ni se pisa:
         * los sistemas los crea la madre («Nuevo sistema…»).           */
        if (why) pr_error_es( &e, why, n );
        g_free( m->proy );
        m->proy = NULL;
        g_free( path );
        return FALSE;
    }
    g_free( path );
    return TRUE;
}

/* El sha256 del fichero: la identidad de una entrada (como drtran_gui).
 * Tambien la usan las firmas de la puerta y del rango (puerta.c, rango.c):
 * un .pre reestimado tiene la misma ruta y otro contenido.             */
gchar *sim_sha( const char *path )
{
    gchar *c = NULL, *h;
    gsize  k = 0;

    if (!g_file_get_contents( path, &c, &k, NULL )) return NULL;
    h = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) c, k );
    g_free( c );
    return h;
}

gboolean sim_caso_carga( Sim *m, char *why, size_t n )
{
    const PrCaso *c;
    GString      *cambian;
    int           i;

    if (why && n) why[0] = '\0';
    if (!m->proy || !m->caso[0]) return TRUE;
    if ((c = pr_caso_ver( m->proy, m->caso )) == NULL) {
        if (why) snprintf( why, n, "«%s» no es un caso del proyecto.", m->caso );
        return FALSE;
    }

    m->sis.n = 0;
    cambian = g_string_new( NULL );
    for (i = 0; i < c->nen; i++) {
        char   ruta[PR_RUTA], w[1024];
        gchar *h;

        if (pr_ruta( m->proy, c->en[i].serie, c->muestra, c->en[i].modelo, ".pre",
                     ruta, sizeof ruta ) != 0 ||
            sis_anade( &m->sis, ruta, w, sizeof w ) != 0) {
            if (why) snprintf( why, n, "No pude cargar la entrada %s/%s del sistema %s: %s",
                               c->en[i].serie, c->en[i].modelo, m->caso, w );
            g_string_free( cambian, TRUE );
            sim_refresca( m );
            return FALSE;
        }
        h = sim_sha( ruta );
        if (c->en[i].sha[0] && (!h || strcmp( h, c->en[i].sha ) != 0))
            g_string_append_printf( cambian, "%s%s/%s", cambian->len ? ", " : "",
                                    c->en[i].serie, c->en[i].modelo );
        g_free( h );
    }
    m->desfasado = cambian->len > 0;

    /* LO QUE YA ESTABA HECHO SE VE: la puerta del sistema, si se paso, y el
     * modelo de --modelo o el elegido, con su .out. La siguiente corrida
     * colgara de el.
     *
     * SALVO CON EL CASO DESFASADO (docs/AUDITORIA-procesos.md, C1): la
     * puerta, el rango y las corridas se hicieron con los .pre de antes, y
     * las entradas que se acaban de cargar son las de hoy. Darlos por
     * buenos era dar la puerta por superada con otros datos. Lo que se
     * haga ahora va a un caso derivado (sim_caso_al_dia).              */
    if (!m->desfasado) {
        gchar *pu = sim_artefacto( m, "puerta.out" );
        const char *de = m->modelo_ini[0] ? m->modelo_ini : pr_corrida_elegida( m->proy, m->caso );
        char out[PR_RUTA];

        if (g_file_test( pu, G_FILE_TEST_EXISTS )) puerta_desde( m, pu );
        g_free( pu );
        pu = sim_artefacto( m, "rango.out" );
        if (g_file_test( pu, G_FILE_TEST_EXISTS )) rango_desde( m, pu );
        g_free( pu );
        if (de && de[0] && pr_corrida_ruta( m->proy, m->caso, de, ".out", out, sizeof out ) == 0 &&
            g_file_test( out, G_FILE_TEST_EXISTS )) {
            snprintf( m->pe.corrida, sizeof m->pe.corrida, "%s", de );
            snprintf( m->pe.previa, sizeof m->pe.previa, "%s", de );
            estima_desde( m, out );
        }
    }
    if (m->desfasado && why)
        snprintf( why, n, "El sistema %s está DESFASADO: el .pre de %s cambió después "
                  "del alta. Se cargan las entradas de hoy, sin la puerta ni los modelos "
                  "de antes; lo que hagas ahora irá a un sistema derivado, con los .pre "
                  "de hoy.", m->caso, cambian->str );
    g_string_free( cambian, TRUE );
    sim_refresca( m );
    return TRUE;
}

/* EL CASO DESFASADO NO SE TOCA (docs/DISENO-casos.md §2.4, §3; AUDITORIA-
 * procesos C1), como en drtran_gui: antes de producir nada --la puerta, el
 * rango, una estimacion-- se busca el caso que ya tenga estas entradas con
 * los .pre de hoy o se deriva uno, y se sigue en el. FALSE si no se pudo
 * (why dice por que).                                                  */
gboolean sim_caso_al_dia( Sim *m, char *why, size_t n )
{
    const PrCaso *c;
    PrEntrada     en[PR_MAX_ENTRADA];
    char          nuevo[PR_ID], viejo[PR_ID], mu[PR_ID], w[1024];
    const char   *ya;
    PrError       e;
    int           i, nen;

    if (why && n) why[0] = '\0';
    if (!m->proy || !m->caso[0] || !m->desfasado) return TRUE;
    if ((c = pr_caso_ver( m->proy, m->caso )) == NULL) {
        if (why) snprintf( why, n, "«%s» ya no es un caso del proyecto.", m->caso );
        return FALSE;
    }
    nen = c->nen;
    snprintf( mu, sizeof mu, "%s", c->muestra );
    for (i = 0; i < nen; i++) {
        char   ruta[PR_RUTA];
        gchar *h;

        en[i] = c->en[i];
        if (pr_ruta( m->proy, en[i].serie, mu, en[i].modelo, ".pre", ruta, sizeof ruta ) != 0 ||
            ( h = sim_sha( ruta ) ) == NULL) {
            if (why) snprintf( why, n, "No pude leer el .pre de %s/%s.", en[i].serie, en[i].modelo );
            return FALSE;
        }
        snprintf( en[i].sha, PR_SHA, "%s", h );
        g_free( h );
    }
    ya = pr_caso_de_entradas( m->proy, en, nen, mu );
    if (ya[0])
        snprintf( nuevo, sizeof nuevo, "%s", ya );
    else if (pr_caso_deriva( m->proy, m->caso, en, nen, nuevo, sizeof nuevo, &e ) != 0 ||
             pr_escribir( m->proy, m->proy->path, &e ) != 0) {
        if (why) pr_error_es( &e, why, n );
        return FALSE;
    }
    snprintf( viejo, sizeof viejo, "%s", m->caso );
    snprintf( m->caso, sizeof m->caso, "%s", nuevo );
    m->desfasado = FALSE;

    /* LO DEL CASO VIEJO, FUERA: la puerta y el rango eran de otras entradas,
       y la cadena de corridas empieza de nuevo en este caso.           */
    m->pp.hecha = FALSE;
    g_free( m->pp.firma ); m->pp.firma = NULL;
    m->pr.hecha = FALSE;
    g_free( m->pr.firma ); m->pr.firma = NULL;
    m->pe.corrida[0] = m->pe.previa[0] = '\0';
    m->modelo_ini[0] = '\0';
    if (!sim_caso_carga( m, w, sizeof w )) {
        if (why) snprintf( why, n, "%s", w );
        return FALSE;
    }
    if (why) snprintf( why, n, "El sistema %s estaba desfasado: lo de ahora va a %s%s, con los "
                       ".pre de hoy.", viejo, nuevo, ya[0] ? " (ya tenía esas entradas)" : "" );
    return TRUE;
}

gchar *sim_artefacto( Sim *m, const char *nombre )
{
    gchar *dir, *r;

    if (m->proy && m->caso[0]) {
        char raiz[PR_RUTA];

        if (pr_raiz( m->proy, raiz, sizeof raiz ) != 0) snprintf( raiz, sizeof raiz, "." );
        dir = g_build_filename( raiz, "_casos", m->caso, "work", NULL );
        {
            gchar *base = g_strdup_printf( "%s_%s", m->caso, nombre );
            r = g_build_filename( dir, base, NULL );
            g_free( base );
        }
    } else {
        dir = g_build_filename( g_get_user_cache_dir(), SIM_CACHE, NULL );
        r = g_build_filename( dir, nombre, NULL );
    }
    g_mkdir_with_parents( dir, 0755 );
    g_free( dir );
    return r;
}


/* ------------------------------------------------------------------------ */
/* «Al cuaderno» en la ventana de graficos (lib/preview; fase P5)            */
/* ------------------------------------------------------------------------ */

gboolean preview_cuaderno_puede( PreviewApp *m )
{
    return m && m->proy && m->proy->path[0] && m->caso[0];
}

static int a_png( const char *eps, const char *png )
{
    return preview_save_as( eps, png, NULL ) ? 0 : 1;
}

/* Que es el grafico, por su nombre, y de que .out sale. La CCF de la
 * identificacion es de antes de estimar: sin corrida ni informe.       */
void preview_cuaderno( PreviewApp *m, const gchar *path )
{
    gchar      *base = g_path_get_basename( path ), *que = NULL, *out = NULL;
    gboolean    del_modelo = TRUE;
    char        why[1024], md[1024];

    if (g_str_has_prefix( base, "ccf_" )) {
        que = g_strdup( "CCF de la identificación" );
        del_modelo = FALSE;
    } else if (g_str_has_prefix( base, "prev_" ) && g_str_has_suffix( base, ".eps" )) {
        gchar *s = g_strndup( base + 5, strlen( base ) - 9 );

        que = g_strdup_printf( "Previsión de %s", s );
        g_free( s );
        if (m->pv.base) out = g_strconcat( m->pv.base, ".out", NULL );
    } else if (g_str_has_suffix( base, ".pdf" ))
        que = g_strdup( "El modelo" );
    else
        que = g_strdup( "Gráfico" );
    if (del_modelo && !out && m->pe.out) out = g_strdup( m->pe.out );

    if (cu_grafico_caso( m->proy ? m->proy->path : NULL,
                         m->proy ? m->proy->id : NULL, que, m->caso,
                         del_modelo && m->pe.corrida[0] ? m->pe.corrida : NULL,
                         out, path, a_png, md, sizeof md, why, sizeof why ) == 0)
        sim_estado( m, "Al cuaderno: %s.", md );
    else
        sim_estado( m, "%s", why );
    g_free( que ); g_free( out ); g_free( base );
}
