/*
 * proyecto_gui.c -- el proyecto, visto desde drtran_gui.
 *
 * LO QUE ESTO ARREGLA, Y ES CULPA NUESTRA: el programa escribia SIEMPRE en
 * modelo.out, red.dag y modelo.cns, dentro de una cache unica. Asi que NO
 * CABIAN DOS MODELOS: estimar otro pisaba el anterior. Es exactamente el
 * defecto de TASTE que el estudio señalo --una sola ranura RESIDUOS,
 * TASTECTV.PAS:475-- reaparecido por la puerta de atras. Diagnosis y Prevision
 * lo esquivaban guardando copias EN MEMORIA, que se pierden al cerrar.
 *
 * Con --proyecto, cada estimacion es una CORRIDA con su nombre y su linaje,
 * y las corridas cuelgan de un CASO: lo que se cruza, con el sha256 del .pre
 * de cada entrada (docs/DISENO-casos.md). Antes colgaban de la serie de
 * salida como si fueran modelos univariantes suyos, revueltas con los de fue,
 * y no quedaba escrito con que se cruzo cada serie.
 *
 * SIN --proyecto el programa sigue funcionando como siempre. Es deliberado: la
 * madre todavia no existe, y obligar a tener un proyecto para estimar una red
 * seria poner la carreta delante.
 *
 * EL NOMBRE DEL FICHERO ES CORTESIA. <salida>_<id>.out se llama asi para que
 * el directorio se entienda a ojo, y NADIE lo vuelve a leer: la identidad esta
 * en el manifiesto. Ver lib/proyecto.
 */

#include <string.h>

#include "gui.h"
#include "previewhost.h"
#include "fue_pre_reader.h"

gboolean mtram_proyecto_abre( Mtram *m, const char *path_dado, char *why,
                              size_t n )
{
    PrError e;
    gchar  *path;

    if (why && n) why[0] = '\0';
    if (!m->proy) m->proy = g_new0( Proyecto, 1 );

    /* LA RUTA, ABSOLUTA. Las de las corridas se resuelven contra el directorio
     * del manifiesto, y el motor se lanza con OTRO directorio de trabajo --la
     * cache--: con «--proyecto p.yaml» el GUI esperaba el .out en ./SYN_Y/work
     * y drtran intentaba escribirlo en la cache/SYN_Y/work, que no existe.
     * Salia con 1 y no habia corrida.
     *
     * Y con '/' tambien en Windows: lib/proyecto busca el directorio con
     * strrchr(.., '/'), asi que con barras invertidas la raiz caia a "." y
     * pasaba lo mismo. Windows acepta '/' en todas partes.              */
    path = g_canonicalize_filename( path_dado, NULL );
#ifdef G_OS_WIN32
    g_strdelimit( path, "\\", '/' );
#endif

    if (pr_leer( path, m->proy, &e ) != 0) {
        /* Si no existe, se empieza uno. Cualquier otro motivo es un fichero
         * roto y NO se pisa: se dice.                                   */
        if (e.cod != PR_ENOFILE) {
            if (why) pr_error_es( &e, why, n );
            g_free( m->proy );
            m->proy = NULL;
            g_free( path );
            return FALSE;
        }
        pr_nuevo( m->proy, "proyecto", "", "." );
        snprintf( m->proy->path, sizeof m->proy->path, "%s", path );
    }
    g_free( path );
    m->hay_proy = TRUE;
    m->corrida[0] = m->previa[0] = '\0';
    return TRUE;
}

gboolean mtram_proyecto_guarda( Mtram *m, char *why, size_t n )
{
    PrError e;

    if (why && n) why[0] = '\0';
    if (!m->hay_proy || !m->proy) return TRUE;

    if (pr_escribir( m->proy, m->proy->path, &e ) != 0) {
        if (why) pr_error_es( &e, why, n );
        return FALSE;
    }
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* El caso (docs/DISENO-casos.md)                                            */
/* ------------------------------------------------------------------------ */

/* EL SHA256 DE UN FICHERO, en hexadecimal (g_free), o NULL si no se lee. Es
 * la identidad de una entrada: lo que art guarda en su guion (base_pre_sha)
 * y mtram en su certificado (BUG-53).                                    */
static gchar *sha_de( const char *path )
{
    gchar *c = NULL, *h;
    gsize  n = 0;

    if (!g_file_get_contents( path, &c, &n, NULL )) return NULL;
    h = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) c, n );
    g_free( c );
    return h;
}

