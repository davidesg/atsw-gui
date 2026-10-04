/*
 * test_inpdet.c -- el bloque de deterministas, barrido sobre el corpus.
 *
 * LA PRUEBA QUE IMPORTA ES LA DE CERO. Añadir cero deterministas a un .inp
 * tiene que devolverlo BYTE A BYTE IGUAL. Como lo que no cambia se copia sin
 * mirarlo, lo único que esa prueba puede pillar es que los límites del bloque
 * estén mal encontrados -- que es lo único que puede salir mal aquí, y lo que
 * en este formato no da error sino un .pre con basura.
 *
 * Se barren los 114 ficheros del corpus del motor, no tres hechos a mano: el
 * formato tiene variantes (sin deterministas, con armónicos, con columnas
 * propias, anuales, de fug, de fuf) y un fichero inventado sólo prueba el que
 * yo me imaginé.
 *
 * Y el juez de lo que se escribe es inp_check_fue, la puerta del PROPIO
 * motor: lo que el motor acepta es lo que esto acepta, por construcción y no
 * por parecido.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "inpdet.h"
#include "inpcheck.h"

/* UN FICHERO TEMPORAL, en la carpeta temporal del sistema y no en "/tmp" a
 * fuego: en Windows nativo "/tmp" es D:\tmp, que no existe, y la prueba
 * fallaba por no poder escribir. Lo vio la CI de Windows.               */
static const char *temporal( const char *nombre )
{
    static char buf[4][1024];
    static int  k;
    const char *d = getenv( "TMPDIR" );

    if ( !d || !*d ) d = getenv( "TEMP" );
    if ( !d || !*d ) d = getenv( "TMP" );
#ifdef _WIN32
    if ( !d || !*d ) d = ".";
#else
    if ( !d || !*d ) d = "/tmp";
#endif
    k = ( k + 1 ) % 4;
    snprintf( buf[k], sizeof buf[k], "%s/%s", d, nombre );
    return buf[k];
}

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static char *lee( const char *ruta, long *n )
{
    FILE *f = fopen( ruta, "rb" );
    char *b;

    *n = 0;
    if ( !f ) return NULL;
    fseek( f, 0, SEEK_END );
    *n = ftell( f );
    fseek( f, 0, SEEK_SET );
    b = (char *) malloc( (size_t) *n + 1 );
    if ( b && fread( b, 1, (size_t) *n, f ) != (size_t) *n ) { free( b ); b = NULL; }
    fclose( f );
    return b;
}

static int iguales( const char *a, const char *b )
{
    long  na, nb;
    char *x = lee( a, &na ), *y = lee( b, &nb );
    int   c = ( x && y && na == nb && memcmp( x, y, (size_t) na ) == 0 );

    free( x ); free( y );
    return c;
}

/* cuantas veces aparece la palabra al principio de una linea */
static int cuenta( const char *ruta, const char *palabra )
{
    FILE *f = fopen( ruta, "rb" );
    char  l[4096];
    int   n = 0, k = (int) strlen( palabra );

    if ( !f ) return -1;
    while ( fgets( l, sizeof l, f ) )
        if ( !strncmp( l, palabra, (size_t) k ) ) n++;
    fclose( f );
    return n;
}

