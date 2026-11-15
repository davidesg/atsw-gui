/* test_trabajo.c -- el contrato del .trabajo, sin motor: un trabajo en
   marcha (el pid de esta prueba), su avance por el fichero de progreso, uno
   que termino (con su resultado) y uno que se cayo (sin el), y la lista del
   proyecto.

     cc -Ilib/trabajo -Ilib/sitio lib/trabajo/test_trabajo.c lib/trabajo/trabajo.c \
        lib/sitio/sitio.c $(pkg-config --cflags --libs glib-2.0)              */

#include "trabajo.h"
#include "sitio.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>

static int fallos = 0;
static void mira( int ok, const char *que )
{
    printf( "%s %s\n", ok ? "  ok   " : "  FALLA", que );
    if ( !ok ) fallos++;
}

int main( void )
{
    gchar  *raiz = g_dir_make_tmp( "trabajo-XXXXXX", NULL );
    gchar  *w = g_build_filename( raiz, "_casos", "C1", "work", NULL );
    gchar  *tr, *boot, *out;
    Trabajo t, u;
    char    lin[256], rutas[8][TR_RUTA];
    double  f;
    long    ahora = (long) time( NULL );

    g_mkdir_with_parents( w, 0700 );
    tr = g_build_filename( w, "C1_rango.trabajo", NULL );
    boot = g_build_filename( w, "C1_rango.boot", NULL );
    out = g_build_filename( w, "C1_rango.out", NULL );

    memset( &t, 0, sizeof t );
    g_strlcpy( t.que, "bootstrap del rango", sizeof t.que );
    g_strlcpy( t.objeto, "C1/rango", sizeof t.objeto );
    t.pid = sitio_mi_pid(); t.inicio = ahora - 20; t.total = 57;
    g_strlcpy( t.por, "asistente", sizeof t.por );
    g_strlcpy( t.progreso, "C1_rango.boot", sizeof t.progreso );
    g_strlcpy( t.resultado, "C1_rango.out", sizeof t.resultado );
    mira( trabajo_escribe( tr, &t ) == 0, "se escribe" );
    mira( trabajo_lee( tr, &u ) == 0 && u.pid == t.pid && u.total == 57 && !strcmp( u.por, "asistente" ) &&
          !strcmp( u.progreso, "C1_rango.boot" ), "se lee como se escribió" );
    mira( trabajo_hechos( tr, &u ) == -1, "sin fichero de progreso: -1" );
    g_file_set_contents( boot, "# drvec -bootstrap 19\nh0,rep,status,lr\n0,1,ok,1.0\n0,2,ok,2.0\n", -1, NULL );
    mira( trabajo_hechos( tr, &u ) == 2, "lo hecho: las líneas que empiezan por un dígito" );
    mira( trabajo_estado( tr, &u ) == TR_EN_MARCHA, "su proceso vive: en marcha" );
    trabajo_linea( tr, &u, ahora, lin, sizeof lin, &f );
    printf( "         %s\n", lin );
    mira( strstr( lin, "2/57" ) && strstr( lin, "quedan" ) && strstr( lin, "asistente" ) && f > 0.03 && f < 0.04,
          "la línea: el avance, lo que falta y quién" );

    u.pid = 999999999;                    /* un proceso que no existe */
    mira( trabajo_estado( tr, &u ) == TR_CAIDO, "sin proceso y sin resultado: se cayó" );
    g_file_set_contents( out, "fin\n", -1, NULL );
    mira( trabajo_estado( tr, &u ) == TR_TERMINADO, "sin proceso y con resultado: terminó" );
    trabajo_linea( tr, &u, ahora, lin, sizeof lin, &f );
    mira( strstr( lin, "terminado" ) && f == 1.0, "la línea de uno terminado" );

    mira( trabajo_lista( raiz, rutas, 8 ) == 1 && g_str_has_suffix( rutas[0], "C1_rango.trabajo" ),
          "la lista del proyecto lo encuentra" );
    g_remove( tr ); g_remove( boot ); g_remove( out );
    printf( "\ntrabajo: %d fallos\n", fallos );
    return fallos ? 1 : 0;
}
