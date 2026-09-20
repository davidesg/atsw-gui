/*
 * diagnosis.c -- la pantalla de diagnosis.
 *
 * DIAGNOSTICAR ES IDENTIFICAR OTRA VEZ. Es el hallazgo del estudio de TASTE, y
 * alli estaba dicho con la estructura del codigo: DiagnTFI (TFEST.PAS:55-62) es
 * literalmente la misma secuencia que la identificacion univariante
 * (USID.PAS:242-251) con 'RESIDUOS' en lugar de la serie.
 *
 * AQUI NO SE CALCULA NADA. El motor ya hace la diagnosis entera. Lo que falta
 * es que 2000 lineas de .out se conviertan en algo con lo que se pueda decidir.
 *
 * SEIS PESTAÑAS, EN EL ORDEN DE LAS PREGUNTAS, que va de lo que INVALIDA el
 * modelo a lo que lo matiza. Si la exogeneidad falla, las otras cinco no
 * importan.
 *
 * Y DOS COSAS QUE LA INTERFAZ NO DISTINGUIA:
 *
 * 1. LA ACICLICIDAD DE RED NO ES LA EXOGENEIDAD DE AQUI. Red dice que el .dag
 *    ADMITE orden de construccion: es estructural y se sabe antes de estimar.
 *    Esta pagina dice que LOS DATOS sostienen que la entrada no responde a la
 *    salida: es empirico y se sabe despues. Un .dag puede ser perfectamente
 *    aciclico y los datos decir que no lo es -- y eso es exactamente "esto
 *    deberia ser un VARMA".
 *
 * 2. EL R² SI TIENE SENTIDO, Y ES EL DE BRAJIN (A.28). Lo que no lo tiene es
 *    un R² sobre el NIVEL de una I(1), que sale cerca de 1 por construccion.
 *    Sobre la serie ESTACIONARIA si dice algo, y es la tercera de las tres
 *    cifras con que la escuela cierra cada caso. Ver lib/gof.
 *
 * Los residuos son los de ESTA corrida. TASTE tenia UNA ranura
 * (TASTECTV.PAS:475) y por eso no se podian comparar dos modelos.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "gui.h"
#include "previewhost.h"
#include "preview.h"
#include "outdiag.h"
#include "gof.h"
#include "tabla.h"
#include "netfile.h"
#include "fugplot.h"
#include "ccfplot.h"

#define NDIAG_LAGS 24        /* los retardos de la CCF de residuos          */

enum { X_ENL, X_Q, X_P, X_SIG, X_VER, X_N };
enum { A_ENL, A_Q, A_P, A_VER, A_N };
enum { J_ECU, J_DTU, J_DTT, J_R2U, J_R2T, J_RED, J_NOTA, J_N };
enum { R_ECU, R_N_, R_MED, R_DT, R_ASI, R_CUR, R_JB, R_LB, R_NOR, R_N };
enum { P_NOM, P_VAL, P_DT, P_T, P_P, P_DE, P_N };

/* ------------------------------------------------------------------------ */

static const char *nom_ser( Mtram *m, int i )
{
    if (i < 1 || i > m->c.n) return "?";
    return m->c.s[i - 1]->ts.name ? m->c.s[i - 1]->ts.name : "?";
}

/* El recorrido: desde la SALIDA y aguas arriba, que es el orden topologico al
 * reves -- el mismo en que el motor construye, leido desde el final. No es
 * alfabetico: es el orden en que el modelo existe.                       */
static int camino( Mtram *m, int *orden )
{
    int topo[NET_MAX_SER + 1], i, n = 0;

    if (m->c.n < 1) return 0;
    if (m->red.n > 0 && net_topo( m->red.lnk, m->red.n, m->c.n, topo ))
        for (i = m->c.n; i >= 1; i--) orden[n++] = topo[i];
    else
        for (i = 1; i <= m->c.n; i++) orden[n++] = i;
    return n;
}

/* ------------------------------------------------------------------------ */
/* 1 · Exogeneidad  -- ¿transferencia, o VARMA?                              */
/* ------------------------------------------------------------------------ */

static void pinta_exo( Mtram *m )
{
    Diag         *D  = &m->dia;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(D->l_exo) ) );
    GtkTreeIter   it;
    char          q[64], p[32], sg[16];
    int           i;

    gtk_list_store_clear( st );
    if (!D->vale) return;

    for (i = 0; i < D->d.ne; i++) {
        const OdEnlace *e = &D->d.e[i];

        snprintf( q, sizeof q, "Q(%d) = %.4f", e->exogen.df, e->exogen.q );
        snprintf( p, sizeof p, "%.4f", e->exogen.p );
        snprintf( sg, sizeof sg, "%d", e->exogen_signif );

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            X_ENL, e->entrada,
            X_Q,   q,
            X_P,   p,
            X_SIG, e->exogen_signif >= 0 ? sg : "—",
            X_VER, e->exogeno
                 ? "exógena · el modelo de transferencia se sostiene"
                 : "NO exógena · la salida antecede — esto es drvarma",
            -1 );
    }
}

/* ------------------------------------------------------------------------ */
/* 2 · Adecuacion  -- ¿la forma (b,r,s) agota la relacion?                   */
/* ------------------------------------------------------------------------ */

static void pinta_ade( Mtram *m )
{
    Diag         *D  = &m->dia;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(D->l_ade) ) );
    GtkTreeIter   it;
    char          q[64], p[32];
    int           i;

    gtk_list_store_clear( st );
    if (!D->vale) return;

    for (i = 0; i < D->d.ne; i++) {
        const OdEnlace *e = &D->d.e[i];

        snprintf( q, sizeof q, "Q(%d) = %.4f", e->transfer.df, e->transfer.q );
        snprintf( p, sizeof p, "%.4f", e->transfer.p );

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            A_ENL, e->entrada,
            A_Q,   q,
            A_P,   p,
            A_VER, e->adecuado
                 ? "adecuada"
                 : "FALTA ESTRUCTURA · cambia (b, r, s) — vuelve a Identificación",
            -1 );
    }
}

/* ------------------------------------------------------------------------ */
/* 3 · Ajuste                                                                */
/*                                                                           */
/* LAS TRES CIFRAS CON QUE LA ESCUELA CIERRA CADA CASO, y no una:             */
/*                                                                           */
/*   la desviacion tipica residual, que PASA DE una a otra                    */
/*   el R² de Brajin (A.28), que PASA DE una a otra                           */
/*   el LR contra el diagonal, que dice si esa mejora se gana su sitio        */
/*                                                                           */
/* "La desviacion tipica residual estimada pasa de 0.53 % en el modelo        */
/*  univariante a 0.42 % en el Modelo rpu6.3. El R² en el modelo univariante  */
/*  de ru es 0.54, mientras que, en el Modelo rpu6.3, es 0.71." (Brajin 6.4)  */
/*                                                                           */
/* EL R² SI TIENE SENTIDO. Lo que no lo tiene es un R² sobre el NIVEL de una  */
/* I(1): sale cerca de 1 por construccion y no dice nada. El de la escuela va */
/* sobre la serie ESTACIONARIA,                                              */
/*                                                                           */
/*     R² = 1 - SUM (a_t - abar)² / SUM (w_t - wbar)²                        */
/*                                                                           */
/* y su denominador es PROPIEDAD DE LOS DATOS --no lleva parametros-- asi que */
/* es el MISMO en las dos estimaciones. Eso, y solo eso, es lo que hace       */
/* comparables los dos R². Entre d distintas w_t es otra variable y dejan de  */
/* serlo: esto es una TRANSICION entre dos ajustes de una especificacion,     */
/* nunca una nota con la que ordenar modelos.                                 */
/* ------------------------------------------------------------------------ */

