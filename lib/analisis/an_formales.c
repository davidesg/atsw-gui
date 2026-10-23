/*
 * an_formales.c -- las ventanas de los contrastes formales del modelo
 * estimado: el formal_tests de art (lib/formales), partido en dos (ver
 * analisis.h): el orden de integracion regular y la estacionalidad. Las
 * abren la diagnosis, el menu de la lista y la barra de fue_gui.
 *
 * fm_plan los plantea todos y aqui se queda cada ventana con los suyos:
 * la frontera entre las dos es de la ventana, no de la biblioteca. Cada
 * una escribe en su propio directorio, y se pueden tener las dos abiertas.
 *
 * Cada contraste es una estimacion mas: el modelo con un parametro fijo en
 * su valor nulo, que fue estima partiendo del optimo. Se corren de uno en
 * uno y SIN BLOQUEAR LA VENTANA, y cada resultado se escribe en cuanto
 * llega.
 *
 * AQUI NO SE DECIDE NADA Y NO SE ENCADENA NADA: cada contraste dice su
 * numero, sus criticos y lo que dice, por separado. El veredicto encadenado
 * de art (describe_formal_tests) es donde viven sus fallos abiertos, y no se
 * porta. Lo que no es un estadistico sale como «no calculable».
 *
 * Suponen un modelo ADECUADO (art BUG-0025): con la diagnosis sin limpiar,
 * la ventana lo dice arriba, y los numeros se leen con esa reserva.
 *
 * EL MEG SI TIENE UN GESTO: reformular. Si una frecuencia sale estocastica,
 * el boton deriva el modelo S de esa frecuencia --el mismo que contrasto el
 * MEG, con su testigo-- como un hijo del modelo, para estimarlo y volver a
 * contrastar (meg_reformulate de art). Una frecuencia cada vez, la mas
 * fuerte primero: el MEG sobre el modelo todo determinista contamina unas
 * frecuencias con otras, y lo que diga la siguiente se lee sobre el
 * reformulado.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "coste.h"
#include "dictamen.h"
#include "engine.h"
#include "formales.h"
#include "inpcheck.h"
#include "outfile.h"
#include "sitio.h"

#include "analisis.h"

typedef struct {
    AnHost     h;
    char       serie[PR_ID], muestra[PR_ID], id[PR_ID];
    FmPlan     pl;
    AnFmFamilia fam;
    double     l_libre;
    int        cur;               /* el que esta corriendo; pl.n al acabar */
    gboolean   corriendo, cerrada;
    gchar     *exe, *dir;
    char       aviso[512];
    GtkWidget *win, *l_texto, *l_estado, *b_sub, *c_freq, *b_ref;
} Fm;

static gchar *programa_fue( void )
{
    static const char *const sitio[] = {     /* relativo al programa        */
        "../../engines/fue/bin/%s",          /* gui/atsw/atsw_gui           */
        "../../../engines/fue/bin/%s",       /* gui/fue/bin/fue_gui         */
        "../fue/bin/%s", "./%s", NULL
    };
    return sitio_busca( "fue", sitio );
}

/* DE QUE VENTANA ES CADA CONTRASTE. El DCD regular con freq es el testigo
 * de Nyquist de un modelo reformulado tras el MEG: va con la estacionalidad. */
static gboolean de_familia( const FmPrueba *x, AnFmFamilia fam )
{
    gboolean integ = x->tipo == FM_SF || x->tipo == FM_SOBRE || x->tipo == FM_SUB ||
                     ( x->tipo == FM_DCD && x->freq == 0 );

    return fam == AN_FM_INTEGRACION ? integ : !integ;
}

/* Las lineas de no_aplica, por contraste (lib/formales/formales.c): 0 la
 * simplificacion estacional, 1 Shin-Fuller, 2 el DCD, 3 el estacional,
 * 4 la sobrediferenciacion, 6 el MEG, 7 el DCD_f.                       */
static void quedate_con( FmPlan *pl, AnFmFamilia fam )
{
    static const int integ[FM_NA] = { 0, 1, 1, 0, 1, 1, 0, 0 };
    int i, n = 0;

    for ( i = 0; i < pl->n; i++ )
        if ( de_familia( &pl->t[i], fam ) )
            { if ( n != i ) pl->t[n] = pl->t[i]; n++; }
    pl->n = n;
    for ( i = 0; i < FM_NA; i++ )
        if ( integ[i] != ( fam == AN_FM_INTEGRACION ) ) pl->no_aplica[i][0] = '\0';
}

int an_formales_s( const Proyecto *p, const char *serie, const char *muestra,
                   const char *id )
{
    char    out[PR_RUTA];
    FueOut *o;
    int     s = 0;

    if ( !p || pr_ruta( p, serie, muestra, id, ".out", out, sizeof out ) != 0 ) return 0;
    o = g_new0( FueOut, 1 );
    if ( fueout_read( out, o ) ) s = o->s;
    g_free( o );
    return s;
}

static void criticos( GString *t, const FmPrueba *x, const char *de )
{
    g_string_append_printf( t, "críticos%s%s: %.2f (10 %%), <b>%.2f (5 %%)</b>, %.2f (1 %%)",
                            de ? " de " : "", de ? de : "", x->c10, x->c5, x->c1 );
}

/* EL DCD DE SOBREDIFERENCIACION, como lo escribe art (describe.py): el
 * titular lleva dentro sus reservas, que un titular invita a leer solo el
 * titular (art BUG-0055). Y las reservas son las del BUG-0011: el critico es
 * el de la ley DESNUDA, y con deterministas en el candidato el de verdad es
 * otro. */
#define CRIT_ARMONICOS_F0 2.15

/* Y EL DEL MEG, por lo mismo (art, formal_tests.py): con media y armonicos
 * en el modelo, el critico del par complejo en muestra finita es mayor
 * --n = 120: 1.63, 2.87, 5.81-- y el efecto se desvanece al crecer n. art
 * lo documenta y no lo aplica; aqui se dice cuando importa. */
#define CRIT_MEG_N120_5 2.87

static const FmPrueba *de_tipo( const Fm *g, FmTipo tipo )
{
    int i;

    for ( i = 0; i < g->pl.n; i++ )
        if ( g->pl.t[i].tipo == tipo ) return &g->pl.t[i];
    return NULL;
}

