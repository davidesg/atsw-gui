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

/* El sha256 del fichero: la identidad de una entrada (como drtran_gui). */
static gchar *sha_de( const char *path )
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
        h = sha_de( ruta );
        if (c->en[i].sha[0] && (!h || strcmp( h, c->en[i].sha ) != 0))
            g_string_append_printf( cambian, "%s%s/%s", cambian->len ? ", " : "",
                                    c->en[i].serie, c->en[i].modelo );
        g_free( h );
    }
    m->desfasado = cambian->len > 0;

    /* LO QUE YA ESTABA HECHO SE VE: la puerta del sistema, si se paso, y el
     * modelo de --modelo o el elegido, con su .out. La siguiente corrida
     * colgara de el.                                                    */
    {
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
                  "del alta. Lo que se estime no será lo que el sistema dice que se "
                  "cruzó.", m->caso, cambian->str );
    g_string_free( cambian, TRUE );
    sim_refresca( m );
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