/* El calculo vive en lib/gof: asi se contrasta contra numeros conocidos sin
 * levantar un GTK, y la pantalla no es el unico sitio donde esta escrito. */
static int n_par_transfer( Mtram *m )
{
    int k, n = 0;

    for (k = 1; k <= m->mod.st.n; k++) {
        if (m->mod.st.kind[k] != SLOT_FREE) continue;
        if (!strncmp( m->mod.st.name[k], "omega", 5 ) &&
             strncmp( m->mod.st.name[k], "omega_d", 7 )) n++;
        else if (!strncmp( m->mod.st.name[k], "delta", 5 ) &&
                  strncmp( m->mod.st.name[k], "delta_d", 7 )) n++;
    }
    return n;
}

static void pinta_ajuste( Mtram *m )
{
    Diag         *D  = &m->dia;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(D->l_aju) ) );
    GtkTreeIter   it;
    GString      *t = g_string_new( NULL );
    int           i;

    gtk_list_store_clear( st );

    /* --- la cabecera: el LR, que es el contraste --- */
    if (!D->vale)
        g_string_append( t, "Estima primero." );
    else if (!D->hay_base)
        g_string_append( t,
            "Sin baseline sólo hay una columna: faltan las dos que la escuela "
            "lee «pasando de».\nPara tenerlas: marca «Diagonal (-0)» en "
            "Estimación, estima, pulsa «Fijar baseline», desmarca y vuelve a "
            "estimar." );
    else {
        int    k  = n_par_transfer( m );
        double lr = 2.0 * ( D->d.logl - D->logl_base );

        g_string_append_printf( t,
            "logL  %.4f  contra  %.4f (%s)\n", D->d.logl, D->logl_base,
            D->base_que );
        if (lr < 0.0)
            g_string_append( t,
                "LR NEGATIVO: el baseline ajusta MEJOR. O no es el diagonal de "
                "este sistema, o uno de los dos no convergió." );
        else
            g_string_append_printf( t,
                "LR = 2·Δ = %.2f  con %d parámetro%s de transferencia  →  "
                "χ²(%d) al 5%% ≈ %.1f%s", lr, k, k == 1 ? "" : "s", k,
                (double) k + 1.645 * sqrt( 2.0 * k ),
                k > 0 && lr > (double) k + 1.645 * sqrt( 2.0 * k )
                    ? "   ·  la transferencia se gana su sitio" : "" );
    }
    gtk_label_set_text( GTK_LABEL(D->l_lr), t->str );
    g_string_free( t, TRUE );

    if (!D->vale || !D->hay_res) return;

    /* --- una fila por ecuacion, con las dos cifras PASANDO DE una a otra --- */
    for (i = 0; i < D->res.m && i < m->c.n; i++) {
        double  sw, r2u = 0, dtu = 0, r2t = 0, dtt = 0, *w;
        int     nw;
        char    cdtu[32], cdtt[32], cr2u[32], cr2t[32], cred[32];
        gboolean hay_u;

        w  = gof_estacionaria( &m->c.s[i]->ts, &m->c.s[i]->tm, &nw );
        sw = w ? gof_suma_cuad_cola( w, nw, D->res.n ) : 0.0;
        free( w );

        if (gof_r2_brajin( D->res.v[i], D->res.n, sw, &r2t, &dtt )) {
            /* Sin w_t no hay R², pero la d.t. residual sigue estando. */
            r2t = -99.0;
            dtt = i < D->d.ns ? D->d.s[i].sd : 0.0;
        }
        hay_u = D->hay_res_base && i < D->res_base.m &&
                gof_r2_brajin( D->res_base.v[i], D->res_base.n, sw,
                               &r2u, &dtu ) == 0;

        snprintf( cdtt, sizeof cdtt, "%.4f", dtt );
        snprintf( cr2t, sizeof cr2t, r2t > -90.0 ? "%.4f" : "—", r2t );
        if (hay_u) {
            snprintf( cdtu, sizeof cdtu, "%.4f", dtu );
            snprintf( cr2u, sizeof cr2u, "%.4f", r2u );
            /* La reduccion de varianza residual: la cifra con la que la
             * escuela cierra cada caso, en las unidades del analista.   */
            snprintf( cred, sizeof cred, "%+.1f %%",
                      100.0 * gof_reduccion( dtu, dtt ) );
        } else {
            snprintf( cdtu, sizeof cdtu, "—" );
            snprintf( cr2u, sizeof cr2u, "—" );
            snprintf( cred, sizeof cred, "—" );
        }

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            J_ECU, nom_ser( m, i + 1 ),
            J_DTU, cdtu, J_DTT, cdtt,
            J_R2U, cr2u, J_R2T, cr2t,
            J_RED, cred,
            J_NOTA, !hay_u ? ""
                  : r2t - r2u > 0.05
                    ? "una parte importante de su variabilidad se explica con "
                      "las entradas"
                    : r2t < r2u ? "el R² BAJA: la transferencia no aporta aquí"
                                : "",
            -1 );
    }
}

/* ------------------------------------------------------------------------ */
/* 4 · Residuos                                                              */
/* ------------------------------------------------------------------------ */

static void pinta_res( Mtram *m )
{
    Diag         *D  = &m->dia;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(D->l_res) ) );
    GtkTreeIter   it;
    char          med[32], dt[32], asi[32], cur[32], jb[48], lb[48], nor[64];
    int           i;

    gtk_list_store_clear( st );
    if (!D->vale) return;

    for (i = 0; i < D->d.ns; i++) {
        const OdSerie *s = &D->d.s[i];

        snprintf( med, sizeof med, "%.4f", s->media );
        snprintf( dt,  sizeof dt,  "%.4f", s->sd );
        snprintf( asi, sizeof asi, "%+.3f", s->skew );
        snprintf( cur, sizeof cur, "%+.3f", s->kurt );

        /* JARQUE-BERA. Si se dan la asimetria y la curtosis hay que darlo:
         * es exactamente la funcion de esas dos --n/6 (S² + K²/4)-- y es el
         * contraste que dice si apartarse de cero significa algo. Dejarlo
         * fuera obliga a hacer la cuenta a ojo con los dos numeros delante. */
        if (s->tiene_stats && s->nobs > 0) {
            double q = JarqueBera( s->skew, s->kurt, s->nobs );

            snprintf( jb, sizeof jb, "JB(2) = %.2f   p = %.4f", q, exp( -q / 2.0 ) );
        } else
            snprintf( jb, sizeof jb, "—" );

        snprintf( lb,  sizeof lb,  "Q(%d) = %.2f", s->lb.df, s->lb.q );
        if (s->tiene_hist)
            snprintf( nor, sizeof nor, "%.1f%% / %.1f%%   y   %.1f%% / %.1f%%",
                      s->fuera1, s->esp1, s->fuera2, s->esp2 );
        else
            snprintf( nor, sizeof nor, "—" );

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            R_ECU, i < m->c.n ? nom_ser( m, i + 1 ) : s->nombre,
            R_N_,  s->nobs,
            R_MED, med, R_DT, dt, R_ASI, asi, R_CUR, cur,
            R_JB,  jb,  R_LB, lb,  R_NOR, nor, -1 );
    }
}

