/*
 * test_eqdraw.c -- la pagina del modelo es UNA para fue y para drtran_gui.
 *
 * Se dibuja la misma ecuacion univariante dos veces, sin y con un termino de
 * transferencia, y se exige:
 *
 *   1. que el ruido salga IGUAL, byte a byte: la transferencia solo anade su
 *      termino en la primera linea, no cambia nada de lo demas;
 *   2. que la transferencia vaya DELANTE de los deterministas, en la misma
 *      linea que ellos, con su entrada (ln X_t) y su omega;
 *   3. que la pagina entera se escriba (eq_page_pdf) con y sin grafico.
 *
 *   cc -I. -I../fugdraw test_eqdraw.c eqdraw.c ../fugdraw/fugdraw.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eqdraw.h"

static int fallos = 0;

static void check( int ok, const char *que )
{
    printf( "%s: %s\n", ok ? "ok" : "FAIL", que );
    if (!ok) fallos++;
}

/* ln Y_t = 0.0016 cos(pi/6) t + N_t ;  (1 - 0.40 B) [ nabla N_t - 0.0015 ] = A_t */
static void univariante( Equation *eq )
{
    EqItem *it;

    eq_new( eq );
    snprintf( eq->name, sizeof eq->name, "Y" );
    snprintf( eq->residuals, sizeof eq->residuals, "AY" );
    eq->is_log = 1;
    eq->freq = 12;
    eq->nobs = 120;
    eq->sigma = 0.25;
    eq->sigma_pct = 1;

    eq_add_text( eq, EQ_DETER, "Y" );
    eq_add_op( eq, EQ_DETER, '=' );
    eq_add_op( eq, EQ_DETER, '-' );
    eq_add_coef( eq, EQ_DETER, CS_OMEGA, ' ', -0.0016, 1, 0.0007 );
    it = eq_add( eq, EQ_DETER, EI_HARMONIC );
    snprintf( it->text, sizeof it->text, "cos" );
    eq_harmonic_angle( 1, 12, &it->a, &it->b );
    eq_add_op( eq, EQ_DETER, '+' );
    eq_add_text( eq, EQ_DETER, "N" );

    eq_add_open( eq, EQ_ARR, '(', 0 );
    eq_add_text( eq, EQ_ARR, "1" );
    eq_add_coef( eq, EQ_ARR, CS_PHI, '-', 0.40, 1, 0.06 );
    eq_add_b( eq, EQ_ARR, 1, 0 );
    eq_add_close( eq, EQ_ARR, ')' );
    eq_add_coef( eq, EQ_MU, CS_MU, '-', 0.0015, 1, 0.0003 );
    eq_add( eq, EQ_NRDIFF, EI_NABLA )->a = 1;
}

/* + 0.027 B ln X_t, delante de los deterministas */
static void transferencia( Equation *eq )
{
    EqItem *it;

    eq_add_op( eq, EQ_TRANS, '+' );
    eq_add_coef( eq, EQ_TRANS, CS_OMEGA, ' ', 0.027, 1, 0.004 );
    eq_add_b( eq, EQ_TRANS, 1, 0 );
    it = eq_add( eq, EQ_TRANS, EI_INPUT );
    snprintf( it->text, sizeof it->text, "WTI" );
    it->a = 1;
}

/* Las ordenes de texto de un dibujo cuya y queda por debajo de ymax, en el
 * orden en que se escribieron: el ruido, bajo la primera linea.          */
static char *por_debajo( const FDFig *f, double ymax )
{
    char       *out = calloc( 1, f->len + 1 );
    const char *p = f->buf;

    while (out && (p = strstr( p, "BT " )) != NULL) {
        const char *e = strstr( p, "ET\n" );
        double      x, y;
        char        fuente[16];
        double      tam;

        if (!e) break;
        if (sscanf( p, "BT %15s %lf Tf %lf %lf Td", fuente, &tam, &x, &y ) == 4 &&
            y < ymax)
            strncat( out, p, (size_t) (e - p) + 3 );
        p = e;
    }
    return out;
}

/* La x de la primera orden que escribe s, o -1 */
static double x_de( const FDFig *f, const char *s )
{
    char        aguja[64];
    const char *p, *bt;
    double      tam, x, y;
    char        fuente[16];

    snprintf( aguja, sizeof aguja, "(%s) Tj", s );
    if (!(p = strstr( f->buf, aguja ))) return -1.0;
    for (bt = p; bt > f->buf && strncmp( bt, "BT ", 3 ); bt--) ;
    if (sscanf( bt, "BT %15s %lf Tf %lf %lf Td", fuente, &tam, &x, &y ) != 4) return -1.0;
    return x;
}

int main( int argc, char **argv )
{
    Equation   u, t;
    FDFig     *fu = fd_fig_new( 842, 595 ), *ft = fd_fig_new( 842, 595 );
    double     du, dt, bu, bt, primera;
    char      *ru, *rt;
    const char *dir = argc > 1 ? argv[1] : ".";
    char       pdf[1024];
    EqOutlier  o[1] = { { 7, "7/2016", -3.17 } };

    univariante( &u );
    univariante( &t );
    transferencia( &t );

    bu = eq_draw( fu, &u, 400.0, 70.866, 771.0, &du );
    bt = eq_draw( ft, &t, 400.0, 70.866, 771.0, &dt );
    check( bu == bt && du == dt, "con la transferencia, las lineas caen donde caian" );

    /* la primera linea: su baseline es la misma en las dos */
    primera = 400.0 - 19.2;
    ru = por_debajo( fu, primera - 12.0 );
    rt = por_debajo( ft, primera - 12.0 );
    check( ru && rt && *ru && !strcmp( ru, rt ),
           "el ruido sale igual, byte a byte, con y sin transferencia" );

    check( x_de( ft, "WTI" ) > 0.0 && x_de( ft, "cos" ) > x_de( ft, "WTI" ),
           "la transferencia (ln WTI_t) va delante de los deterministas" );
    check( x_de( ft, "ln" ) > 0.0 && x_de( fu, "WTI" ) < 0.0,
           "sin transferencia no hay entrada que escribir" );
    check( x_de( ft, "0.027" ) > 0.0 && x_de( ft, "0.027" ) < x_de( ft, "WTI" ),
           "el omega de la transferencia, delante de su entrada" );

    snprintf( pdf, sizeof pdf, "%s/eqdraw_u.pdf", dir );
    check( eq_page_pdf( pdf, NULL, &u, o, 1, 1 ) == 0, "la pagina univariante se escribe" );
    snprintf( pdf, sizeof pdf, "%s/eqdraw_t.pdf", dir );
    check( eq_page_pdf( pdf, NULL, &t, o, 1, 1 ) == 0, "la pagina con transferencia se escribe" );

    free( ru );
    free( rt );
    fd_fig_free( fu );
    fd_fig_free( ft );
    eq_release( &u );
    eq_release( &t );
    printf( "%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
