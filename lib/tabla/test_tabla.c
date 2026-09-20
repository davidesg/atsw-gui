/*
 * test_tabla.c -- que los tres renderizados digan LOS MISMOS NUMEROS, y que la
 * procedencia no se pueda perder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tabla.h"

static int fallos = 0;

/* Cuantos CARACTERES hay entre a y b. Para comprobar que dos lineas cuadran
 * aunque lleven acentos.                                                 */
static int ancho_hasta( const char *a, const char *b )
{
    int n = 0;

    for ( ; a < b; a++ )
        if ( ( (unsigned char) *a & 0xC0 ) != 0x80 ) n++;
    return n;
}

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

/* El fichero entero, para poder buscar dentro. */
static char *lee( const char *path )
{
    FILE  *f = fopen( path, "rb" );
    char  *b;
    long   n;

    if ( f == NULL ) return NULL;
    fseek( f, 0, SEEK_END ); n = ftell( f ); fseek( f, 0, SEEK_SET );
    b = (char *) malloc( (size_t) n + 1 );
    if ( b == NULL ) { fclose( f ); return NULL; }
    if ( fread( b, 1, (size_t) n, f ) != (size_t) n ) { free(b); fclose(f); return NULL; }
    b[n] = '\0';
    fclose( f );
    return b;
}