/* ------------------------------------------------------------------------ */
/* 5 · Modelo estimado  -- con sus d.t., y DE DONDE viene cada uno            */
/* ------------------------------------------------------------------------ */

/* ¿Se mantuvo fijo el ruido de esta serie? Entonces sus parametros NO se
 * estimaron, y presentarlos como resultado seria la peor clase de mentira
 * de una pantalla.                                                       */
static const char *de_donde( Mtram *m, const char *nombre )
{
    int i;

    if (sscanf( nombre, "phi_%d[", &i ) == 1 ||
        sscanf( nombre, "theta_%d[", &i ) == 1) {
        if (i == 1 ? m->est.fix_N : m->est.fix_X) return "DEL .pre, no se estimó";
        return "";
    }
    if (sscanf( nombre, "omega_d%d[", &i ) == 1 ||
        sscanf( nombre, "delta_d%d[", &i ) == 1) {
        if (i == 1 ? m->est.fix_D : m->est.fix_E) return "DEL .pre, no se estimó";
        return "";
    }
    if (sscanf( nombre, "mu[%d]", &i ) == 1 && m->est.fix_M)
        return "DEL .pre, no se estimó";
    return "";
}

static void pinta_par( Mtram *m )
{
    Diag         *D  = &m->dia;
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(D->l_par) ) );
    GtkTreeIter   it;
    char          v[32], dt[32], t[32], p[32];
    int           i;

    gtk_list_store_clear( st );
    if (!D->hay_par) return;

    for (i = 0; i < D->par.n; i++) {
        const OdPar *q = &D->par.p[i];
        const char  *de;

        snprintf( v, sizeof v, "%.6f", q->valor );
        if (q->libre) {
            snprintf( dt, sizeof dt, "%.6f", q->dt );
            snprintf( t,  sizeof t,  "%.3f", q->t );
            snprintf( p,  sizeof p,  "%.4f", q->p );
            de = de_donde( m, q->nombre );
            if (!de[0]) de = q->p < 0.05 ? "" : "no significativo";
        } else {
            snprintf( dt, sizeof dt, "—" );
            snprintf( t,  sizeof t,  "—" );
            snprintf( p,  sizeof p,  "—" );
            de = q->atado[0] ? q->atado : "atado";
        }

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            P_NOM, q->nombre, P_VAL, v, P_DT, dt, P_T, t, P_P, p,
            P_DE, de, -1 );
    }
}

/* ------------------------------------------------------------------------ */
/* 6 · La salida entera                                                      */
/* ------------------------------------------------------------------------ */

static void pinta_out( Mtram *m )
{
    Diag          *D = &m->dia;
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(D->t_out) );
    gchar         *txt = NULL;
    gsize          n = 0;

    if (!D->path || !g_file_get_contents( D->path, &txt, &n, NULL )) {
        gtk_text_buffer_set_text( b, "", -1 );
        return;
    }
    if (!g_utf8_validate( txt, n, NULL )) {
        gchar *u = g_utf8_make_valid( txt, n );

        g_free( txt ); txt = u;
    }
    gtk_text_buffer_set_text( b, txt, -1 );
    g_free( txt );
}

/* ------------------------------------------------------------------------ */
/* Los dos veredictos, y a donde hay que volver                              */
/* ------------------------------------------------------------------------ */

static void pinta_veredicto( Mtram *m )
{
    Diag    *D = &m->dia;
    int      mal, nex;

    D->ir_a = -1;

    if (!D->vale) {
        mtram_verdicto( D->ver_global, MT_AMBAR,
            "Estima primero. La diagnosis sale del .out: aquí no se recalcula." );
        gtk_label_set_text( GTK_LABEL(D->ver_ojo), "" );
        gtk_widget_set_sensitive( D->b_volver, FALSE );
        return;
    }

    mal = od_no_adecuados( &D->d );
    nex = od_no_exogenos( &D->d );

    /* El global: Hosking primero, que es el del conjunto. */
    if (D->d.hosking.hay)
        mtram_verdicto( D->ver_global,
            D->d.hosking_blanco ? MT_VERDE : MT_ROJO,
            "Hosking P(%d) = %.1f, p = %.4f · los residuos %s%s",
            D->d.hosking.df, D->d.hosking.q, D->d.hosking.p,
            D->d.hosking_blanco ? "son ruido blanco conjuntamente"
                                : "NO son ruido blanco: falta estructura",
            D->d.jb.hay && !D->d.jb_normal
                ? " · normalidad rechazada (JB)" : "" );
    else
        mtram_verdicto( D->ver_global, MT_AMBAR,
            "No hay diagnosis multivariante en este .out." );

    /* El segundo ELIGE, y ademas dice A DONDE VOLVER. */
    if (nex) {
        GString *q = g_string_new( NULL );
        int      i;

        for (i = 0; i < D->d.ne; i++)
            if (D->d.e[i].exogeno == 0)
                g_string_append_printf( q, "%s%s", q->len ? ", " : "",
                                        D->d.e[i].entrada );
        mtram_verdicto( D->ver_ojo, MT_ROJO,
            "%d enlace%s sin exogeneidad (%s): la transferencia no se sostiene "
            "— esto es drvarma", nex, nex == 1 ? "" : "s", q->str );
        g_string_free( q, TRUE );
        D->ir_a = PG_RED;
        gtk_button_set_label( GTK_BUTTON(D->b_volver), "Ir a Red" );
    } else if (mal) {
        GString *q = g_string_new( NULL );
        int      i;

        for (i = 0; i < D->d.ne; i++)
            if (D->d.e[i].adecuado == 0)
                g_string_append_printf( q, "%s%s", q->len ? ", " : "",
                                        D->d.e[i].entrada );
        mtram_verdicto( D->ver_ojo, MT_AMBAR,
            "%d enlace%s con falta de estructura (%s): se arregla en (b, r, s)",
            mal, mal == 1 ? "" : "s", q->str );
        g_string_free( q, TRUE );
        D->ir_a = PG_IDENT;
        gtk_button_set_label( GTK_BUTTON(D->b_volver), "Ir a Identificación" );
    } else
        mtram_verdicto( D->ver_ojo, MT_VERDE,
            "Los %d enlaces pasan los dos contrastes.", D->d.ne );

    gtk_widget_set_sensitive( D->b_volver, D->ir_a >= 0 );
    gtk_widget_set_visible( D->b_volver, D->ir_a >= 0 );
}

/* ------------------------------------------------------------------------ */

