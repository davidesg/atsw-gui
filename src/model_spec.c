/* model_spec.c */
#include "model_spec.h"
#include "fue_globals.h"
#include <string.h>

extern struct Tseries Ts;
extern struct Tusmodel Tm;

void on_series_name_changed(GtkEntry *entry, FueContext *ctx) {
    const char *name = gtk_entry_get_text(entry);
    /* Remove punctuation and spaces to generate input name */
    char *name2 = g_strdup(name);
    char *p = name2;
    while (*p) {
        if (ispunct(*p) || isspace(*p)) {
            /* skip */
        } else {
            *p++ = *p;
        }
    }
    *p = '\0';
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), name2);
    g_free(name2);
    Ts.name = g_strdup(name); /* store in Ts */
}

void on_freq_changed(GtkComboBox *combo, FueContext *ctx) {
    int freq = 1;
    const char *active = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (active) {
        if (strstr(active, "4")) freq = 4;
        else if (strstr(active, "12")) freq = 12;
        g_free((gchar*)active);
    }
    Ts.freq = freq;

    /* Adjust start period range */
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(ctx->start_period_spin), 1, freq);
    /* Show/hide season label? We'll just use spin button; season label is not used. */
    /* Update individual factors sensitivity */
    update_individual_factors_sensitivity(ctx);
    /* Also update the model tab: for monthly, show all 7 factors; for quarterly, only 0-3; for annual, hide all. */
}

void update_individual_factors_sensitivity(FueContext *ctx) {
    if (Ts.freq == 1) {
        gtk_widget_set_sensitive(ctx->f0_check, FALSE);
        gtk_widget_set_sensitive(ctx->f1_check, FALSE);
        gtk_widget_set_sensitive(ctx->f2_check, FALSE);
        gtk_widget_set_sensitive(ctx->f3_check, FALSE);
        gtk_widget_set_sensitive(ctx->f4_check, FALSE);
        gtk_widget_set_sensitive(ctx->f5_check, FALSE);
        gtk_widget_set_sensitive(ctx->f6_check, FALSE);
    } else if (Ts.freq == 4) {
        gtk_widget_set_sensitive(ctx->f0_check, TRUE);
        gtk_widget_set_sensitive(ctx->f1_check, TRUE);
        gtk_widget_set_sensitive(ctx->f2_check, TRUE);
        gtk_widget_set_sensitive(ctx->f3_check, TRUE);
        gtk_widget_set_sensitive(ctx->f4_check, FALSE);
        gtk_widget_set_sensitive(ctx->f5_check, FALSE);
        gtk_widget_set_sensitive(ctx->f6_check, FALSE);
    } else {
        gtk_widget_set_sensitive(ctx->f0_check, TRUE);
        gtk_widget_set_sensitive(ctx->f1_check, TRUE);
        gtk_widget_set_sensitive(ctx->f2_check, TRUE);
        gtk_widget_set_sensitive(ctx->f3_check, TRUE);
        gtk_widget_set_sensitive(ctx->f4_check, TRUE);
        gtk_widget_set_sensitive(ctx->f5_check, TRUE);
        gtk_widget_set_sensitive(ctx->f6_check, TRUE);
    }
}

void sync_individual_factors_from_ui(FueContext *ctx) {
    if (Ts.freq <= 1) return;
    for (int i = 0; i <= Ts.freq/2; i++) {
        GtkToggleButton *btn = NULL;
        switch (i) {
            case 0: btn = GTK_TOGGLE_BUTTON(ctx->f0_check); break;
            case 1: btn = GTK_TOGGLE_BUTTON(ctx->f1_check); break;
            case 2: btn = GTK_TOGGLE_BUTTON(ctx->f2_check); break;
            case 3: btn = GTK_TOGGLE_BUTTON(ctx->f3_check); break;
            case 4: btn = GTK_TOGGLE_BUTTON(ctx->f4_check); break;
            case 5: btn = GTK_TOGGLE_BUTTON(ctx->f5_check); break;
            case 6: btn = GTK_TOGGLE_BUTTON(ctx->f6_check); break;
        }
        if (btn) Tm.ifadf[i] = gtk_toggle_button_get_active(btn) ? 1 : 0;
    }
}

