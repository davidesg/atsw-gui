/*
 * modelo.c -- la pantalla del modelo: el .cns.
 *
 * LO QUE ESTA PANTALLA NO ES: un editor de texto con resaltado. El .cns no se
 * teclea a ciegas, porque sus nombres no son libres -- omega3[2] existe o no
 * existe segun el .dag y segun lo que traigan los .pre, y equivocarse de
 * nombre es el error mas facil de cometer y el mas tonto de diagnosticar.
 *
 * LO QUE ES: una vista de LA TABLA DE SLOTS. El modelo genera sus parametros
 * --tantos omega como diga s, tantos phi como el .pre deje libres-- y la
 * pantalla enseña esa lista y deja decir de cada uno lo que el .cns sabe
 * decir. Es la idea de TASTE llevada a donde ahora hace falta: alli el
 * formulario se desplegaba segun el orden (tecleas s y aparecen s+1 campos
 * OMEGA(j), TFMOD.PAS:149-177), aqui la LISTA es el modelo.
 *
 * Y CON LO QUE TASTE NO TENIA. En TASTE todo lo que declarabas por orden se
 * estimaba --npar := s+1+r, TFMOD.PAS:519-- y la unica forma de fijar un
 * coeficiente era bajar el orden. Aqui hay bandera por coeficiente, porque el
 * contrato lo exige: un parametro FIJO es ESPECIFICACION, no semilla.
 *
 * Las cinco formas del lenguaje, todas en el mismo sitio:
 *
 *     libre                    se estima
 *     fijo en un valor         es especificacion
 *     igual a otro             un grado de libertad en dos sitios
 *     producto de otros dos    numerador factorizado con MA compartida
 *     combinacion lineal       un factor FIJO (1-B) impone nu(1) = 0
 *
 * La tabla la construye lib/slots, que ES la del motor. mtram no puede ofrecer
 * un slot que el motor no tenga, ni escribir un .cns que el motor rechace.
 */

#include <string.h>
#include <stdlib.h>

#include "gui.h"
#include "previewhost.h"
#include "slots.h"
#include "nsop.h"

enum { M_NOMBRE, M_QUE, M_DICE, M_IDX, M_N };

/* Los grupos, en el orden en que se enseñan. Ya existian como columna; aqui
 * son la ESTRUCTURA, porque 67 renglones heterogeneos en una lista plana no se
 * recorren -- y lo que el analista toca de verdad son seis.              */
static const char *GRUPOS[] = {
    "transferencia", "ARMA del ruido", "deterministas",
    "medias", "varianzas", "covarianzas", "otros"
};
#define N_GRUPOS ((int)(sizeof GRUPOS / sizeof GRUPOS[0]))

/* De que grupo es un slot. Sale del nombre, que es como lo bautiza el motor. */
static const char *grupo_de( const char *n )
{
    if (!strncmp(n, "omega_d", 7) || !strncmp(n, "delta_d", 7)) return GRUPOS[2];
    if (!strncmp(n, "omega", 5)   || !strncmp(n, "delta", 5))   return GRUPOS[0];
    if (!strncmp(n, "phi_", 4)    || !strncmp(n, "theta_", 6))  return GRUPOS[1];
    if (!strncmp(n, "mu[", 3))     return GRUPOS[3];
    if (!strncmp(n, "log(var", 7)) return GRUPOS[4];
    if (!strncmp(n, "q[", 2))      return GRUPOS[5];
    return GRUPOS[6];
}

static const char *que_es( int kind )
{
    switch (kind) {
    case SLOT_FREE:    return "libre";
    case SLOT_FIXED:   return "FIJO";
    case SLOT_ALIAS:   return "compartido";
    case SLOT_PRODUCT: return "producto";
    case SLOT_LINCOMB: return "comb. lineal";
    }
    return "?";
}

/* ------------------------------------------------------------------------ */
/* Construir la tabla a partir de lo que hay cargado                         */
/* ------------------------------------------------------------------------ */

/* La tabla se reconstruye entera cada vez que cambia algo --cargar una serie,
 * tocar un enlace-- porque su forma DEPENDE de eso. Pero lo que el analista
 * haya dicho de cada parametro no se pierde por el camino: se lleva a la tabla
 * nueva emparejando por nombre. Perderlo en silencio seria la peor forma de
 * perderlo.                                                               */
static void construye( Mtram *m )
{
    Modelo          *M = &m->mod;
    struct Tusmodel  Tm[GUI_MAX_SER + 1];
    SlotTable        viejo;
    gboolean         habia;
    int              i;

    habia   = M->vale;
    if (habia) viejo = M->st;

    M->vale = FALSE;
    M->st.n = 0;
    M->perdidas = 0;
    if (m->c.n < 2) return;

    for (i = 1; i <= m->c.n; i++) Tm[i] = m->c.s[i - 1]->tm;

    /* Lo que el .pre declare fijo se respeta siempre. Ademas, el analista
     * puede MANTENER partes enteras en vez de reestimarlas al juntar: es
     * -N/-X/-D/-E/-M, y pasarselo a slots_build hace que la cuenta de libres
     * lo refleje AL MOMENTO, que es la realimentacion que hace falta.   */
    {
    int     arma[GUI_MAX_SER + 1], det[GUI_MAX_SER + 1], mu[GUI_MAX_SER + 1];
    SlotFix fix;

    for (i = 1; i <= m->c.n; i++) {
        arma[i] = i == 1 ? m->est.fix_N : m->est.fix_X;
        det[i]  = i == 1 ? m->est.fix_D : m->est.fix_E;
        mu[i]   = m->est.fix_M;
    }
    fix.arma = arma;  fix.det = det;  fix.mu = mu;
    slots_build( &M->st, Tm, m->c.n, m->red.lnk, m->red.n, &fix );
    }
    M->vale = TRUE;

    /* Salvo que las series se hayan MOVIDO: los nombres llevan la posicion
     * dentro --q[3,2], phi_2[B^1], mu[4]-- asi que despues de reordenar el
     * mismo nombre significa otra cosa, y emparejar por nombre seria
     * exactamente lo contrario de conservar.                            */
    if (habia && !M->orden_cambio)
        slots_carry( &M->st, &viejo, &M->perdidas );

    M->orden_cambio = FALSE;
}

/* ------------------------------------------------------------------------ */

/* El nombre de la serie i (1..n). */
static const char *nom_serie( Mtram *m, int i )
{
    if (i < 1 || i > m->c.n) return "?";
    return m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "?";
}

/* Cuantas covarianzas hay libres, y cuantas hay. */
static void covarianzas( const SlotTable *st, int *libres, int *total )
{
    int i;

    *libres = *total = 0;
    for (i = 1; i <= st->n; i++)
        if (!strncmp( st->name[i], "q[", 2 )) {
            (*total)++;
            if (st->kind[i] == SLOT_FREE) (*libres)++;
        }
}

/* Los enlaces contemporaneos con su covarianza libre: explican lo mismo dos
 * veces. Devuelve cuantos, y el primero en *k.                           */
static int colineales( Mtram *m, int *primero )
{
    int k, n = 0;

    for (k = 0; k < m->red.n; k++) {
        char nm[40];
        int  s1, s2;

        if (m->red.lnk[k].b != 0) continue;
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].out, m->red.lnk[k].inp );
        s1 = slots_find( &m->mod.st, nm );
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].inp, m->red.lnk[k].out );
        s2 = slots_find( &m->mod.st, nm );

        if ((s1 && m->mod.st.kind[s1] == SLOT_FREE) ||
            (s2 && m->mod.st.kind[s2] == SLOT_FREE)) {
            if (!n++ && primero) *primero = k;
        }
    }
    return n;
}