static void sobre( GString *t, const Fm *g, const FmPrueba *x )
{
    const FmPrueba *sf = de_tipo( g, FM_SF );
    int             otros = g->pl.n_det - g->pl.n_armon;
    gboolean        sin_par = !sf;

    g_string_append_printf( t, "    θ̂ = %.4f (%s).  L = %.4f con el testigo,  L<sub>0</sub> = %.4f.  "
                            "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b>\n    ", x->coef, x->testigo,
                            x->l_libre, x->l_restr, x->est );
    criticos( t, x, "Davis-Dunsmuir (ley desnuda, s = 1)" );
    g_string_append( t, "\n    → " );
    if ( x->est < x->c5 )
        {
        if ( x->coef < 0.0 )
            g_string_append( t, "el testigo se fue al eje de π (θ̂ &lt; 0): <b>no mide f = 0</b>, "
                                "así que este lado no da veredicto sobre d." );
        else if ( 1.0 - x->coef <= FM_BANDA )
            g_string_append_printf( t, "testigo en la frontera (θ̂ → +1): la diferencia extra "
                                       "sobra → <b>con d = %d basta</b>.", g->pl.d );
        else
            g_string_append_printf( t, "no se rechaza θ = 1, pero el testigo no se apila en +1 "
                "(está a %.4f): la diferencia extra no se demuestra necesaria → d basta por "
                "este lado, sin confirmación fuerte.", 1.0 - x->coef );
        }
    else if ( g->pl.n_det || sin_par )
        {
        g_string_append( t, "testigo invertible → <b>este lado, POR SÍ SOLO, apuntaría a d + 1, "
                            "pero NO es concluyente</b>: " );
        if ( otros )
            g_string_append( t, "el crítico impreso puede estar subestimado (deterministas de "
                                "baja frecuencia, resonantes en f = 0)" );
        else if ( g->pl.n_armon )
            g_string_append_printf( t, "el crítico impreso es algo bajo (≈ %.2f con armónicos, "
                                       "n = 216)", CRIT_ARMONICOS_F0 );
        if ( sin_par ) g_string_append_printf( t, "%sfalta el lado AR del par",
                                               g->pl.n_det ? " y " : "" );
        g_string_append( t, ". Lee las reservas de abajo ANTES de mover d." );
        }
    else
        g_string_append( t, "testigo invertible → raíz unitaria regular genuina → este lado "
                            "apunta a d + 1 (un solo lado: confírmalo con el par en f = 0)." );
    g_string_append_printf( t, "\n    <small>Candidato: %s. θ̂ mide la persistencia de ∇<sup>%d</sup> y "
                            "que %s.</small>", x->candidato, g->pl.d,
                            g->pl.p_ar ? "ese AR no explica: depende del modelo base, y sobre otro "
                                         "AR sale otro θ̂" : "nada más explica" );
    if ( g->pl.n_det )
        {
        g_string_append_printf( t, "\n    <small>ⓘ El crítico usado (%.2f) es el de la ley "
            "desnuda s = 1, y el candidato no lleva media (la diferencia extra la elimina).",
            x->c5 );
        if ( g->pl.n_armon )
            g_string_append_printf( t, " Lleva %d armónicos, que no resuenan en f = 0: medido "
                "con el motor exacto del estudio (n = 216), el crítico al 5 %% sube a ≈ %.2f. Un "
                "LR por encima de eso no se explica por el crítico.", g->pl.n_armon,
                CRIT_ARMONICOS_F0 );
        if ( otros )
            g_string_append_printf( t, " Lleva %d determinista(s) de baja frecuencia (escalón, "
                "rampa, impulso…), que sí pueden resonar en f = 0 como la media; su efecto sobre "
                "el crítico no está medido. Un LR apenas por encima del impreso NO es evidencia "
                "de d + 1.", otros );
        g_string_append( t, "</small>" );
        }
    if ( fabs( fabs( x->coef ) - 1.0 ) > 1e-6 )
        g_string_append_printf( t, "\n    <small>ⓘ θ̂ se queda dentro, cerca de la frontera: "
            "∇<sup>%d</sup> y es persistente, no un error de cálculo (la verosimilitud de fue en "
            "θ = 1 coincide con la exacta del estudio, validada sobre candidatos sin AR).%s"
            "</small>", g->pl.d, g->pl.p_ar ? " Este candidato lleva el AR del modelo, y con AR "
            "esa validación no está hecha." : "" );
    if ( sin_par )
        {
        gchar *m = g_markup_escape_text( g->pl.no_aplica[1], -1 );

        g_string_append_printf( t, "\n    ⚠ <b>Sin par confirmatorio</b> (%s): este veredicto es "
            "un solo lado, y los contrastes de frontera se leen en pareja. Antes de mover d, "
            "estima el candidato d + 1 y compáralo por diagnosis y criterios de información.", m );
        g_free( m );
        }
    g_string_append( t, "\n\n" );
}

/* EL DCD DE SUBDIFERENCIACION (_estado_subdif y _texto_subdif de art) */
static void sub( GString *t, const Fm *g, const FmPrueba *x )
{
    gboolean genuina = x->est >= x->c5;
    gboolean cancelada = !genuina && !x->raices_complejas && !x->acotado &&
                         1.0 - x->coef <= FM_BANDA;

    if ( x->raices_complejas )
        g_string_append_printf( t, "    El MA del modelo solo tiene raíces complejas (módulo "
            "%.4f): ninguna puede ser la (1 − B) que cancelaría la diferencia.  ", x->coef );
    else
        g_string_append_printf( t, "    θ̂ = %.4f (%s).  ", x->coef, x->testigo );
    g_string_append_printf( t, "L = %.4f,  L<sub>0</sub> = %.4f.  LR = <b>%.3f</b>\n    ",
                            x->l_libre, x->l_restr, x->est );
    criticos( t, x, "Davis-Dunsmuir" );
    g_string_append( t, "\n    → " );
    if ( genuina )
        g_string_append_printf( t, "testigo invertible → la diferencia es genuina → <b>d = %d "
                                   "confirmado por abajo</b>.", g->pl.d );
    else if ( x->raices_complejas )
        g_string_append( t, "no se rechaza, pero sin raíz real que se apile en +1: <b>no "
                            "concluyente</b>." );
    else if ( cancelada )
        g_string_append_printf( t, "testigo en la frontera (θ̂ → +1) → la diferencia está "
                                   "CANCELADA → <b>con d = %d bastaba</b>.", g->pl.d - 1 );
    else if ( x->acotado )
        g_string_append_printf( t, "no se rechaza θ = 1, pero <b>no concluyente</b>: con θ = 1 "
            "el AR del modelo en d − 1 se va a φ̂ = %.4f (≥ ρ = %.4f) y reconstruye la diferencia "
            "cancelada; por el lado AR la diferencia hace falta.", x->phi_r, x->rho_m );
    else
        g_string_append_printf( t, "no se rechaza θ = 1, pero θ̂ está a %.4f de la frontera: "
            "el LR es bajo porque la verosimilitud es plana, <b>no concluyente</b>.",
            1.0 - x->coef );
    if ( isfinite( x->phi_r ) )
        g_string_append_printf( t, x->acotado
            ? "\n    <small>ⓘ Con θ = 1 —el modelo en d − 1— el AR libre se iba a φ̂ = %.4f, en "
              "ρ = %.4f o por encima: (1 − φB) con φ → 1 es otra vez (1 − B), y reconstruía la "
              "diferencia que el MA acaba de cancelar. Se acota en ρ, el umbral de Shin-Fuller, "
              "y el LR es contra «d − 1 con un AR estacionario».</small>"
            : "\n    <small>ⓘ El AR del restringido (el modelo en d − 1) tiene su raíz real "
              "dominante en φ̂ = %.4f, por debajo de ρ = %.4f: no reconstruye la diferencia."
              "</small>", x->phi_r, x->rho_m );
    g_string_append( t, "\n\n" );
}

