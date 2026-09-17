/*
 * slots.c -- ver slots.h.
 *
 * add_slot, add_arma_slots, build_slots, find_slot y read_constraints vienen de
 * engines/drtran/src/drtran.c. La cuenta y la gramatica son las mismas: lo
 * unico que cambia es que los globales pasan a ser campos de SlotTable, y que
 * los errores se devuelven como HECHO en vez de escribirse a stderr, para que
 * el motor los cuente en ingles y el GUI en espanol.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "slots.h"

/* ------------------------------------------------------------------------ */
/* La tabla                                                                  */
/* ------------------------------------------------------------------------ */

static void add_slot(SlotTable *st, const char *fmt, ...)
{
    va_list ap;
    if (st->n >= SLOT_MAX) return;
    st->n++;
    va_start(ap, fmt);
    vsnprintf(st->name[st->n], sizeof st->name[0], fmt, ap);
    va_end(ap);
    st->kind[st->n]  = SLOT_FREE;
    st->alias[st->n] = 0;
    st->value[st->n] = 0.0;
    st->pa[st->n]    = 0;
    st->pb[st->n]    = 0;
    st->nlc[st->n]   = 0;
}

/* Nombres de los factores ARMA, en el MISMO orden que pack_ar/ma_factors */
static void add_arma_slots(SlotTable *st, struct Tusmodel *Tmi, int i, int is_ar)
{
    const char *sym = is_ar ? "phi" : "theta";
    int Num1 = is_ar ? Tmi->NumAr1  : Tmi->NumMa1;
    int Num2 = is_ar ? Tmi->NumAr2  : Tmi->NumMa2;
    int Numf = is_ar ? Tmi->NumAr1f : Tmi->NumMa1f;
    int *o1 = is_ar ? Tmi->p1 : Tmi->q1;
    int *o2 = is_ar ? Tmi->p2 : Tmi->q2;
    int **f1 = is_ar ? Tmi->Ia1  : Tmi->Im1;
    int **f2 = is_ar ? Tmi->Ia2  : Tmi->Im2;
    int  *ff = is_ar ? Tmi->Ia1f : Tmi->Im1f;
    int  *fr = is_ar ? Tmi->pfre1 : Tmi->qfre1;
    int k, j;

    for (k = 1; k <= Num1; k++)
        for (j = 1; j <= o1[k]; j++)
            if (f1[k][j] == 1) add_slot(st, "%s_%d[B^%d]", sym, i, j);
    for (k = 1; k <= Num2; k++)
        for (j = 1; j <= o2[k]; j++)
            if (f2[k][j] == 1) add_slot(st, "%s_%d[B^%d]", sym, i, j * Tmi->sper);
    for (k = 1; k <= Numf; k++)
        if (ff[k] == 1) add_slot(st, "%s_%d[f=%d]", sym, i, fr[k]);
}

/* Construye la tabla de slots EN EL MISMO ORDEN que el vector de parámetros */
void slots_build( SlotTable *st, struct Tusmodel *Tm, int nser,
                  const NetLink *lnk, int nlink, const SlotFix *fix )
{
    const int *fix_arma = fix ? fix->arma : NULL;
    const int *fix_det  = fix ? fix->det  : NULL;
    const int *fix_mu   = fix ? fix->mu   : NULL;
    int i, j, k;

    st->n = 0;

    /* El enlace j se numera 1..nlink en los nombres, como en el .out. */
    for (j = 0; j < nlink; j++) {
        for (k = 0; k <= lnk[j].s; k++) add_slot(st, "omega%d[%d]", j + 1, k);
        for (k = 1; k <= lnk[j].r; k++) add_slot(st, "delta%d[%d]", j + 1, k);
    }
    for (i = 1; i <= nser; i++) {
        if (fix_arma && fix_arma[i]) continue;
        add_arma_slots(st, &Tm[i], i, 1);
        add_arma_slots(st, &Tm[i], i, 0);
    }
    for (i = 1; i <= nser; i++) {
        int iv, kk;
        if (fix_det && fix_det[i]) continue;
        for (iv = 1; iv <= Tm[i].NdetVar; iv++) {
            for (kk = 0; kk <= Tm[i].Nomega[iv]; kk++)
                if (Tm[i].Imega[iv][kk] == 1)
                    add_slot(st, "omega_d%d[%d,%d]", i, iv, kk);
            for (kk = 1; kk <= Tm[i].Ndelta[iv]; kk++)
                if (Tm[i].Ielta[iv][kk] == 1)
                    add_slot(st, "delta_d%d[%d,%d]", i, iv, kk);
        }
    }
    /* La media: el .pre MANDA. Imu = 0 es una media FIJA, o sea
       especificacion, y entonces no hay parametro que estimar. La linea de
       ordenes (-M) puede clavar ademas una que el .pre dejaba libre, pero no
       al reves: nadie libera lo que el fichero declaro fijo.              */
    for (i = 1; i <= nser; i++) {
        if (!Tm[i].Imu)             continue;   /* el .pre dice que es fija */
        if (fix_mu && fix_mu[i])    continue;   /* o la linea de ordenes    */
        add_slot(st, "mu[%d]", i);
    }
    for (i = 2; i <= nser; i++)
        add_slot(st, "log(var%d/var1)", i);

    /* Covarianzas de las innovaciones. Van SIEMPRE al mapa, pero FIJAS EN CERO:
       la covarianza diagonal es el caso por defecto, y liberar una covarianza es
       una decision del analista, no algo que se active en bloque. m6-1 no libera
       las 15 de su sistema: libera TRES (sigma42, sigma62, sigma54). El fichero
       de restricciones lo dice en el mismo sitio y con el mismo lenguaje que
       todo lo demas:   q[4,2] = free                                          */
    for (i = 2; i <= nser; i++)
        for (j = 1; j < i; j++) {
            add_slot(st, "q[%d,%d]", i, j);
            st->kind[st->n]  = SLOT_FIXED;
            st->value[st->n] = 0.0;
        }
}

