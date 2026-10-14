/*
 * test_formales.c -- LOS CONTRASTES FORMALES, contra art-python.
 *
 *   test_formales FUE DIR_GOLDEN_FUE DIR_PRUEBAS DIR_TRABAJO
 *
 * Para cada modelo: su .pre se estima con fue (es lo que hace art con
 * estimar(pre)), se plantean los contrastes, fue estima cada restringido y
 * se comparan las log-verosimilitudes y los estadisticos con los que dio
 * art.formal_tests el 2026-10-14 sobre los MISMOS .pre: los golden del motor
 * y AIR.pre (lib/formales/pruebas), un «airline» del IPC con MA anual.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#ifdef G_OS_UNIX
#include <sys/wait.h>
#endif

#include "formales.h"

static int fallos;

static void ok( int c, const char *que )
{
    printf( "  %s  %s\n", c ? "ok  " : "FALLA", que );
    if ( !c ) fallos++;
}

static int cerca( double a, double b, double tol )
{
    return fabs( a - b ) <= tol * ( 1.0 + fabs( b ) );
}

static const char *FUE, *TRABAJO;

/* fue BASE en dir; TRUE si estimo (0) */
static int corre( const char *dir, const char *base )
{
    gchar *argv[3] = { (gchar *) FUE, (gchar *) base, NULL };
    gint   st = -1;

    if ( !g_spawn_sync( dir, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                        NULL, NULL, NULL, NULL, &st, NULL ) ) return 0;
#ifdef G_OS_UNIX
    if ( WIFEXITED( st ) ) st = WEXITSTATUS( st );
#endif
    return st == 0;            /* con un 3 deja los valores iniciales */
}

static double logelf( const char *out )
{
    gchar *t = NULL, *p;
    double v = NAN;

    if ( g_file_get_contents( out, &t, NULL, NULL ) && ( p = strstr( t, "logelf:" ) ) )
        v = g_ascii_strtod( p + 7, NULL );
    g_free( t );
    return v;
}

typedef struct { FmTipo tipo; double coef, lr, l_restr, c5; int rechaza;
                 double l_testigo; int acotado; double phi_r; } Ref;

/* EL MEG de art.formal_tests.meg, frecuencia a frecuencia: el testigo, la L
 * de S libre y con el testigo en -1, el LR y el critico al 5 %. que: 1
 * estocastica, 0 determinista, -1 no calculable (art la llama determinista:
 * un LR negativo no es un veredicto, BUG-0232). */
typedef struct { int f, que; double coef, l_s, l_restr, lr, c5; int laxo; } Meg;

/* fue sobre t->base mientras el contraste pida otra estimacion */
static void contrasta( FmPlan *pl, FmPrueba *t, const char *dir, double l )
{
    int vueltas = 0;

    while ( fm_avanza( pl, t, l, corre( dir, t->base ) ) && ++vueltas < 12 ) ;
}

static void compara( const FmPrueba *t, const Ref *r )
{
    static const char *nom[] = { "simplificación estacional", "Shin-Fuller", "DCD",
                                 "DCD estacional", "sobrediferenciación", "subdiferenciación",
                                 "MEG", "DCD_f" };
    char m[640];
    int  bien;

    snprintf( m, sizeof m, "%s: L0 %.6f (art %.6f), estadístico %.6f (art %.6f), "
              "crítico 5%% %.4f, %s", nom[t->tipo], t->l_restr, r->l_restr, t->est,
              r->lr, t->c5, t->rechaza ? "rechaza" : "no rechaza" );
    bien = t->tipo == r->tipo && t->calculable && cerca( t->l_restr, r->l_restr, 1e-6 )
           && cerca( t->est, r->lr, 1e-5 ) && cerca( t->c5, r->c5, 1e-5 )
           && t->rechaza == r->rechaza
           /* el coeficiente sale del .out, con seis decimales */
           && ( t->tipo == FM_ESTACIONAL || cerca( t->coef, r->coef, 2e-6 ) );
    if ( t->tipo == FM_SOBRE || t->tipo == FM_SUB )
        {
        char x[200];

        snprintf( x, sizeof x, "; testigo θ̂ %.6f (art %.6f), L %.6f (art %.6f)%s",
                  t->coef, r->coef, t->l_testigo, r->l_testigo,
                  t->acotado ? ", AR acotado en ρ" : "" );
        strncat( m, x, sizeof m - strlen( m ) - 1 );
        bien = bien && cerca( t->l_testigo, r->l_testigo, 1e-6 ) && t->acotado == r->acotado
               && ( isnan( r->phi_r ) ? isnan( t->phi_r ) : cerca( t->phi_r, r->phi_r, 1e-5 ) );
        }
    if ( !bien )
        printf( "        [tipo %d/%d calc %d coef %.9g/%.9g acot %d/%d phi %.9g/%.9g]\n", t->tipo,
                r->tipo, t->calculable, t->coef, r->coef, t->acotado, r->acotado, t->phi_r, r->phi_r );
    ok( bien, m );
}