/* EL PAR CONFIRMATORIO EN f = 0: Shin-Fuller y el DCD de sobrediferenciacion
 * tienen nulas opuestas sobre el mismo orden de integracion. Su desacuerdo
 * no se resuelve eligiendo uno: dentro de la banda, ES el diagnostico de la
 * cuasi-cancelacion (art, describe_formal_tests). */
static void par( GString *t, const Fm *g )
{
    const FmPrueba *sf = de_tipo( g, FM_SF ), *od = de_tipo( g, FM_SOBRE ), *ud = de_tipo( g, FM_SUB );
    gboolean        basta, mas, fuera;
    double          dist;

    if ( !sf || !od || !sf->hecho || !od->hecho || !sf->calculable || !od->calculable ) return;
    basta = sf->rechaza;
    mas   = od->est >= od->c5;
    fuera = od->coef < 0.0;
    dist  = 1.0 - od->coef;
    g_string_append_printf( t, "<b>Par confirmatorio en f = 0</b> — dos contrastes con nulas "
        "opuestas sobre el mismo orden de integración\n"
        "    lado AR (Shin-Fuller, H<sub>0</sub>: ρ = 1): %s\n"
        "    lado MA (DCD de sobrediferenciación, H<sub>0</sub>: θ = 1): %s\n",
        basta ? "d basta" : "raíz unitaria → d + 1",
        mas ? "raíz genuina → d + 1" : "la diferencia extra sobra → d basta" );

    if ( fuera )
        g_string_append_printf( t, "    ⚠ <b>El testigo se salió del eje f = 0.</b> θ̂ = %.4f es "
            "negativo: su raíz apunta a B = −1, así que mide la frecuencia de Nyquist, no la "
            "cero. <b>El lado MA no es interpretable como veredicto sobre d</b> en esta corrida%s\n",
            od->coef, basta == !mas ? "; que coincidan no confirma nada: queda el lado AR solo."
                                    : "." );
    else if ( basta != !mas )
        {
        if ( !basta )
            {
            g_string_append_printf( t, "    ⚠ <b>DISCREPAN: ninguno rechaza su nula.</b> Shin-Fuller "
                "no rechaza la raíz unitaria (φ̂ = %.4f) y apunta a d + 1; el DCD no rechaza θ = 1 "
                "(θ̂ = %.4f, LR = %.3f &lt; %.2f): la diferencia extra se cancela y d basta.\n",
                sf->coef, od->coef, od->est, od->c5 );
            g_string_append_printf( t, dist <= FM_BANDA
                ? "    Con el testigo a %.4f de la frontera es la <b>banda de cuasi-cancelación</b> "
                  "vista desde el lado sin potencia: las dos representaciones son <b>equivalentes "
                  "en previsión</b>, y un no-rechazo de Shin-Fuller no demuestra la raíz unitaria. "
                  "<b>No subas d con esta evidencia</b>: decide por parsimonia o comparando "
                  "previsiones fuera de muestra.\n"
                : "    Con el testigo a %.4f de la frontera NO es la banda de cuasi-cancelación. "
                  "Un no-rechazo de Shin-Fuller no demuestra la raíz unitaria: antes de mover d, "
                  "estima el candidato d + 1 y compáralo por diagnosis y criterios de "
                  "información.\n", dist );
            }
        else if ( dist <= FM_BANDA )
            g_string_append_printf( t, "    ⚠ <b>DISCREPAN, y eso es el diagnóstico.</b> Con θ̂ = %.4f "
                "el testigo está a %.4f de la frontera: es la <b>banda de cuasi-cancelación</b> "
                "(r ≈ 0.90–0.95 en el artículo), donde el lado MA detecta que r &lt; 1 y el lado AR "
                "ve un proceso casi estacionario. Los dos tienen razón. En esa banda las "
                "representaciones son <b>equivalentes en previsión</b>: la decisión se toma por "
                "parsimonia, o comparando previsiones fuera de muestra. <b>No leas «d + 1» como "
                "una conclusión.</b>\n", od->coef, dist );
        else
            g_string_append_printf( t, "    ⚠ <b>DISCREPAN, y NO es la banda de cuasi-cancelación.</b> "
                "Con θ̂ = %.4f el testigo está a %.4f de la frontera, y la banda exige %.2f o "
                "menos. Sin la banda tampoco vale la equivalencia en previsión: antes de mover d, "
                "estima el candidato rival y compáralo por diagnosis y criterios de información. "
                "Descarta antes un modelo todavía inadecuado y deterministas de baja frecuencia "
                "(escalones), que pueden subir el crítico real del lado MA.\n",
                od->coef, dist, FM_BANDA );
        }
    else
        {
        g_string_append( t, "    ✓ Los dos coinciden: el orden de integración no está en la "
                            "banda ambigua.\n" );
        if ( ud && ud->hecho )
            {
            gboolean genuina = ud->calculable && ud->est >= ud->c5;
            gboolean cancelada = ud->calculable && !genuina && !ud->raices_complejas &&
                                 !ud->acotado && 1.0 - ud->coef <= FM_BANDA;

            g_string_append( t, genuina
                ? "    ✓ Y acotado por abajo: la última diferencia es genuina, así que d − 1 no "
                  "habría bastado. Las dos direcciones cierran sobre la misma d.\n"
                : cancelada
                ? "    ⚠ <b>Pero por abajo no cierra:</b> el testigo de subdiferenciación se apila "
                  "en θ = +1, así que la última diferencia está cancelada y con d − 1 bastaba. "
                  "Estima el candidato d − 1 y compáralo por diagnosis y criterios de "
                  "información.\n"
                : "    ⚠ Por abajo no hay veredicto: la subdiferenciación no es concluyente o no "
                  "es calculable. El orden queda acotado solo por arriba.\n" );
            }
        else
            g_string_append( t, "    ⓘ Solo por arriba: el lado d − 1 no se ha pedido.\n" );
        }
    g_string_append( t, "\n" );
}