/* ------------------------------------------------------------------------ */
/* El arbol: UNA RAMA POR ECUACION                                           */
/*                                                                           */
/* No por tipo de parametro. Un grupo "ARMA del ruido" con los theta de las   */
/* seis series juntos es el vector de parametros DEL MOTOR, no el modelo del  */
/* analista -- y enseñandolo asi no hay forma de saber que sistema se esta    */
/* estimando.                                                                */
/*                                                                           */
/* El modelo es un SISTEMA DE ECUACIONES, una por serie. Dentro de cada una,  */
/* sus transferencias --que es lo que se decide aqui-- y su ruido, que VIENE  */
/* DE fue y aqui no se re-especifica: de el solo interesa cuantos parametros  */
/* mete y que estructura tiene.                                              */
/*                                                                           */
/* De los 67 del m6: 11 de transferencia y 15 covarianzas son de este         */
/* escalon; 36 son ruido que viene de los .pre --y 28 de esos, deterministas, */
/* mas que todo lo demas junto-- y 5 son varianzas relativas, consecuencia de */
/* juntarlas.                                                                */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Del idioma del motor al idioma del modelo                                 */
/*                                                                           */
/* El .cns habla de slots: "omega1[1] = omega1[0] * theta_2[B^1]". Eso es     */
/* COMO el motor lo impone, no QUE dice. Lo que dice es que el numerador se   */
/* factoriza, omega1(B) = w0 (1 - theta_EI B), y que theta_EI ES LA MA DEL    */
/* MODELO UNIVARIANTE DE EI.                                                  */
/*                                                                           */
/* Y ESO HAY QUE DECIRLO. Un producto asi NO es un parametro mas de la        */
/* transferencia: ATA la transferencia al modelo de ruido de la entrada, y    */
/* deja de ser separable de el. Enseñarlo como un coeficiente mas de omega1   */
/* esconde justo lo unico que hay que ver.                                    */
/* ------------------------------------------------------------------------ */

/* ¿Es esta restriccion la FORMA EMPOTRADA y no una afirmacion sobre nu(B)?
 *
 * La marca es el PRODUCTO de dos parametros. LEGACY_M6.md §9 lo dice: los
 * coeficientes FUERA DE LA DIAGONAL del shootx de m6-1 son productos --x5*x6,
 * x12*x14-x13, x2*x3*x4-- y al factorizarlos salen los numeradores. El
 * mecanismo PRODUCTO del .cns se añadio para poder reproducir ese VARMA
 * restringido, no para especificar una FLT.
 *
 * Un analista que especifica una FLT elige (b, r, s). Estas no se le ofrecen:
 * se conservan tal como vienen del fichero, se dicen aparte, y no se mezclan
 * con la especificacion.
 *
 * Lo que SI es afirmacion sobre nu, y por tanto se queda en el arbol:
 *    omega_j(1) = 0        un (1-B) fijo: ganancia a largo plazo cero
 *    delta_j = phi_X       el denominador ES el AR de la entrada
 *    omega_j[i] = valor    un coeficiente fijado                        */
static int n_empotradas( const SlotTable *st );

static int es_empotrado( const SlotTable *st, int k )
{
    int t;

    if (k < 1 || k > st->n) return 0;
    if (st->kind[k] == SLOT_PRODUCT) return 1;
    if (st->kind[k] == SLOT_LINCOMB)
        for (t = 0; t < st->nlc[k]; t++)
            if (st->lc_b[k][t]) return 1;     /* un producto por dentro */
    return 0;
}

/* Un slot que pertenece al modelo univariante de alguna serie. */
static int es_del_ruido( const char *n )
{
    return !strncmp( n, "phi_", 4 ) || !strncmp( n, "theta_", 6 ) ||
           !strncmp( n, "omega_d", 7 ) || !strncmp( n, "delta_d", 7 ) ||
           !strncmp( n, "mu[", 3 );
}

/* De que serie es, si es del ruido. 0 si no lo es. */
static int serie_del_ruido( const char *n )
{
    const char *p = strchr( n, '_' );

    if (!strncmp( n, "mu[", 3 )) return atoi( n + 3 );
    if (!es_del_ruido( n )) return 0;
    if (!strncmp( n, "omega_d", 7 )) return atoi( n + 7 );
    if (!strncmp( n, "delta_d", 7 )) return atoi( n + 7 );
    return p ? atoi( p + 1 ) : 0;
}

/* El nombre del slot en el idioma del modelo. */
static void nombre_modelo( Mtram *m, const char *n, char *out, size_t size )
{
    int  i, j;
    char resto[32];

    if (sscanf( n, "theta_%d[B^%d]", &i, &j ) == 2) {
        if (j == 1) snprintf( out, size, "θ_%s", nom_serie( m, i ) );
        else snprintf( out, size, "θ_%s[B^%d]", nom_serie( m, i ), j );
        return;
    }
    if (sscanf( n, "phi_%d[B^%d]", &i, &j ) == 2) {
        if (j == 1) snprintf( out, size, "φ_%s", nom_serie( m, i ) );
        else snprintf( out, size, "φ_%s[B^%d]", nom_serie( m, i ), j );
        return;
    }
    if (sscanf( n, "mu[%d]", &i ) == 1) {
        snprintf( out, size, "μ_%s", nom_serie( m, i ) );
        return;
    }
    if (sscanf( n, "omega%d[%31[^]]]", &i, resto ) == 2 &&
        strncmp( n, "omega_d", 7 )) {
        snprintf( out, size, "ω%d[%s]", i, resto );
        return;
    }
    if (sscanf( n, "delta%d[%31[^]]]", &i, resto ) == 2 &&
        strncmp( n, "delta_d", 7 )) {
        snprintf( out, size, "δ%d[%s]", i, resto );
        return;
    }
    snprintf( out, size, "%s", n );
}

/* Lo que el slot k DICE, en el idioma del modelo. Devuelve 0 si no dice nada
 * --esta libre y nacio libre-- y en *ruido, la serie del modelo univariante
 * con la que interactua, si la hay.                                     */