void sync_mean_from_ui(FueContext *ctx) {
    Tm.Imu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctx->mean_check)) ? 1 : 0;
    Tm.mu = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->mean_spin));
}

void sync_differences_from_ui(FueContext *ctx) {
    Tm.nrdiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->nrdiff_spin));
    Tm.nadiff = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->nadiff_spin));
}

void sync_boxcox_from_ui(FueContext *ctx) {
    Tm.boxlam = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin));
    Tm.boxm = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->boxcox_m_spin));
}

void sync_all_from_ui(FueContext *ctx) {
    sync_boxcox_from_ui(ctx);
    sync_differences_from_ui(ctx);
    sync_individual_factors_from_ui(ctx);
    sync_mean_from_ui(ctx);
    /* Also get Ts attributes */
    Ts.nobs = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->n_obs_spin));
    Ts.begtime = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_period_spin));
    Ts.begyear = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ctx->start_year_spin));
    Ts.name = gtk_entry_get_text(GTK_ENTRY(ctx->series_name_entry));
    Ts.refactor = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->refactor_spin));
    /* Frequency already set by on_freq_changed */
}

void update_ui_from_model(FueContext *ctx) {
    /* Series name and frequency */
    gtk_entry_set_text(GTK_ENTRY(ctx->series_name_entry), Ts.name ? Ts.name : "");
    if (Ts.freq == 1)
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 0);
    else if (Ts.freq == 4)
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 1);
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 2);

    /* Observations and dates */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->n_obs_spin), Ts.nobs);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_period_spin), Ts.begtime);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_year_spin), Ts.begyear);

    /* Box‑Cox & differences */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin), Tm.boxlam);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_m_spin), Tm.boxm);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->nrdiff_spin), Tm.nrdiff);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->nadiff_spin), Tm.nadiff);

    /* Individual factors (only if frequency > 1) */
    if (Ts.freq > 1) {
        for (int i = 0; i <= Ts.freq/2; i++) {
            GtkToggleButton *btn = NULL;
            switch (i) {
                case 0: btn = GTK_TOGGLE_BUTTON(ctx->f0_check); break;
                case 1: btn = GTK_TOGGLE_BUTTON(ctx->f1_check); break;
                case 2: btn = GTK_TOGGLE_BUTTON(ctx->f2_check); break;
                case 3: btn = GTK_TOGGLE_BUTTON(ctx->f3_check); break;
                case 4: btn = GTK_TOGGLE_BUTTON(ctx->f4_check); break;
                case 5: btn = GTK_TOGGLE_BUTTON(ctx->f5_check); break;
                case 6: btn = GTK_TOGGLE_BUTTON(ctx->f6_check); break;
            }
            if (btn) gtk_toggle_button_set_active(btn, Tm.ifadf[i] == 1);
        }
    }

    /* Mean */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->mean_spin), Tm.mu);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->mean_check), Tm.Imu);

    /* Rescaling factor */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->refactor_spin), Ts.refactor);
    populate_all_treeviews(ctx); /* new function in order to populate  the ui with a model */

    /* Populate tree views – these functions need to be implemented or reused */
    /* For now, we call the existing populating functions from the respective modules */
    /* We can reuse the functions from deterministic_dialog.c and operator_dialog.c */
    /* For simplicity, we'll assume they exist and are called here */
    /* Actually, we need to implement these populating functions or reuse the ones we have. */
    /* We'll call the populating functions that already exist (e.g., populate_intervention_treeview). */
    /* Since they are static in deterministic_dialog.c, we need to expose them or move them. */
    /* For now, we leave them as placeholders. */
}

void on_refactor_changed(GtkSpinButton *spin, FueContext *ctx) {
    Ts.refactor = gtk_spin_button_get_value(spin);
}