static void un_contraste( GString *t, const Fm *g, const FmPrueba *x, int i )
{
    static const char *const nom[] = { "Simplificación estacional", "Shin-Fuller",
                                       "DCD", "DCD estacional", "Sobrediferenciación",
                                       "Subdiferenciación (pedida)", "MEG", "DCD_f" };
    gchar *h = g_markup_escape_text( x->hipotesis, -1 );

    g_string_append_printf( t, "<b>%s%s</b>   H<sub>0</sub>: %s\n", nom[x->tipo],
                            x->tipo == FM_DCD && x->freq > 0 ? " en Nyquist" : "", h );
    g_free( h );
    if ( x->tipo == FM_SF )
        g_string_append_printf( t, "    raíz real positiva dominante del AR: φ̂ = %.4f\n", x->coef );
    else if ( x->tipo == FM_DCD )
        g_string_append_printf( t, "    θ̂ = %.4f\n", x->coef );
    else if ( x->tipo == FM_DCD_S )
        g_string_append_printf( t, "    Θ̂ = %.4f\n", x->coef );
    else if ( x->tipo == FM_DCD_F )
        g_string_append_printf( t, "    θ̂₂ = %.4f  (λ̂ = %.4f; la frontera es θ₂ = −1)\n", x->coef,
                                x->coef < 0.0 ? sqrt( -x->coef ) : 0.0 );

    if ( !x->hecho )
        {
        g_string_append( t, i == g->cur && g->corriendo ? "    <i>estimando el modelo restringido…</i>\n\n"
                                                         : "    <i>en cola</i>\n\n" );
        return;
        }
    if ( !x->calculable )
        {
        gchar *m = g_markup_escape_text( x->motivo, -1 );

        g_string_append_printf( t, "    <b>No calculable</b>: %s.\n\n", m );
        g_free( m );
        return;
        }

    if ( x->tipo == FM_SOBRE ) { sobre( t, g, x ); return; }
    if ( x->tipo == FM_SUB )   { sub( t, g, x );   return; }
    g_string_append_printf( t, "    L = %.4f,  L<sub>0</sub> = %.4f.   ", x->l_libre, x->l_restr );
    switch ( x->tipo )
        {
        case FM_ESTACIONAL:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b> con %d g.l., ",
                                    x->est, x->gl );
            if ( x->p < 1e-4 ) g_string_append( t, "p &lt; 0.0001" );
            else               g_string_append_printf( t, "p = %.4f", x->p );
            g_string_append( t, "\n    " );
            criticos( t, x, "la χ²" );
            g_string_append( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: los armónicos son, en conjunto, significativos.\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: los armónicos se podrían quitar en bloque.\n\n" );
            break;
        case FM_SF:
            g_string_append_printf( t, "Φ = L − L<sub>0</sub> = <b>%.3f</b>  (no 2(L − L<sub>0</sub>))\n    ",
                                    x->est );
            {
            gchar *de = g_strdup_printf( "Shin-Fuller (1998) para n = %d", g->pl.nobs );

            criticos( t, x, de );
            g_free( de );
            }
            if ( x->al_otro_lado )
                g_string_append_printf( t, "\n    → <b>No rechaza H<sub>0</sub></b>: φ̂ = %.4f está "
                    "por encima de ρ, así que Φ = 0 por construcción: los datos son al menos tan "
                    "integrados como la nula.%s\n\n", x->coef,
                    g->pl.d >= 2 ? " Con d = 2 no se propone otra diferencia." : "" );
            else if ( x->rechaza )
                g_string_append_printf( t, "\n    → <b>Rechaza H<sub>0</sub></b>: el AR es "
                    "estacionario; con d = %d basta.\n\n", g->pl.d );
            else
                g_string_append_printf( t, "\n    → <b>No rechaza H<sub>0</sub></b>: el AR puede "
                    "tener una raíz unitaria%s.\n\n", g->pl.d >= 2
                    ? "; con d = 2 no se propone otra diferencia"
                    : ": la alternativa es una diferencia regular más" );
            break;
        case FM_DCD:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b>\n    ", x->est );
            criticos( t, x, "Davis-Dunsmuir" );
            if ( x->freq > 0 )
                {
                g_string_append_printf( t, x->rechaza
                    ? "\n    → <b>Rechaza H<sub>0</sub></b>: el testigo es invertible, la raíz en "
                      "Nyquist es genuina: <b>la estacionalidad en f = %d es estocástica</b>.\n\n"
                    : "\n    → <b>No rechaza H<sub>0</sub></b>: el testigo se apila en −1 y cancela "
                      "el (1 + B): la estacionalidad en f = %d es <b>determinista</b>; vuelve al "
                      "término alter.\n\n", x->freq );
                break;
                }
            g_string_append( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: el MA es invertible; la diferencia regular no sobra.\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: θ está en la frontera de la "
                  "invertibilidad: con una diferencia regular menos bastaría.\n\n" );
            break;
        case FM_DCD_S:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b>\n    ", x->est );
            {
            gchar *de = g_strdup_printf( "Davis, Chen y Dunsmuir para s = %d", g->pl.s );

            criticos( t, x, de );
            g_free( de );
            }
            g_string_append( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: el MA anual es invertible; la diferencia "
                  "estacional es genuina (estacionalidad estocástica).\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: Θ está en la frontera: la diferencia "
                  "estacional sobra y la estacionalidad sería determinista.\n\n" );
            break;
        case FM_DCD_F:
            g_string_append_printf( t, "LR = 2(L − L<sub>0</sub>) = <b>%.3f</b>\n    ", x->est );
            {
            gchar *de = g_strdup_printf( "la ley del par complejo (s = 2) para n = %d", x->n_crit );

            criticos( t, x, de );
            g_free( de );
            }
            g_string_append_printf( t, x->rechaza
                ? "\n    → <b>Rechaza H<sub>0</sub></b>: el testigo es invertible, la raíz unitaria "
                  "en f = %d es genuina: <b>la estacionalidad ahí es estocástica</b>.\n\n"
                : "\n    → <b>No rechaza H<sub>0</sub></b>: el testigo se apila en la frontera y "
                  "cancela la raíz: la estacionalidad en f = %d es <b>determinista</b> (o una "
                  "cuasi-cancelación); vuelve a sus armónicos cos/sin.\n\n", x->freq );
            break;
        default:
            break;
        }
}