static int enunciado( Mtram *m, int k, char *out, size_t size, int *ruido )
{
    const SlotTable *st = &m->mod.st;
    char             a[64], b[64];
    int              t;

    *ruido = 0;
    if (k < 1 || k > st->n) { if (size) out[0] = 0; return 0; }

    switch (st->kind[k]) {

    case SLOT_FREE:
        if (strncmp( st->name[k], "q[", 2 )) { out[0] = 0; return 0; }
        snprintf( out, size, "libre" );
        return 1;

    case SLOT_FIXED:
        if (!strncmp( st->name[k], "q[", 2 ) && st->value[k] == 0.0 )
            { out[0] = 0; return 0; }
        nombre_modelo( m, st->name[k], a, sizeof a );
        snprintf( out, size, "%s = %g", a, (double) st->value[k] );
        return 1;

    case SLOT_ALIAS:
        nombre_modelo( m, st->name[k], a, sizeof a );
        nombre_modelo( m, st->name[st->alias[k]], b, sizeof b );
        snprintf( out, size, "%s = %s", a, b );
        *ruido = serie_del_ruido( st->name[st->alias[k]] );
        return 1;

    case SLOT_PRODUCT: {
        /* El caso de la escuela: omega_j[1] = omega_j[0] * theta_X, que es
         * el numerador factorizado w0 (1 - theta_X B).                 */
        int i1, i2, jj;

        nombre_modelo( m, st->name[st->pa[k]], a, sizeof a );
        nombre_modelo( m, st->name[st->pb[k]], b, sizeof b );

        *ruido = serie_del_ruido( st->name[st->pa[k]] );
        if (!*ruido) *ruido = serie_del_ruido( st->name[st->pb[k]] );

        if (sscanf( st->name[k], "omega%d[%d]", &i1, &jj ) == 2 && jj == 1 &&
            sscanf( st->name[st->pa[k]], "omega%d[0]", &i2 ) == 1 && i1 == i2 &&
            st->value[k] > 0.0)
            snprintf( out, size,
                "ω%d(B) = ω%d₀ (1 − %s B)",
                i1, i1, b );
        else {
            nombre_modelo( m, st->name[k], a, sizeof a );
            snprintf( out, size, "%s = %s%s · %s", a,
                      st->value[k] < 0.0 ? "−" : "",
                      st->name[st->pa[k]], b );
        }
        return 1;
    }

    case SLOT_LINCOMB: {
        /* omega_j[0] = omega_j[1] + omega_j[2] + ... es el factor (1-B) fijo:
         * impone omega_j(1) = 0, o sea GANANCIA A LARGO PLAZO CERO.      */
        int i1, jj, todos = 1;

        if (sscanf( st->name[k], "omega%d[%d]", &i1, &jj ) == 2 && jj == 0) {
            for (t = 0; t < st->nlc[k]; t++) {
                int i2, j2;

                if (st->lc_b[k][t] || st->lc_sign[k][t] < 0.0 ||
                    sscanf( st->name[st->lc_a[k][t]], "omega%d[%d]", &i2, &j2 ) != 2 ||
                    i2 != i1) { todos = 0; break; }
            }
            if (todos && st->nlc[k]) {
                snprintf( out, size,
                    "ω%d(1) = 0 · un (1−B) FIJO: "
                    "ganancia a largo plazo CERO", i1 );
                return 1;
            }
        }

        nombre_modelo( m, st->name[k], a, sizeof a );
        {
        GString *g = g_string_new( NULL );

        g_string_append_printf( g, "%s =", a );
        for (t = 0; t < st->nlc[k]; t++) {
            nombre_modelo( m, st->name[st->lc_a[k][t]], b, sizeof b );
            g_string_append_printf( g, " %s%s",
                st->lc_sign[k][t] < 0.0 ? "− " : (t ? "+ " : ""), b );
            if (st->lc_b[k][t]) {
                nombre_modelo( m, st->name[st->lc_b[k][t]], b, sizeof b );
                g_string_append_printf( g, " · %s", b );
            }
            if (!*ruido) *ruido = serie_del_ruido( st->name[st->lc_a[k][t]] );
        }
        snprintf( out, size, "%s", g->str );
        g_string_free( g, TRUE );
        }
        return 1;
    }
    }
    out[0] = 0;
    return 0;
}

/* LA PRIMERA ECUACION: la transferencia, EN NIVELES.
 *
 * El modelo dice que la transferencia relaciona LOS NIVELES y que la
 * diferenciacion la lleva el ruido -- esta escrito en drtran.c:51 y la bateria
 * lo comprueba (BUG-8: si el cast empotrado ajustara nu*Delta con Delta(1)=0,
 * la ganancia saldria aniquilada).
 *
 * Por eso aqui NO aparece ningun operador de diferencias: van en la segunda
 * ecuacion, que es la del ruido. Escribirlo todo junto sugeriria que se
 * diferencia la transferencia, y es justo lo contrario.              */
static gchar *ecuacion_nivel( Mtram *m, int i )
{
    GString *t = g_string_new( NULL );
    int      k, primero = 1, hay = 0;

    g_string_append_printf( t, "%s_t  =  ", nom_serie( m, i ) );

    for (k = 0; k < m->red.n; k++) {
        if (m->red.lnk[k].out != i) continue;
        g_string_append_printf( t, "%sν%d(B) %s_t", primero ? "" : " + ",
                                k + 1, nom_serie( m, m->red.lnk[k].inp ) );
        primero = 0;  hay = 1;
    }
    g_string_append_printf( t, "%sN_%s,t", hay ? "  +  " : "", nom_serie( m, i ) );
    return g_string_free( t, FALSE );
}

/* LA SEGUNDA ECUACION: el ruido, que ES donde va la diferenciacion.
 *
 *     phi(B) grad^d [ N_t - D_t ]  =  theta(B) a_t
 *
 * Viene entera del .pre y aqui no se toca. Se enseña porque sin ella el
 * modelo no esta escrito -- y porque su estructura y su cuenta SON la
 * informacion relevante de la parte univariante.                     */
static gchar *ecuacion_ruido( Mtram *m, int i, int ndet )
{
    GString  *t = g_string_new( NULL );
    NsopForm  o;
    char      pol[64];
    int       p1 = m->c.s[i - 1]->tm.NumAr1 + m->c.s[i - 1]->tm.NumAr2
                 + m->c.s[i - 1]->tm.NumAr1f;

    nsop_canon( m->c.s[i - 1]->tm.rnsop, m->c.s[i - 1]->tm.ornsop,
                m->c.s[i - 1]->tm.sper, &o );
    nsop_texto( &o, m->c.s[i - 1]->tm.sper, pol, sizeof pol );

    if (p1) g_string_append_printf( t, "φ_%s(B) ", nom_serie( m, i ) );
    if (o.d || o.D || o.nf) g_string_append_printf( t, "%s ", pol );

    if (ndet)
        g_string_append_printf( t, "[ N_%s,t − D_%s,t ]",
                                nom_serie( m, i ), nom_serie( m, i ) );
    else
        g_string_append_printf( t, "N_%s,t", nom_serie( m, i ) );

    g_string_append_printf( t, "  =  θ_%s(B) a_%s,t",
                            nom_serie( m, i ), nom_serie( m, i ) );
    return g_string_free( t, FALSE );
}

/* Cuantos slots de esta serie y de este prefijo, y cuantos libres. */
static void cuenta_pref( const SlotTable *st, const char *pref, int i,
                         int *n, int *libres )
{
    char p[64];
    int  k;

    *n = *libres = 0;
    snprintf( p, sizeof p, pref[0] == 'q' ? "%s" : "%s_%d[", pref, i );
    if (pref[0] == 'o') snprintf( p, sizeof p, "omega_d%d[", i );
    for (k = 1; k <= st->n; k++)
        if (!strncmp( st->name[k], p, strlen( p ) )) {
            (*n)++;
            if (st->kind[k] == SLOT_FREE) (*libres)++;
        }
}