static void compara_meg( const FmPrueba *t, const Meg *r )
{
    char m[640];
    int  bien, que = !t->calculable ? -1 : t->rechaza;

    snprintf( m, sizeof m, "MEG f = %d: testigo %.6f (art %.6f), L de S %.6f (art %.6f), L0 %.6f "
              "(art %.6f), LR %.6f (art %.6f), crítico 5%% %.3f (art %.3f), %s",
              t->freq, t->coef, r->coef, t->l_testigo, r->l_s, t->l_restr, r->l_restr, t->est,
              r->lr, t->c5, r->c5, que < 0 ? "no calculable" : que ? "estocástica" : "determinista" );
    if ( r->laxo )
        /* MAL CONDICIONADO: ni art ni fue anulan el gradiente, y los numeros
           dependen del camino. Se exige el veredicto: ninguna estocastica,
           y que la duda se diga. */
        bien = t->tipo == FM_MEG && t->freq == r->f && !t->omitida && que != 1 && r->que != 1
               && cerca( t->c5, r->c5, 1e-9 ) && ( que >= 0 || t->dudoso || !t->calculable );
    else
        bien = t->tipo == FM_MEG && t->freq == r->f && que == r->que && !t->omitida
               && cerca( t->l_testigo, r->l_s, 1e-6 ) && cerca( t->l_restr, r->l_restr, 1e-6 )
               && cerca( t->coef, r->coef, 2e-6 ) && cerca( t->c5, r->c5, 1e-9 )
               && ( que < 0 || cerca( t->est, r->lr, 1e-5 ) );
    if ( r->laxo ) strncat( m, t->dudoso ? " [mal condicionado; dudoso]" : " [mal condicionado]",
                            sizeof m - strlen( m ) - 1 );
    if ( !bien )
        printf( "        [tipo %d f %d omit %d calc %d motivo «%s»]\n", t->tipo, t->freq, t->omitida,
                t->calculable, t->motivo );
    ok( bien, m );
}

/* fue sobre t->base mientras el contraste pida otra estimacion; las
   omitidas ya vienen hechas */
static void contrasta_todo( FmPlan *pl, const char *dir, double l )
{
    int i;

    for ( i = 0; i < pl->n; i++ )
        if ( !pl->t[i].hecho ) contrasta( pl, &pl->t[i], dir, l );
}

static void modelo( const char *pre_origen, const char *nombre, int nobs, int s, int d,
                    double l_ref, const Ref *ref, int nref, const Ref *sub,
                    const Meg *meg, int nmeg )
{
    gchar   *dir = g_build_filename( TRABAJO, nombre, NULL );
    gchar   *pre, *out, *txt = NULL;
    FmPlan   pl;
    char     why[256], m[512];
    double   l;
    int      i, k;

    printf( "\n%s\n", nombre );
    g_mkdir_with_parents( dir, 0700 );
    pre = g_strdup_printf( "%s/%s.inp", dir, nombre );
    out = g_strdup_printf( "%s/%s.out", dir, nombre );
    if ( !g_file_get_contents( pre_origen, &txt, NULL, NULL ) ||
         !g_file_set_contents( pre, txt, -1, NULL ) )
        { ok( 0, "copiar el .pre" ); goto fin; }
    ok( corre( dir, nombre ), "fue estima el .pre" );
    l = logelf( out );
    snprintf( m, sizeof m, "la log-verosimilitud del estimado, %.6f (art %.6f)", l, l_ref );
    ok( cerca( l, l_ref, 1e-7 ), m );

    /* el .pre que sale es el optimo: de ahi parten los restringidos */
    g_free( pre );
    pre = g_strdup_printf( "%s/%s.pre", dir, nombre );
    ok( fm_plan( pre, out, dir, nobs, s, d, &pl, why, sizeof why ) == 0, "plantear los contrastes" );
    snprintf( m, sizeof m, "%d contrastes, los mismos que art", pl.n );
    ok( pl.n == nref + nmeg, m );
    contrasta_todo( &pl, dir, l );
    for ( i = 0; i < pl.n && i < nref; i++ ) compara( &pl.t[i], &ref[i] );
    for ( i = 0; i < nmeg && nref + i < pl.n; i++ ) compara_meg( &pl.t[nref + i], &meg[i] );
    /* el lado d-1 se pide: aqui se pide siempre, para cotejarlo */
    {
    int pudo = fm_plan_sub( &pl, why, sizeof why ) == 0;

    snprintf( m, sizeof m, "subdiferenciación pedida: %s", pudo ? "se plantea" : why );
    ok( pudo == ( sub != NULL ), m );
    if ( pudo && sub )
        {
        contrasta( &pl, &pl.t[pl.n - 1], dir, l );
        compara( &pl.t[pl.n - 1], sub );
        }
    }
    for ( k = 0; k < FM_NA; k++ ) if ( pl.no_aplica[k][0] ) printf( "        (%s)\n", pl.no_aplica[k] );
fin:
    g_free( txt ); g_free( pre ); g_free( out ); g_free( dir );
}