int main( int argc, char **argv )
{
    const char *dir = ( argc > 1 ) ? argv[1] : ".";
    char   csv[512], txt[512], tex[512];
    Tabla *t;

    snprintf( csv, sizeof csv, "%s/t.csv", dir );
    snprintf( txt, sizeof txt, "%s/t.txt", dir );
    snprintf( tex, sizeof tex, "%s/t.tex", dir );

    t = tb_new( "Parámetros estimados" );
    ok( t != NULL, "se crea la tabla" );

    tb_procedencia( t, "Modelo",  "m6, 4 enlaces" );
    tb_procedencia( t, "Muestra", "1/1977 - 4/1992, 64 obs" );
    tb_procedencia( t, "Motor",   "drtran 1.0" );

    tb_col( t, "Parámetro", NULL,  TB_TXT, 0 );
    tb_col( t, "Estimado",  NULL,  TB_NUM, 4 );
    tb_col( t, "d.t.",      NULL,  TB_NUM, 4 );
    tb_col( t, "n",         NULL,  TB_ENT, 0 );
    tb_col( t, "Reducción", "%",   TB_NUM, 1 );
    ok( tb_ncols( t ) == 5, "cinco columnas" );

    tb_fila( t );
    tb_pon_txt( t, 0, "omega1[0]" );
    tb_pon_num( t, 1, 0.749918 );
    tb_pon_num( t, 2, 0.279073 );
    tb_pon_num( t, 3, 64 );
    tb_pon_num( t, 4, -12.34 );

    tb_fila( t );
    tb_pon_txt( t, 0, "omega1[1]" );
    tb_pon_num( t, 1, 0.300377 );
    tb_pon_vacio( t, 2 );                     /* atado: no tiene d.t.      */
    tb_pon_num( t, 3, 64 );
    /* la columna 4 NO se pone: tiene que salir vacia, no basura           */

    /* Una celda que hay que entrecomillar en CSV, y escapar en TeX. */
    tb_fila( t );
    tb_pon_txt( t, 0, "q[5,2] \"libre\" 100% _x_" );
    tb_pon_num( t, 1, 1.5 );
    tb_pon_num( t, 2, 0.0 );
    tb_pon_num( t, 3, 64 );
    tb_pon_num( t, 4, 0.05 );

    ok( tb_nfilas( t ) == 3, "tres filas" );

    printf( "\nlos tres renderizados\n" );
    ok( tb_write_csv( t, csv ) == 0, "se escribe el CSV" );
    ok( tb_write_txt( t, txt ) == 0, "se escribe el TXT" );
    ok( tb_write_tex( t, tex ) == 0, "se escribe el TeX" );

    printf( "\nLOS MISMOS NUMEROS EN LOS TRES\n" );
    {
    char *a = lee( csv ), *b = lee( txt ), *c = lee( tex );

    ok( a && b && c, "los tres se leen" );
    if ( a && b && c )
        {
        /* Los decimales los declara la COLUMNA, no el formato. */
        ok( strstr( a, "0.7499" ) && strstr( b, "0.7499" ) &&
            strstr( c, "0.7499" ), "0.7499 aparece en los tres" );
        ok( !strstr( a, "0.749918" ), "y el CSV NO lleva mas precision que el resto" );
        ok( strstr( a, "-12.3" ) && strstr( b, "-12.3" ),
            "un decimal donde la columna dice uno" );
        }

    printf( "\nla procedencia no se puede perder\n" );
    if ( a && b && c )
        {
        ok( strstr( a, "# Modelo: m6, 4 enlaces" ) != NULL,
            "CSV: en lineas de '#', como el fichero de residuos" );
        ok( strstr( b, "Motor: drtran 1.0" ) != NULL, "TXT: al pie" );
        ok( strstr( c, "drtran 1.0" ) != NULL, "TeX: al pie" );
        }

    printf( "\nlas celdas vacias y el escapado\n" );
    if ( a && b && c )
        {
        ok( strstr( a, ",-," ) != NULL,
            "una celda vacia se escribe '-', no basura" );
        ok( strstr( a, "\"q[5,2] \"\"libre\"\" 100% _x_\"" ) != NULL,
            "CSV: coma y comillas, segun RFC 4180" );
        ok( strstr( c, "100\\%" ) && strstr( c, "\\_x\\_" ),
            "TeX: % y _ escapados" );
        ok( strstr( b, "----" ) != NULL, "TXT: la regla bajo la cabecera" );

        /* R5: UNA TABLA QUE NO MANTIENE LOS ESPACIOS DEJA DE LEERSE. El
         * ancho se cuenta en CARACTERES: "Parámetro" son 9, no 10 bytes. */
        {
        /* Con el espacio detras: si no, encuentra el TITULO
         * ("Parámetros estimados") en vez de la cabecera.               */
        const char *l1 = strstr( b, "Parámetro " );
        const char *l2 = strstr( b, "omega1[0]" );
        const char *e1 = l1 ? strstr( l1, "Estimado" ) : NULL;
        const char *e2 = l2 ? strstr( l2, "0.7499" )   : NULL;
        /* Las columnas de numeros van a la DERECHA, asi que lo que tiene
         * que coincidir es donde ACABAN, no donde empiezan.            */
        int col1 = ( l1 && e1 ) ? ancho_hasta( l1, e1 + strlen( "Estimado" ) ) : -1;
        int col2 = ( l2 && e2 ) ? ancho_hasta( l2, e2 + strlen( "0.7499" ) )   : -2;

        ok( col1 == col2,
            "la cabecera con acento CUADRA con los datos (R5)" );
        }
        }
    free( a ); free( b ); free( c );
    }

    printf( "\nla extension elige el formato\n" );
    {
    char *c;

    ok( tb_write( t, tex ) == 0, "tb_write con .tex" );
    c = lee( tex );
    ok( c && strstr( c, "\\begin{tabular}" ) != NULL, "y sale un tabular" );
    free( c );
    }

    printf( "\nlos bordes\n" );
    {
    Tabla *v = tb_new( NULL );

    ok( tb_fila( v ) == -1, "sin columnas no se puede abrir fila" );
    tb_col( v, "a", NULL, TB_TXT, 0 );
    tb_fila( v );
    tb_pon_txt( v, 9, "fuera" );              /* no debe petar           */
    ok( tb_nfilas( v ) == 1, "una columna fuera de rango no hace nada" );
    ok( tb_write_csv( v, csv ) == 0, "una tabla sin titulo se escribe igual" );
    tb_free( v );
    tb_free( NULL );                          /* tampoco debe petar      */
    }

    tb_free( t );
    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