static void refresca_lista( Mtram *m )
{
    Modelo       *M  = &m->mod;
    GtkTreeStore *st = GTK_TREE_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(M->lista) ) );
    GtkTreeIter   ec, enl, fila;
    char          dice[256], nom[80];
    int           i, k, kk;

    gtk_tree_store_clear( st );
    if (!M->vale) return;

    for (i = 1; i <= m->c.n; i++) {
        gchar *eq  = ecuacion_nivel( m, i );
        gchar *eqr;
        int    pa, pal, qa, qal, nd, ndl, nm, total;

        cuenta_pref( &M->st, "phi",     i, &pa, &pal );
        cuenta_pref( &M->st, "theta",   i, &qa, &qal );
        cuenta_pref( &M->st, "omega_d", i, &nd, &ndl );
        {
        char q[16];
        snprintf( q, sizeof q, "mu[%d]", i );
        nm = slots_find( &M->st, q ) ? 1 : 0;
        }
        total = pa + qa + nd + nm;

        /* LA PRIMERA ECUACION: la transferencia, en NIVELES. */
        gtk_tree_store_append( st, &ec, NULL );
        gtk_tree_store_set( st, &ec, M_NOMBRE, eq, M_IDX, 0, -1 );
        g_free( eq );

        /* Que es cada nu, y sus parametros: ESTO es lo que se especifica. */
        for (k = 0; k < m->red.n; k++) {
            gchar *nmb, *ords, *par;
            int    tot = 0, lib = 0;

            if (m->red.lnk[k].out != i) continue;

            for (kk = 1; kk <= M->st.n; kk++) {
                char p[32];

                snprintf( p, sizeof p, "omega%d[", k + 1 );
                if (strncmp( M->st.name[kk], p, strlen( p ) )) {
                    snprintf( p, sizeof p, "delta%d[", k + 1 );
                    if (strncmp( M->st.name[kk], p, strlen( p ) )) continue;
                }
                tot++;
                if (M->st.kind[kk] == SLOT_FREE) lib++;
            }

            /* nu = omega(B)/delta(B) B^b, escrito tal cual. */
            {
            GString *v = g_string_new( NULL );

            g_string_append_printf( v, "   ν%d = ω%d(B)", k + 1, k + 1 );
            if (m->red.lnk[k].r)
                g_string_append_printf( v, " / δ%d(B)", k + 1 );
            if (m->red.lnk[k].b)
                g_string_append_printf( v, " B^%d", m->red.lnk[k].b );
            g_string_append_printf( v, "      ← %s",
                                    nom_serie( m, m->red.lnk[k].inp ) );
            nmb = g_string_free( v, FALSE );
            }
            ords = g_strdup_printf( "b=%d  r=%d  s=%d", m->red.lnk[k].b,
                                    m->red.lnk[k].r, m->red.lnk[k].s );
            par  = g_strdup_printf( "%d par, %d libres", tot, lib );

            gtk_tree_store_append( st, &enl, &ec );
            gtk_tree_store_set( st, &enl, M_NOMBRE, nmb, M_QUE, ords,
                                M_DICE, par, M_IDX, -( k + 1 ), -1 );
            g_free( nmb ); g_free( ords ); g_free( par );

            for (kk = 1; kk <= M->st.n; kk++) {
                char p[32];

                snprintf( p, sizeof p, "omega%d[", k + 1 );
                if (strncmp( M->st.name[kk], p, strlen( p ) )) {
                    snprintf( p, sizeof p, "delta%d[", k + 1 );
                    if (strncmp( M->st.name[kk], p, strlen( p ) )) continue;
                }
                /* En el idioma del modelo, no en el del motor.
                 *
                 * Y SIN ETIQUETARLO DE NADA. Un producto como
                 * omega1[1] = omega1[0]*theta_2 es la forma EMPOTRADA --los
                 * coeficientes fuera de la diagonal de Theta(B) del VARMA son
                 * productos de parametros, LEGACY_M6.md §9-- y el analista
                 * que especifica una FLT elige (b, r, s), no productos.
                 * Ponerle "atado al ruido de EI" era vestir la maquina de
                 * modelo.                                             */
                {
                int   ruido = 0;
                gchar *nmr;

                /* Las del empotrado no son especificacion: van a su panel. */
                if (es_empotrado( &M->st, kk )) continue;

                if (!enunciado( m, kk, dice, sizeof dice, &ruido )) {
                    if (M->solo) continue;
                    snprintf( dice, sizeof dice, "libre" );
                }
                nombre_modelo( m, M->st.name[kk], nom, sizeof nom );
                nmr = g_strdup_printf( "      %s", dice );

                gtk_tree_store_append( st, &fila, &enl );
                gtk_tree_store_set( st, &fila,
                    M_NOMBRE, nmr,
                    M_QUE,    que_es( M->st.kind[kk] ),
                    M_DICE,   nom,
                    M_IDX,    kk, -1 );
                g_free( nmr );
                }
            }
        }

        /* LA SEGUNDA ECUACION: el ruido, que es DONDE VA LA DIFERENCIACION.
         * Viene entera del .pre y aqui no se toca: se enseña porque sin ella
         * el modelo no esta escrito.                                    */
        {
        gchar *estr, *par;

        eqr  = ecuacion_ruido( m, i, nd );
        estr = g_strdup_printf( "AR %d · MA %d · det %d · media %s",
                                pa, qa, nd, nm ? "libre" : "fija" );
        par  = g_strdup_printf( "%d par · de fue", total );

        gtk_tree_store_append( st, &enl, &ec );
        gtk_tree_store_set( st, &enl, M_NOMBRE, eqr, M_QUE, estr,
                            M_DICE, par, M_IDX, 0, -1 );
        g_free( eqr ); g_free( estr ); g_free( par );
        }
        (void) pal; (void) qal; (void) ndl;
    }

    /* --- Sigma ------------------------------------------------------- */
    {
    int    cl, ct;
    gchar *q;

    covarianzas( &M->st, &cl, &ct );
    q = g_strdup_printf( "%d de %d covarianzas libres", cl, ct );

    gtk_tree_store_append( st, &ec, NULL );
    gtk_tree_store_set( st, &ec, M_NOMBRE, "Σ   (innovaciones)",
                        M_QUE, q, M_IDX, 0, -1 );
    g_free( q );

    for (k = 1; k <= M->st.n; k++) {
        if (strncmp( M->st.name[k], "q[", 2 )) continue;
        {
        int ruido = 0;

        if (!enunciado( m, k, dice, sizeof dice, &ruido )) {
            if (M->solo) continue;
            snprintf( dice, sizeof dice, "fija en 0" );
        }
        /* q[i,j] se lee mejor con los nombres de las dos series. */
        {
        int a1, a2;

        if (sscanf( M->st.name[k], "q[%d,%d]", &a1, &a2 ) == 2)
            snprintf( nom, sizeof nom, "   q(%s, %s)",
                      nom_serie( m, a1 ), nom_serie( m, a2 ) );
        else snprintf( nom, sizeof nom, "   %s", M->st.name[k] );
        }
        gtk_tree_store_append( st, &fila, &ec );
        gtk_tree_store_set( st, &fila,
            M_NOMBRE, nom,
            M_QUE,    que_es( M->st.kind[k] ),
            M_DICE,   dice,
            M_IDX,    k, -1 );
        }
    }
    }

    gtk_tree_view_expand_all( GTK_TREE_VIEW(M->lista) );
}

/* ------------------------------------------------------------------------ */
/* Los dos veredictos                                                        */
/*                                                                           */
/* El primero contesta la pregunta que la pagina existe para contestar: QUE   */
/* PARTE DEL MODELO SE ESTA MODELIZANDO AQUI.                                */
/* ------------------------------------------------------------------------ */

static void refresca_cuenta( Mtram *m )
{
    Modelo *M = &m->mod;
    int     libres, cl, ct, mal, primero = 0;
    int     tr = 0, k;

    if (!M->vale) {
        mtram_verdicto( M->ver_cuenta, MT_AMBAR,
            "Carga las series y define la red: los parámetros salen de ahí, "
            "no se escriben." );
        gtk_label_set_text( GTK_LABEL(M->ver_ojo), "" );
        return;
    }

    libres = slots_nfree( &M->st );
    covarianzas( &M->st, &cl, &ct );
    mal = colineales( m, &primero );

    for (k = 1; k <= M->st.n; k++)
        if (!strncmp( M->st.name[k], "omega", 5 ) &&
            strncmp( M->st.name[k], "omega_d", 7 )) tr++;
        else if (!strncmp( M->st.name[k], "delta", 5 ) &&
                 strncmp( M->st.name[k], "delta_d", 7 )) tr++;

    {
    int emp = n_empotradas( &M->st );

    if (emp)
        mtram_verdicto( M->ver_cuenta, MT_VERDE,
            "%d de transferencia + %d covarianzas libres — esto es "
            "lo que se decide aquí · los otros %d vienen de los .pre "
            "· %d restricción%s del empotrado, del .cns",
            tr, cl, M->st.n - tr - ct, emp, emp == 1 ? "" : "es" );
    else
        mtram_verdicto( M->ver_cuenta, MT_VERDE,
            "%d de transferencia + %d covarianzas libres — esto es lo "
            "que se decide aquí · los otros %d vienen de los .pre y de "
            "juntarlos",
            tr, cl, M->st.n - tr - ct );
    }

    if (mal)
        mtram_verdicto( M->ver_ojo, MT_ROJO,
            "OJO — %s ← %s es contemporáneo (b=0) y su covarianza está libre%s",
            nom_serie( m, m->red.lnk[primero].out ),
            nom_serie( m, m->red.lnk[primero].inp ),
            mal > 1 ? " · y no es el único" : "" );
    else if (M->perdidas)
        mtram_verdicto( M->ver_ojo, MT_AMBAR,
            "%d restricción%s se quedó%s por el camino: nombraba%s un "
            "parámetro que este modelo ya no tiene",
            M->perdidas, M->perdidas == 1 ? "" : "es",
            M->perdidas == 1 ? "" : "n", M->perdidas == 1 ? "" : "n" );
    else
        mtram_verdicto( M->ver_ojo, MT_VERDE,
            "%d parámetros en total, %d libres", M->st.n, libres );
}