void diagnosis_refresca( Mtram *m )
{
    Diag  *D = &m->dia;
    int    orden[NET_MAX_SER + 1], n;
    gchar *s;

    n = camino( m, orden );
    if (D->actual < 1 || D->actual > m->c.n) D->actual = n ? orden[0] : 0;

    s = g_strdup_printf( "  %s  ", D->actual ? nom_ser( m, D->actual ) : "—" );
    gtk_label_set_text( GTK_LABEL(D->l_ecu), s );
    g_free( s );

    pinta_exo( m );
    pinta_ade( m );
    pinta_ajuste( m );
    pinta_res( m );
    pinta_par( m );
    pinta_out( m );
    pinta_veredicto( m );
}

static void fija_baseline( Mtram *m, gboolean callado );

/* La llama la estimacion al acabar, con SU .out. Los residuos son de ESTA
 * corrida: TASTE tenia una sola ranura y por eso no se podian comparar dos
 * modelos.                                                               */
void diagnosis_desde( Mtram *m, const char *path )
{
    Diag  *D = &m->dia;
    gchar *res;

    D->vale = D->hay_par = D->hay_res = FALSE;
    g_free( D->path );
    D->path = NULL;

    if (path && od_parse_file( path, &D->d ) == 0) {
        D->vale = TRUE;
        D->path = g_strdup( path );
        D->hay_par = od_params_file( path, &D->par ) == 0;
    }

    /* Los residuos, del fichero que escribe "drtran -e". */
    res = g_build_filename( g_get_user_cache_dir(), GUI_CACHE, "residuos.txt", NULL );
    D->hay_res = od_residuos( res, &D->res ) == 0;
    g_free( res );

    /* Si esta corrida era la del baseline, se fija SOLA y se deja el modo como
     * estaba. Lo que el analista pidio fue "dame el baseline", no "pon el modo
     * diagonal": dejarselo puesto seria dejarle una trampa armada.    */
    if (D->pidiendo_base) {
        D->pidiendo_base = FALSE;
        fija_baseline( m, TRUE );
        m->est.diagonal = FALSE;
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->est.c_diag), FALSE );
        if (D->hay_base)
            preview_show_status( m, "Baseline listo: logL = %.6f. El modo "
                "diagonal queda desmarcado; vuelve a estimar y mira Ajuste.",
                D->logl_base );
        else
            preview_show_status( m, "El diagonal no dejó verosimilitud: "
                                    "mira la consola en Estimación." );
    }

    diagnosis_refresca( m );
}

/* ------------------------------------------------------------------------ */
/* Botones                                                                   */
/* ------------------------------------------------------------------------ */

static void on_releer( GtkButton *b, Mtram *m )
{
    if (!m->est.out_path) {
        preview_show_status( m, "No hay ningún .out todavía: estima primero." );
        return;
    }
    diagnosis_desde( m, m->est.out_path );
    if (!m->dia.vale)
        preview_show_status( m, "En %s no hay diagnosis. ¿Llegó a estimar?",
                             m->est.out_path );
}

static void mueve_ecu( Mtram *m, int paso )
{
    int orden[NET_MAX_SER + 1], n = camino( m, orden ), i;

    for (i = 0; i < n; i++)
        if (orden[i] == m->dia.actual) {
            int j = i + paso;

            if (j < 0) j = n - 1;
            if (j >= n) j = 0;
            m->dia.actual = orden[j];
            break;
        }
    diagnosis_refresca( m );
}

static void on_ante( GtkButton *b, Mtram *m ) { mueve_ecu( m, -1 ); }
static void on_sig ( GtkButton *b, Mtram *m ) { mueve_ecu( m,  1 ); }

static void on_volver( GtkButton *b, Mtram *m )
{
    if (m->dia.ir_a >= 0)
        gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), m->dia.ir_a );
}

/* CALCULAR EL BASELINE, ENTERO Y DESDE AQUI.
 *
 * Antes esto solo FIJABA lo que hubiera en pantalla, y conseguir que en
 * pantalla hubiera un diagonal costaba ocho pasos: ir a Estimacion, marcar
 * «Diagonal», estimar, volver, fijar, ir otra vez, desmarcar, estimar. Ningun
 * sitio decia que hubiera que hacer ese viaje.
 *
 * Ahora el boton lo hace: pone el modo diagonal, lanza, y al acabar se fija
 * solo y deja el modo como estaba. La misma regla que «Calcular» en Prevision
 * -- cada pantalla fabrica lo que pide.                                 */
static void on_baseline( GtkButton *b, Mtram *m )
{
    Diag *D = &m->dia;

    if (m->est.corriendo) {
        preview_show_status( m, "El motor ya está corriendo." );
        return;
    }
    if (m->c.n < 2) {
        preview_show_status( m, "Carga al menos dos .pre." );
        return;
    }

    /* El modo diagonal, puesto por nosotros y VISIBLE en Estimacion: que la
     * casilla cambie sola delante del analista es parte del mensaje.   */
    D->pidiendo_base = TRUE;
    m->est.diagonal  = TRUE;
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->est.c_diag), TRUE );

    if (estima_lanzar( m ))
        preview_show_status( m, "Estimando el modelo DIAGONAL para el "
            "baseline… al acabar se fija solo y vuelvo a dejar el modo como "
            "estaba." );
    else {
        D->pidiendo_base = FALSE;
        m->est.diagonal  = FALSE;
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->est.c_diag), FALSE );
    }
}

/* Fijar como baseline lo que hay en pantalla. */
static void fija_baseline( Mtram *m, gboolean callado )
{
    Diag *D = &m->dia;

    if (!D->vale || !D->d.tiene_logl) {
        if (!callado)
            preview_show_status( m, "No hay una estimación con verosimilitud." );
        return;
    }
    D->logl_base = D->d.logl;
    D->npar_base = D->par.n;
    D->hay_base  = TRUE;
    snprintf( D->base_que, sizeof D->base_que, "%s, %d enlaces",
              m->est.diagonal ? "diagonal -0" : "con transferencias",
              m->red.n );

    /* Y SUS RESIDUOS, que es lo que da la d.t. y el R² del lado izquierdo.
     * El logL solo da el LR; las otras dos cifras de la escuela necesitan
     * los a_t del diagonal.                                             */
    D->res_base     = D->res;
    D->hay_res_base = D->hay_res;

    if (!callado) {
        if (!m->est.diagonal)
            preview_show_status( m, "Fijado — pero OJO: este modelo NO es el "
                "diagonal. El baseline del LR tiene que ser el -0." );
        else
            preview_show_status( m, "Baseline fijado: logL = %.6f.",
                                 D->logl_base );
        diagnosis_refresca( m );
    }
}

/* ------------------------------------------------------------------------ */
/* EXPORTAR                                                                  */
/*                                                                           */
/* Es el modulo que TASTE declaro y nunca escribio --TABLA, Graba_Ser_PRN,   */
/* Graba_Ser_TSF, los tres vacios-- y que el inventario encontro igual de    */
/* vacio treinta años despues: siete pantallas y la unica forma de sacar un  */
/* numero era copiarlo de una etiqueta.                                      */
/*                                                                           */
/* LA TABLA SE ARMA DE LOS DATOS, NO DEL WIDGET. Raspar el GtkTreeView seria */
/* mas corto y convertiria todo en texto: un numero dejaria de ser un numero */
/* y el CSV no valdria para nada. Los decimales los declara la columna.      */
/*                                                                           */
/* Y LA PROCEDENCIA VA DENTRO. Una tabla que sale sin decir de que modelo y  */
/* de que muestra viene es la forma mas facil de que un numero acabe en un   */
/* paper sin poder reproducirlo.                                             */
/* ------------------------------------------------------------------------ */