/* EL MEG, frecuencia a frecuencia, en un bloque: cada frecuencia es un
 * contraste, pero se leen juntas. */
static void meg( GString *t, const Fm *g )
{
    int i, hay = 0, todas = 1, est[16], ne = 0, sin = 0, k;
    int n_int = 0;

    for ( i = 0; i < g->pl.n; i++ )
        if ( g->pl.t[i].tipo == FM_MEG )
            {
            hay++;
            if ( !g->pl.t[i].hecho ) todas = 0;
            if ( 2 * g->pl.t[i].freq != g->pl.s ) n_int = g->pl.t[i].n_crit;
            }
    if ( !hay ) return;
    g_string_append( t, "<b>MEG — estacionalidad estocástica o determinista</b>, frecuencia a "
        "frecuencia\n    <small>En cada f, el modelo estocástico S quita el "
        "armónico determinista, pone la raíz unitaria (ifadf[f] = 1) y un testigo de "
        "sobrediferenciación libre del mismo orden. H<sub>0</sub>: el testigo en la frontera, donde "
        "cancela la raíz. Testigo invertible → estocástica; en la frontera → determinista.</small>\n" );
    for ( i = 0; i < g->pl.n; i++ )
        {
        const FmPrueba *x = &g->pl.t[i];
        int             nyq = 2 * x->freq == g->pl.s;
        gchar          *per = nyq ? g_strdup( "Nyquist" )
                                  : g_strdup_printf( "ciclo de %g períodos", (double) g->pl.s / x->freq );

        if ( x->tipo != FM_MEG ) { g_free( per ); continue; }
        g_string_append_printf( t, "    f = %d <small>(%s)</small>: ", x->freq, per );
        g_free( per );
        if ( !x->hecho )
            g_string_append( t, i == g->cur && g->corriendo ? "<i>estimando…</i>\n" : "<i>en cola</i>\n" );
        else if ( !x->calculable )
            {
            gchar *m = g_markup_escape_text( x->motivo, -1 );

            g_string_append_printf( t, "%s%s.\n", x->omitida ? "" : "<b>no calculable</b>: ", m );
            g_free( m );
            sin++;
            }
        else
            {
            g_string_append_printf( t, "testigo %s = %.4f;  L<sub>S</sub> = %.4f,  L<sub>0</sub> = "
                "%.4f;  LR = <b>%.3f</b> frente a %.2f → <b>%s</b>\n", nyq ? "θ̂" : "θ̂₂", x->coef,
                x->l_libre, x->l_restr, x->est, x->c5, x->rechaza ? "estocástica" : "determinista" );
            if ( x->rechaza && ne < 16 ) est[ne++] = i;
            }
        if ( x->hecho && x->dudoso )
            g_string_append( t, "        <small>⚠ alguna de sus estimaciones paró sin anular el "
                                "gradiente, también reiniciando: el LR hereda esa duda.</small>\n" );
        }
    if ( n_int > 0 )
        {
        double c10, c5, c1;

        fm_criticos_dcd_f( n_int, &c10, &c5, &c1 );
        g_string_append_printf( t, "    <small>Críticos: en las interiores, los del par complejo "
            "(ley s = 2) para n = %d: %.2f (10 %%), %.2f (5 %%), %.2f (1 %%); en Nyquist, los de una "
            "raíz real (s = 1): 1.00, 1.94, 4.41. Salen de simulación (el Monte Carlo del estudio "
            "SF_MEG, sin publicar).</small>\n", n_int, c10, c5, c1 );
        }
    if ( todas )
        {
        /* las estocasticas, de la mas fuerte a la mas debil */
        for ( i = 0; i < ne; i++ )
            for ( k = i + 1; k < ne; k++ )
                if ( g->pl.t[est[k]].est > g->pl.t[est[i]].est ) { int a = est[i]; est[i] = est[k]; est[k] = a; }
        if ( ne )
            {
            g_string_append( t, "    → <b>Estocástica(s)</b>:" );
            for ( i = 0; i < ne; i++ )
                g_string_append_printf( t, "%s f = %d (LR %.2f)", i ? "," : "", g->pl.t[est[i]].freq,
                                        g->pl.t[est[i]].est );
            g_string_append_printf( t, ". Reformula <b>una cada vez, la más fuerte primero</b> (f = %d), "
                "estima el reformulado y vuelve a contrastar: el MEG sobre el modelo todo "
                "determinista contamina unas frecuencias con otras. En el reformulado, el DCD del "
                "testigo dice si la raíz es genuina o una cuasi-cancelación.\n",
                g->pl.t[est[0]].freq );
            }
        else if ( sin < hay )
            g_string_append( t, "    → Ninguna estocástica: los armónicos deterministas son la "
                                "especificación que sostienen los datos.\n" );
        for ( i = 0, k = 0; i < ne; i++ )
            k |= 2 * g->pl.t[est[i]].freq != g->pl.s && g->pl.t[est[i]].est < CRIT_MEG_N120_5;
        if ( k )
            g_string_append_printf( t, "    <small>ⓘ El crítico impreso es el de la ley desnuda. Con "
                "media y armónicos en el modelo, en muestra finita el de verdad es mayor (n = 120: "
                "%.2f al 5 %%), y el efecto se desvanece al crecer n: un LR apenas por encima del "
                "impreso no es concluyente por sí solo.</small>\n", CRIT_MEG_N120_5 );
        if ( sin )
            g_string_append( t, "    <small>Una frecuencia sin veredicto no es determinista: no se "
                                "pudo mirar.</small>\n" );
        }
    g_string_append( t, "\n" );
}

/* EL TEXTO, en marcado de Pango: lo pinta la etiqueta y lo lleva el
   cuaderno.                                                           */