void modelo_refresca( Mtram *m )
{
    construye( m );
    refresca_lista( m );
    refresca_cuenta( m );
}

/* ------------------------------------------------------------------------ */
/* Cambiar lo que dice un slot                                               */
/* ------------------------------------------------------------------------ */

static int slot_marcado( Mtram *m )
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(
                                GTK_TREE_VIEW(m->mod.lista) );
    GtkTreeModel *mod;
    GtkTreeIter   it;
    int           i = 0;

    if (!gtk_tree_selection_get_selected( sel, &mod, &it )) return 0;
    gtk_tree_model_get( mod, &it, M_IDX, &i, -1 );
    return i;
}

static void on_libre( GtkButton *b, Mtram *m )
{
    int i = slot_marcado( m );

    if (!i) { preview_show_status( m, "Marca primero un parámetro." ); return; }
    m->mod.st.kind[i]  = SLOT_FREE;
    m->mod.st.alias[i] = 0;
    modelo_refresca( m );
    preview_show_status( m, "%s queda libre: se estima.", m->mod.st.name[i] );
}

static void on_fijar( GtkButton *b, Mtram *m )
{
    int        i = slot_marcado( m );
    GtkWidget *d, *e, *caja, *av;
    double     v = 0.0;

    if (!i) { preview_show_status( m, "Marca primero un parámetro." ); return; }

    d = gtk_dialog_new_with_buttons( "Fijar un parámetro",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Fijar", GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    gtk_container_add( GTK_CONTAINER(caja),
        gtk_label_new( m->mod.st.name[i] ) );
    e = gtk_entry_new();
    gtk_entry_set_text( GTK_ENTRY(e),
        m->mod.st.kind[i] == SLOT_FIXED ? g_strdup_printf( "%g",
            (double) m->mod.st.value[i] ) : "0" );
    gtk_entry_set_activates_default( GTK_ENTRY(e), TRUE );
    gtk_container_add( GTK_CONTAINER(caja), e );

    av = gtk_label_new( "Un parámetro fijo es ESPECIFICACIÓN, no una semilla:\n"
                        "no se estima, y su valor es parte de lo que el modelo\n"
                        "afirma." );
    gtk_container_add( GTK_CONTAINER(caja), av );

    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_ACCEPT );
    gtk_widget_show_all( d );

    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        v = g_ascii_strtod( gtk_entry_get_text( GTK_ENTRY(e) ), NULL );
        m->mod.st.kind[i]  = SLOT_FIXED;
        m->mod.st.value[i] = v;
        gtk_widget_destroy( d );
        modelo_refresca( m );
        preview_show_status( m, "%s = %g, fijo.", m->mod.st.name[i], v );
        return;
    }
    gtk_widget_destroy( d );
}

/* Compartir: un grado de libertad en dos sitios. Sólo se ofrecen los slots que
 * de verdad hay, que es la mitad de la gracia de tener la tabla.          */
static void on_compartir( GtkButton *b, Mtram *m )
{
    int        i = slot_marcado( m );
    GtkWidget *d, *cb, *caja;
    int        k, n = 0;
    int       *idx;

    if (!i) { preview_show_status( m, "Marca primero un parámetro." ); return; }

    idx = g_new0( int, m->mod.st.n + 1 );
    d = gtk_dialog_new_with_buttons( "Compartir con otro parámetro",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Compartir", GTK_RESPONSE_ACCEPT,
            NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    gtk_container_add( GTK_CONTAINER(caja), gtk_label_new(
        g_strdup_printf( "%s  =", m->mod.st.name[i] ) ) );

    cb = gtk_combo_box_text_new();
    for (k = 1; k <= m->mod.st.n; k++) {
        if (k == i) continue;
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(cb),
                                        m->mod.st.name[k] );
        idx[n++] = k;
    }
    if (n) gtk_combo_box_set_active( GTK_COMBO_BOX(cb), 0 );
    gtk_container_add( GTK_CONTAINER(caja), cb );

    gtk_container_add( GTK_CONTAINER(caja), gtk_label_new(
        "Los dos pasan a ser el MISMO parámetro: un solo grado de\n"
        "libertad, estimado una vez y usado en dos sitios." ) );

    gtk_widget_show_all( d );
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT && n) {
        int a = gtk_combo_box_get_active( GTK_COMBO_BOX(cb) );
        int j = idx[a];

        /* seguir la cadena hasta el representante, como hace el motor */
        while (m->mod.st.kind[j] == SLOT_ALIAS) j = m->mod.st.alias[j];
        if (j == i)
            preview_show_status( m, "%s no puede compartirse consigo mismo.",
                                 m->mod.st.name[i] );
        else {
            m->mod.st.kind[i]  = SLOT_ALIAS;
            m->mod.st.alias[i] = j;
            preview_show_status( m, "%s y %s son ahora el mismo parámetro.",
                                 m->mod.st.name[i], m->mod.st.name[j] );
        }
    }
    gtk_widget_destroy( d );
    g_free( idx );
    modelo_refresca( m );
}

/* ------------------------------------------------------------------------ */
/* Sigma, como matriz y EDITABLE                                             */
/*                                                                           */
/* Sigma ES una matriz, y verla como matriz es ver su estructura de un golpe. */
/* Y aqui se pulsa, al reves que la matriz de operadores de la pagina Series: */
/* aquella es de solo lectura porque el operador viene del .pre y no se       */
/* decide ahi; esta SI es una decision del analista, asi que se decide donde  */
/* se ve. Liberar covarianzas es el uso mas comun del .cns --el m6-1 libera   */
/* tres de quince-- y buscarlas entre 67 renglones es absurdo.                */
/*                                                                           */
/* Va en DIALOGO y no en panel: editar Sigma es un acto deliberado y conviene */
/* poder cancelarlo entero. Un panel que se cierra al perder el foco no es    */
/* sitio para varios clics seguidos.                                          */
/* ------------------------------------------------------------------------ */

/* Lo que el dialogo necesita saber para avisar mientras se pulsa. */
typedef struct {
    Mtram     *m;
    GtkWidget *aviso;
    GtkWidget *bot[NET_MAX_SER + 1][NET_MAX_SER + 1];
} CovDlg;

/* AVISAR SI, BLOQUEAR NO.
 *
 * Un enlace contemporaneo (b=0) con su covarianza libre explica lo mismo dos
 * veces en k=0. Es tentador impedirlo, y seria un error por dos razones.
 *
 * LA PRIMERA es que NO es una especificacion incompatible. El motor lo llama
 * "near-collinearity" --near-- y dice que las dos cosas se separan por como
 * decae la covarianza cruzada en k>0: phi_X^k la transferencia, phi_N^k la
 * covarianza. La cresta plana aparece CUANDO los dos AR se parecen; si
 * difieren, el modelo esta identificado y tener las dos es legitimo.
 *
 * LA SEGUNDA es que el motor LO ESTIMA y avisa. Si mtram lo prohibiera, el GUI
 * y el motor discreparian sobre que es admisible -- y la regla de todo este
 * programa es la contraria: lo que mtram acepte es lo que acepta drtran.
 *
 * Asi que se avisa EN EL MOMENTO de pulsar, que es cuando sirve, y se deja
 * hacer. Es ademas la regla que salio de TASTE: nada se grisa.          */