static void procedencia( Mtram *m, Tabla *t )
{
    Diag    *D = &m->dia;
    GString *s = g_string_new( NULL );
    char     b[192];
    int      i;

    /* tb_procedencia COPIA, asi que aqui se le pasa un buffer y no un
     * g_strdup_printf: eso seria una fuga en cada exportacion.        */
    for (i = 1; i <= m->c.n; i++)
        g_string_append_printf( s, "%s%s", i > 1 ? ", " : "", nom_ser( m, i ) );
    tb_procedencia( t, "Series", s->str );
    g_string_free( s, TRUE );

    snprintf( b, sizeof b, "%d enlace%s%s", m->red.n,
              m->red.n == 1 ? "" : "s",
              m->est.diagonal ? ", DIAGONAL (-0)" : "" );
    tb_procedencia( t, "Modelo", b );

    if (D->hay_res && D->res.n > 0) {
        snprintf( b, sizeof b, "%s - %s, %d obs", D->res.fecha[0],
                  D->res.fecha[D->res.n - 1], D->res.n );
        tb_procedencia( t, "Muestra", b );
    }

    if (D->d.tiene_logl) {
        snprintf( b, sizeof b, "%.6f", D->d.logl );
        tb_procedencia( t, "logL", b );
    }

    if (D->path) tb_procedencia( t, "Fichero", D->path );

    /* Que motor. Hoy drtran no estampa version ni fecha de compilacion --
     * queda anotado en DISENO-madre.md-- asi que se dice lo que se sabe. */
    tb_procedencia( t, "Motor", "drtran" );
}

/* La tabla de la pestaña en la que se este. NULL si esa no tiene tabla. */
static Tabla *tabla_actual( Mtram *m, int pagina )
{
    Diag  *D = &m->dia;
    Tabla *t = NULL;
    int    i;

    if (!D->vale) return NULL;

    switch (pagina) {
    case 0:                                        /* exogeneidad */
        t = tb_new( "Exogeneidad: ¿transferencia, o VARMA?" );
        tb_col( t, "Entrada",   NULL, TB_TXT, 0 );
        tb_col( t, "Q",         NULL, TB_NUM, 4 );
        tb_col( t, "g.l.",      NULL, TB_ENT, 0 );
        tb_col( t, "p",         NULL, TB_NUM, 4 );
        tb_col( t, "signif.",   NULL, TB_ENT, 0 );
        tb_col( t, "Exógena",   NULL, TB_TXT, 0 );
        for (i = 0; i < D->d.ne; i++) {
            const OdEnlace *e = &D->d.e[i];

            tb_fila( t );
            tb_pon_txt( t, 0, e->entrada );
            tb_pon_num( t, 1, e->exogen.q );
            tb_pon_num( t, 2, e->exogen.df );
            tb_pon_num( t, 3, e->exogen.p );
            if (e->exogen_signif >= 0) tb_pon_num( t, 4, e->exogen_signif );
            tb_pon_txt( t, 5, e->exogeno ? "sí" : "NO" );
        }
        break;

    case 1:                                        /* adecuacion */
        t = tb_new( "Adecuación: ¿la forma (b, r, s) agota la relación?" );
        tb_col( t, "Entrada",  NULL, TB_TXT, 0 );
        tb_col( t, "Q",        NULL, TB_NUM, 4 );
        tb_col( t, "g.l.",     NULL, TB_ENT, 0 );
        tb_col( t, "p",        NULL, TB_NUM, 4 );
        tb_col( t, "Adecuada", NULL, TB_TXT, 0 );
        for (i = 0; i < D->d.ne; i++) {
            const OdEnlace *e = &D->d.e[i];

            tb_fila( t );
            tb_pon_txt( t, 0, e->entrada );
            tb_pon_num( t, 1, e->transfer.q );
            tb_pon_num( t, 2, e->transfer.df );
            tb_pon_num( t, 3, e->transfer.p );
            tb_pon_txt( t, 4, e->adecuado ? "sí" : "NO" );
        }
        break;

    case 2:                                        /* ajuste */
        if (!D->hay_res) return NULL;
        t = tb_new( "Ajuste: la desviación típica residual y el R² de Brajín (A.28)" );
        tb_col( t, "Ecuación",        NULL, TB_TXT, 0 );
        tb_col( t, "d.t. diagonal",   NULL, TB_NUM, 4 );
        tb_col( t, "d.t. transfer.",  NULL, TB_NUM, 4 );
        tb_col( t, "R² diagonal",     NULL, TB_NUM, 4 );
        tb_col( t, "R² transfer.",    NULL, TB_NUM, 4 );
        tb_col( t, "Varianza",        "%",  TB_NUM, 1 );
        for (i = 0; i < D->res.m && i < m->c.n; i++) {
            double  sw, r2u = 0, dtu = 0, r2t = 0, dtt = 0, *w;
            int     nw;
            gboolean hay_u;

            w  = gof_estacionaria( &m->c.s[i]->ts, &m->c.s[i]->tm, &nw );
            sw = w ? gof_suma_cuad_cola( w, nw, D->res.n ) : 0.0;
            free( w );

            tb_fila( t );
            tb_pon_txt( t, 0, nom_ser( m, i + 1 ) );

            if (gof_r2_brajin( D->res.v[i], D->res.n, sw, &r2t, &dtt ) == 0) {
                tb_pon_num( t, 2, dtt );
                tb_pon_num( t, 4, r2t );
            }
            hay_u = D->hay_res_base && i < D->res_base.m &&
                    gof_r2_brajin( D->res_base.v[i], D->res_base.n, sw,
                                   &r2u, &dtu ) == 0;
            if (hay_u) {
                tb_pon_num( t, 1, dtu );
                tb_pon_num( t, 3, r2u );
                tb_pon_num( t, 5, 100.0 * gof_reduccion( dtu, dtt ) );
            }
        }
        break;

    case 3:                                        /* residuos */
        t = tb_new( "Residuos: ¿son ruido blanco?" );
        tb_col( t, "Ecuación",   NULL, TB_TXT, 0 );
        tb_col( t, "n",          NULL, TB_ENT, 0 );
        tb_col( t, "media",      NULL, TB_NUM, 4 );
        tb_col( t, "d.t.",       NULL, TB_NUM, 4 );
        tb_col( t, "asimetría",  NULL, TB_NUM, 3 );
        tb_col( t, "curtosis",   NULL, TB_NUM, 3 );
        tb_col( t, "Jarque-Bera", NULL, TB_NUM, 2 );
        tb_col( t, "Ljung-Box",  NULL, TB_NUM, 2 );
        tb_col( t, "g.l.",       NULL, TB_ENT, 0 );
        for (i = 0; i < D->d.ns; i++) {
            const OdSerie *s = &D->d.s[i];

            tb_fila( t );
            tb_pon_txt( t, 0, i < m->c.n ? nom_ser( m, i + 1 ) : s->nombre );
            tb_pon_num( t, 1, s->nobs );
            if (s->tiene_stats) {
                tb_pon_num( t, 2, s->media );
                tb_pon_num( t, 3, s->sd );
                tb_pon_num( t, 4, s->skew );
                tb_pon_num( t, 5, s->kurt );
                if (s->nobs > 0)
                    tb_pon_num( t, 6, JarqueBera( s->skew, s->kurt, s->nobs ) );
            }
            tb_pon_num( t, 7, s->lb.q );
            tb_pon_num( t, 8, s->lb.df );
        }
        break;

    case 4:                                        /* modelo estimado */
        if (!D->hay_par) return NULL;
        t = tb_new( "Modelo estimado" );
        tb_col( t, "Parámetro", NULL, TB_TXT, 0 );
        tb_col( t, "Estimado",  NULL, TB_NUM, 6 );
        tb_col( t, "d.t.",      NULL, TB_NUM, 6 );
        tb_col( t, "t",         NULL, TB_NUM, 3 );
        tb_col( t, "p",         NULL, TB_NUM, 4 );
        tb_col( t, "Nota",      NULL, TB_TXT, 0 );
        for (i = 0; i < D->par.n; i++) {
            const OdPar *q = &D->par.p[i];
            const char  *de;

            tb_fila( t );
            tb_pon_txt( t, 0, q->nombre );
            tb_pon_num( t, 1, q->valor );
            if (q->libre) {
                tb_pon_num( t, 2, q->dt );
                tb_pon_num( t, 3, q->t );
                tb_pon_num( t, 4, q->p );
                /* La misma nota que en pantalla: lo que se ve es lo que se
                 * exporta, o son dos tablas distintas.                 */
                de = de_donde( m, q->nombre );
                if (!de[0]) de = q->p < 0.05 ? "" : "no significativo";
                tb_pon_txt( t, 5, de );
            } else
                tb_pon_txt( t, 5, q->atado[0] ? q->atado : "atado" );
        }
        break;

    default:
        return NULL;
    }

    if (t) procedencia( m, t );
    return t;
}

