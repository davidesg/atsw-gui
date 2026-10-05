/*
 * artcand.c -- reading DATA_art.cand. See artcand.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "artcand.h"

/* The values after the key, as doubles; returns how many (up to max). */
static int nums( const char *s, double *v, int max )
{
    int k = 0;
    char *end;
    while ( k < max )
        {
        double x = strtod( s, &end );
        if ( end == s ) break;
        v[k++] = x;
        s = end;
        }
    return k;
}

/* "key1 v1 key2 v2 ..." -> the value of `name`, as a double. */
static int field( const char *s, const char *name, double *out )
{
    char buf[1024];
    snprintf( buf, sizeof buf, " %s", s );
    char pat[64];
    snprintf( pat, sizeof pat, " %s ", name );
    const char *p = strstr( buf, pat );
    if ( !p ) return 0;
    char *end;
    *out = strtod( p + strlen( pat ), &end );
    return end != p + strlen( pat );
}

#define F( name, dst ) do { double _v; if ( !field( rest, name, &_v ) ) goto bad; \
                            dst = _v; } while ( 0 )

int ac_leer( const char *path, ArtCand *c, int *line )
{
    memset( c, 0, sizeof *c );
    if ( line ) *line = 0;
    FILE *f = fopen( path, "r" );
    if ( !f ) return AC_ENOFILE;

    char buf[16384];
    int ln = 0;
    AcCand *k = NULL;
    while ( fgets( buf, sizeof buf, f ) )
        {
        ln++;
        buf[strcspn( buf, "\r\n" )] = 0;
        if ( !buf[0] || buf[0] == '#' ) continue;
        char key[32];
        int used = 0;
        if ( sscanf( buf, "%31s%n", key, &used ) != 1 ) continue;
        const char *rest = buf + used;
        while ( *rest == ' ' ) rest++;

        if ( !strcmp( key, "series" ) )
            snprintf( c->series, sizeof c->series, "%s", rest );
        else if ( !strcmp( key, "transform" ) )
            { F( "log", c->log ); F( "d", c->d ); F( "D", c->D ); F( "s", c->s ); }
        else if ( !strcmp( key, "sample" ) )
            {
            F( "n_used", c->n_used ); F( "lags", c->lags ); F( "band", c->band );
            if ( c->lags < 0 || c->lags > AC_MAX_LAGS ) goto bad;
            }
        else if ( !strcmp( key, "seasonal" ) )
            {
            c->has_seasonal = 1;
            F( "F", c->seasonal_F ); F( "p", c->seasonal_p );
            F( "detected", c->seasonal_detected ); F( "s", c->seasonal_s );
            }
        else if ( !strcmp( key, "dummies" ) )
            nums( rest, c->dummies, AC_MAX_S );
        else if ( !strcmp( key, "adf" ) )
            {
            c->has_unit_root = 1;
            F( "stat", c->adf_stat ); F( "p", c->adf_p ); F( "crit", c->adf_crit );
            F( "lags", c->adf_lags );
            }
        else if ( !strcmp( key, "kpss" ) )
            {
            F( "stat", c->kpss_stat ); F( "p", c->kpss_p ); F( "crit", c->kpss_crit );
            F( "lags", c->kpss_lags );
            }
        else if ( !strcmp( key, "acf" ) )
            nums( rest, c->acf, AC_MAX_LAGS );
        else if ( !strcmp( key, "pacf" ) )
            nums( rest, c->pacf, AC_MAX_LAGS );
        else if ( !strcmp( key, "candidate" ) )
            {
            if ( c->ncand >= AC_MAX_CAND ) { k = NULL; continue; }
            k = &c->cand[c->ncand++];
            F( "p", k->p ); F( "q", k->q ); F( "P", k->P ); F( "Q", k->Q );
            F( "sim", k->sim ); F( "weight", k->weight ); F( "aicc", k->aicc );
            F( "scored", k->scored ); F( "proposed", k->proposed );
            if ( k->p > AC_MAX_ORD || k->q > AC_MAX_ORD ||
                 k->P > AC_MAX_ORD || k->Q > AC_MAX_ORD ) goto bad;
            }
        else if ( k && !strcmp( key, "phi" ) )   nums( rest, k->phi, AC_MAX_ORD );
        else if ( k && !strcmp( key, "theta" ) ) nums( rest, k->theta, AC_MAX_ORD );
        else if ( k && !strcmp( key, "Phi" ) )   nums( rest, k->Phi, AC_MAX_ORD );
        else if ( k && !strcmp( key, "Theta" ) ) nums( rest, k->Theta, AC_MAX_ORD );
        else if ( k && !strcmp( key, "tacf" ) )  nums( rest, k->tacf, AC_MAX_LAGS );
        else if ( k && !strcmp( key, "tpacf" ) ) nums( rest, k->tpacf, AC_MAX_LAGS );
        else if ( !strcmp( key, "message" ) )
            {
            if ( c->nmsg < AC_MAX_MSG )
                snprintf( c->msg[c->nmsg++], AC_MSG_LEN, "%s", rest );
            }
        else if ( !strcmp( key, "end" ) )
            {
            c->complete = 1;
            break;
            }
        /* unknown keys: ignored, so the format can grow */
        }
    fclose( f );
    return c->complete ? AC_OK : AC_ETRUNC;

bad:
    fclose( f );
    if ( line ) *line = ln;
    return AC_EFORMAT;
}

const char *ac_kind( const AcCand *c, char *b, int n )
{
    int k = 0;
    b[0] = 0;
    if ( c->p ) k += snprintf( b + k, n - k, "AR(%d)", c->p );
    if ( c->q ) k += snprintf( b + k, n - k, "%sMA(%d)", k ? "+" : "", c->q );
    if ( c->P || c->Q )
        {
        k += snprintf( b + k, n - k, "%s", k ? " x " : "" );
        if ( c->P ) k += snprintf( b + k, n - k, "SAR(%d)", c->P );
        if ( c->Q ) k += snprintf( b + k, n - k, "%sSMA(%d)", c->P ? "+" : "", c->Q );
        }
    if ( !k ) snprintf( b, n, "white noise" );
    return b;
}