/* LA REFORMULACION DESPUES DEL MEG: fm_reformula sobre el .pre que dejo
 * modelo(), fue la estima, y en el reformulado el testigo se contrasta solo:
 * DCD_f en una interior, el DCD en theta = -1 en Nyquist. Contra
 * art.formal_tests.reformulate_stochastic + fit, y dcd_f o _dcd_nyquist_ma.
 * Es el MEG de esa frecuencia, ahora como modelo del analista.          */
static void reformulado( const char *nombre, int nobs, int s, int d, int f,
                         double l_ref, double coef, double l_restr, double lr, double c5 )
{
    gchar   *dir = g_build_filename( TRABAJO, nombre, NULL ), *pre, *out, *base, *inp;
    FmPlan   pl;
    char     why[256], m[512];
    double   l;
    int      i, k = -1, idx = -1;

    printf( "\n%s reformulado en f = %d\n", nombre, f );
    pre  = g_strdup_printf( "%s/%s.pre", dir, nombre );
    out  = g_strdup_printf( "%s/%s.out", dir, nombre );
    base = g_strdup_printf( "%s_f%d", nombre, f );
    inp  = g_strdup_printf( "%s/%s.inp", dir, base );
    ok( fm_reformula( pre, out, inp, s, f, &idx, why, sizeof why ) == 0, "fm_reformula escribe S" );
    ok( corre( dir, base ), "fue estima el reformulado" );
    g_free( pre ); g_free( out );
    pre = g_strdup_printf( "%s/%s.pre", dir, base );
    out = g_strdup_printf( "%s/%s.out", dir, base );
    l = logelf( out );
    snprintf( m, sizeof m, "su log-verosimilitud, %.6f (art %.6f)", l, l_ref );
    ok( cerca( l, l_ref, 1e-6 ), m );
    {
    gchar *sub = g_build_filename( dir, "reformulado", NULL );

    g_mkdir_with_parents( sub, 0700 );
    ok( fm_plan( pre, out, sub, nobs, s, d, &pl, why, sizeof why ) == 0, "plantear sus contrastes" );
    for ( i = 0; i < pl.n; i++ )
        if ( ( 2 * f == s ? pl.t[i].tipo == FM_DCD && pl.t[i].nulo < 0.0 : pl.t[i].tipo == FM_DCD_F )
             && pl.t[i].freq == f ) k = i;
    ok( k >= 0, 2 * f == s ? "el testigo de Nyquist se contrasta en θ = −1"
                           : "el testigo de frecuencia fija se contrasta con DCD_f" );
    if ( k >= 0 )
        {
        FmPrueba *t = &pl.t[k];

        contrasta( &pl, t, sub, l );
        snprintf( m, sizeof m, "testigo %.6f (art %.6f), L0 %.6f (art %.6f), LR %.6f (art %.6f), "
                  "crítico 5%% %.3f (art %.3f)", t->coef, coef, t->l_restr, l_restr, t->est, lr,
                  t->c5, c5 );
        ok( t->calculable && cerca( t->coef, coef, 2e-6 ) && cerca( t->l_restr, l_restr, 1e-6 )
            && cerca( t->est, lr, 1e-5 ) && cerca( t->c5, c5, 1e-9 ) && t->rechaza == ( lr > c5 ), m );
        }
    /* y el MEG ya no vuelve a contrastar esa frecuencia */
    for ( i = 0; i < pl.n; i++ )
        if ( pl.t[i].tipo == FM_MEG && pl.t[i].freq == f )
            ok( pl.t[i].omitida, "el MEG la da por estocástica (ifadf = 1) y no la repite" );
    g_free( sub );
    }
    g_free( dir ); g_free( pre ); g_free( out ); g_free( base ); g_free( inp );
}

