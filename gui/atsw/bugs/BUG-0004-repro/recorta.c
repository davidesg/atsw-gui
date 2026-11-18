/* BUG-0004: el nodo de datos de una muestra «desde 1/2010 hasta 12/2019».
 * Uso: recorta DIR CSV. Escribe en DIR un proyecto con la serie ipc (su
 * datos.csv es CSV), declara la muestra y la puebla como la madre al
 * crearla (atsw_puebla_muestra). Imprime «AAAA PP NOBS» del .inp generado. */
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "atsw.h"
#include "inpfile.h"

int main( int argc, char **argv )
{
    static Proyecto p;
    static Atsw     a;
    PrError         e;
    InpFile         in;
    char            why[512], csv[PR_RUTA], ruta[PR_RUTA], err[256];
    const char     *d0;
    gchar          *c = NULL;

    if ( argc != 3 ) return 2;
    pr_nuevo( &p, "bug4", "", "." );
    snprintf( p.path, sizeof p.path, "%s/proyecto.yaml", argv[1] );
    if ( pr_serie_add( &p, "ipc", &e ) != 0 ) return 2;
    if ( atsw_csv_de( &p, "ipc", csv, sizeof csv ) != 0 ) return 2;
    { gchar *dir = g_path_get_dirname( csv ); g_mkdir_with_parents( dir, 0755 ); g_free( dir ); }
    if ( !g_file_get_contents( argv[2], &c, NULL, NULL ) ||
         !g_file_set_contents( csv, c, -1, NULL ) ) return 2;
    if ( pr_muestra_add( &p, "m2010", "1/2010", "12/2019", "la del bug", &e ) != 0 ) return 2;

    a.p = &p; a.hay = TRUE;
    if ( atsw_puebla_muestra( &a, "m2010", why, sizeof why ) < 1 )
        { fprintf( stderr, "no se puebla: %s\n", why ); return 2; }
    d0 = pr_datos_de( &p, "ipc", "m2010" );
    if ( pr_ruta( &p, "ipc", "m2010", d0, ".inp", ruta, sizeof ruta ) != 0 ) return 2;
    memset( &in, 0, sizeof in );
    if ( inp_read( ruta, &in, err, sizeof err ) != 0 ) { fprintf( stderr, "%s\n", err ); return 2; }
    printf( "%d %d %d\n", in.begyear, in.begtime, in.nobs );
    return 0;
}
