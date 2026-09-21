/* outfile.c -- lo que se lee del .out que escribe el motor.
 *
 * Aparte para poder probarlo: file_io.c entero habla con GTK, y esto no.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include "outfile.h"

static const struct { const char *mark; ConvKind kind; const char *brief; } kinds[] = {
    { "GRADIENT STOPPING",             CONV_GRADTOL,  "converged (gradtol)"          },
    { "PARAMETER STOPPING",            CONV_STEPTOL,  "converged (steptol)"          },
    { "FAILED TO LOCATE A LOWER POINT",CONV_NO_LOWER, "stopped: no lower point"      },
    { "ITERATION LIMIT",               CONV_MAXITS,   "NOT converged: iteration limit" },
    { "MAX-LENGTH",                    CONV_MAXSTEPS, "stopped: five max-length steps" },
};

gboolean convergence_of(const char *out_path, Convergence *c) {
    gchar  *text = NULL, **lines, *how = NULL, *many = NULL;
    gsize   len = 0;
    int     i, k;

    if (c == NULL) return FALSE;
    c->kind = CONV_NONE;
    c->iterations = -1;
    c->gradient = -1.0;
    c->brief = c->full = NULL;
    if (out_path == NULL) return FALSE;
    if (!g_file_get_contents(out_path, &text, &len, NULL)) return FALSE;

    lines = g_strsplit(text, "\n", -1);
    for (i = 0; lines[i] != NULL; i++) {
        if (!g_str_has_prefix(lines[i], "****")) continue;
        if (strstr(lines[i], "CONVERGENCE OBTAINED")) {
            g_free(many);
            many = g_strdup(g_strstrip(lines[i] + 4));
        } else {
            g_free(how);
            how = g_strdup(g_strstrip(lines[i] + 4));
        }
    }
    g_strfreev(lines);
    g_free(text);

    if (how != NULL)
        for (k = 0; k < (int)(sizeof(kinds) / sizeof(kinds[0])); k++)
            if (strstr(how, kinds[k].mark) != NULL) {
                c->kind  = kinds[k].kind;
                c->brief = g_strdup(kinds[k].brief);
                break;
            }
    if (many != NULL) {
        const char *p = strstr(many, "AFTER ");
        const char *g = strstr(many, "NORM = ");

        if (p != NULL) c->iterations = atoi(p + 6);
        if (g != NULL) c->gradient   = g_ascii_strtod(g + 7, NULL);
    }
    if (c->brief == NULL && c->iterations >= 0)
        c->brief = g_strdup("finished");
    if (how != NULL && many != NULL) c->full = g_strdup_printf("%s\n%s", how, many);
    else if (many != NULL) c->full = g_strdup(many);
    else if (how != NULL)  c->full = g_strdup(how);
    g_free(how);
    g_free(many);
    return c->brief != NULL || c->full != NULL;
}

void convergence_clear(Convergence *c) {
    if (c == NULL) return;
    g_free(c->brief);
    g_free(c->full);
    c->brief = c->full = NULL;
}

gboolean convergence_is_good(const Convergence *c) {
    return c != NULL && (c->kind == CONV_GRADTOL || c->kind == CONV_STEPTOL);
}

/* ------------------------------------------------------------------------ */
/* EL .out DE fue                                                            */
/* ------------------------------------------------------------------------ */

/* La cola superior de una chi-cuadrado, por la gamma incompleta regularizada
 * Q(a,x) con a = df/2, x = X2/2. Serie para x < a+1, fraccion continua para
 * el resto: es el reparto clasico, y la unica razon de escribirlo aqui es no
 * arrastrar GSL a una ventana que no estima nada.
 *
 * Que esto viva en el lector y no en el motor es deliberado: el motor YA da
 * el estadistico y sus grados de libertad. Esto no rehace su cuenta, la
 * traduce -- un Q de 117.7 con 39 g.l. no se lee, un p de 0.000 si.      */
double chisq_cola( double x, int df )
{
    double a = df / 2.0, xx = x / 2.0;
    double gln, ap, sum, del, an, b, c, d, h;
    int    i;

    if ( df <= 0 || x < 0.0 ) return -1.0;
    if ( x == 0.0 ) return 1.0;

    gln = lgamma( a );

    if ( xx < a + 1.0 )
        {                                   /* serie: da P(a,x), y Q = 1-P */
        ap = a; del = sum = 1.0 / a;
        for ( i = 0; i < 1000; i++ )
            {
            ap += 1.0;
            del *= xx / ap;
            sum += del;
            if ( fabs( del ) < fabs( sum ) * 1e-14 ) break;
            }
        return 1.0 - sum * exp( -xx + a * log( xx ) - gln );
        }

    /* fraccion continua (Lentz): da Q(a,x) directamente */
    b = xx + 1.0 - a;
    c = 1.0 / 1e-300;
    d = 1.0 / b;
    h = d;
    for ( i = 1; i < 1000; i++ )
        {
        an = -i * ( i - a );
        b += 2.0;
        d  = an * d + b; if ( fabs( d ) < 1e-300 ) d = 1e-300;
        c  = b + an / c; if ( fabs( c ) < 1e-300 ) c = 1e-300;
        d  = 1.0 / d;
        del = d * c;
        h *= del;
        if ( fabs( del - 1.0 ) < 1e-14 ) break;
        }
    return exp( -xx + a * log( xx ) - gln ) * h;
}