int main( int argc, char **argv )
{
    gchar *f;

    if ( argc < 5 )
        { fprintf( stderr, "uso: test_formales FUE DIR_GOLDEN DIR_PRUEBAS DIR_TRABAJO\n" ); return 2; }
    FUE = g_canonicalize_filename( argv[1], NULL );     /* fue corre en otro directorio */
    TRABAJO = argv[4];
    printf( "\nLOS CONTRASTES FORMALES, CONTRA art.formal_tests\n" );

    {   /* AR(1) con armónicos: la simplificación y Shin-Fuller simple */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 358.88774308, -186.8465201887, 19.67513757, 1 },
        { FM_SF, 0.40272317, 37.53626609, -44.9389147360, 1.757733, 1 },
        /* el BUG-0011 de art: LR 4.22 sobre un indice de precios estacional */
        { FM_SOBRE, 0.97089581, 4.22034610, -10.0351265618, 1.94, 1, -7.9249535104, 0, NAN } };
    /* sin MA: testigo añadido, y el AR del restringido se acota en ρ (BUG-0224) */
    static const Ref sub = { FM_SUB, -0.27539460, 52.31816749, -32.6834880036, 1.94, 1,
                             -6.5244042595, 1, 0.99929756 };
    /* el MEG: f = 3 estocastica (2.31 frente a 2.068, n = 213) */
    static const Meg meg[] = {
        { 1, 0, -0.98971777, -12.9181438548, -12.9301825080, 0.02407731, 2.068 },
        { 2, 0, -0.98856187, -12.8644156230, -12.8764848348, 0.02413842, 2.068 },
        { 3, 1, -0.95391789, -11.9990055912, -13.1555026016, 2.31299402, 2.068 },
        { 4, 0, -0.96775161, -13.1374038843, -13.5943297412, 0.91385171, 2.068 },
        { 5, 0, -0.96465057, -13.4295062015, -14.3147824829, 1.77055256, 2.068 },
        { 6, 0, -0.99999996, -10.8888724518, -10.8888724517, 0.0, 1.94 } };
    f = g_build_filename( argv[2], "IPC_ES_m01_ar1", "IPC_ES_m01_ar1.pre", NULL );
    modelo( f, "IPC_ES_m01_ar1", 216, 12, 1, -7.4026486474, r, 3, &sub, meg, 6 );
    g_free( f );
    /* la estocastica, reformulada: su DCD_f es el MEG de f = 3 */
    reformulado( "IPC_ES_m01_ar1", 216, 12, 1, 3, -11.9990055912, -0.95391789, -13.1555026016,
                 2.31299402, 2.068 );
    }
    {   /* MA(1) con armónicos: el DCD regular, muy lejos de la frontera */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 359.58413947, -186.6359942551, 19.67513757, 1 },
        { FM_DCD, -0.43127924, 1057.36081526, -535.5243321509, 1.94, 1 },
        { FM_SOBRE, 0.86460318, 18.61907860, -29.5696121999, 1.94, 1, -20.2600729000, 0, NAN } };
    /* el testigo es el MA(1) del modelo: lo mismo que el DCD */
    static const Ref sub = { FM_SUB, -0.43127924, 1057.36081526, -535.5243321509, 1.94, 1,
                             -6.8439245194, 0, NAN };
    /* el MEG: sobre el MA(1), la estocastica es f = 5 */
    static const Meg meg[] = {
        { 1, 0, -0.98578250, -12.4466554003, -12.4872693635, 0.08122793, 2.068 },
        { 2, 0, -0.99999999, -12.1168400773, -12.1168400773, 0.0, 2.068 },
        { 3, 0, -0.97175693, -11.9537794194, -12.2838761135, 0.66019339, 2.068 },
        { 4, 0, -0.97566894, -12.6715914281, -12.8788366609, 0.41449047, 2.068 },
        { 5, 1, -0.95411294, -12.3920175199, -13.9585301457, 3.13302525, 2.068 },
        { 6, 0, -0.99999999, -10.5515198028, -10.5515198028, 0.0, 1.94 } };
    f = g_build_filename( argv[2], "IPC_ES_m01_ma1", "IPC_ES_m01_ma1.pre", NULL );
    modelo( f, "IPC_ES_m01_ma1", 216, 12, 1, -6.8439245194, r, 3, &sub, meg, 6 );
    g_free( f );
    /* Nyquist reformulado: el testigo (1 + θB) se apila en −1 */
    reformulado( "IPC_ES_m01_ma1", 216, 12, 1, 6, -10.5515198028, -0.99999999, -10.5515198028,
                 0.0, 1.94 );
    }
    {   /* AR(2) con armónicos: Shin-Fuller PARTE el factor (art BUG-0065) */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 15.77767809, -47.4221134029, 19.67513757, 0 },
        { FM_SF, 0.72703297, 1.69946526, -41.2327396237, 1.75, 0 },
        /* d = 0: el testigo se apila en +1 */
        { FM_SOBRE, 0.99999999, 0.0, -39.4822974885, 1.94, 0, -39.4822974885, 0, NAN } };
    /* EL MEG, MAL CONDICIONADO: en S el AR(2) se va a la raiz unitaria en
       f = 0 (d = 0 y mu = -88), la verosimilitud es una cresta y ni art ni
       fue anulan el gradiente (art: converged False tras reiniciar; norma
       ~1e5). Los numeros dependen del camino --art parte de los valores
       exactos, aqui del .pre y el .out--, asi que solo se coteja el
       veredicto: ninguna frecuencia estocastica. */
    static const Meg meg[] = {
        { 1, -1, -0.96585098, -50.5428372477, -50.4866936117, -0.11228727, 2.06, 1 },
        { 2, 0, -1.00000000, -42.8330230953, -42.8330230953, 0.0, 2.06, 1 },
        { 3, 0, -0.92118629, -50.4758163912, -50.5744915718, 0.19735036, 2.06, 1 },
        { 4, 0, -0.92625069, -51.5169303965, -51.6916980042, 0.34953522, 2.06, 1 },
        { 5, 0, -0.94560379, -52.6251494788, -52.7415079791, 0.23271700, 2.06, 1 },
        { 6, -1, -0.99939991, -49.3332467132, -49.3210061715, -0.02448108, 1.94, 1 } };
    f = g_build_filename( argv[2], "RIPC.2", "RIPC.2.pre", NULL );
    modelo( f, "RIPC.2", 78, 12, 0, -39.5332743594, r, 3, NULL, meg, 6 );
    g_free( f );
    }
    {   /* trimestral, AR(1) y un MA(1) con θ = 1 exacto: el DCD da LR = 0 */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 9.13194846, 166.1470940066, 7.814727903, 1 },
        { FM_SF, 0.84669164, 0.82005753, 169.8930107062, 1.75, 0 },
        { FM_DCD, 1.0, 0.0, 170.7130682380, 1.94, 0 },
        { FM_SOBRE, 1.0, 0.0, 165.3807175177, 1.94, 0, 165.3807175177, 0, NAN } };
    static const Ref sub = { FM_SUB, 1.0, 0.0, 170.7130682380, 1.94, 0,
                             170.7130682380, 0, 0.84669164 };
    /* trimestral: la interior f = 1 y Nyquist */
    static const Meg meg[] = {
        { 1, 0, -1.00000000, 161.2450742263, 161.2450742263, 0.0, 2.06 },
        { 2, 0, -0.91677814, 165.8544418654, 165.6322354639, 0.44441280, 1.94 } };
    f = g_build_filename( argv[2], "R.6", "R.6.pre", NULL );
    modelo( f, "R.6", 68, 4, 1, 170.7130682380, r, 4, &sub, meg, 2 );
    g_free( f );
    /* trimestral, Nyquist: el MA(1) del modelo (θ = 1) sigue en +1, el
       testigo en −1 */
    reformulado( "R.6", 68, 4, 1, 2, 165.8544418654, -0.91677814, 165.6322354639, 0.44441280, 1.94 );
    }
    {   /* el airline: el DCD estacional, al borde (2.20 frente a 2.31) */
    static const Ref r[] = {
        { FM_DCD, -0.40622301, 623.58028854, -335.4654220017, 1.94, 1 },
        { FM_DCD_S, 0.86250753, 2.19569975, -24.7731276093, 2.31, 0 },
        { FM_SOBRE, 0.83889664, 4.84724131, -43.9858645049, 1.94, 1, -41.5622438510, 0, NAN } };
    static const Ref sub = { FM_SUB, -0.40622301, 623.58028854, -335.4654220017, 1.94, 1,
                             -23.6752777327, 0, NAN };
    f = g_build_filename( argv[3], "AIR.pre", NULL );
    modelo( f, "AIR", 216, 12, 1, -23.6752777327, r, 3, &sub, NULL, 0 );
    g_free( f );
    }

    if ( fallos ) { printf( "\n%d fallos\n", fallos ); return 1; }
    printf( "\ntodo bien\n" );
    return 0;
}