/* ========================================================================= */
/* Poblar todos los tree views a partir de las estructuras globales          */
/* ========================================================================= */
void populate_all_treeviews(FueContext *ctx) {
    GtkListStore *store;
    GtkTreeIter iter;

    /* --- Deterministic tree view --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->int_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NdetVar; i++) {
        const char *type_name = NULL;
        switch (It[i].type) {
            case 0: type_name = "Impulse"; break;
            case 1: type_name = "Compensated Impulse"; break;
            case 2: type_name = "Step"; break;
            case 3: type_name = "Ramp"; break;
            case 4: type_name = "Linear Trend"; break;
            case 5: type_name = "Easter Effect"; break;
            case 6: type_name = "Cos"; break;
            case 7: type_name = "Sin"; break;
            case 8: type_name = "Alter"; break;
            default: type_name = "Unknown";
        }
        gtk_list_store_append(store, &iter);
        if (It[i].type < 6) {
            gtk_list_store_set(store, &iter,
                               COL_NUM, i+1,
                               COL_NAME, type_name,
                               COL_SEASON, It[i].period,
                               COL_YEAR, It[i].year,
                               COL_MAOR, It[i].ma_order,
                               COL_AROR, It[i].ar_order,
                               -1);
        } else {
            gtk_list_store_set(store, &iter,
                               COL_NUM, i+1,
                               COL_NAME, type_name,
                               COL_SEASON, (int)It[i].freq,
                               COL_YEAR, 0,
                               COL_MAOR, It[i].ma_order,
                               COL_AROR, It[i].ar_order,
                               -1);
        }
    }

    /* --- Regular AR operators (Arr) --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->arr_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NopArr; i++) {
        int restrictions = 0;
        for (int j = 1; j <= Arr[i].order; j++)
            if (Arr[i].op_fixed && Arr[i].op_fixed[j] == 0) restrictions++;
        char restr[16];
        snprintf(restr, sizeof(restr), "%d", restrictions);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, restr,
                           2, Arr[i].order,
                           -1);
    }

    /* --- Annual AR operators (Ara) --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->ara_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NopAra; i++) {
        int restrictions = 0;
        for (int j = 1; j <= Ara[i].order; j++)
            if (Ara[i].op_fixed && Ara[i].op_fixed[j] == 0) restrictions++;
        char restr[16];
        snprintf(restr, sizeof(restr), "%d", restrictions);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, restr,
                           2, Ara[i].order,
                           -1);
    }

    /* --- Regular MA operators (Mar) --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->mar_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NopMar; i++) {
        int restrictions = 0;
        for (int j = 1; j <= Mar[i].order; j++)
            if (Mar[i].op_fixed && Mar[i].op_fixed[j] == 0) restrictions++;
        char restr[16];
        snprintf(restr, sizeof(restr), "%d", restrictions);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, restr,
                           2, Mar[i].order,
                           -1);
    }

    /* --- Annual MA operators (Maa) --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->maa_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NopMaa; i++) {
        int restrictions = 0;
        for (int j = 1; j <= Maa[i].order; j++)
            if (Maa[i].op_fixed && Maa[i].op_fixed[j] == 0) restrictions++;
        char restr[16];
        snprintf(restr, sizeof(restr), "%d", restrictions);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, restr,
                           2, Maa[i].order,
                           -1);
    }

    /* --- Fixed-frequency AR operators (Ar2f) --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->ar_fix_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NumAr2f; i++) {
        char restr[16];
        snprintf(restr, sizeof(restr), "%d", Ar2f[i].op_fixed == 0 ? 1 : 0);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, restr,
                           2, Ar2f[i].freq,
                           -1);
    }

    /* --- Fixed-frequency MA operators (Ma2f) --- */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->ma_fix_treeview)));
    gtk_list_store_clear(store);
    for (int i = 0; i < NumMa2f; i++) {
        char restr[16];
        snprintf(restr, sizeof(restr), "%d", Ma2f[i].op_fixed == 0 ? 1 : 0);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, restr,
                           2, Ma2f[i].freq,
                           -1);
    }
}