static GString *texto( Fm *g )
{
    GString *t = g_string_new( "" );
    int      i, k;

    if ( g->aviso[0] )
        g_string_append_printf( t, "<span foreground=\"#9a6700\"><b>%s</b></span>\n\n", g->aviso );
    for ( i = 0; i < g->pl.n; i++ )
        if ( g->pl.t[i].tipo != FM_MEG )
            {
            un_contraste( t, g, &g->pl.t[i], i );
            if ( g->pl.t[i].hecho && g->pl.t[i].dudoso )
                g_string_append( t, "    <small>⚠ Alguna de sus estimaciones paró sin anular el "
                    "gradiente, también reiniciando: el LR hereda esa duda.</small>\n\n" );
            }
    par( t, g );
    meg( t, g );
    if ( g->pl.n == 0 )
        g_string_append( t, g->fam == AN_FM_INTEGRACION
            ? "Este modelo no admite ninguno de los contrastes del orden de integración.\n\n"
            : "Este modelo no admite ninguno de los contrastes de la estacionalidad.\n\n" );
    for ( k = 0; k < FM_NA; k++ )
        if ( g->pl.no_aplica[k][0] )
            {
            gchar *m = g_markup_escape_text( g->pl.no_aplica[k], -1 );

            g_string_append_printf( t, "<small>%s</small>\n", m );
            g_free( m );
            }
    {
    gchar *d = g_markup_escape_text( g->dir, -1 );

    g_string_append_printf( t, "\n<small>Cada contraste es una estimación más: el modelo con el "
        "parámetro fijo en su valor nulo, que fue reestima partiendo del óptimo. Los .inp y los "
        ".out de los restringidos quedan en %s. %s Una estimación que para sin anular el "
        "gradiente se reinicia desde donde paró, hasta tres veces, como hace fue-python."
        "</small>", d, g->fam == AN_FM_INTEGRACION
        ? "Shin-Fuller usa Φ = L − L<sub>0</sub> y no el doble; el DCD, 2(L − L<sub>0</sub>) "
          "con su propia ley, que no es la χ²."
        : "El MEG y los DCD, 2(L − L<sub>0</sub>) con su propia ley, que no es la χ²; la "
          "simplificación estacional, con la χ²." );
    g_free( d );
    }
    return t;
}

static void pinta( Fm *g )
{
    GString *t = texto( g );

    gtk_label_set_markup( GTK_LABEL(g->l_texto), t->str );
    g_string_free( t, TRUE );
}

static void on_cuaderno( GtkButton *b, Fm *g )
{
    Cuaderno *c = an_cuaderno( &g->h, g->fam == AN_FM_INTEGRACION
                                      ? "Orden de integración"
                                      : "Estacionalidad (MEG)",
                               g->serie, g->muestra, g->id );
    GString  *t;
    gchar    *md, *v;

    (void) b;
    if ( c == NULL ) { an_cuaderno_cierra( &g->h, NULL ); return; }
    v = g_strdup_printf( "%d", g->pl.nobs );
    cu_dato( c, "Observaciones", v );
    g_free( v );
    if ( g->cur < g->pl.n )
        cu_texto( c, "*(Incompleto: quedaban contrastes por estimar al "
                     "mandarlo aquí.)*" );
    t  = texto( g );
    md = an_markup_md( t->str );
    cu_texto( c, md );
    g_free( md );
    g_string_free( t, TRUE );
    an_cuaderno_cierra( &g->h, c );
}

static void siguiente( Fm *g );

static void acabo( const EngineResult *r, gpointer data )
{
    Fm *g = data;

    g->corriendo = FALSE;
    if ( g->cerrada )
        { g_free( g->exe ); g_free( g->dir ); g_free( g ); return; }
    /* SOLO EL 0: con un 3 fue no pudo estimar y deja los valores
       iniciales, y su logelf no es el maximo de nada. Si al contraste le
       falta otra estimacion, fm_avanza la deja escrita y se corre esa. */
    if ( !fm_avanza( &g->pl, &g->pl.t[g->cur], g->l_libre, r->status == 0 ) ) g->cur++;
    siguiente( g );
}

/* LAS ESTOCASTICAS DEL MEG, para reformular: de la mas fuerte a la mas
 * debil, la primera elegida. */
static void ofrece_reformular( Fm *g )
{
    int i, k, est[16], ne = 0;

    if ( !g->c_freq ) return;
    gtk_combo_box_text_remove_all( GTK_COMBO_BOX_TEXT(g->c_freq) );
    for ( i = 0; i < g->pl.n; i++ )
        if ( g->pl.t[i].tipo == FM_MEG && g->pl.t[i].hecho && g->pl.t[i].calculable &&
             g->pl.t[i].rechaza && ne < 16 ) est[ne++] = i;
    for ( i = 0; i < ne; i++ )
        for ( k = i + 1; k < ne; k++ )
            if ( g->pl.t[est[k]].est > g->pl.t[est[i]].est ) { int a = est[i]; est[i] = est[k]; est[k] = a; }
    for ( i = 0; i < ne; i++ )
        {
        gchar *id = g_strdup_printf( "%d", g->pl.t[est[i]].freq );
        gchar *tx = g_strdup_printf( "f = %d  (LR %.2f)", g->pl.t[est[i]].freq, g->pl.t[est[i]].est );

        gtk_combo_box_text_append( GTK_COMBO_BOX_TEXT(g->c_freq), id, tx );
        g_free( id ); g_free( tx );
        }
    if ( ne ) gtk_combo_box_set_active( GTK_COMBO_BOX(g->c_freq), 0 );
    gtk_widget_set_sensitive( g->c_freq, ne > 0 );
    gtk_widget_set_sensitive( g->b_ref, ne > 0 );
}

