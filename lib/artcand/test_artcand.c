/* test_artcand.c -- the .cand reader against engines/art's goldens.
 *   cc -I. test_artcand.c artcand.c -o t && ./t ../../engines/art/tests/golden */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "artcand.h"

static int fails = 0;
#define CHECK( c, msg ) do { if ( !(c) ) { fails++; printf( "FAIL: %s\n", msg ); } } while ( 0 )

int main( int argc, char **argv )
{
    const char *dir = argc > 1 ? argv[1] : "../../engines/art/tests/golden";
    char p[1024];
    ArtCand *c = malloc( sizeof *c );
    char kb[64];

    snprintf( p, sizeof p, "%s/wti.cand", dir );
    int rc = ac_leer( p, c, NULL );
    CHECK( rc == AC_OK, "wti.cand reads" );
    CHECK( !strcmp( c->series, "wti" ), "series name" );
    CHECK( c->log == 1 && c->d == 1 && c->D == 0 && c->s == 12, "transform" );
    CHECK( c->lags == 40 && c->n_used == 215, "sample" );
    CHECK( fabs( c->band - 1.96 / sqrt( 215.0 ) ) < 1e-9, "band" );
    CHECK( c->has_seasonal && !c->seasonal_detected && c->seasonal_s == 12, "seasonal" );
    CHECK( c->has_unit_root && c->adf_stat < -8, "adf" );
    CHECK( c->ncand >= 10 && c->cand[0].proposed && !c->cand[1].proposed, "candidates" );
    CHECK( c->cand[0].p == 1 && c->cand[0].q == 0, "proposal (1,0)(0,0)" );
    CHECK( fabs( c->cand[0].phi[0] - c->acf[0] ) < 1e-9, "AR(1) by Yule-Walker: phi = r1" );
    CHECK( fabs( c->cand[0].tacf[0] - c->cand[0].phi[0] ) < 1e-6, "theoretical acf(1) = phi" );
    CHECK( !strcmp( ac_kind( &c->cand[0], kb, sizeof kb ), "AR(1)" ), "kind" );
    CHECK( c->nmsg >= 1 && c->complete, "messages and end" );

    snprintf( p, sizeof p, "%s/resid.cand", dir );
    rc = ac_leer( p, c, NULL );
    CHECK( rc == AC_OK, "resid reads" );
    CHECK( !strcmp( ac_kind( &c->cand[0], kb, sizeof kb ), "white noise" ), "resid: white noise proposed" );

    /* truncated and malformed files */
    FILE *f = fopen( "trunc.cand", "w" );
    fprintf( f, "series x\nsample n_used 10 lags 5 band 0.6\n" );
    fclose( f );
    CHECK( ac_leer( "trunc.cand", c, NULL ) == AC_ETRUNC, "truncated" );
    f = fopen( "bad.cand", "w" );
    fprintf( f, "series x\nsample n_used 10 lags 500 band 0.6\nend\n" );
    fclose( f );
    int line = 0;
    CHECK( ac_leer( "bad.cand", c, &line ) == AC_EFORMAT && line == 2, "too many lags is a format error" );
    f = fopen( "new.cand", "w" );
    fprintf( f, "series x\nsomething_new 1 2 3\nend\n" );
    fclose( f );
    CHECK( ac_leer( "new.cand", c, NULL ) == AC_OK, "unknown keys are ignored" );
    CHECK( ac_leer( "no/such.cand", c, NULL ) == AC_ENOFILE, "no file" );
    remove( "trunc.cand" ); remove( "bad.cand" ); remove( "new.cand" );

    free( c );
    printf( "artcand: %s\n", fails ? "FAILED" : "ok" );
    return fails != 0;
}