/* El directorio de la corrida tiene que existir antes de que el motor
 * intente escribir en el.                                               */
static void crea_dir_corrida( Mtram *m )
{
    char   ruta[PR_RUTA];
    gchar *dir;

    if (pr_corrida_ruta( m->proy, m->caso, m->corrida, ".out", ruta,
                         sizeof ruta ) != 0) return;
    dir = g_path_get_dirname( ruta );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
}

gboolean mtram_caso_carga( Mtram *m, char *why, size_t n )
{
    const PrCaso *c;
    const char   *de;
    GString      *cambian = g_string_new( NULL );
    int           i;

    if (why && n) why[0] = '\0';
    if (!m->hay_proy || !m->proy || !m->caso[0]) return TRUE;

    if ((c = pr_caso_ver( m->proy, m->caso )) == NULL) {
        if (why) snprintf( why, n, "«%s» no es un caso del proyecto.", m->caso );
        g_string_free( cambian, TRUE );
        return FALSE;
    }

    /* LAS ENTRADAS, EN SU ORDEN. Lo que hubiera cargado se va: el caso dice
     * exactamente que se cruza, y en que posicion.                    */
    for (i = 0; i < m->c.n; i++) serie_libre( m->c.s[i] );
    m->c.n = 0;

    for (i = 0; i < c->nen && i < GUI_MAX_SER; i++) {
        char   ruta[PR_RUTA], w[512];
        gchar *h;
        Serie *s;

        if (pr_ruta( m->proy, c->en[i].serie, c->muestra, c->en[i].modelo,
                     ".pre", ruta, sizeof ruta ) != 0 ||
            (s = serie_cargar( ruta, w, sizeof w )) == NULL) {
            if (why) snprintf( why, n, "No pude cargar la entrada %s/%s del "
                               "caso %s: %s", c->en[i].serie, c->en[i].modelo,
                               m->caso, w );
            g_string_free( cambian, TRUE );
            return FALSE;
        }
        m->c.s[m->c.n++] = s;

        /* EL HASH: si el .pre ya no es el del alta, lo que se estime aqui no
         * corresponde a lo que el caso dice que se cruzo.              */
        h = sha_de( ruta );
        if (c->en[i].sha[0] && (!h || strcmp( h, c->en[i].sha ) != 0))
            g_string_append_printf( cambian, "%s%s/%s", cambian->len ? ", " : "",
                                    c->en[i].serie, c->en[i].modelo );
        g_free( h );
    }
    m->caso_desfasado = cambian->len > 0;
    mtram_refresca( m );

    /* DE DONDE SE PARTE: la corrida pedida, o la elegida. Su red y sus
     * restricciones, y la siguiente colgara de ella.                  */
    de = m->corrida_ini[0] ? m->corrida_ini : pr_corrida_elegida( m->proy, m->caso );
    if (de[0]) {
        char dag[PR_RUTA], cns[PR_RUTA];

        if (pr_corrida_idx( m->proy, m->caso, de ) < 0) {
            if (why) snprintf( why, n, "«%s» no es una corrida del caso %s.",
                               de, m->caso );
            g_string_free( cambian, TRUE );
            return FALSE;
        }
        snprintf( m->previa, sizeof m->previa, "%s", de );
        pr_corrida_ruta( m->proy, m->caso, de, ".dag", dag, sizeof dag );
        pr_corrida_ruta( m->proy, m->caso, de, ".cns", cns, sizeof cns );
        if (g_file_test( dag, G_FILE_TEST_EXISTS )) red_lee( m, dag );
        if (g_file_test( cns, G_FILE_TEST_EXISTS )) modelo_lee( m, cns );
    }

    if (m->caso_desfasado && why)
        snprintf( why, n, "El caso %s está DESFASADO: el .pre de %s cambió "
                  "después del alta. Lo que estimes irá a un caso nuevo, "
                  "derivado de %s con los .pre de hoy.", m->caso, cambian->str,
                  m->caso );
    g_string_free( cambian, TRUE );
    return TRUE;
}