int slots_find( const SlotTable *st, const char *name )
{
    int i;
    for (i = 1; i <= st->n; i++)
        if (strcmp(st->name[i], name) == 0) return i;
    return 0;
}


int slots_nfree( const SlotTable *st )
{
    int i, n = 0;

    for (i = 1; i <= st->n; i++) if (st->kind[i] == SLOT_FREE) n++;
    return n;
}

/* Lo que el slot dice de si mismo, en el lenguaje del .cns. Un slot que esta
 * libre y nacio libre no dice nada: es el caso por defecto, y escribirlo seria
 * ruido. Las q[i,j] nacen FIJAS en cero, asi que una q libre SI dice algo.  */
int slots_line( const SlotTable *st, int i, char *out, size_t size )
{
    if (i < 1 || i > st->n) { if (size) out[0] = 0; return 0; }

    switch (st->kind[i]) {

    case SLOT_FREE:
        /* Solo las covarianzas nacen fijas; liberarlas es una decision. */
        if (strncmp(st->name[i], "q[", 2) != 0) { if (size) out[0] = 0; return 0; }
        snprintf(out, size, "%s = free", st->name[i]);
        return 1;

    case SLOT_FIXED:
        /* Una q[i,j] fija en cero es como nacio: tampoco dice nada. */
        if (strncmp(st->name[i], "q[", 2) == 0 && st->value[i] == 0.0) {
            if (size) out[0] = 0;
            return 0;
        }
        snprintf(out, size, "%s = %g", st->name[i], (double) st->value[i]);
        return 1;

    case SLOT_ALIAS:
        snprintf(out, size, "%s = %s", st->name[i], st->name[st->alias[i]]);
        return 1;

    case SLOT_PRODUCT:
        snprintf(out, size, "%s = %s%s * %s", st->name[i],
                 st->value[i] < 0.0 ? "-" : "",
                 st->name[st->pa[i]], st->name[st->pb[i]]);
        return 1;

    case SLOT_LINCOMB: {
        char  b[256];
        int   t, pos = 0;

        pos += snprintf(b + pos, sizeof b - pos, "%s =", st->name[i]);
        for (t = 0; t < st->nlc[i]; t++) {
            const char *sg = st->lc_sign[i][t] < 0.0 ? "- " : (t ? "+ " : "");

            pos += snprintf(b + pos, sizeof b - pos, " %s%s", sg,
                            st->name[st->lc_a[i][t]]);
            if (st->lc_b[i][t])
                pos += snprintf(b + pos, sizeof b - pos, " * %s",
                                st->name[st->lc_b[i][t]]);
        }
        snprintf(out, size, "%s", b);
        return 1;
    }
    }
    if (size) out[0] = 0;
    return 0;
}

/* ------------------------------------------------------------------------ */
/* El .cns                                                                   */
/* ------------------------------------------------------------------------ */