int main( int argc, char **argv )
{
    const char *dir = ( argc > 1 ) ? argv[1] : "../../engines/fue/tests/corpus";
    const char *tmp = temporal( "test_inpdet_salida.inp" );
    char        porque[512], msg[512];
    DIR        *d;
    struct dirent *e;
    int         vistos = 0, copiados = 0, rechazados = 0;

    printf( "\nAÑADIR CERO DEVUELVE EL FICHERO IDENTICO\n" );
    d = opendir( dir );
    if ( !d ) { printf( "  FALLO no pude abrir «%s»\n", dir ); return 1; }
    while ( ( e = readdir( d ) ) != NULL )
        {
        char ruta[1024];
        int  n = (int) strlen( e->d_name );

        if ( n < 5 || strcmp( e->d_name + n - 4, ".inp" ) ) continue;
        snprintf( ruta, sizeof ruta, "%s/%s", dir, e->d_name );
        if ( inp_check_fue( ruta, msg, sizeof msg ) != 0 ) continue;  /* no es de fue */
        vistos++;
        if ( id_anade( ruta, tmp, NULL, NULL, 0, porque, sizeof porque ) != 0 )
           {
           /* Rechazar es legítimo; reescribir mal, no. Se cuenta y se dice. */
           printf( "  ---   %-34s rechazado: %s\n", e->d_name, porque );
           rechazados++;
           continue;
           }
        if ( iguales( ruta, tmp ) ) copiados++;
        else
           {
           printf( "  FALLO %s NO sale idéntico al copiarlo\n", e->d_name );
           fallos++;
           }
        }
    closedir( d );
    printf( "        %d ficheros de fue en el corpus: %d idénticos, %d rechazados\n",
            vistos, copiados, rechazados );
    ok( vistos > 20, "el barrido ha visto un corpus de verdad" );
    ok( rechazados == 0, "y ninguno se ha rechazado" );

    printf( "\nUNA INTERVENCION ENTRA EN LOS CINCO SITIOS\n" );
    {
    char ruta[1024];
    const char *nueva[1] = { "step 10 2008" };
    int   det_antes, det_despues;

    snprintf( ruta, sizeof ruta, "%s/CPI_USA_model.inp", dir );
    det_antes = cuenta( ruta, "step " ) + cuenta( ruta, "impulse " );
    ok( id_anade( ruta, tmp, nueva, NULL, 1, porque, sizeof porque ) == 0,
        "se escribe" );
    ok( inp_check_fue( tmp, msg, sizeof msg ) == 0,
        "y el MOTOR lo acepta, que es el único juez que cuenta" );
    if ( inp_check_fue( tmp, msg, sizeof msg ) != 0 ) printf( "        %s\n", msg );
    det_despues = cuenta( tmp, "step " ) + cuenta( tmp, "impulse " );
    ok( det_despues == det_antes + 1, "hay una intervención más" );
    ok( cuenta( tmp, "16" ) >= 1, "y el número de deterministas ha subido a 16" );
    }

    printf( "\nY EN UN FICHERO QUE NO TENIA NINGUNO\n" );
    {
    char ruta[1024];
    const char *nueva[2] = { "step 3 2020", "impulse 4 2020" };

    snprintf( ruta, sizeof ruta, "%s/CPI_USA.inp", dir );
    if ( inp_check_fue( ruta, msg, sizeof msg ) == 0 )
       {
       ok( id_anade( ruta, tmp, nueva, NULL, 2, porque, sizeof porque ) == 0, "se escriben las dos" );
       ok( inp_check_fue( tmp, msg, sizeof msg ) == 0, "y el motor las acepta" );
       if ( inp_check_fue( tmp, msg, sizeof msg ) != 0 ) printf( "        %s\n", msg );
       }
    else printf( "  ---   CPI_USA.inp no es un .inp de fue con modelo; se salta\n" );
    }

    printf( "\nLO QUE YA ESTA PUESTO SE PUEDE MIRAR ANTES DE PONERLO DOS VECES\n" );
    {
    char ruta[1024], det[16][ID_LINEA];
    int  k, n;

    snprintf( ruta, sizeof ruta, "%s/CPI_USA_model.inp", dir );
    n = id_intervenciones( ruta, det, 16, porque, sizeof porque );
    ok( n == 4, "las cuatro que trae el fichero" );
    for ( k = 0; k < n && k < 16; k++ ) printf( "        %s\n", det[k] );
    ok( n > 0 && !strcmp( det[0], "step 10 2008" ), "leídas como el motor las lee" );
    }

    printf( "\nLO QUE NO SE ESCRIBE, NO SE ESCRIBE A MEDIAS\n" );
    {
    char ruta[1024];
    const char *mala[1]  = { "PIB_real" };      /* trae columna de datos propia */
    const char *corta[1] = { "step 2008" };     /* mensual: falta el periodo    */

    snprintf( ruta, sizeof ruta, "%s/CPI_USA_model.inp", dir );
    remove( tmp );
    ok( id_anade( ruta, tmp, mala, NULL, 1, porque, sizeof porque ) != 0,
        "un determinista con datos propios se rechaza" );
    printf( "        %s\n", porque );
    ok( fopen( tmp, "rb" ) == NULL, "y el destino NO se ha tocado" );
    ok( id_anade( ruta, tmp, corta, NULL, 1, porque, sizeof porque ) != 0,
        "una intervención sin período en una serie mensual, también" );
    printf( "        %s\n", porque );
    }

    printf( "\nEL PELDAÑO 2 ES UNA «step» CON L+1 OMEGAS\n" );
    {
    char ruta[1024];
    const char *nueva[1] = { "step 3 2022" };
    int   om[1] = { 2 };            /* L = 2 -> TRES coeficientes */
    int   ceros;

    snprintf( ruta, sizeof ruta, "%s/CPI_USA_model.inp", dir );
    ok( id_anade( ruta, tmp, nueva, om, 1, porque, sizeof porque ) == 0,
        "se escribe con tres coeficientes" );
    ok( inp_check_fue( tmp, msg, sizeof msg ) == 0,
        "y el MOTOR lo acepta: tres omegas donde dice que hay tres" );
    if ( inp_check_fue( tmp, msg, sizeof msg ) != 0 ) printf( "        %s\n", msg );

    /* el fichero tenia 15 deterministas con un coeficiente cada uno;
       ahora tiene 16, y el ultimo lleva TRES. */
    ceros = cuenta( tmp, "0.000000  1" );
    ok( ceros == 15 + 3, "quince de antes mas los tres nuevos" );

    /* y una cuenta que el motor no admitiria se recorta en vez de colarse */
    {
    int malo[1] = { -5 };

    ok( id_anade( ruta, tmp, nueva, malo, 1, porque, sizeof porque ) == 0 &&
        inp_check_fue( tmp, msg, sizeof msg ) == 0,
        "una cuenta negativa no escribe un fichero roto" );
    }
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