/* LAS ENTRADAS DE LO QUE HAY CARGADO, si todas son modelos del proyecto en
 * una misma muestra. 0 si se pudo; si no, el motivo en why.              */
static int entradas_cargadas( Mtram *m, PrEntrada *en, char *muestra,
                              size_t nmu, char *why, size_t n )
{
    int i;

    muestra[0] = '\0';
    for (i = 0; i < m->c.n; i++) {
        char   se[PR_ID], mu[PR_ID], id[PR_ID];
        gchar *h, *base;

        if (pr_de_ruta( m->proy, m->c.s[i]->path, se, sizeof se, mu, sizeof mu,
                        id, sizeof id ) != 0) {
            base = g_path_get_basename( m->c.s[i]->path );
            snprintf( why, n, "%s no es un modelo de este proyecto: la corrida "
                      "va a la caché, fuera del proyecto.", base );
            g_free( base );
            return 1;
        }
        if (i == 0) snprintf( muestra, nmu, "%s", mu );
        else if (strcmp( mu, muestra ) != 0) {
            snprintf( why, n, "%s/%s es de otra muestra que %s: un caso cruza "
                      "series de UNA ventana. La corrida va a la caché.", se, id,
                      en[0].serie );
            return 1;
        }
        memset( &en[i], 0, sizeof en[i] );
        snprintf( en[i].serie, PR_ID, "%s", se );
        snprintf( en[i].modelo, PR_ID, "%s", id );
        h = sha_de( m->c.s[i]->path );
        snprintf( en[i].sha, PR_SHA, "%s", h ? h : "" );
        g_free( h );
    }
    return 0;
}