static void on_exportar( GtkButton *b, Mtram *m )
{
    int        pg = gtk_notebook_get_current_page( GTK_NOTEBOOK(m->dia.libreta) );
    Tabla     *t  = tabla_actual( m, pg );
    GtkWidget *d;
    GtkFileFilter *f;
    gchar     *sug;

    if (t == NULL) {
        preview_show_status( m, pg == 5
            ? "La pestaña Salida ya ES un fichero: el .out está en %s"
            : "No hay tabla que exportar en esta pestaña.",
            m->dia.path ? m->dia.path : "" );
        return;
    }
    if (tb_nfilas( t ) == 0) {
        preview_show_status( m, "Esa tabla está vacía." );
        tb_free( t );
        return;
    }

    d = gtk_file_chooser_dialog_new( "Exportar la tabla",
            GTK_WINDOW(m->ventana_p), GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Guardar", GTK_RESPONSE_ACCEPT,
            NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );

    /* La extension elige el formato, y se dice en el propio filtro. */
    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "CSV — para una hoja de cálculo (*.csv)" );
    gtk_file_filter_add_pattern( f, "*.csv" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );

    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "Texto de ancho fijo — para pegar (*.txt)" );
    gtk_file_filter_add_pattern( f, "*.txt" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );

    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "LaTeX — un tabular con su caption (*.tex)" );
    gtk_file_filter_add_pattern( f, "*.tex" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );

    sug = g_strdup_printf( "%s.csv",
              pg == 0 ? "exogeneidad" : pg == 1 ? "adecuacion" :
              pg == 2 ? "ajuste"      : pg == 3 ? "residuos"   : "parametros" );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), sug );
    g_free( sug );

    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        gchar *path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );

        if (tb_write( t, path ) == 0)
            preview_show_status( m, "Escrito %s — con la procedencia dentro.",
                                 path );
        else
            preview_show_status( m, "No pude escribir %s", path );
        g_free( path );
    }
    gtk_widget_destroy( d );
    tb_free( t );
}

/* ------------------------------------------------------------------------ */
/* Los graficos                                                              */
/* ------------------------------------------------------------------------ */

static gchar *dibuja( Mtram *m, int que, int entrada )
{
    Diag           *D = &m->dia;
    struct Tseries  ts;
    gchar          *dir, *base, *eps = NULL;
    FDFig          *f = NULL;
    int             i, col = D->actual - 1;

    if (!D->hay_res || col < 0 || col >= D->res.m) return NULL;

    dir = g_build_filename( g_get_user_cache_dir(), GUI_CACHE, NULL );
    g_mkdir_with_parents( dir, 0700 );

    memset( &ts, 0, sizeof ts );
    ts.name = (char *) nom_ser( m, D->actual );
    ts.nobs = D->res.n;
    ts.freq = D->res.freq > 0 ? D->res.freq : 1;
    ts.data = vector( 1, D->res.n );
    for (i = 0; i < D->res.n; i++) ts.data[i + 1] = D->res.v[col][i];
    sscanf( D->res.fecha[0], "%d/%d", &ts.begtime, &ts.begyear );

    /* fugplot lee la media y la varianza de la propia serie -- la ACF y la
     * tipificacion del panel salen de ahi.                               */
    ts.mean = Mean( ts.data, ts.nobs );
    ts.var  = Stdev( ts.data, ts.nobs );
    ts.var *= ts.var;

    /* Los residuos NO se transforman: lambda 1 y sin diferencias, como hace
     * fue con los suyos. Y no se pierde ninguna observacion al principio, asi
     * que el desplazamiento del eje es 0.                                */
    base = g_build_filename( dir, "res", NULL );

    switch (que) {
    case 0:   /* serie + acf/pacf: la bateria de fue, la misma figura */
        f = fp_PlotSer_CorrSer( &ts, 0, ts.nobs, 0, ts.begyear, 1.0, 0, 0,
                                0, 0.0, base, ts.name );
        eps = g_strdup_printf( "%s.eps", base );
        break;
    case 1:   /* histograma */
        f = fp_histogram( &ts, 0, 0, 1.0, base, ts.name );
        eps = g_build_filename( dir, "hist_res.eps", NULL );
        break;
    /* LA CCF DE LOS RESIDUOS CONTRA LA ENTRADA. No es la de Identificacion:
     * alli se preblanquea con el modelo univariante para DECIDIR (b, r, s);
     * aqui las dos series ya son residuos --ya estan blancas si el modelo
     * vale-- y lo que se mira es si queda algo que la transferencia no cogio.
     *
     * El calculo es Ccf() del motor, la misma rutina que usa su diagnosis, y
     * en los dos sentidos: k > 0 la transferencia, k < 0 la exogeneidad. */
    case 3: {
        int   lags = D->res.n / 4, k, c2 = entrada - 1;
        real *x, *y, *cpos, *cneg;
        double mx, my, sx, sy;
        double ccf[2 * NDIAG_LAGS + 1];

        if (lags > NDIAG_LAGS) lags = NDIAG_LAGS;
        if (lags < 1 || c2 < 0 || c2 >= D->res.m || c2 == col) break;

        x = vector( 1, D->res.n );
        y = vector( 1, D->res.n );
        for (k = 0; k < D->res.n; k++) {
            x[k + 1] = D->res.v[c2][k];      /* la entrada  */
            y[k + 1] = D->res.v[col][k];     /* la salida   */
        }
        mx = Mean( x, D->res.n ); sx = Stdev( x, D->res.n );
        my = Mean( y, D->res.n ); sy = Stdev( y, D->res.n );

        if (sx > 1e-12 && sy > 1e-12) {
            cpos = vector( 1, lags + 1 );
            cneg = vector( 1, lags + 1 );
            Ccf( x, y, D->res.n, lags, cpos, mx, my, sx, sy );
            Ccf( y, x, D->res.n, lags, cneg, my, mx, sy, sx );
            for (k = 0; k <= lags; k++) {
                ccf[lags + k] = cpos[k + 1];
                ccf[lags - k] = cneg[k + 1];
            }
            eps = g_build_filename( dir, "ccf_res.eps", NULL );
            if (ccf_write_eps( eps, ccf, lags, D->res.n,
                               nom_ser( m, entrada ), ts.name, 0.0, -1 ) != 0) {
                g_free( eps ); eps = NULL;
            }
            free_vector( cpos, 1, lags + 1 );
            free_vector( cneg, 1, lags + 1 );
        }
        free_vector( x, 1, D->res.n );
        free_vector( y, 1, D->res.n );
        break;
    }
    }

    if (f) fd_fig_free( f );
    free_vector( ts.data, 1, D->res.n );
    g_free( base ); g_free( dir );
    return eps;
}