static void cov_revisa( CovDlg *c )
{
    Mtram   *m = c->m;
    GString *t = g_string_new( NULL );
    int      k, n = 0;

    for (k = 0; k < m->red.n; k++) {
        int o = m->red.lnk[k].out, e = m->red.lnk[k].inp;
        int i = o > e ? o : e, j = o > e ? e : o;
        GtkWidget *b = c->bot[i][j];

        if (m->red.lnk[k].b != 0 || !b) continue;
        if (!gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(b) )) continue;

        if (!n++)
            g_string_append( t, "OJO — la interacción contemporánea está "
                                "especificada DOS VECES:\n" );
        g_string_append_printf( t,
            "   %s ← %s tiene b=0 y q(%s, %s) libre\n",
            nom_serie( m, o ), nom_serie( m, e ),
            nom_serie( m, i ), nom_serie( m, j ) );
    }

    if (n)
        g_string_append( t,
            "\nEn k = 0 las dos cosas explican lo mismo. Sólo se separan por "
            "cómo decae la\ncovarianza cruzada en k > 0 — φ_X^k la "
            "transferencia, φ_N^k la covarianza — así\nque si los dos AR se "
            "parecen, la verosimilitud tiene una cresta casi plana.\n"
            "La doctrina de la escuela es usar UNA de las dos, no las dos.\n\n"
            "No se impide: el motor lo estima y avisa, y si los AR difieren el "
            "modelo está\nidentificado. Pero conviene saberlo antes." );

    gtk_label_set_text( GTK_LABEL(c->aviso), t->str );
    gtk_widget_set_visible( c->aviso, n > 0 );
    g_string_free( t, TRUE );
}

/* El rotulo tiene que cambiar al pulsar. Sin esto el boton se hunde --que es
 * un cambio casi invisible-- y sigue poniendo "0", asi que parece que no hace
 * nada aunque el valor SI quede guardado. Es el fallo de dar por hecho que el
 * estado de un GtkToggleButton se ve.                                    */
static void on_cov_pulsa( GtkToggleButton *b, CovDlg *c )
{
    gtk_button_set_label( GTK_BUTTON(b),
        gtk_toggle_button_get_active( b ) ? "libre" : "0" );
    cov_revisa( c );
}

static void on_covarianzas( GtkButton *bt, Mtram *m )
{
    Modelo    *M = &m->mod;
    GtkWidget *d, *caja, *rej, *av;
    CovDlg     c;
    int        i, j, n = m->c.n;

    memset( &c, 0, sizeof c );
    c.m = m;
#define bot c.bot

    if (!M->vale) {
        preview_show_status( m, "Carga las series y define la red primero." );
        return;
    }

    d = gtk_dialog_new_with_buttons( "Covarianzas de las innovaciones",
            GTK_WINDOW(m->ventana_p),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL,
            "_Aceptar",  GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    gtk_box_set_spacing( GTK_BOX(caja), 10 );

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 3 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 3 );
    gtk_container_add( GTK_CONTAINER(caja), rej );

    /* Solo por debajo de la diagonal: q[i,j] con i > j, que es como las
     * nombra el motor y como las cuenta -- n(n-1)/2.                    */
    for (j = 1; j < n; j++) {
        GtkWidget *h = gtk_label_new( NULL );
        gchar     *mk = g_strdup_printf( "<b>%s</b>",
                            m->c.s[j - 1]->ts.name ? m->c.s[j - 1]->ts.name : "?" );

        gtk_label_set_markup( GTK_LABEL(h), mk );
        g_free( mk );
        gtk_grid_attach( GTK_GRID(rej), h, j, 0, 1, 1 );
    }

    for (i = 2; i <= n; i++) {
        GtkWidget *h = gtk_label_new( NULL );
        gchar     *mk = g_strdup_printf( "<b>%d %s</b>", i,
                            m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "?" );

        gtk_label_set_markup( GTK_LABEL(h), mk );
        g_free( mk );
        gtk_widget_set_halign( h, GTK_ALIGN_START );
        gtk_grid_attach( GTK_GRID(rej), h, 0, i - 1, 1, 1 );

        for (j = 1; j < i; j++) {
            char nm[40];
            int  k;

            snprintf( nm, sizeof nm, "q[%d,%d]", i, j );
            k = slots_find( &M->st, nm );
            if (!k) { bot[i][j] = NULL; continue; }

            bot[i][j] = gtk_toggle_button_new_with_label(
                            M->st.kind[k] == SLOT_FREE ? "libre" : "0" );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(bot[i][j]),
                                          M->st.kind[k] == SLOT_FREE );
            g_signal_connect( bot[i][j], "toggled",
                              G_CALLBACK(on_cov_pulsa), &c );
            gtk_widget_set_tooltip_text( bot[i][j], nm );
            gtk_widget_set_size_request( bot[i][j], 60, -1 );
            gtk_grid_attach( GTK_GRID(rej), bot[i][j], j, i - 1, 1, 1 );
        }
    }

    av = gtk_label_new(
        "Pulsa una casilla para pasarla de 0 a libre y al revés.\n\n"
        "Las covarianzas nacen FIJAS en cero: la diagonal es el caso por "
        "defecto y\nliberar una es una decisión de modelo, no un interruptor. "
        "El m6-1 no libera\nlas quince de su sistema: libera tres.\n\n"
        "Es, junto con los órdenes (b, r, s), lo único que se decide en esta "
        "etapa.\nY hay una cautela: si un enlace es CONTEMPORÁNEO (b=0) y su "
        "covarianza\nestá libre, las dos cosas explican lo mismo en k = 0 — "
        "usa una, no las dos.\nEl panel «Avisos…» lo dice con los números." );
    gtk_widget_set_halign( av, GTK_ALIGN_START );
    gtk_container_add( GTK_CONTAINER(caja), av );

    gtk_widget_show_all( d );

    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        int cambios = 0;

        for (i = 2; i <= n; i++)
            for (j = 1; j < i; j++) {
                char nm[40];
                int  k, quiere;

                if (!bot[i][j]) continue;
                snprintf( nm, sizeof nm, "q[%d,%d]", i, j );
                k = slots_find( &M->st, nm );
                if (!k) continue;

                quiere = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(bot[i][j]) );
                if (quiere && M->st.kind[k] != SLOT_FREE) {
                    M->st.kind[k] = SLOT_FREE;  M->st.alias[k] = 0;  cambios++;
                } else if (!quiere && M->st.kind[k] != SLOT_FIXED) {
                    M->st.kind[k] = SLOT_FIXED; M->st.value[k] = 0.0; cambios++;
                }
            }
        gtk_widget_destroy( d );

        if (cambios) {
            refresca_lista( m );
            refresca_cuenta( m );
            preview_show_status( m, "%d covarianza%s cambiada%s.",
                                 cambios, cambios == 1 ? "" : "s",
                                 cambios == 1 ? "" : "s" );
        }
        return;
    }
    gtk_widget_destroy( d );
#undef bot
}

/* ------------------------------------------------------------------------ */
/* Lo que viene del .cns y NO es especificacion                              */
/* ------------------------------------------------------------------------ */

static int n_empotradas( const SlotTable *st )
{
    int k, n = 0;

    for (k = 1; k <= st->n; k++) if (es_empotrado( st, k )) n++;
    return n;
}