static int falla_cns( CnsError *e, CnsErr k, int line,
                      const char *lhs, const char *tok )
{
    if (e) {
        e->err  = k;
        e->line = line;
        snprintf(e->lhs,   sizeof e->lhs,   "%.39s", lhs ? lhs : "");
        snprintf(e->token, sizeof e->token, "%.127s", tok ? tok : "");
    }
    return -1;
}

const char *cns_error_en( const CnsError *e, char *out, size_t size )
{
    if (!e) { if (size) out[0] = 0; return out; }

    switch (e->err) {
    case CNS_OK:
        snprintf(out, size, "ok"); break;
    case CNS_ENOFILE:
        snprintf(out, size, "opening constraints file: %s", e->token); break;
    case CNS_EUNKNOWN:
        snprintf(out, size, "unknown parameter '%s' in the constraints file",
                 e->token); break;
    case CNS_EOPERAND:
        snprintf(out, size, "unknown operand in product '%s = %s'",
                 e->lhs, e->token); break;
    case CNS_ESELF:
        snprintf(out, size, "'%s' cannot be defined in terms of itself",
                 e->lhs); break;
    case CNS_ELC:
        snprintf(out, size, "cannot parse linear combination '%s = %s'",
                 e->lhs, e->token); break;
    case CNS_EPARSE:
        snprintf(out, size, "cannot parse '%s = %s'", e->lhs, e->token); break;
    }
    return out;
}

/* Lee el fichero de restricciones:
     NOMBRE = NOMBRE   compartir (un solo grado de libertad en varios sitios)
     NOMBRE = valor    fijar
     NOMBRE = free     liberar (las covarianzas q[i,j] nacen fijas en cero)
   Comentarios con '#'.                                                       */