static void siguiente( Fm *g )
{
    while ( g->cur < g->pl.n && g->pl.t[g->cur].hecho ) g->cur++;   /* las omitidas */
    if ( g->cur < g->pl.n && g->exe )
        {
        const char *argv[2] = { g->pl.t[g->cur].base, NULL };
        gchar      *viejo = g_strdup( g->pl.t[g->cur].inp ), *e;

        /* el .out de una vuelta anterior, fuera: el que se lea sera este */
        strcpy( viejo + strlen( viejo ) - 4, ".out" );
        g_unlink( viejo );
        g_free( viejo );
        e = g_strdup_printf( "Contraste %d de %d: estimando %s…", g->cur + 1, g->pl.n,
                             g->pl.t[g->cur].base );

        gtk_label_set_text( GTK_LABEL(g->l_estado), e );
        g_free( e );
        /* CADA REESTIMACION DEL CONTRASTE CUESTA, y es de este modelo: el
           MEG son dos por frecuencia. El contexto, solo mientras se lanza. */
        {
        char raiz[PR_RUTA];

        if ( g->h.p && pr_raiz( g->h.p, raiz, sizeof raiz ) == 0 )
            coste_contexto_modelo( raiz, g->serie, g->muestra, g->id,
                                   g->pl.t[g->cur].tipo == FM_MEG ? "meg" : "contraste" );
        }
        g->corriendo = engine_run_async( g->dir, g->exe, argv, NULL, acabo, g );
        coste_contexto_limpia();
        if ( !g->corriendo )
            {
            while ( fm_avanza( &g->pl, &g->pl.t[g->cur], g->l_libre, FALSE ) ) ;
            g_snprintf( g->pl.t[g->cur].motivo, sizeof g->pl.t[g->cur].motivo,
                        "no pude lanzar %s", g->exe );
            g->cur++;
            siguiente( g );
            return;
            }
        }
    else
        {
        gtk_label_set_text( GTK_LABEL(g->l_estado), g->exe
            ? "Hecho. Cada contraste dice lo suyo; qué hacer con ello lo decides tú."
            : "No encuentro «fue»: ni al lado de esta ventana ni en el PATH." );
        ofrece_reformular( g );
        }
    pinta( g );
}

/* EL LADO d - 1 SE PIDE, NO SE OFRECE (art BUG-0167): correrlo siempre
 * seria pre-testing. Lo pide el analista cuando hay motivo. */
static void on_sub( GtkButton *b, Fm *g )
{
    char why[256];

    if ( fm_plan_sub( &g->pl, why, sizeof why ) != 0 )
        { gtk_label_set_text( GTK_LABEL(g->l_estado), why ); return; }
    gtk_widget_set_sensitive( GTK_WIDGET(b), FALSE );
    if ( !g->corriendo ) siguiente( g );
    else pinta( g );
}

/* LA REFORMULACION DESPUES DEL MEG: el modelo S de la frecuencia elegida,
 * como hijo del modelo (pr_deriva), juzgado por fue y abierto para
 * estimarlo. Es lo que hace el identificador al derivar. */
static void on_reformular( GtkButton *b, Fm *g )
{
    const char     *fid = gtk_combo_box_get_active_id( GTK_COMBO_BOX(g->c_freq) );
    const FmPrueba *x = NULL;
    char            destino[PR_RUTA], nuevo[PR_ID], msg[512], why[256];
    PrError         e;
    gchar          *dir, *razon, *aviso;
    int             f, i;
    AnHerramienta   con;

    (void) b;
    if ( !fid ) return;
    f = atoi( fid );
    for ( i = 0; i < g->pl.n; i++ )
        if ( g->pl.t[i].tipo == FM_MEG && g->pl.t[i].freq == f ) x = &g->pl.t[i];
    if ( !x ) return;
    if ( pr_deriva( g->h.p, g->serie, g->muestra, g->id, nuevo, sizeof nuevo,
                    destino, sizeof destino, &e ) != 0 )
        { pr_error_es( &e, msg, sizeof msg ); gtk_label_set_text( GTK_LABEL(g->l_estado), msg ); return; }
    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
    if ( fm_reformula( g->pl.pre, g->pl.out[0] ? g->pl.out : NULL, destino, g->pl.s, f, NULL,
                       why, sizeof why ) != 0 )
        {
        pr_borra( g->h.p, g->serie, g->muestra, nuevo, &e );
        gtk_label_set_text( GTK_LABEL(g->l_estado), why );
        return;
        }
    /* EL JUEZ ES EL MOTOR: un .inp que fue no acepta se deshace */
    if ( inp_check_fue( destino, msg, sizeof msg ) != 0 )
        {
        gchar *m = g_strdup_printf( "Lo que salió no lo acepta fue: %s", msg );

        g_unlink( destino );
        pr_borra( g->h.p, g->serie, g->muestra, nuevo, &e );
        gtk_label_set_text( GTK_LABEL(g->l_estado), m );
        g_free( m );
        return;
        }
    razon = g_strdup_printf( "Reformulación MEG en f = %d: estacionalidad estocástica (LR = %.3f "
        "frente a %.2f al 5 %%). %s, la raíz unitaria con ifadf[%d] = 1 y un testigo libre %s, "
        "desde −0.9: el modelo S que contrastó el MEG. Derivado de %s.", f, x->est, x->c5,
        2 * f == g->pl.s ? "Sin el término alter" : "Sin los armónicos cos/sin de esa frecuencia",
        f, 2 * f == g->pl.s ? "(1 + θB)" : "de frecuencia fija", g->id );
    pr_razon( g->h.p, g->serie, g->muestra, nuevo, razon, &e );
    g_free( razon );
    con = ( g->h.puede && !( g->h.puede & AN_PUEDE_FUE ) ) ? AN_CON_EDITOR : AN_CON_FUE;
    pr_pon_herramienta( g->h.p, con == AN_CON_EDITOR );
    if ( g->h.guarda && g->h.guarda( g->h.dueno ) != 0 )
        an_di( &g->h, "El .inp está en %s, pero no pude guardar el proyecto.", nuevo );
    if ( g->h.refresca ) g->h.refresca( g->h.dueno );
    {
    AnHost host = g->h;
    char   serie[PR_ID], muestra[PR_ID], padre[PR_ID];

    g_snprintf( serie, sizeof serie, "%s", g->serie );
    g_snprintf( muestra, sizeof muestra, "%s", g->muestra );
    g_snprintf( padre, sizeof padre, "%s", g->id );
    aviso = g_strdup_printf( "%s nace de %s con la estacionalidad en f = %d estocástica. Estímalo "
                             "y vuelve a pasar los contrastes: el DCD de su testigo dice si la raíz "
                             "es genuina, y el MEG mira las demás frecuencias sobre él.", nuevo, padre, f );
    gtk_widget_destroy( g->win );
    if ( host.cierra ) host.cierra( host.dueno, serie, muestra, padre );
    if ( host.abre ) host.abre( host.dueno, serie, muestra, nuevo, con );
    an_di( &host, "%s", aviso );
    g_free( aviso );
    }
}

static void on_cerrar( GtkWidget *w, Fm *g )
{
    (void) w;
    /* un fue en marcha llamara a acabo(): ahi se suelta */
    if ( g->corriendo ) { g->cerrada = TRUE; return; }
    g_free( g->exe ); g_free( g->dir ); g_free( g );
}