static void on_empotrado( GtkButton *b, Mtram *m )
{
    Modelo  *M = &m->mod;
    GString *t = g_string_new( NULL );
    char     dice[256], nom[80];
    int      k, n = 0, ruido;

    if (!M->vale) {
        g_string_append( t, "Carga las series y define la red." );
        goto pinta;
    }

    for (k = 1; k <= M->st.n; k++) {
        if (!es_empotrado( &M->st, k )) continue;
        if (!n++)
            g_string_append( t, "RESTRICCIONES DE LA FORMA EMPOTRADA\n\n" );

        nombre_modelo( m, M->st.name[k], nom, sizeof nom );
        enunciado( m, k, dice, sizeof dice, &ruido );
        g_string_append_printf( t, "   %-14s %s\n", nom, dice );
        g_string_append_printf( t, "   %-14s   en el .cns:  %s\n\n", "",
                                M->st.name[k] );
    }

    if (!n) {
        g_string_append( t,
            "Ninguna.\n\n"
            "Aquí saldrían las restricciones del .cns que NO son una\n"
            "afirmación sobre ν(B) sino la forma en que el modelo se empotra\n"
            "en el VARMA: los PRODUCTOS de dos parámetros." );
        goto pinta;
    }

    g_string_append( t,
        "Éstas NO son especificación de la función de transferencia.\n\n"
        "Un analista que especifica una FLT elige (b, r, s). Los productos de\n"
        "dos parámetros aparecen cuando esa FLT se escribe como un VARMA\n"
        "RESTRINGIDO: son los coeficientes de fuera de la diagonal de Θ(B).\n\n"
        "Está documentado en LEGACY_M6.md §9 — los coeficientes fuera de la\n"
        "diagonal del m6-1 son x5*x6, x12*x14−x13, x2*x3*x4, y al factorizarlos\n"
        "salen los numeradores. El mecanismo PRODUCTO del .cns se añadió para\n"
        "poder reproducir ese VARMA, no para especificar una transferencia.\n\n"
        "Vienen del fichero y SE RESPETAN: quitan grados de libertad y el\n"
        "recuento las cuenta. Pero no se ofrecen para editar aquí, y no se\n"
        "mezclan con la especificación.\n\n"
        "Lo que SÍ es afirmación sobre ν(B) se queda en el árbol:\n"
        "   ω(1) = 0            un (1−B) fijo: ganancia a largo plazo cero\n"
        "   δ = φ de la entrada  el denominador ES el AR de la entrada" );

pinta:
    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
    g_string_free( t, TRUE );
}

/* ------------------------------------------------------------------------ */
/* Los avisos                                                                */
/* ------------------------------------------------------------------------ */

static void on_avisos( GtkButton *b, Mtram *m )
{
    Modelo  *M = &m->mod;
    GString *t = g_string_new( NULL );
    int      k, n = 0;

    if (!M->vale) {
        g_string_append( t, "Carga las series y define la red." );
        goto pinta;
    }

    for (k = 0; k < m->red.n; k++) {
        char nm[40];
        int  s1, s2;

        if (m->red.lnk[k].b != 0) continue;
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].out, m->red.lnk[k].inp );
        s1 = slots_find( &M->st, nm );
        snprintf( nm, sizeof nm, "q[%d,%d]", m->red.lnk[k].inp, m->red.lnk[k].out );
        s2 = slots_find( &M->st, nm );

        if ((s1 && M->st.kind[s1] == SLOT_FREE) ||
            (s2 && M->st.kind[s2] == SLOT_FREE)) {
            if (!n++)
                g_string_append( t, "CASI-COLINEALIDAD\n\n" );
            g_string_append_printf( t, "   %s ← %s  es contemporáneo (b=0) y "
                "su covarianza está libre\n",
                m->c.s[m->red.lnk[k].out - 1]->ts.name,
                m->c.s[m->red.lnk[k].inp - 1]->ts.name );
        }
    }

    if (!n) {
        g_string_append( t, "Ningún aviso.\n\n"
            "Aquí saldría la casi-colinealidad: un enlace contemporáneo (b=0)\n"
            "con su covarianza de innovaciones libre al mismo tiempo." );
        goto pinta;
    }

    g_string_append( t,
        "\nEn el retardo k = 0 las dos cosas explican EXACTAMENTE LO MISMO.\n"
        "Sólo se separan por cómo decae la covarianza cruzada en k > 0:\n"
        "   phi_X^k   si es transferencia\n"
        "   phi_N^k   si es covarianza\n\n"
        "Cuando los dos AR se parecen, la verosimilitud tiene una cresta casi\n"
        "plana: el ajuste apenas mejora mientras omega y la correlación se van\n"
        "a una esquina. Medido en IPC<-WTI (phi_X=0.30, phi_N=0.40): LR = 0.03,\n"
        "la correlación se va a -0.98, omega se multiplica por 9 y los t-ratios\n"
        "llegan a 2424.\n\n"
        "LA DOCTRINA DE LA ESCUELA ES USAR UNA DE LAS DOS, NO LAS DOS.\n"
        "El m6-1 tiene covarianzas fuera de la diagonal y NINGUNA estructura\n"
        "contemporánea. La tesis de Muñoz Polo (2001, §2.4) dice que la\n"
        "especificación de una relación bivariante «puede comenzar con la\n"
        "modificación de la matriz Sigma».\n\n"
        "El motor da este aviso DESPUÉS de estimar. Aquí se da antes, que es\n"
        "cuando sirve." );

pinta:
    mtram_popover_mostrar( GTK_WIDGET(b), t->str );
    g_string_free( t, TRUE );
}

/* ------------------------------------------------------------------------ */
/* Abrir y guardar                                                           */
/* ------------------------------------------------------------------------ */

static gchar *elige( Mtram *m, GtkFileChooserAction accion, const char *titulo )
{
    GtkWidget     *d;
    GtkFileFilter *f;
    gchar         *p = NULL;

    d = gtk_file_chooser_dialog_new( titulo, GTK_WINDOW(m->ventana_p), accion,
            "_Cancelar", GTK_RESPONSE_CANCEL,
            accion == GTK_FILE_CHOOSER_ACTION_SAVE ? "_Guardar" : "_Abrir",
            GTK_RESPONSE_ACCEPT, NULL );
    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "Restricciones (*.cns)" );
    gtk_file_filter_add_pattern( f, "*.cns" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    if (accion == GTK_FILE_CHOOSER_ACTION_SAVE) {
        gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), "modelo.cns" );
    }
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT)
        p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
    gtk_widget_destroy( d );
    return p;
}

/* La frase de mtram. El motor tiene la suya, en inglés (cns_error_en); el
 * hecho es el mismo porque viene del mismo lector.                        */
static void por_que( Mtram *m, const CnsError *e, const char *path )
{
    gchar *base = g_path_get_basename( path );

    switch (e->err) {
    case CNS_ENOFILE:
        preview_show_status( m, "No puedo abrir %s.", base ); break;
    case CNS_EUNKNOWN:
        preview_show_status( m, "%s, línea %d: este modelo no tiene ningún "
                                "parámetro «%s». Mira la lista: los nombres "
                                "los pone el modelo, no se eligen.",
                             base, e->line, e->token ); break;
    case CNS_EOPERAND:
        preview_show_status( m, "%s, línea %d: en «%s» hay un operando que no "
                                "es un parámetro de este modelo.",
                             base, e->line, e->token ); break;
    case CNS_ESELF:
        preview_show_status( m, "%s, línea %d: «%s» no puede definirse en "
                                "función de sí mismo.",
                             base, e->line, e->lhs ); break;
    case CNS_ELC:
        preview_show_status( m, "%s, línea %d: no entiendo la combinación "
                                "lineal «%s». Es  a + b - c,  con cada término "
                                "un parámetro o un producto de dos.",
                             base, e->line, e->token ); break;
    case CNS_EPARSE:
        preview_show_status( m, "%s, línea %d: no entiendo «%s = %s». A la "
                                "derecha va: free, un número, otro parámetro, "
                                "a * b, o una suma.",
                             base, e->line, e->lhs, e->token ); break;
    case CNS_OK:
        break;
    }
    g_free( base );
}

/* Las q[i,j] nombran a las series POR SU POSICION, y esa posicion no esta en
 * el fichero: esta en la linea de ordenes. Por eso los .cns de la escuela la
 * escriben en un comentario, y por eso conviene compararla.
 *
 * No es hipotetico: en el m6, m6.cns espera 1=P 2=EA 3=EP... y m6_net.cns
 * espera 1=EP 2=EI 3=EU... SON DISTINTOS, y abrir uno con el orden del otro
 * aplica las covarianzas a parejas que no son -- sin que nada lo diga, porque
 * el .cns solo lleva numeros.
 *
 * Es UN AVISO. No reordena nada: un comentario no manda sobre el analista.  */