gboolean mtram_corrida_nueva( Mtram *m, char *why, size_t n )
{
    PrError e;
    char    id[PR_ID], nuevo[PR_ID];

    if (why && n) why[0] = '\0';
    m->corrida[0] = '\0';
    if (!m->hay_proy || !m->proy) return TRUE;
    if (m->c.n < 1) {
        if (why) snprintf( why, n, "Carga las series antes." );
        return FALSE;
    }

    /* EL CASO DESFASADO NO SE TOCA: se deriva uno con los .pre de hoy, y la
     * corrida va a el. Nunca se reescribe un caso por debajo.          */
    if (m->caso[0] && m->caso_desfasado) {
        PrEntrada en[PR_MAX_ENTRADA];
        char      mu[PR_ID], w[512];
        const char *ya;

        if (entradas_cargadas( m, en, mu, sizeof mu, w, sizeof w ) != 0) {
            if (why) snprintf( why, n, "%s", w );
            return FALSE;
        }
        ya = pr_caso_de_entradas( m->proy, en, m->c.n, mu );
        if (ya[0])
            snprintf( nuevo, sizeof nuevo, "%s", ya );
        else if (pr_caso_deriva( m->proy, m->caso, en, m->c.n, nuevo,
                                 sizeof nuevo, &e ) != 0) {
            if (why) pr_error_es( &e, why, n );
            return FALSE;
        }
        if (why) snprintf( why, n, "El caso %s estaba desfasado: esta corrida "
                           "va a %s, con los .pre de hoy.", m->caso, nuevo );
        snprintf( m->caso, sizeof m->caso, "%s", nuevo );
        m->caso_desfasado = FALSE;
        m->previa[0] = '\0';
    }

    /* EL ALTA AUTOMATICA: drtran_gui lanzado a mano, con proyecto. Si lo
     * cargado son modelos del proyecto, ESO es un caso -- el analista lo
     * acaba de decir al cargarlos. Se busca el que ya tenga esas entradas
     * (mismo orden, mismos hashes) y, si no hay, se da de alta.        */
    if (!m->caso[0]) {
        PrEntrada  en[PR_MAX_ENTRADA];
        struct Tseries ts[GUI_MAX_SER + 1];
        char       mu[PR_ID], w[512];
        const char *ya;
        int        i;

        if (m->c.n > PR_MAX_ENTRADA) {
            if (why) snprintf( why, n, "Más de %d series: no caben en un caso. "
                               "La corrida va a la caché.", PR_MAX_ENTRADA );
            return TRUE;
        }
        if (entradas_cargadas( m, en, mu, sizeof mu, w, sizeof w ) != 0) {
            if (why) snprintf( why, n, "%s", w );
            return TRUE;
        }
        /* LA VENTANA COMUN (BUG-2): un caso que el motor va a rechazar no
         * tiene que existir.                                          */
        for (i = 0; i < m->c.n; i++) ts[i + 1] = m->c.s[i]->ts;
        if (fuepre_check_alignment( ts, m->c.n, w, sizeof w ) != 0) {
            if (why) snprintf( why, n, "No es un caso: %s", w );
            return FALSE;
        }
        ya = pr_caso_de_entradas( m->proy, en, m->c.n, mu );
        if (ya[0])
            snprintf( m->caso, sizeof m->caso, "%s", ya );
        else if (pr_caso_add( m->proy, en, m->c.n, mu, "drtran", NULL, NULL,
                              m->caso, sizeof m->caso, &e ) != 0) {
            if (why) pr_error_es( &e, why, n );
            m->caso[0] = '\0';
            return FALSE;
        }
        /* De un caso que ya existia se sigue por su elegida, si nadie dijo
         * otra cosa en esta sesion.                                     */
        if (!m->previa[0])
            snprintf( m->previa, sizeof m->previa, "%s",
                      pr_corrida_elegida( m->proy, m->caso ) );
    }

    /* EL LINAJE, SIN PREGUNTAR: la corrida nueva cuelga de la anterior de
     * ESTE caso. Eso es la cadena de iteracion, un piso por encima de la
     * de los modelos.                                                 */
    if (pr_corrida_nueva( m->proy, m->caso, m->previa[0] ? m->previa : NULL,
                          id, sizeof id, NULL, 0, &e ) != 0) {
        if (why) pr_error_es( &e, why, n );
        return FALSE;
    }
    snprintf( m->corrida, sizeof m->corrida, "%s", id );
    crea_dir_corrida( m );
    return TRUE;
}

gchar *mtram_artefacto( Mtram *m, const char *sufijo )
{
    char   ruta[PR_RUTA];
    gchar *d, *r;

    /* --- sin caso: la cache y el nombre FIJO de siempre ---------------- */
    if (!m->hay_proy || !m->proy || !m->caso[0] || !m->corrida[0]) {
        static const struct { const char *suf, *fijo; } F[] = {
            { ".out",     "modelo.out" },
            { ".dag",     "red.dag" },
            { ".cns",     "modelo.cns" },
            { "_res.txt", "residuos.txt" },
            { "_eval.csv","evaluacion.csv" },
            { NULL, NULL }
        };
        int i;

        d = g_build_filename( g_get_user_cache_dir(), GUI_CACHE, NULL );
        g_mkdir_with_parents( d, 0700 );
        for (i = 0; F[i].suf; i++)
            if (!strcmp( sufijo, F[i].suf )) {
                r = g_build_filename( d, F[i].fijo, NULL );
                g_free( d );
                return r;
            }
        r = g_build_filename( d, sufijo, NULL );
        g_free( d );
        return r;
    }

    /* --- con caso: el nombre de cortesia de la corrida ----------------- */
    if (pr_corrida_ruta( m->proy, m->caso, m->corrida, sufijo, ruta,
                         sizeof ruta ) != 0)
        return g_build_filename( g_get_user_cache_dir(), GUI_CACHE, "x", NULL );
    return g_strdup( ruta );
}
