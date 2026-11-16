/*
 * construye.c -- el modelo de partida, en el proyecto (pt_construye). Va
 * aparte de partida.c porque pide el proyecto, fue y el lector de .inp: la
 * ventana de identificacion (fue_gui, la madre) solo usa la derivacion.
 */

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "partida.h"
#include "inpcheck.h"
#include "inpfile.h"
#include "engine.h"

/* --- todo junto, en el proyecto ------------------------------------------ */

int pt_construye( Proyecto *pr, const char *serie, const char *muestra, const char *art,
                  const char *cache, const char *sistema, int estimar, char *id, size_t nid,
                  PtPartida *p, char *why, size_t nw )
{
    const char *datos = pr_datos_de( pr, serie, muestra );
    char        origen[PR_RUTA], destino[PR_RUTA], msg[512], razon[PR_RAZON];
    InpFile     f;
    ArtCand    *c;
    PrError     e;
    gchar      *txt, *nom, *dir, *base;
    int         rc, i;

    if ( !*datos || pr_ruta( pr, serie, muestra, datos, ".inp", origen, sizeof origen ) != 0 )
        { g_snprintf( why, nw, "%s no tiene datos en esa muestra", serie ); return 1; }
    if ( inp_read( origen, &f, msg, sizeof msg ) != 0 )
        { g_snprintf( why, nw, "no pude leer %s: %s", origen, msg ); return 1; }

    g_mkdir_with_parents( cache, 0700 );
    nom = g_strdup_printf( "partida_%s_%s.txt", serie, muestra && *muestra ? muestra : "completa" );
    for ( i = 0; nom[i]; i++ )
        if ( !g_ascii_isalnum( nom[i] ) && nom[i] != '_' && nom[i] != '-' && nom[i] != '.' ) nom[i] = '_';
    txt = g_build_filename( cache, nom, NULL );
    g_free( nom );
    c = g_new0( ArtCand, 1 );
    rc = pt_identifica( art, txt, f.data, f.nobs, f.freq > 0 ? f.freq : 1, f.begtime, f.begyear,
                        p, c, why, nw );
    g_free( txt );
    if ( rc != 0 ) { inp_free( &f ); g_free( c ); return 1; }

    if ( pr_deriva( pr, serie, muestra, datos, id, nid, destino, sizeof destino, &e ) != 0 )
        { pr_error_es( &e, why, nw ); inp_free( &f ); g_free( c ); return 1; }
    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    rc = pt_deriva_inp( origen, destino, p->lam, p->d, 0, f.freq > 0 ? f.freq : 1, &c->cand[0],
                        p->armonicos, p->media_libre, NULL, why, nw );
    inp_free( &f );
    /* EL JUEZ ES EL MOTOR: un .inp que fue no acepta se deshace. */
    if ( rc == 0 && inp_check_fue( destino, msg, sizeof msg ) != 0 )
        { g_snprintf( why, nw, "lo que salió no lo acepta fue: %s", msg ); rc = 1; }
    if ( rc != 0 )
        {
        g_unlink( destino );
        pr_borra( pr, serie, muestra, id, &e );
        g_free( dir ); g_free( c );
        return 1;
        }
    pt_razon( p, &c->cand[0], sistema, razon, sizeof razon );
    pr_razon( pr, serie, muestra, id, razon, &e );
    pr_pon_sin_revisar( pr, serie, muestra, id, 1, &e );
    g_free( c );

    rc = 0;
    if ( estimar )
        {
        EngineResult r;

        base = g_path_get_basename( destino );
        base[strlen( base ) - 4] = '\0';
        r = engine_run( dir, "fue", base, NULL );
        if ( !engine_wrote_results( &r ) )
            {
            g_snprintf( why, nw, "%s/%s está, pero fue no lo estimó: %s", serie, id, r.message );
            rc = 2;
            }
        engine_result_clear( &r );
        g_free( base );
        }
    g_free( dir );
    return rc;
}