static void avisa_orden( Mtram *m, const char *path )
{
    char     dice[64][SLOT_NAME];
    int      n = cns_orden_declarado( path, dice, 64 );
    GString *t;
    int      i, mal = 0;

    if ( n == 0 ) return;                 /* no lo declara: nada que decir */

    for ( i = 1; i <= n && i <= m->c.n; i++ )
        if ( g_ascii_strcasecmp( dice[i],
                 m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "" ) )
            mal++;

    if ( !mal && n == m->c.n ) return;     /* coincide: callarse */

    t = g_string_new( NULL );
    g_string_append_printf( t, "OJO — %s dice que espera este orden:\n   ",
                            g_path_get_basename( path ) );
    for ( i = 1; i <= n; i++ )
        g_string_append_printf( t, "%d=%s  ", i, dice[i] );

    g_string_append( t, "\n\ny las series cargadas van en este:\n   " );
    for ( i = 0; i < m->c.n; i++ )
        g_string_append_printf( t, "%d=%s  ", i + 1,
            m->c.s[i]->ts.name ? m->c.s[i]->ts.name : "?" );

    g_string_append( t,
        "\n\nLas q[i,j] nombran a las series por su POSICIÓN, así que con "
        "otro orden\nse aplican a parejas distintas de las que el fichero "
        "quería — y nada lo dice,\nporque el .cns sólo lleva números.\n\n"
        "Reordena en la pestaña Series si el fichero tiene razón. No se toca "
        "nada solo:\nun comentario no manda sobre el analista." );

    {
    GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(m->ventana_p),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, "%s", t->str );

    gtk_window_set_title( GTK_WINDOW(d), "El orden de las series" );
    gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
    }
    g_string_free( t, TRUE );
}

static void on_abrir( GtkButton *b, Mtram *m )
{
    CnsError e;
    gchar   *p;
    int      nc;

    if (!m->mod.vale) {
        preview_show_status( m, "Carga las series y define la red primero: el "
                                ".cns nombra parámetros que salen de ahí." );
        return;
    }

    p = elige( m, GTK_FILE_CHOOSER_ACTION_OPEN, "Abrir restricciones" );
    if (!p) return;

    construye( m );                       /* partir de la tabla limpia */
    nc = cns_read( p, &m->mod.st, &e );
    if (nc < 0) {
        por_que( m, &e, p );
        construye( m );                   /* no dejar media aplicada   */
    } else {
        g_free( m->mod.path );
        m->mod.path = g_strdup( p );
        preview_show_status( m, "%d restricción%s de %s.",
                             nc, nc == 1 ? "" : "es", g_path_get_basename( p ) );
        avisa_orden( m, p );
    }
    refresca_lista( m );
    refresca_cuenta( m );
    g_free( p );
}

static void on_guardar( GtkButton *b, Mtram *m )
{
    gchar *p;
    int    n;

    if (!m->mod.vale) return;

    p = elige( m, GTK_FILE_CHOOSER_ACTION_SAVE, "Guardar las restricciones" );
    if (!p) return;

    n = cns_write( p, &m->mod.st, "restricciones escritas por mtram" );
    if (n < 0)
        preview_show_status( m, "No pude escribir %s.", p );
    else {
        g_free( m->mod.path );
        m->mod.path = g_strdup( p );
        preview_show_status( m, "%d línea%s en %s. El motor lo lee con  -c %s",
                             n, n == 1 ? "" : "s", g_path_get_basename( p ),
                             g_path_get_basename( p ) );
    }
    g_free( p );
}

/* ------------------------------------------------------------------------ */

static void on_solo( GtkToggleButton *b, Mtram *m )
{
    m->mod.solo = gtk_toggle_button_get_active( b );
    refresca_lista( m );
}

static void columna( GtkWidget *tv, const char *titulo, int col )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                               titulo, r, "text", col, NULL );

    gtk_tree_view_column_set_resizable( c, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), c );
}

GtkWidget *modelo_pagina_new( Mtram *m )
{
    Modelo       *M = &m->mod;
    GtkWidget    *caja, *barra, *b, *sc, *vb;
    GtkTreeStore *store;

    M->vale = FALSE;
    M->path = NULL;
    M->st.n = 0;
    M->solo = FALSE;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

#define BOTON(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON( "Libre", on_libre,
           "Se estima. Las covarianzas q[i,j] nacen fijas en cero: liberarlas "
           "es una decisión." )
    BOTON( "Fijar…", on_fijar,
           "Un parámetro fijo es ESPECIFICACIÓN, no una semilla: no se estima." )
    BOTON( "Compartir…", on_compartir,
           "Un solo grado de libertad, estimado una vez y usado en dos sitios." )
    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );
    BOTON( "Abrir…", on_abrir,
           "Un .cns. Se lee con el lector del motor: lo que mtram acepte es "
           "lo que acepta drtran." )
    BOTON( "Guardar…", on_guardar, "El fichero que el motor lee con -c." )
#undef BOTON

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    /* Lo que el .cns CONTIENE son los slots que dicen algo: en el m6, seis de
     * 67. Es la vista que casi siempre se quiere, porque es el fichero que se
     * va a escribir.                                                     */
    M->c_solo = gtk_check_button_new_with_label( "Sólo lo restringido" );
    gtk_widget_set_tooltip_text( M->c_solo,
        "Sólo los parámetros que dicen algo — que es lo que el .cns contiene. "
        "En el m6 son 6 de 67." );
    g_signal_connect( M->c_solo, "toggled", G_CALLBACK(on_solo), m );
    gtk_box_pack_start( GTK_BOX(barra), M->c_solo, FALSE, FALSE, 0 );

    /* A la derecha, los dos que ABREN algo. */
#define BOTON_DER(txt, fn, tip) \
    b = gtk_button_new_with_label( txt ); \
    gtk_widget_set_tooltip_text( b, tip ); \
    g_signal_connect( b, "clicked", G_CALLBACK(fn), m ); \
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    BOTON_DER( "Empotrado…", on_empotrado,
               "Lo que el .cns trae y NO es especificación: los productos de "
               "parámetros, que son la forma en que el modelo se empotra en el "
               "VARMA. Se respetan, pero no se editan aquí." )
    BOTON_DER( "Avisos…", on_avisos,
               "La casi-colinealidad: un enlace contemporáneo y su covarianza "
               "libre explican lo mismo dos veces." )
    BOTON_DER( "Covarianzas…", on_covarianzas,
               "Σ como matriz, y se pulsa para liberar o fijar. Es el uso más "
               "común del .cns." )
#undef BOTON_DER

    /* Un ARBOL, no una lista: 67 renglones heterogeneos en una lista plana no
     * se recorren. Plegado son seis filas.                              */
    store = gtk_tree_store_new( M_N, G_TYPE_STRING, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_INT );
    M->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) );
    columna( M->lista, "El modelo",  M_NOMBRE );
    columna( M->lista, "Es",        M_QUE );
    columna( M->lista, "Parámetro",  M_DICE );
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(M->lista), M_NOMBRE );
    gtk_tree_view_set_enable_tree_lines( GTK_TREE_VIEW(M->lista), TRUE );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), M->lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    /* Dos lineas de altura fija. El segundo elige que contar. */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_widget_set_margin_top( vb, 2 );

    M->ver_cuenta = gtk_label_new( "Carga las series y define la red." );
    M->ver_ojo    = gtk_label_new( "" );
    gtk_widget_set_halign( M->ver_cuenta, GTK_ALIGN_START );
    gtk_widget_set_halign( M->ver_ojo,    GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(M->ver_cuenta), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(M->ver_ojo),    PANGO_ELLIPSIZE_END );

    gtk_box_pack_start( GTK_BOX(vb), M->ver_cuenta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), M->ver_ojo,    FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), vb, FALSE, FALSE, 0 );

    return caja;
}
