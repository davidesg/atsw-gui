/***************************************************************************/
/*  TRANSFORM.C                                                            */
/*  Box-Cox transformation and regular/seasonal differencing for DRVARMA. */
/*  A single (global) lambda, d and D are applied to every series.        */
/***************************************************************************/

#include "transform.h"

/*-------------------------------------------------------------------------*/
/*  Box-Cox (scalar).                                                      */
/*-------------------------------------------------------------------------*/
real boxcox_fwd( real x, real lambda )
{
    if ( lambda == 0.0 ) return log( x );
    return ( pow( x, lambda ) - 1.0 ) / lambda;
}

real boxcox_inv( real y, real lambda )
{
    if ( lambda == 0.0 ) return exp( y );
    return pow( lambda * y + 1.0, 1.0 / lambda );
}

/*-------------------------------------------------------------------------*/
/*  Single differencing steps (1-based vectors).                           */
/*  regular: out[t] = in[t+1]   - in[t]      (length n-1)                   */
/*  seasonal: out[t] = in[t+s]  - in[t]      (length n-s)                   */
/*-------------------------------------------------------------------------*/
static void diff_lag( real *in, int n, int lag, real *out )
{
    for ( int t = 1; t <= n - lag; t++ )
        out[t] = in[t + lag] - in[t];
}

/*-------------------------------------------------------------------------*/
/*  transform_series                                                       */
/*-------------------------------------------------------------------------*/
real **transform_series( real **raw, int nobs_raw, int m,
                         real lambda, real scale, int d, int D, int s,
                         int *nobs_eff, real ***bc_out, int *ifault )
{
    *ifault = 0;
    int neff = nobs_raw - d - D * s;
    if ( neff < 1 )
    {
        *ifault = 1;                 /* not enough data for d / D diffs   */
        return NULL;
    }

    /* Box-Cox series (before differencing), kept for forecast inversion. */
    real **bc = matrix( 1, nobs_raw, 1, m );
    for ( int t = 1; t <= nobs_raw; t++ )
        for ( int i = 1; i <= m; i++ )
        {
            real x = raw[t][i];
            if ( lambda != 1.0 && x <= 0.0 )
            {
                *ifault = 2;         /* non-positive value, Box-Cox undef. */
                free_matrix( bc, 1, nobs_raw, 1, m );
                return NULL;
            }
            bc[t][i] = scale * boxcox_fwd( x, lambda );
        }

    real **out = matrix( 1, neff, 1, m );

    /* Difference each series independently (global d, D, s). */
    real *work = vector( 1, nobs_raw );
    real *tmp  = vector( 1, nobs_raw );
    for ( int i = 1; i <= m; i++ )
    {
        int len = nobs_raw;
        for ( int t = 1; t <= len; t++ ) work[t] = bc[t][i];

        for ( int k = 1; k <= d; k++ )       /* regular differences (lag 1) */
        {
            diff_lag( work, len, 1, tmp );
            len -= 1;
            for ( int t = 1; t <= len; t++ ) work[t] = tmp[t];
        }
        for ( int k = 1; k <= D; k++ )       /* seasonal differences (lag s) */
        {
            diff_lag( work, len, s, tmp );
            len -= s;
            for ( int t = 1; t <= len; t++ ) work[t] = tmp[t];
        }
        /* len should now equal neff */
        for ( int t = 1; t <= neff; t++ ) out[t][i] = work[t];
    }
    free_vector( tmp, 1, nobs_raw );
    free_vector( work, 1, nobs_raw );

    *nobs_eff = neff;
    *bc_out   = bc;
    return out;
}

/*-------------------------------------------------------------------------*/
/*  integrate_forecast                                                     */
/*  Undo seasonal (D, lag s) then regular (d, lag 1) differencing on the   */
/*  forecast path, then invert Box-Cox.  Done per series.                  */
/*-------------------------------------------------------------------------*/
void integrate_forecast( real **bc, int nobs_raw, int m,
                         real **wf, int L,
                         real lambda, real scale, int d, int D, int s,
                         real **level_out )
{
    int nstage = d + D;                       /* number of difference steps */

    for ( int i = 1; i <= m; i++ )
    {
        /* Rebuild, from the Box-Cox history, the input series of every     */
        /* differencing stage: chain[0] = bc, chain[k] = diff(chain[k-1]).  */
        /* chain[k] has length len[k].                                      */
        real **chain = (real **) malloc( (nstage + 1) * sizeof(real *) );
        int   *len   = (int *)   malloc( (nstage + 1) * sizeof(int) );

        len[0]   = nobs_raw;
        chain[0] = vector( 1, len[0] );
        for ( int t = 1; t <= len[0]; t++ ) chain[0][t] = bc[t][i];

        for ( int k = 1; k <= nstage; k++ )
        {
            int lag = ( k <= d ) ? 1 : s;
            len[k]   = len[k-1] - lag;
            chain[k] = vector( 1, len[k] );
            diff_lag( chain[k-1], len[k-1], lag, chain[k] );
        }

        /* Forecast of the fully-differenced series (stage nstage). */
        real *cf = vector( 1, L );
        for ( int l = 1; l <= L; l++ ) cf[l] = wf[l][i];

        /* Integrate stage by stage back to chain[0] (Box-Cox level). */
        for ( int k = nstage; k >= 1; k-- )
        {
            int lag    = ( k <= d ) ? 1 : s;
            int n_prev = len[k-1];
            real *pf   = vector( 1, L );
            for ( int l = 1; l <= L; l++ )
            {
                real prev;
                if ( l - lag <= 0 )
                    prev = chain[k-1][n_prev + (l - lag)];   /* actual value */
                else
                    prev = pf[l - lag];                      /* forecast     */
                pf[l] = cf[l] + prev;
            }
            free_vector( cf, 1, L );
            cf = pf;
        }

        /* cf now holds (scale * Box-Cox) level forecasts; undo the scale and
           invert Box-Cox to return to original units. */
        for ( int l = 1; l <= L; l++ )
            level_out[i][l] = boxcox_inv( cf[l] / scale, lambda );

        free_vector( cf, 1, L );
        for ( int k = 0; k <= nstage; k++ ) free_vector( chain[k], 1, len[k] );
        free( chain );
        free( len );
    }
}