/* Cuantos coeficientes lleva el factor que EMPIEZA en la linea siguiente:
 * son las lineas que empiezan por espacios y un numero, hasta la primera
 * que no. El motor las imprime con "%14.6f".                            */
static int cuenta_coef( gchar **lin, int i, int n )
{
    int k = 0;

    while ( ++i < n )
        {
        const char *s = lin[i];

        while ( *s == ' ' || *s == '\t' ) s++;
        if ( *s != '-' && *s != '+' && !g_ascii_isdigit( *s ) ) break;
        k++;
        }
    return k;
}

gboolean fueout_read( const char *path, FueOut *o )
{
    gchar  *txt = NULL, **lin;
    gsize   len = 0;
    int     i, n;

    if ( o == NULL ) return FALSE;
    memset( o, 0, sizeof *o );
    o->lambda = 1.0;
    if ( path == NULL || !g_file_get_contents( path, &txt, &len, NULL ) )
        return FALSE;

    lin = g_strsplit( txt, "\n", -1 );
    for ( n = 0; lin[n]; n++ ) ;

    for ( i = 0; i < n; i++ )
        {
        const char *l = lin[i];
        double      v;
        int         k;

        if ( sscanf( l, "Observations: %d", &k ) == 1 ) { o->nobs = k; o->hay = TRUE; }
        else if ( sscanf( l, "Parameters  : %d", &k ) == 1 ) o->npar = k;
        else if ( sscanf( l, "Box-Cox lambda     : %lf", &v ) == 1 ) o->lambda = v;
        else if ( sscanf( l, "Seasonal period    : %d", &k ) == 1 ) o->s = k;
        else if ( sscanf( l, "Regular differences: %d", &k ) == 1 ) o->d = k;
        else if ( sscanf( l, "Annual differences : %d", &k ) == 1 ) o->D = k;
        else if ( sscanf( l, "Number of deterministic variables: %d", &k ) == 1 )
            o->ndet = k;

        /* LOS ORDENES, SUMANDO FACTORES. El motor los imprime por separado
           porque la especificacion es factorizada; (p,d,q)(P,D,Q) es el
           resumen, y el numero de factores va al globo para que no mienta
           por omision cuando hay mas de uno.                            */
        else if ( g_str_has_prefix( l, "Coefficients for " ) )
            {
            const char *c = l + 17;
            int         cuantos = cuenta_coef( lin, i, n );
            gboolean    anual = ( strstr( c, "annual" ) != NULL );
            gboolean    ma    = ( strstr( c, "MA" ) != NULL );

            if ( strstr( c, "f-fixed" ) ) o->ffijo = TRUE;
            o->factores++;
            if ( ma ) { if ( anual ) o->Q += cuantos; else o->q += cuantos; }
            else      { if ( anual ) o->P += cuantos; else o->p += cuantos; }
            }

        /* LOS RESIDUOS. El bloque de "Unconditional residuals". */
        else if ( sscanf( l, "                  Mean: %lf", &v ) == 1 )
            { o->media = v; o->tiene_res = TRUE; }
        else if ( sscanf( l, "    Standard deviation: %lf", &v ) == 1 ) o->sd = v;
        else if ( sscanf( l, "              Skewness: %lf", &v ) == 1 ) o->skew = v;
        else if ( sscanf( l, "              Kurtosis: %lf", &v ) == 1 ) o->kurt = v;
        else if ( sscanf( l, "           Jarque-Bera: %lf", &v ) == 1 )
            { o->jb = v; o->tiene_jb = TRUE; o->jb_p = chisq_cola( v, 2 ); }

        /* EL LJUNG-BOX: la escalera del margen derecho de la ACF. Se queda
           el ULTIMO, que es el que resume toda la ventana.              */
        else if ( strchr( l, '+' ) || strchr( l, '|' ) )
            {
            const char *bar = strrchr( l, '+' );
            const char *pip = strrchr( l, '|' );
            const char *fin = ( bar > pip ) ? bar : pip;
            double      qq;
            int         df;

            if ( fin && sscanf( fin + 1, "%lf %d", &qq, &df ) == 2 && df > 0 )
                { o->lb_q = qq; o->lb_df = df; o->tiene_lb = TRUE;
                  o->lb_p = chisq_cola( qq, df ); }
            }
        }

    g_strfreev( lin );
    g_free( txt );
    return o->hay;
}

char *fueout_estructura( const FueOut *o, char *out, size_t n )
{
    if ( out == NULL || n == 0 ) return out;
    out[0] = '\0';
    if ( o == NULL || !o->hay ) return out;

    /* La parte anual SOLO si la hay: "(0,1,1)" a secas se lee mejor que
       "(0,1,1)(0,0,0)0" para una serie sin estacionalidad.            */
    if ( o->P || o->D || o->Q )
        g_snprintf( out, n, "(%d,%d,%d)(%d,%d,%d)%d",
                    o->p, o->d, o->q, o->P, o->D, o->Q, o->s );
    else
        g_snprintf( out, n, "(%d,%d,%d)", o->p, o->d, o->q );

    /* λ sólo cuando NO es 1: transformar es una decision, no transformar
       es no haber decidido nada, y una columna llena de "λ=1" no dice.  */
    if ( o->lambda != 1.0 )
        {
        char b[32];

        g_snprintf( b, sizeof b, o->lambda == 0.0 ? "  log" : "  λ=%.2f",
                    o->lambda );
        g_strlcat( out, b, n );
        }
    return out;
}
