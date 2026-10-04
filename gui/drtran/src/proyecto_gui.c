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
 * Con --proyecto, cada estimacion es una CORRIDA con su nombre y su linaje.
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

/* La serie de SALIDA es la primera: es la que el modelo modela, y es a ella a
 * la que pertenece la corrida.                                          */
static const char *salida( Mtram *m )
{
    if (m->c.n < 1 || !m->c.s[0]->ts.name) return NULL;
    return m->c.s[0]->ts.name;
}

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

gboolean mtram_corrida_nueva( Mtram *m, char *why, size_t n )
{
    PrError  e;
    const char *ser = salida( m );
    char     id[PR_ID], ruta[PR_RUTA], *dir;

    if (why && n) why[0] = '\0';
    if (!m->hay_proy || !m->proy) return TRUE;

    if (!ser) {
        if (why) snprintf( why, n, "Carga las series antes." );
        return FALSE;
    }

    /* La serie se da de alta la primera vez, sin preguntar: el analista ya
     * dijo lo que queria al cargarla.                                   */
    if (pr_serie_idx( m->proy, ser ) < 0 &&
        pr_serie_add( m->proy, ser, &e ) != 0) {
        if (why) pr_error_es( &e, why, n );
        return FALSE;
    }

    /* EL LINAJE, SIN PREGUNTAR: la corrida nueva cuelga de la anterior de
     * esta misma serie. Eso ES la cadena de iteracion.                  */
    /* drtran no tiene submuestras: sus corridas van en la muestra completa.
     * Cruzar series recortadas de distinta forma seria justo lo que una red
     * de transferencias no puede permitirse.                            */
    if (pr_deriva( m->proy, ser, "", m->previa[0] ? m->previa : NULL,
                   id, sizeof id, ruta, sizeof ruta, &e ) != 0) {
        if (why) pr_error_es( &e, why, n );
        return FALSE;
    }
    snprintf( m->corrida, sizeof m->corrida, "%s", id );

    /* El directorio de la corrida tiene que existir antes de que el motor
     * intente escribir en el.                                           */
    pr_ruta( m->proy, ser, "", id, ".inp", ruta, sizeof ruta );
    dir = g_path_get_dirname( ruta );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    return TRUE;
}

gchar *mtram_artefacto( Mtram *m, const char *sufijo )
{
    const char *ser;
    char        ruta[PR_RUTA];
    gchar      *d, *r;

    /* --- sin proyecto: la cache y el nombre FIJO de siempre ----------- */
    if (!m->hay_proy || !m->proy || m->corrida[0] == '\0' ||
        (ser = salida( m )) == NULL) {
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

    /* --- con proyecto: el nombre de cortesia de la corrida ------------ */
    if (pr_ruta( m->proy, ser, "", m->corrida, sufijo, ruta, sizeof ruta ) != 0)
        return g_build_filename( g_get_user_cache_dir(), GUI_CACHE, "x", NULL );

    return g_strdup( ruta );
}