static void on_grafico( GtkMenuItem *mi, Mtram *m )
{
    int    que = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(mi), "que" ) );
    int    ent = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(mi), "ent" ) );
    gchar *eps = dibuja( m, que, ent );

    if (!eps) {
        preview_show_status( m, "No pude dibujarlo." );
        return;
    }
    if (!preview_show( m, eps ))
        preview_show_status( m, "No pude enseñar %s", eps );
    g_free( eps );
}

static void on_graficos( GtkButton *b, Mtram *m )
{
    Diag      *D = &m->dia;
    GtkWidget *menu, *mi;
    int        k;

    if (!D->hay_res) {
        preview_show_status( m, "No hay residuos: estima con la casilla de "
                                "residuos puesta (mtram la pone sola)." );
        return;
    }

    menu = gtk_menu_new();

#define ITEM(txt, q, e) \
    mi = gtk_menu_item_new_with_label( txt ); \
    g_object_set_data( G_OBJECT(mi), "que", GINT_TO_POINTER(q) ); \
    g_object_set_data( G_OBJECT(mi), "ent", GINT_TO_POINTER(e) ); \
    g_signal_connect( mi, "activate", G_CALLBACK(on_grafico), m ); \
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );

    /* NO va el grafico media - desviacion tipica. Ese existe para decidir la
     * TRANSFORMACION de una serie: si la dispersion crece con el nivel, hay
     * que tomar logaritmos. Un residuo no tiene nivel con el que crecer --su
     * media es cero por construccion-- asi que el grafico no puede decir
     * nada. fue tampoco lo dibuja sobre sus residuos.                    */
    ITEM( "Serie y ACF / PACF", 0, 0 )
    ITEM( "Histograma", 1, 0 )

    /* Las CCF con cada entrada de ESTA ecuacion. */
    {
    int hay = 0;

    for (k = 0; k < m->red.n; k++) {
        gchar *txt;

        if (m->red.lnk[k].out != D->actual) continue;
        if (!hay++)
            gtk_menu_shell_append( GTK_MENU_SHELL(menu),
                                   gtk_separator_menu_item_new() );
        txt = g_strdup_printf( "CCF de los residuos con %s",
                               nom_ser( m, m->red.lnk[k].inp ) );
        ITEM( txt, 3, m->red.lnk[k].inp )
        g_free( txt );
    }
    }
#undef ITEM

    gtk_widget_show_all( menu );
    gtk_menu_popup_at_widget( GTK_MENU(menu), GTK_WIDGET(b),
                              GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
                              NULL );
}

/* ------------------------------------------------------------------------ */

static void columna( GtkWidget *tv, const char *titulo, int col )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                               titulo, r, "text", col, NULL );

    gtk_tree_view_column_set_resizable( c, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), c );
}

static GtkWidget *en_scroll( GtkWidget *w )
{
    GtkWidget *sc = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), w );
    return sc;
}

static GtkWidget *texto_mono( void )
{
    GtkWidget *t = gtk_text_view_new();

    gtk_text_view_set_editable( GTK_TEXT_VIEW(t), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(t), TRUE );
    gtk_text_view_set_left_margin( GTK_TEXT_VIEW(t), 10 );
    gtk_text_view_set_top_margin( GTK_TEXT_VIEW(t), 8 );
    return t;
}