void an_formales( const AnHost *h, const char *serie, const char *muestra, const char *id,
                  AnFmFamilia fam )
{
    Fm         *g;
    FueOut     *o;
    Convergence c;
    char        out[PR_RUTA], pre[PR_RUTA], why[256];
    GtkWidget  *raiz, *cab, *sw;

    if ( !h || !h->p || !serie || !*serie || !id || !*id ) return;
    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 ||
         pr_ruta( h->p, serie, muestra, id, ".pre", pre, sizeof pre ) != 0 )
        { an_di( h, "No pude componer la ruta." ); return; }
    if ( !g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { an_di( h, "«%s» no tiene .pre: hay que estimarlo antes de contrastar nada.", id ); return; }

    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) || !o->tiene_logelf )
        { an_di( h, "El .out de «%s» no trae su log-verosimilitud (logelf).", id );
          g_free( o ); return; }

    if ( fam == AN_FM_ESTACIONAL && o->s <= 1 )
        { an_di( h, "«%s» no es estacional (s = %d): no hay estacionalidad que contrastar.",
                 id, o->s );
          g_free( o ); return; }

    g = g_new0( Fm, 1 );
    g->h = *h;
    g->fam = fam;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );
    g->l_libre = o->logelf;

    /* LA DIAGNOSIS PRIMERO (art BUG-0025): no se impide, se dice. */
    memset( &c, 0, sizeof c );
    if ( !convergence_of( out, &c ) ) memset( &c, 0, sizeof c );
    {
    Dictamen *d = g_new0( Dictamen, 1 );

    dx_dictamen( o, &c, NULL, 0, d );
    if ( d->peor == DX_NO )
        g_snprintf( g->aviso, sizeof g->aviso, "La diagnosis no está limpia. Estos contrastes "
                    "suponen un modelo adecuado: léelos con esa reserva, o arregla antes el "
                    "modelo." );
    g_free( d );
    }
    convergence_clear( &c );

    g->dir = an_fichero( fam == AN_FM_INTEGRACION ? "integracion" : "estacionalidad",
                         serie, muestra, id );
    g_mkdir_with_parents( g->dir, 0700 );
    if ( fm_plan( pre, out, g->dir, o->nobs, o->s > 0 ? o->s : 1, o->d, &g->pl,
                  why, sizeof why ) != 0 )
        { an_di( h, "%s", why ); g_free( o ); g_free( g->dir ); g_free( g ); return; }
    g_free( o );
    quedate_con( &g->pl, fam );
    g->exe = programa_fue();

    g->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( h->padre ) gtk_window_set_transient_for( GTK_WINDOW(g->win), h->padre );
    gtk_window_set_default_size( GTK_WINDOW(g->win), 860, 600 );
    {
    gchar *ti = g_strdup_printf( "%s — %s / %s", fam == AN_FM_INTEGRACION
                                 ? "Orden de integración" : "Estacionalidad", serie, id );

    gtk_window_set_title( GTK_WINDOW(g->win), ti );
    g_free( ti );
    }
    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(g->win), raiz );

    cab = gtk_label_new( NULL );
    {
    gchar *m = g_markup_printf_escaped( "<b>%s / %s</b>%s%s   <small>%d observaciones, "
                                        "s = %d, d = %d</small>", serie, id,
                                        ( muestra && *muestra ) ? "   muestra " : "",
                                        ( muestra && *muestra ) ? muestra : "",
                                        g->pl.nobs, g->pl.s, g->pl.d );

    gtk_label_set_markup( GTK_LABEL(cab), m );
    g_free( m );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    g->l_texto = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_texto), 0.0 );
    gtk_label_set_yalign( GTK_LABEL(g->l_texto), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_texto), TRUE );
    gtk_label_set_selectable( GTK_LABEL(g->l_texto), TRUE );
    gtk_widget_set_can_focus( g->l_texto, FALSE );   /* si no, sale todo seleccionado */
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sw), g->l_texto );
    gtk_box_pack_start( GTK_BOX(raiz), sw, TRUE, TRUE, 0 );

    {
    GtkWidget *barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );

    g->l_estado = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_estado), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_estado), TRUE );
    gtk_box_pack_start( GTK_BOX(barra), g->l_estado, TRUE, TRUE, 0 );
    if ( fam == AN_FM_INTEGRACION )
    {
    g->b_sub = gtk_button_new_with_label( "Contrastar también d − 1…" );
    gtk_widget_set_sensitive( g->b_sub, g->pl.d >= 1 );
    gtk_widget_set_tooltip_text( g->b_sub, g->pl.d >= 1
        ? "¿Sobraba la última diferencia? Se pide, no se ofrece: correrlo siempre sería "
          "pre-testing. Tiene sentido cuando la serie parece estacionaria en nivel, es un "
          "precio relativo o se está en un estudio de cointegración."
        : "Con d = 0 no hay diferencia que cuestionar." );
    g_signal_connect( g->b_sub, "clicked", G_CALLBACK(on_sub), g );
    gtk_box_pack_end( GTK_BOX(barra), g->b_sub, FALSE, FALSE, 0 );
    }
    /* reformular tras el MEG: solo si hay MEG y el anfitrion sabe derivar */
    {
    gboolean hay_meg = FALSE;
    int      i;

    for ( i = 0; i < g->pl.n; i++ ) hay_meg |= g->pl.t[i].tipo == FM_MEG;
    if ( hay_meg && h->abre && h->guarda )
        {
        g->b_ref = gtk_button_new_with_label( "Reformular como estocástica…" );
        gtk_widget_set_tooltip_text( g->b_ref, "Deriva el modelo S de esa frecuencia --sin sus "
            "armónicos, con la raíz unitaria y su testigo-- como un hijo de este, para estimarlo "
            "y volver a contrastar. Una frecuencia cada vez, la más fuerte primero." );
        g->c_freq = gtk_combo_box_text_new();
        gtk_widget_set_sensitive( g->b_ref, FALSE );
        gtk_widget_set_sensitive( g->c_freq, FALSE );
        g_signal_connect( g->b_ref, "clicked", G_CALLBACK(on_reformular), g );
        gtk_box_pack_end( GTK_BOX(barra), g->b_ref, FALSE, FALSE, 0 );
        gtk_box_pack_end( GTK_BOX(barra), g->c_freq, FALSE, FALSE, 0 );
        }
    }
    {
    GtkWidget *bc = an_boton_cuaderno( h, G_CALLBACK(on_cuaderno), g );

    if ( bc ) gtk_box_pack_end( GTK_BOX(barra), bc, FALSE, FALSE, 0 );
    }
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );
    }

    g_signal_connect( g->win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( g->win );
    siguiente( g );
}
