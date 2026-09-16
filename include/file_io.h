
#ifndef FILE_IO_H
#define FILE_IO_H

#include <gtk/gtk.h>
#include "fue_context.h"
#include "engine.h"

void save_inp_file(FueContext *ctx);
void load_input_fue(const char *inputf);   /* single argument */

void on_new_file(GtkToolButton *btn, FueContext *ctx);
void on_save_inp(GtkToolButton *btn, FueContext *ctx);
void on_run_fue(GtkWidget *widget, FueContext *ctx);
void on_view_output(GtkWidget *widget, FueContext *ctx);
void on_view_inp(GtkWidget *widget, FueContext *ctx);
void on_quit(GtkWidget *widget, FueContext *ctx);

void on_edit_inp_clicked(GtkButton *button, FueContext *ctx);
void on_save_inp_clicked(GtkButton *button, FueContext *ctx);
void load_output_to_console(FueContext *ctx);
/* Lo que el motor escribio, a la consola: cuando no deja fichero de salida
 * es lo unico que explica por que.                                        */
void show_engine_output(FueContext *ctx, const EngineResult *r);
void on_forecast_button_clicked(GtkToolButton *btn, FueContext *ctx);
#endif