GtkWidget *diagnosis_pagina_new( Mtram *m )
{
    Diag         *D = &m->dia;
    GtkWidget    *caja, *barra, *b, *vb;
    GtkListStore *st;

    D->vale = D->hay_par = D->hay_res = D->hay_base = FALSE;
    D->hay_res_base = D->pidiendo_base = FALSE;
    D->path = NULL;
    D->actual = 0;
    D->ir_a = -1;

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 8 );

    /* --- la barra --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(caja), barra, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Releer" );
    gtk_widget_set_tooltip_text( b,
        "La diagnosis sale del .out de la última estimación. Aquí no se "
        "recalcula nada: el motor ya la hace." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_releer), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    /* El recorrido por el grafo: desde la salida y aguas arriba. */
    b = gtk_button_new_with_label( "◀" );
    gtk_widget_set_tooltip_text( b,
        "La ecuación anterior, en el orden topológico: desde la salida y "
        "aguas arriba, que es el orden en que el modelo existe." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_ante), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    D->l_ecu = gtk_label_new( "  —  " );
    gtk_box_pack_start( GTK_BOX(barra), D->l_ecu, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "▶" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_sig), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    /* Graficos VA CON LAS FLECHAS: opera sobre la ecuacion que ellas eligen,
     * asi que pertenece a ese grupo. Exportar es de la PESTAÑA, no de la
     * ecuacion, y por eso va despues y tras el separador.              */
    b = gtk_button_new_with_label( "Gráficos…" );
    gtk_widget_set_tooltip_text( b,
        "De la ecuación marcada: la batería de fue sobre sus residuos, y las "
        "CCF con cada una de sus entradas." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_graficos), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_separator_new(
                            GTK_ORIENTATION_VERTICAL ), FALSE, FALSE, 6 );

    b = gtk_button_new_with_label( "Exportar…" );
    gtk_widget_set_tooltip_text( b,
        "La tabla de la PESTAÑA en la que estés —no de la ecuación—, a CSV, "
        "texto de ancho fijo o LaTeX. La extensión elige el formato.\n\nVa "
        "con la PROCEDENCIA dentro —series, modelo, muestra, logL y fichero—: "
        "una tabla que sale sin decir de dónde viene no se puede reproducir." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_exportar), m );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    /* A la derecha: el baseline del LR. */
    b = gtk_button_new_with_label( "Calcular baseline" );
    gtk_widget_set_tooltip_text( b,
        "Estima el modelo DIAGONAL y lo guarda como referencia: es el que "
        "está anidado —el completo con todos los ω = 0— y da las tres cifras "
        "de la izquierda de la pestaña Ajuste.\n\nNo hay que ir a Estimación "
        "a marcar nada: el botón pone el modo, lanza, y lo deja como estaba." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_baseline), m );
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    D->b_volver = gtk_button_new_with_label( "Ir a…" );
    gtk_widget_set_no_show_all( D->b_volver, TRUE );
    g_signal_connect( D->b_volver, "clicked", G_CALLBACK(on_volver), m );
    gtk_box_pack_end( GTK_BOX(barra), D->b_volver, FALSE, FALSE, 0 );

    /* --- las seis pestañas, EN EL ORDEN DE LAS PREGUNTAS --- */
    D->libreta = gtk_notebook_new();
    gtk_box_pack_start( GTK_BOX(caja), D->libreta, TRUE, TRUE, 0 );

    /* 1 exogeneidad */
    st = gtk_list_store_new( X_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    D->l_exo = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( D->l_exo, "Entrada",   X_ENL );
    columna( D->l_exo, "Q (k < 0)", X_Q );
    columna( D->l_exo, "p",         X_P );
    columna( D->l_exo, "signif.",   X_SIG );
    columna( D->l_exo, "Veredicto", X_VER );
    gtk_widget_set_tooltip_text( D->l_exo,
        "¿Transferencia o VARMA?\n\nRed dice si el .dag ADMITE orden de "
        "construcción: es estructural y se sabe antes de estimar. Esto dice si "
        "LOS DATOS lo sostienen. Un .dag acíclico puede no serlo en los datos, "
        "y entonces el escalón que toca es drvarma." );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), en_scroll( D->l_exo ),
                              gtk_label_new( "1 · Exogeneidad" ) );

    /* 2 adecuacion */
    st = gtk_list_store_new( A_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING );
    D->l_ade = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( D->l_ade, "Entrada",    A_ENL );
    columna( D->l_ade, "Q (k ≥ 0)",  A_Q );
    columna( D->l_ade, "p",          A_P );
    columna( D->l_ade, "Veredicto",  A_VER );
    gtk_widget_set_tooltip_text( D->l_ade,
        "¿La forma (b, r, s) agota la relación? Si queda señal en k ≥ 0, no: "
        "falta estructura en la transferencia, y se arregla en los órdenes." );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), en_scroll( D->l_ade ),
                              gtk_label_new( "2 · Adecuación" ) );

    /* 3 ajuste: el LR arriba, y debajo las dos cifras PASANDO DE una a otra */
    {
    GtkWidget *caja3 = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );

    D->l_lr = gtk_label_new( "Estima primero." );
    gtk_widget_set_halign( D->l_lr, GTK_ALIGN_START );
    gtk_label_set_line_wrap( GTK_LABEL(D->l_lr), TRUE );
    gtk_widget_set_margin_start( D->l_lr, 6 );
    gtk_widget_set_margin_top( D->l_lr, 6 );
    gtk_box_pack_start( GTK_BOX(caja3), D->l_lr, FALSE, FALSE, 0 );

    st = gtk_list_store_new( J_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING );
    D->l_aju = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( D->l_aju, "Ecuación",        J_ECU );
    columna( D->l_aju, "d.t. diagonal",   J_DTU );
    columna( D->l_aju, "d.t. con transf.", J_DTT );
    columna( D->l_aju, "R² diagonal",     J_R2U );
    columna( D->l_aju, "R² con transf.",  J_R2T );
    columna( D->l_aju, "varianza",        J_RED );
    columna( D->l_aju, "",                J_NOTA );
    gtk_widget_set_tooltip_text( D->l_aju,
        "El R² de Brajín (A.28), sobre la serie ESTACIONARIA:\n\n"
        "    R² = 1 − Σ(a−ā)² / Σ(w−w̄)²,   w = ∇ᵈ∇ₛᴰ z\n\n"
        "No sobre el nivel: sobre el nivel de una I(1) sale cerca de 1 por "
        "construcción y no dice nada. El denominador es propiedad de los "
        "DATOS —no lleva parámetros— así que es el mismo en las dos "
        "estimaciones, y eso es lo que hace comparables los dos R².\n\n"
        "Es una transición entre dos ajustes de UNA especificación, no una "
        "nota con la que ordenar modelos: entre d distintas, w es otra "
        "variable y dejan de ser comparables." );
    gtk_box_pack_start( GTK_BOX(caja3), en_scroll( D->l_aju ), TRUE, TRUE, 0 );

    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), caja3,
                              gtk_label_new( "3 · Ajuste" ) );
    }

    /* 4 residuos */
    st = gtk_list_store_new( R_N, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    D->l_res = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( D->l_res, "Ecuación",   R_ECU );
    columna( D->l_res, "n",          R_N_ );
    columna( D->l_res, "media",      R_MED );
    columna( D->l_res, "d.t.",       R_DT );
    columna( D->l_res, "asimetría",  R_ASI );
    columna( D->l_res, "curtosis",   R_CUR );
    columna( D->l_res, "Jarque-Bera", R_JB );
    columna( D->l_res, "Ljung-Box",  R_LB );
    columna( D->l_res, "fuera ±1 y ±2 · obs / esperado", R_NOR );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), en_scroll( D->l_res ),
                              gtk_label_new( "4 · Residuos" ) );

    /* 5 modelo estimado */
    st = gtk_list_store_new( P_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    D->l_par = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( D->l_par, "Parámetro", P_NOM );
    columna( D->l_par, "Estimado",  P_VAL );
    columna( D->l_par, "d.t.",      P_DT );
    columna( D->l_par, "t",         P_T );
    columna( D->l_par, "p",         P_P );
    columna( D->l_par, "",          P_DE );
    gtk_widget_set_tooltip_text( D->l_par,
        "Un parámetro que se mantuvo del .pre NO se estimó, y se dice: "
        "presentar como resultado algo que no se estimó sería la peor clase "
        "de mentira de una pantalla." );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), en_scroll( D->l_par ),
                              gtk_label_new( "5 · Modelo estimado" ) );

    /* 6 salida */
    D->t_out = texto_mono();
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), en_scroll( D->t_out ),
                              gtk_label_new( "6 · Salida" ) );

    /* --- los dos veredictos --- */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_widget_set_margin_top( vb, 2 );

    D->ver_global = gtk_label_new( "Estima primero." );
    D->ver_ojo    = gtk_label_new( "" );
    gtk_widget_set_halign( D->ver_global, GTK_ALIGN_START );
    gtk_widget_set_halign( D->ver_ojo,    GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(D->ver_global), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(D->ver_ojo),    PANGO_ELLIPSIZE_END );

    gtk_box_pack_start( GTK_BOX(vb), D->ver_global, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), D->ver_ojo,    FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), vb, FALSE, FALSE, 0 );

    return caja;
}