int cns_read( const char *path, SlotTable *st, CnsError *e )
{
    FILE *f;

    if (e) { e->err = CNS_OK; e->line = 0; e->lhs[0] = 0; e->token[0] = 0; }
    f = fopen(path, "r");
    char line[256];
    int nc = 0, nlin = 0;

    if (f == NULL) {
        return falla_cns(e, CNS_ENOFILE, 0, "", path);
    }

    while (fgets(line, sizeof line, f)) {
        char lhs[64], rhs[64], rhsfull[128];
        char *hash = strchr(line, '#');
        char *star;
        int a, b;
        double v;

        nlin++;
        if (hash) *hash = '\0';
        if (sscanf(line, " %63[^= \t] = %127[^\n]", lhs, rhsfull) != 2) continue;

        a = slots_find(st, lhs);
        if (a == 0) {
            fclose(f);
            return falla_cns(e, CNS_EUNKNOWN, nlin, lhs, lhs);
        }

        /* COMBINACION LINEAL:  x = [±]t1 [±]t2 ...  con ti = slot o slot*slot.
           Generaliza el PRODUCTO a sumas/diferencias de terminos.  Cubre el factor
           FIJO (1−B) de una FLT — que impone nu_num(1)=0, i.e. omega[0]=omega[1]+
           omega[2]+... — y los coeficientes producto±termino de un numerador
           factorizado (p.ej. x12*x14 − x13 del legacy).  Se detecta por un
           separador +/- interno (los nombres de slot no llevan +/-).  El gradiente
           lo capta cdgrad por diferencias finitas, como el producto.              */
        {
            char *q = rhsfull;
            int   is_lc = 0;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '+' || *q == '-') q++;          /* signo inicial: no separa */
            for (; *q; q++) if (*q == '+' || *q == '-') { is_lc = 1; break; }
            if (is_lc) {
                char *p = rhsfull;
                real  sg = 1.0;
                int   nt = 0, ok = 1;
                while (*p) {
                    char tok[80], f1[64], f2[64], *w, *ast;   /* *st -> *w: el
                             puntero de escritura se llamaba igual que la tabla */
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p == '+') { sg =  1.0; p++; continue; }
                    if (*p == '-') { sg = -1.0; p++; continue; }
                    if (!*p) break;
                    w = tok;                           /* leer termino hasta +/- o fin */
                    while (*p && *p != '+' && *p != '-' &&
                           (size_t)(w - tok) < sizeof tok - 1) *w++ = *p++;
                    *w = '\0';
                    if (nt >= SLOT_LC_TERMS) { ok = 0; break; }
                    ast = strchr(tok, '*');
                    if (ast) {
                        int s1, s2;
                        *ast = '\0';
                        if (sscanf(tok, " %63s", f1) != 1 ||
                            sscanf(ast + 1, " %63s", f2) != 1) { ok = 0; break; }
                        s1 = slots_find(st, f1); s2 = slots_find(st, f2);
                        if (!s1 || !s2 || s1 == a || s2 == a) { ok = 0; break; }
                        st->lc_sign[a][nt] = sg;
                        st->lc_a[a][nt] = s1; st->lc_b[a][nt] = s2; nt++;
                    } else {
                        int s1;
                        if (sscanf(tok, " %63s", f1) != 1) { ok = 0; break; }
                        s1 = slots_find(st, f1);
                        if (!s1 || s1 == a) { ok = 0; break; }
                        st->lc_sign[a][nt] = sg;
                        st->lc_a[a][nt] = s1; st->lc_b[a][nt] = 0; nt++;
                    }
                }
                if (!ok || nt < 1) {
                    fclose(f);
                    return falla_cns(e, CNS_ELC, nlin, lhs, rhsfull);
                }
                st->kind[a] = SLOT_LINCOMB;
                st->nlc[a]  = nt;
                nc++;
                continue;
            }
        }

        /* PRODUCTO:  x = [-] y * z.  El coeficiente ES el producto de otros dos
           slots, con un signo opcional (numerador factorizado del legacy: p.ej.
           omega1[1] = -omega1[0] * theta_4 reproduce -x5*(1-x6B) con x6 compartido
           con la MA del input).  El gradiente lo maneja cdgrad por diferencias
           finitas: no hace falta regla de la cadena analitica.                    */
        star = strchr(rhsfull, '*');
        if (star) {
            char pa[64], pb[64];
            char *p = rhsfull;
            real sign = 1.0;
            *star = '\0';
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '-') { sign = -1.0; p++; while (*p == ' ' || *p == '\t') p++; }
            if (sscanf(p, "%63s", pa) == 1 && sscanf(star + 1, " %63s", pb) == 1) {
                int sa = slots_find(st, pa), sb = slots_find(st, pb);
                if (sa == 0 || sb == 0) {
                    char both[128];
                    snprintf(both, sizeof both, "%.60s * %.60s", pa, pb);
                    fclose(f);
                    return falla_cns(e, CNS_EOPERAND, nlin, lhs, both);
                }
                if (sa == a || sb == a) {
                    fclose(f);
                    return falla_cns(e, CNS_ESELF, nlin, lhs, lhs);
                }
                st->kind[a]  = SLOT_PRODUCT;
                st->pa[a] = sa;  st->pb[a] = sb;
                st->value[a] = sign;         /* +1 o -1: el signo del producto */
                nc++;
                continue;
            }
        }

        if (sscanf(rhsfull, " %63s", rhs) != 1) continue;

        if (strcmp(rhs, "free") == 0) {     /* LIBERAR (una covarianza) */
            st->kind[a]  = SLOT_FREE;
            st->alias[a] = 0;
            nc++;
            continue;
        }

        b = slots_find(st, rhs);
        if (b != 0) {                       /* COMPARTIDO */
            /* seguir la cadena hasta el representante final */
            while (st->kind[b] == SLOT_ALIAS) b = st->alias[b];
            if (b == a) {
                fclose(f);
                return falla_cns(e, CNS_ESELF, nlin, lhs, lhs);
            }
            st->kind[a]  = SLOT_ALIAS;
            st->alias[a] = b;
        } else if (sscanf(rhs, "%lf", &v) == 1) {   /* FIJO */
            st->kind[a]  = SLOT_FIXED;
            st->value[a] = v;
        } else {
            fclose(f);
            return falla_cns(e, CNS_EPARSE, nlin, lhs, rhs);
        }
        nc++;
    }
    fclose(f);
    return nc;
}
int cns_write( const char *path, const SlotTable *st, const char *cabecera )
{
    FILE *f = fopen(path, "w");
    char  b[256];
    int   i, n = 0;

    if (!f) return -1;

    if (cabecera) fprintf(f, "# %s\n", cabecera);
    fprintf(f, "# Las covarianzas q[i,j] nacen FIJAS en cero: liberar una es\n"
               "# una decision del analista, no algo que se active en bloque.\n");

    for (i = 1; i <= st->n; i++)
        if (slots_line(st, i, b, sizeof b)) { fprintf(f, "%s\n", b); n++; }

    return fclose(f) == 0 ? n : -1;
}
