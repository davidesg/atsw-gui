/* file_io.c */
#include "file_io.h"
#include "fue_globals.h"
//#include "fue_core.h"
#include "model_spec.h"
#include "utils.h"
#include "inpcheck.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "engine.h"         // ejecutar fue/fuf sin shell, con sus codigos
#include "preview.h"        // la ventana de graficos
#include "outfile.h"        // lo que se lee del .out
#include "main_window.h"    // update_model_label: estaba implicita

#ifdef _WIN32
#include <windows.h>
#endif

/* ========================================================================= */
/* Helper to write the .inp file using the current model (Ts, Tm, etc.)     */
/* ========================================================================= */
/* Los deterministas no estandar: cuantos hay y en que columna va cada uno.
 * Los rellena load_input_fue(); el escritor los necesita para volver a
 * poner sus datos detras de la serie, que si no se pierden. Estaban
 * declarados DENTRO de load_input_fue, asi que el escritor no podia verlos.
 */
static int nstdet = 0;
static int *det = NULL;

/* El nombre con el que venia cada determinista no estandar: el formato no lo
 * fija (fue escribe "non-standard", art escribe "custom") y hay que devolver
 * el que se leyo, no inventar otro.                                       */
#define MAX_DET 50
static char *nonstd_name[MAX_DET];

static void write_inp_file(FILE *f, FueContext *ctx) {
    /* Copy of the old SaveInpFile_fue logic, but using ctx to get values */
    /* We'll write the same format as the original */

    fprintf(f, "************************************************\n");
    fprintf(f, "* Input file for program FUE                   *\n");
    fprintf(f, "* DOCTYPE ATSW-interface SYSTEM                *\n");
    fprintf(f, "************************************************\n\n");
    fprintf(f, "** Frequency of time series: either 1(A), 4(Q) or 12(M):\n");
    fprintf(f, " %d\n", Ts.freq);
    fprintf(f, "** Number of observations and starting date of time series:\n");
    fprintf(f, " %d", Ts.nobs);
    if (Ts.freq > 1)
        fprintf(f, " %2d", Ts.begtime);
    else
        fprintf(f, " %2d", Ts.outyear);
    fprintf(f, " %2d ", Ts.begyear);
    {
    /* un token: los motores lo leen con %s. Se quitan los espacios y nada
     * mas -- "PE/PU" es un nombre legitimo.                              */
    char *token = single_token(Ts.name ? Ts.name : "");
    fprintf(f, "%s\n", *token ? token : "series");
    g_free(token);
    }
    fprintf(f, "** Number of deterministic variables (including seasonal components):\n");
    fprintf(f, "%d\n", NdetVar);
    if (NdetVar > 0) {
        fprintf(f, "**\n");
        for (int i = 0; i < NdetVar; i++) {
            const char *type_str;
            switch (It[i].type) {
                case 0: type_str = "impulse"; break;
                case 1: type_str = "compimp"; break;
                case 2: type_str = "step"; break;
                case 3: type_str = "ramp"; break;
                case 4: type_str = "trend"; break;
                case 5: type_str = "easter"; break;
                case 6: type_str = "cos"; break;
                case 7: type_str = "sin"; break;
                case 8: type_str = "alter"; break;
                case 9: type_str = (i < MAX_DET && nonstd_name[i]) ? nonstd_name[i]
                                                                  : "non-standard";
                        break;
                default: type_str = "unknown";
            }
            fprintf(f, "%s", type_str);
            /* Que lleva cada tipo detras del nombre, segun lo que el motor
             * lee (fue.c [3.2]): los cuatro fechados llevan fecha; trend,
             * easter y alter no llevan nada -- el motor hace fscanf("\n") y
             * pasa a la siguiente palabra--; cos y sin llevan el armonico.
             * Antes, la guarda era `type < 6` y metia una fecha detras de
             * trend y de easter, que no la tienen: salia "trend 0 0", que el
             * propio inpcheck rechaza ("...not '0'").                     */
            if (It[i].type <= 3) {                      /* impulse..ramp    */
                if (Ts.freq > 1)
                    fprintf(f, " %d %d\n", It[i].period, It[i].year);
                else
                    fprintf(f, " %d\n", It[i].year);
            } else if (It[i].type == 6 || It[i].type == 7) {   /* cos, sin  */
                char buf[64];
                /* el armonico es del usuario, no un entero nuestro */
                fprintf(f, " %s\n", inp_format(buf, sizeof(buf), It[i].freq));
            } else {                      /* trend, easter, alter, y el 9   */
                fprintf(f, "\n");
            }
        }
        fprintf(f, "**\n");
        for (int i = 0; i < NdetVar; i++)
            fprintf(f, "%d ", It[i].ma_order);
        fprintf(f, "\n");
        fprintf(f, "**\n");
        for (int i = 0; i < NdetVar; i++) {
            for (int j = 0; j <= It[i].ma_order; j++) {
                fprintf(f, "%.6f", It[i].ma_parameter[j]);
                fprintf(f, "  %d\n", It[i].ma_fixed[j]);
            }
            fprintf(f, "**\n");
        }
        for (int i = 0; i < NdetVar; i++)
            fprintf(f, "%d ", It[i].ar_order);
        fprintf(f, "\n");
        for (int i = 0; i < NdetVar; i++) {
            if (It[i].ar_order > 0) {
                fprintf(f, "**\n");
                for (int j = 1; j <= It[i].ar_order; j++) {
                    fprintf(f, "%.6f", It[i].ar_parameter[j]);
                    fprintf(f, "  %d\n", It[i].ar_fixed[j]);
                }
            }
        }
    }
    /* Operators: Arr, Ara, Mar, Maa, Ar2f, Ma2f */
    fprintf(f, "** Number and orders of regular AR operators:\n");
    fprintf(f, "%d", NopArr);
    if (NopArr > 0) {
        for (int i = 0; i < NopArr; i++) fprintf(f, " %d", Arr[i].order);
        fprintf(f, "\n");
        for (int i = 0; i < NopArr; i++) {
            fprintf(f, "**\n");
            for (int j = 1; j <= Arr[i].order; j++) {
                fprintf(f, "%.6f", Arr[i].op_parameter[j]);
                fprintf(f, "  %d\n", Arr[i].op_fixed[j]);
            }
        }
    } else fprintf(f, "\n");

    fprintf(f, "** Number and orders of annual AR operators:\n");
    fprintf(f, "%d", NopAra);
    if (NopAra > 0) {
        for (int i = 0; i < NopAra; i++) fprintf(f, " %d", Ara[i].order);
        fprintf(f, "\n");
        for (int i = 0; i < NopAra; i++) {
            fprintf(f, "**\n");
            for (int j = 1; j <= Ara[i].order; j++) {
                fprintf(f, "%.6f", Ara[i].op_parameter[j]);
                fprintf(f, "  %d\n", Ara[i].op_fixed[j]);
            }
        }
    } else fprintf(f, "\n");

    fprintf(f, "** Number and orders of regular MA operators:\n");
    fprintf(f, "%d", NopMar);
    if (NopMar > 0) {
        for (int i = 0; i < NopMar; i++) fprintf(f, " %d", Mar[i].order);
        fprintf(f, "\n");
        for (int i = 0; i < NopMar; i++) {
            fprintf(f, "**\n");
            for (int j = 1; j <= Mar[i].order; j++) {
                fprintf(f, "%.6f", Mar[i].op_parameter[j]);
                fprintf(f, "  %d\n", Mar[i].op_fixed[j]);
            }
        }
    } else fprintf(f, "\n");

    fprintf(f, "** Number and orders of anual MA operators:\n");
    fprintf(f, "%d", NopMaa);
    if (NopMaa > 0) {
        for (int i = 0; i < NopMaa; i++) fprintf(f, " %d", Maa[i].order);
        fprintf(f, "\n");
        for (int i = 0; i < NopMaa; i++) {
            fprintf(f, "**\n");
            for (int j = 1; j <= Maa[i].order; j++) {
                fprintf(f, "%.6f", Maa[i].op_parameter[j]);
                fprintf(f, "  %d\n", Maa[i].op_fixed[j]);
            }
        }
    } else fprintf(f, "\n");

    fprintf(f, "** Number and frequencies of regular AR(2) operators with fixed frequency:\n");
    fprintf(f, "%d", NumAr2f);
    if (NumAr2f > 0) {
        for (int i = 0; i < NumAr2f; i++) {
            char buf[64];   /* k puede ser fraccionario: viene del fichero */
            fprintf(f, " %s", inp_format(buf, sizeof(buf), Ar2f[i].freq));
        }
        fprintf(f, "\n**");
        for (int i = 0; i < NumAr2f; i++) {
            fprintf(f, "\n%.6f", Ar2f[i].op_parameter);
            fprintf(f, " %d\n**", Ar2f[i].op_fixed);
        }
    } else fprintf(f, "\n");

    fprintf(f, "** Number and frequencies of regular MA(2) operators with fixed frequency:\n");
    fprintf(f, "%d", NumMa2f);
    if (NumMa2f > 0) {
        for (int i = 0; i < NumMa2f; i++) {
            char buf[64];
            fprintf(f, " %s", inp_format(buf, sizeof(buf), Ma2f[i].freq));
        }
        fprintf(f, "\n**");
        for (int i = 0; i < NumMa2f; i++) {
            fprintf(f, "\n%.6f", Ma2f[i].op_parameter);
            fprintf(f, " %d\n**", Ma2f[i].op_fixed);
        }
    } else fprintf(f, "\n");

    /* Mean */
    fprintf(f, "** Mean parameter (mu):\n");
    {
    char buf[64];
    /* El valor SIEMPRE, y la bandera aparte. Antes, con la media fija se
     * escribia un "0" pelado que tiraba el valor ademas de la bandera: una
     * media fija en -88.72 volvia fijada en cero, que es otro modelo. El
     * formato lo admite -- los dos lectores leen "valor bandera" -- y es
     * solo el escritor el que no lo escribia.                            */
    fprintf(f, "%s %d\n", inp_format(buf, sizeof(buf), Tm.mu), Tm.Imu ? 1 : 0);
    }

    /* Box‑Cox and differences */
    fprintf(f, "** Box-Cox lambda, m. Regular differences and complete annual differences:\n");
    {
    char buf[64];   /* lambda tambien es del usuario: 1/3 no cabe en %2.2f */
    fprintf(f, " %s", inp_format(buf, sizeof(buf), Tm.boxlam));
    }
    fprintf(f, " %2d", Tm.nrdiff);
    fprintf(f, " %2d\n", Tm.nadiff);
    fprintf(f, "** Individual factors of the annual difference (starting at freq 0.0):\n");
    if (Ts.freq > 1) {
        for (int i = 0; i <= Ts.freq/2; i++) fprintf(f, " %d", Tm.ifadf[i]);
    } else fprintf(f, " 0");
    fprintf(f, "\n");

    /* ACF/PACF bands and rescaling factor */
    fprintf(f, "** ACF/PACF bands (0 Automatic) and reescaling factor:\n");
    fprintf(f, " 0 %.2f\n", Ts.refactor);

 /* Time series data */
fprintf(f, "** Time series (stochastic and non-standard deterministic variables):\n");
if (!Ts.data || Ts.nobs == 0) {
    fprintf(f, "\n");
} else {
    char buf[64];

    for (int i = 1; i <= Ts.nobs; i++) {
        /* Los datos son del usuario, no una estimacion: se escriben con las
         * cifras que hagan falta para que vuelvan a leerse iguales. Con el
         * "%lf" de antes, 0.3680397019 salia 0.368040.                    */
        fprintf(f, "%s", inp_format(buf, sizeof(buf), Ts.data[i]));
        /* Y detras, una columna por cada determinista NO ESTANDAR: son
         * datos que trae el fichero, no una receta, asi que si no se
         * vuelven a escribir se pierden. El lector los espera aqui.      */
        for (int k = 1; k <= nstdet; k++)
            fprintf(f, " %s", inp_format(buf, sizeof(buf), DataMat[det[k]][i]));
        fprintf(f, "\n");
    }
}
}

/* ========================================================================= */
/* Save the current model to an .inp file using the context                 */
/* ========================================================================= */

void save_inp_file(FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    if (!input_name || strlen(input_name) == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Please enter an input name.");
        return;
    }
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No workspace folder selected.");
        return;
    }
    char *inp_filename = g_strdup_printf("%s.inp", input_name);
    char *inp_path = g_build_filename(workspace, inp_filename, NULL);

    FILE *f = NULL;

#ifdef _WIN32
    wchar_t *wpath = g_utf8_to_utf16(inp_path, -1, NULL, NULL, NULL);
    if (wpath) {
        f = _wfopen(wpath, L"w");
        g_free(wpath);
    }
#else
    f = g_fopen(inp_path, "w");
#endif

    if (!f) {
        gchar *msg = g_strdup_printf("Error opening %s for writing: %s", inp_path, strerror(errno));
        gtk_label_set_text(GTK_LABEL(ctx->status_label), msg);
        g_free(msg);
        g_free(inp_path);
        g_free(inp_filename);
        g_free(workspace);
        return;
    }

    write_inp_file(f, ctx);
    fclose(f);
    g_free(inp_path);
    g_free(inp_filename);
    g_free(workspace);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), "INP file saved.");
    update_model_label(ctx);
}

/* ========================================================================= */
/* Libera toda la memoria de las estructuras globales antes de cargar otro  */
/* modelo. Evita fugas y accesos a punteros inválidos.                      */
/* ========================================================================= */
static void free_model_globals(void) {
    /* Intervenciones */
    for (int i = 0; i < NdetVar; i++) {
        if (It[i].ma_parameter) free_vector(It[i].ma_parameter, 0, It[i].ma_order);
        if (It[i].ma_fixed)     free_ivector(It[i].ma_fixed, 0, It[i].ma_order);
        if (It[i].ar_parameter) free_vector(It[i].ar_parameter, 1, It[i].ar_order);
        if (It[i].ar_fixed)     free_ivector(It[i].ar_fixed, 1, It[i].ar_order);
    }
    /* Operadores AR regulares */
    for (int i = 0; i < NopArr; i++) {
        if (Arr[i].op_parameter) free_vector(Arr[i].op_parameter, 1, Arr[i].order);
        if (Arr[i].op_fixed)     free_ivector(Arr[i].op_fixed, 1, Arr[i].order);
    }
    /* Operadores AR anuales */
    for (int i = 0; i < NopAra; i++) {
        if (Ara[i].op_parameter) free_vector(Ara[i].op_parameter, 1, Ara[i].order);
        if (Ara[i].op_fixed)     free_ivector(Ara[i].op_fixed, 1, Ara[i].order);
    }
    /* Operadores MA regulares */
    for (int i = 0; i < NopMar; i++) {
        if (Mar[i].op_parameter) free_vector(Mar[i].op_parameter, 1, Mar[i].order);
        if (Mar[i].op_fixed)     free_ivector(Mar[i].op_fixed, 1, Mar[i].order);
    }
    /* Operadores MA anuales */
    for (int i = 0; i < NopMaa; i++) {
        if (Maa[i].op_parameter) free_vector(Maa[i].op_parameter, 1, Maa[i].order);
        if (Maa[i].op_fixed)     free_ivector(Maa[i].op_fixed, 1, Maa[i].order);
    }
    /* Los operadores de frecuencia fija (Ar2f, Ma2f) no usan vectores dinámicos */

    if (Ts.data) free_vector(Ts.data, 1, Ts.nobs);
    if (Tm.ifadf && Ts.freq > 1) free_ivector(Tm.ifadf, 0, Ts.freq/2);

    /* Reiniciar contadores */
    NdetVar = NopArr = NopAra = NopMar = NopMaa = NumAr2f = NumMa2f = 0;
}

/* ========================================================================= */
/* Load a complete model from an .inp or .pre file (from callbacks.c)       */
/* ========================================================================= */
void
load_input_fue ( const char *inputf )
{
const  double PI = 3.141592654;
const  int  NT = 10;                  /* Maximum number of non-standard   */
                                      /* detvars (handle otherwise).      */

STRING	dumstrg, series_name, model_residuals;
FILE   *inputv;
dumstrg  = NEW_STR( MAXSTR );
series_name = NEW_STR( 80 );
model_residuals = NEW_STR( 80 );
int i, j, i1, i2, i3, i4;


/* The following variables have to do with the quasi-Newton optimizer:       */
int  npar, nparma;
double r1;

#ifdef _WIN32
    wchar_t *wpath = g_utf8_to_utf16(inputf, -1, NULL, NULL, NULL);
    inputv = wpath ? _wfopen(wpath, L"r") : NULL;
    g_free(wpath);
#else
    inputv = fopen(inputf, "r");
#endif

   if ( NULL == inputv)
      {
      /* Esto era exit(1): se llevaba el GUI por delante.                 */
      g_warning( "Error opening input file: %s", inputf );
      return;
      }
   npar   = 0;                /* Total number of parameters:                 */
   nparma = 0;                /* Number of parameters of the ARMA structure: */

free_model_globals();

/* [3.0]: Read the first six lines (may contain anything):                   */

   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );

/* [3.1]: Read seasonal period, number of observations and starting date:    OK */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%s\n", dumstrg );
    if ( strcmp( dumstrg, "number" ) == 0 ) {
	Ts.freq = 1;
	Ts.numbering = 1;
	}
    else {
	sscanf( dumstrg, "%u", &Ts.freq);
	Ts.numbering = 0;
	}

/*   fscanf( inputv, "%d\n", &Ts.freq ); */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Ts.nobs );
   if ( Ts.freq > 1 )
      {
      fscanf( inputv, "%d", &Ts.begtime );
      fscanf( inputv, "%d", &Ts.begyear );
      fscanf( inputv, "%s", series_name );
      fscanf( inputv, "%s\n", model_residuals );
      }
   else
      {
      fscanf( inputv, "%d", &Ts.outyear );
      fscanf( inputv, "%d", &Ts.begyear );
      fscanf( inputv, "%s", series_name  );
      fscanf( inputv, "%s\n", model_residuals );
      Ts.begtime = 1;
      }

      ts_set_name( series_name );
      Tm.residuals = g_strconcat( model_residuals, NULL );
 //  strcpy( Ts.name,  series_name);


   Ts.data = vector( 1, Ts.nobs );                      /* Time series data: */

/* [3.2]: Read deterministic structure of time series model:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d\n", &Tm.NdetVar );               /* N§ of detvars:    */
   NdetVar = Tm.NdetVar;

   DataMat = matrix( 0, Tm.NdetVar, 1, Ts.nobs );       /* Working data set: */

   if ( Tm.NdetVar > 0 )
      {

      fgets( dumstrg, MAXSTR, inputv );

 /* [3.2.0]: Read and generate list of detvars (store as DataMat):         */

      nstdet = 0;                   /* Non-standard detvars (read from input */
      det    = ivector( 1, NT );    /* file as indexed by det):              */

      for ( i = 1; i <= Tm.NdetVar; i++ )
          {
          fscanf( inputv, "%s", dumstrg );
          for ( j = 1; j <= Ts.nobs; j++ ) DataMat[i][j]  = 0.0;

   /* Generate a unit impulse:                                               */

          if ( strcmp( dumstrg, "impulse" ) == 0 )
             {

             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs) ) DataMat[i][i3] = 1.0;
	     It[ i - 1 ].type = 0;
	     It[ i - 1 ].period   = i1;
	     It[ i - 1 ].year     = i2;
             }

   /* Generate a unit compensated impulse:                                   */

          else if ( strcmp( dumstrg, "compimp" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs) )   DataMat[i][i3]   =  1.0;
             if ( (i3 >= 0) && (i3+1 <= Ts.nobs) ) DataMat[i][i3+1] = -1.0;
	     It[ i - 1 ].type = 1;
	     It[ i - 1 ].period   = i1;
	     It[ i - 1 ].year     = i2;
             }

   /* Generate a unit step:                                                  */

          else if ( strcmp( dumstrg, "step" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs) )
                for ( j = i3; j <= Ts.nobs; j++ ) DataMat[i][j] = 1.0;
	     It[ i - 1 ].type = 2;
	     It[ i - 1 ].period   = i1;
	     It[ i - 1 ].year     = i2;
             }

   /* Generate a unit ramp:                                                  */

          else if ( strcmp( dumstrg, "ramp" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs) )
                for ( j = i3; j <= Ts.nobs; j++ ) DataMat[i][j] = j - i3 + 1;
	     It[ i - 1 ].type = 3;
	     It[ i - 1 ].period   = i1;
	     It[ i - 1 ].year     = i2;
             }

   /* Generate a variable representing the Easter holiday:                   */

          else if ( (strcmp( dumstrg, "easter" ) == 0) && (Ts.freq == 12) )
             {
             for ( j = 1; j <= Ts.nobs; j++ )
                 {
                 ObsToDate( Ts.begyear, Ts.begtime, j, Ts.freq, &i1, &i2 );
                 Easter( &i3, &i4, i1 );
                 if ( (i4 == 4) && (i2 == i4) && (i3 >= 4) )
                    DataMat[i][j] = 1.0;
                 else if ( (i4 == 4) && (i2 == i4) && (i3 < 4) )
                    {
                    DataMat[i][j] = 0.5;
                    if ( j > 1 )
                       DataMat[i][j-1] = 0.5;
                    }
                 else if ( (i4 == 3) && (i2 == i4) )
                    DataMat[i][j] = 1.0;
                 }

	     It[ i - 1 ].type = 5;
             fscanf( inputv, "\n" );
             }

   /* Generate a deterministic linear trend:                                 */

          else if ( strcmp( dumstrg, "trend" ) == 0 )
             {
             for ( j = 1; j <= Ts.nobs; j++ ) DataMat[i][j] = j;
	     It[ i - 1 ].type = 4;
             fscanf( inputv, "\n" );
             }

   /* Generate a cosine seasonal component:                                  */

          else if ( strcmp( dumstrg, "cos" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             for ( j = 1; j <= Ts.nobs; j++ )
                 DataMat[i][j] = cos( 2.0 * PI * r1 / Ts.freq * j );
	     It[ i - 1 ].type = 6;
	     It[ i - 1 ].freq   = r1;
             }

   /* Generate a sine seasonal component:                                    */

          else if ( strcmp( dumstrg, "sin" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             for ( j = 1; j <= Ts.nobs; j++ )
                 DataMat[i][j] = sin( 2.0 * PI * r1 / Ts.freq * j );
	     It[ i - 1 ].type = 7;
	     It[ i - 1 ].freq   = r1;
             }

   /* Generate an "alternator" seasonal component:                           */

          else if ( strcmp( dumstrg, "alter" ) == 0 )
             {
             for ( j = 1; j <= Ts.nobs; j++ ) DataMat[i][j] = pow( -1.0, j );
             fscanf( inputv, "\n" );
	     It[ i - 1 ].type = 8;
	     It[ i - 1 ].freq = Ts.freq/2;
             }

   /* Read non-standard (unknown) deterministic variable from input file:    */

          else
             {
             nstdet += 1;          /* Update number of non-standard detvars: */
             /* det es ivector( 1, NT ) con NT = 10 y nonstd_name tiene
              * MAX_DET: sin cota, el numero once escribia fuera del bloque.
              * inpcheck para el primero e inp_ok_to_load el segundo; esto es
              * el cinturon, para cuando se llame al lector sin pasar por la
              * puerta -- como hace el banco de conformidad.               */
             if ( nstdet > NT || i - 1 >= MAX_DET ) { nstdet--; break; }
             det[nstdet] = i;
             /* Tiene tipo propio: sin el se quedaba en 0, que el escritor
              * interpreta como "impulse", y al guardar salia
              * "impulse 0 0" -- un impulso en el ano cero. Su nombre, tal
              * como venia, se guarda para volver a escribirlo.           */
             It[ i - 1 ].type = 9;
             g_free( nonstd_name[ i - 1 ] );
             nonstd_name[ i - 1 ] = g_strdup( g_strstrip( dumstrg ) );
             fgets( dumstrg, MAXSTR, inputv );
             }
          }

   /* [3.2.1]: Allocate workspace for omegas and deltas:                     */

      Tm.Nomega = ivector( 1, Tm.NdetVar );
      Tm.Omega  = (double **)calloc((size_t)(Tm.NdetVar + 1), sizeof(double *));
      Tm.Imega  = (int **)calloc((size_t)(Tm.NdetVar + 1), sizeof(int *));
      Tm.Ndelta = ivector( 1, Tm.NdetVar );
      Tm.Delta  = (double **)calloc((size_t)(Tm.NdetVar + 1), sizeof(double *));
      Tm.Ielta  = (int **)calloc((size_t)(Tm.NdetVar + 1), sizeof(int *));

   /* [3.2.2]: Read omegas for each deterministic variable (if any):         */

      fgets( dumstrg, MAXSTR, inputv );
      for ( i = 1; i <= Tm.NdetVar; i++ ){
        fscanf( inputv, "%d", &Tm.Nomega[i] );
	It[ i - 1 ].ma_order = Tm.Nomega[i];
	}
      fscanf( inputv, "\n" );

      for ( i = 1; i <= Tm.NdetVar; i++ )
          {
          Tm.Omega[i] = vector( 0, Tm.Nomega[i] );
          Tm.Imega[i] = ivector( 0, Tm.Nomega[i] );
	  It[ i - 1 ].ma_parameter = vector ( 0,  It[ i - 1 ].ma_order + 1);
	  It[ i - 1 ].ma_fixed     = ivector( 0,  It[ i - 1 ].ma_order + 1);
          fgets( dumstrg, MAXSTR, inputv );
          for ( j = 0; j <= Tm.Nomega[i]; j++ )
              {
              fscanf( inputv, "%lf", &Tm.Omega[i][j] );
              fscanf( inputv, "%d\n", &Tm.Imega[i][j] );
	      It[ i - 1 ].ma_parameter[j] = Tm.Omega[i][j];
	      It[ i - 1 ].ma_fixed[j]     = Tm.Imega[i][j];
              if ( Tm.Imega[i][j] == 1 ) npar += 1;
              }
          }

   /* [3.2.3]: Read deltas for each deterministic variable (if any):         */

      fgets( dumstrg, MAXSTR, inputv );
      /* El cuerpo del bucle era SOLO el fscanf: la asignacion de ar_order
       * quedaba fuera, la sangria enganaba, y al salir i valia NdetVar+1.
       * Resultado: ningun determinista se quedaba con su orden de delta, el
       * escritor los ponia todos a cero y el denominador se perdia.       */
      for ( i = 1; i <= Tm.NdetVar; i++ )
          {
          fscanf( inputv, "%d", &Tm.Ndelta[i] );
          It[ i - 1 ].ar_order = Tm.Ndelta[i];
          }
      fscanf( inputv, "\n" );

      for ( i = 1; i <= Tm.NdetVar; i++ ) if ( Tm.Ndelta[i] > 0 )
          {
          Tm.Delta[i] = vector( 1, Tm.Ndelta[i] );
          Tm.Ielta[i] = ivector( 1, Tm.Ndelta[i] );
	  It[ i - 1 ].ar_parameter = vector ( 1,  It[ i - 1 ].ar_order );
	  It[ i - 1 ].ar_fixed     = ivector( 1,  It[ i - 1 ].ar_order );
          fgets( dumstrg, MAXSTR, inputv );
          for ( j = 1; j <= Tm.Ndelta[i]; j++ )
              {
              fscanf( inputv, "%lf", &Tm.Delta[i][j] );
              fscanf( inputv, "%d\n", &Tm.Ielta[i][j] );
	      It[ i - 1 ].ar_parameter[j] = Tm.Delta[i][j];
	      /* Era ma_fixed: la bandera del delta se escribia encima de la
	       * del omega, y ar_fixed se quedaba sin poner.                */
	      It[ i - 1 ].ar_fixed[j]     = Tm.Ielta[i][j];
              if ( Tm.Ielta[i][j] == 1 ) npar += 1;
              }
          }
      }

/* [3.3.1]: Read number and order for each regular AR factor:                */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr1 );
   NopArr = Tm.NumAr1;
   if ( Tm.NumAr1 > 0 )
      {
      Tm.p1  = ivector( 1, Tm.NumAr1 );
      Tm.Ar1 = (double **)calloc((size_t)(Tm.NumAr1 + 1), sizeof(double *));
      Tm.Ia1 = (int **)calloc((size_t)(Tm.NumAr1 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumAr1; i++ ){
       fscanf( inputv, "%d", &Tm.p1[i] );
       Arr[ i - 1 ].order  = Tm.p1[i];
	}
   fscanf( inputv, "\n" );

/* [3.3.2]: Read phis for each regular AR factor (if any):                   */

   for ( i = 1; i <= Tm.NumAr1; i++ )
       {
       Tm.Ar1[i] = vector( 0, Tm.p1[i] );
       Tm.Ia1[i] = ivector( 0, Tm.p1[i] );
	Arr[ i - 1 ].op_parameter = vector ( 1,  Arr[ i - 1 ].order + 1  );
	Arr[ i - 1 ].op_fixed     = ivector( 1,  Arr[ i - 1 ].order + 1  );
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.p1[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ar1[i][j] );
           fscanf( inputv, "%d\n", &Tm.Ia1[i][j] );
           Arr[ i - 1 ].op_parameter[j] = Tm.Ar1[i][j];
           Arr[ i - 1 ].op_fixed[j] = Tm.Ia1[i][j];
           if ( Tm.Ia1[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }

/* [3.3.3]: Read number and order for each annual AR factor:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr2 );
   NopAra  = Tm.NumAr2;
   if ( Tm.NumAr2 > 0 )
      {
      Tm.p2  = ivector( 1, Tm.NumAr2 );
      Tm.Ar2 = (double **)calloc((size_t)(Tm.NumAr2 + 1), sizeof(double *));
      Tm.Ia2 = (int **)calloc((size_t)(Tm.NumAr2 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumAr2; i++ ){
        fscanf( inputv, "%d", &Tm.p2[i] );
	 Ara [ i - 1 ].order = Tm.p2[i];
	}
   fscanf( inputv, "\n" );

/* [3.3.4]: Read phis for each annual AR factor (if any):                    */

   for ( i = 1; i <= Tm.NumAr2; i++ )
       {
       Tm.Ar2[i] = vector( 0, Tm.p2[i] );
       Tm.Ia2[i] = ivector( 0, Tm.p2[i] );
       Ara [ i - 1 ].op_parameter = vector( 0,  Ara [ i - 1 ].order );
       Ara [ i - 1 ].op_fixed  = ivector( 0,   Ara [ i - 1 ].order);
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.p2[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ar2[i][j] );
           fscanf( inputv, "%d\n", &Tm.Ia2[i][j] );
	   Ara[ i - 1 ].op_parameter[ j ] = Tm.Ar2[i][j];
	   Ara[ i - 1 ].op_fixed[ j ] = Tm.Ia2[i][j];
           if ( Tm.Ia2[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }

/* [3.3.5]: Read number and order for each regular MA factor:                */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa1 );
   NopMar = Tm.NumMa1;
   if ( Tm.NumMa1 > 0 )
      {
      Tm.q1  = ivector( 1, Tm.NumMa1 );
      Tm.Ma1 = (double **)calloc((size_t)(Tm.NumMa1 + 1), sizeof(double *));
      Tm.Im1 = (int **)calloc((size_t)(Tm.NumMa1 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumMa1; i++ ){
        fscanf( inputv, "%d", &Tm.q1[i] );
	 Mar[ i - 1 ].order = Tm.q1[i];
	}
   fscanf( inputv, "\n" );

/* [3.3.6]: Read thetas for each regular MA factor (if any):                 */

   for ( i = 1; i <= Tm.NumMa1; i++ )
       {
       Tm.Ma1[i] = vector( 0, Tm.q1[i] );
       Tm.Im1[i] = ivector( 0, Tm.q1[i] );
       Mar [ i - 1 ].op_parameter = vector( 0,  Mar [ i - 1 ].order );
       Mar [ i - 1 ].op_fixed  = ivector( 0,   Mar [ i - 1 ].order);
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.q1[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ma1[i][j] );
           fscanf( inputv, "%d\n", &Tm.Im1[i][j] );
	   Mar[ i - 1 ].op_parameter[ j ] = Tm.Ma1[i][j];
	   Mar[ i - 1 ].op_fixed[ j ] = Tm.Im1[i][j];
           if ( Tm.Im1[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }

/* [3.3.7]: Read number and order for each annual MA factor:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa2 );
   NopMaa = Tm.NumMa2;
   if ( Tm.NumMa2 > 0 )
      {
      Tm.q2  = ivector( 1, Tm.NumMa2 );
      Tm.Ma2 = (double **)calloc((size_t)(Tm.NumMa2 + 1), sizeof(double *));
      Tm.Im2 = (int **)calloc((size_t)(Tm.NumMa2 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumMa2; i++ ){
       fscanf( inputv, "%d", &Tm.q2[i] );
	Maa[ i - 1 ].order = Tm.q2[i];
	}
   fscanf( inputv, "\n" );

/* [3.3.8]: Read thetas for each annual MA factor (if any):                  */

   for ( i = 1; i <= Tm.NumMa2; i++ )
       {
       Tm.Ma2[i] = vector( 0, Tm.q2[i] );
       Tm.Im2[i] = ivector( 0, Tm.q2[i] );
	Maa[ i - 1 ].op_parameter = vector( 0, Maa[ i - 1 ].order);
	Maa[ i - 1 ].op_fixed = ivector( 0, Maa[ i - 1 ].order);
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.q2[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ma2[i][j] );
           fscanf( inputv, "%d\n", &Tm.Im2[i][j] );
	   Maa[ i - 1 ].op_parameter[ j ] = Tm.Ma2[i][j];
	   Maa[ i - 1 ].op_fixed[ j ] = Tm.Im2[i][j];
           if ( Tm.Im2[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }

/* [3.3.9]: Read number and frequency for each regular f-fixed AR factor:    */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr1f );
   NumAr2f = Tm.NumAr1f;
   if ( Tm.NumAr1f > 0 )
      {
      Tm.pfre1 = vector( 1, Tm.NumAr1f );
      Tm.Ar1f  = (double **)calloc((size_t)(Tm.NumAr1f + 1), sizeof(double *));
      Tm.Ia1f  = ivector( 1, Tm.NumAr1f );
      }
   for ( i = 1; i <= Tm.NumAr1f; i++ ){
       fscanf( inputv, "%lf", &Tm.pfre1[i] );
	Ar2f[ i - 1 ].freq = Tm.pfre1[i];
	}
   fscanf( inputv, "\n" );

/* [3.3.10]: Read 2nd phi for each regular f-fixed AR factor (if any):       */

   for ( i = 1; i <= Tm.NumAr1f; i++ )
       {
       Tm.Ar1f[i] = vector( 0, 2 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ar1f[i][2] );
       fscanf( inputv, "%d\n", &Tm.Ia1f[i] );
	Ar2f[ i - 1 ].op_parameter = Tm.Ar1f[i][2];
	Ar2f[ i - 1 ].op_fixed = Tm.Ia1f[i];
       if ( Tm.Ia1f[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }



/* [3.3.13]: Read number and frequency for each regular f-fixed MA factor:   */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa1f );
   NumMa2f = Tm.NumMa1f;
   if ( Tm.NumMa1f > 0 )
      {
      Tm.qfre1 = vector( 1, Tm.NumMa1f );
      Tm.Ma1f  = (double **)calloc((size_t)(Tm.NumMa1f + 1), sizeof(double *));
      Tm.Im1f  = ivector( 1, Tm.NumMa1f );
      }
   for ( i = 1; i <= Tm.NumMa1f; i++ ){
       fscanf( inputv, "%lf", &Tm.qfre1[i] );
	Ma2f[ i - 1 ].freq = Tm.qfre1[i];
	}
   fscanf( inputv, "\n" );

/* [3.3.14]: Read 2nd theta for each regular f-fixed MA factor (if any):     */

   for ( i = 1; i <= Tm.NumMa1f; i++ )
       {
       Tm.Ma1f[i] = vector( 0, 2 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ma1f[i][2] );
       fscanf( inputv, "%d\n", &Tm.Im1f[i] );
	Ma2f[ i - 1 ].op_parameter = Tm.Ma1f[i][2];
	Ma2f[ i - 1 ].op_fixed = Tm.Im1f[i];
       if ( Tm.Im1f[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }



/* [3.4]: Read mean parameter (with flag):                                   */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf\n", &Tm.mu );
   fscanf( inputv, "%d\n", &Tm.Imu );
   if ( Tm.Imu == 1 ) npar += 1;

/* [3.5]: Read Box-Cox lambda and differences (regular and complete annual): */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf", &Tm.boxlam );
   fscanf( inputv, "%d", &Tm.nrdiff );
   fscanf( inputv, "%d\n", &Tm.nadiff );

/* [3.6]: Read individual factors of the annual difference (from freq 0.0):  */

   if ( Ts.freq > 1 )
      {
      Tm.ifadf = ivector( 0, Ts.freq / 2 );
      fgets( dumstrg, MAXSTR, inputv );
      for ( i = 0; i <= Ts.freq / 2; i++ )
          fscanf( inputv, "%d", &Tm.ifadf[i] );
      fscanf( inputv, "\n" );
      }
   else                                        /* Annual data (no ifadf):    */
      {
      fgets( dumstrg, MAXSTR, inputv );
      fgets( dumstrg, MAXSTR, inputv );
      }
/* [3.7]: Read cbands and refactor:*/

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf", &Tm.cbands );
   fscanf( inputv, "%lf\n", &Ts.refactor );
	if (Ts.refactor == 0){Ts.refactor=1;}
/* [3.7]: Read time series and non-standard detvars data:                    */

   fgets( dumstrg, MAXSTR, inputv );

   for ( i = 1; i <= Ts.nobs; i++ )
       {
       fscanf( inputv, "%lf", &Ts.data[i] );
       Data[ i - 1 ] = Ts.data[i];
       if ( Tm.NdetVar > 0 )
          for ( j = 1; j <= nstdet; j++ )
              fscanf( inputv, "%lf", &DataMat[det[j]][i] );
       fscanf( inputv, "\n" );
       }

   fclose( inputv );
FREE_STR( model_residuals );
FREE_STR( series_name );
FREE_STR( dumstrg );

}


void on_new_file(GtkToolButton *btn, FueContext *ctx) {
    /* Reset all model structures to defaults */
    Ts.nobs = 0;
    Ts.freq = 12;
    Ts.begyear = 2000;
    Ts.begtime = 1;
    ts_set_name(NULL);
    Ts.refactor = 1.0;
    Tm.boxlam = 1.0;
    Tm.boxm = 1.0;
    Tm.nrdiff = 0;
    Tm.nadiff = 0;
    Tm.Imu = 0;
    Tm.mu = 0.0;
    if (Ts.freq > 1) {
        Tm.ifadf = ivector(0, Ts.freq/2);
        for (int i = 0; i <= Ts.freq/2; i++) Tm.ifadf[i] = 0;
    }
    NdetVar = 0;
    NopArr = NopAra = NopMar = NopMaa = 0;
    NumAr2f = NumMa2f = 0;

    /* Clear UI */
    gtk_entry_set_text(GTK_ENTRY(ctx->series_name_entry), "");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->freq_combo), 2); /* monthly default */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->n_obs_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_period_spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->start_year_spin), 2000);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->refactor_spin), 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_lambda_spin), 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->boxcox_m_spin), 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->nrdiff_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->nadiff_spin), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->mean_check), FALSE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ctx->mean_spin), 0.0);

    /* Clear tree views */
    GtkListStore *store;
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->int_treeview)));
    gtk_list_store_clear(store);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->arr_treeview)));
    gtk_list_store_clear(store);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->ara_treeview)));
    gtk_list_store_clear(store);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->mar_treeview)));
    gtk_list_store_clear(store);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->maa_treeview)));
    gtk_list_store_clear(store);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->ar_fix_treeview)));
    gtk_list_store_clear(store);
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ctx->ma_fix_treeview)));
    gtk_list_store_clear(store);

    gtk_label_set_text(GTK_LABEL(ctx->status_label), "New model created.");
}

/* ========================================================================= */
/* Cross‑platform run of the fue executable                                 */
/* ========================================================================= */

/* ========================================================================= */
/* Callbacks for toolbar buttons                                            */
/* ========================================================================= */
void on_save_inp(GtkToolButton *btn, FueContext *ctx) {
    sync_all_from_ui(ctx);
    save_inp_file(ctx);
}

/* ------------------------------------------------------------------------ */
/* Correr fue sin bloquear la interfaz                                       */
/* ------------------------------------------------------------------------ */

/* La barra se queda puesta al acabar, con lo que conto: una estimacion de
 * estas tarda diez milisegundos, asi que si se escondiera al terminar no
 * daria tiempo ni a dibujarla y no se veria nada.                        */
static void run_busy(FueContext *ctx, gboolean busy) {
    ctx->running = busy;
    if (ctx->btn_run) gtk_widget_set_sensitive(ctx->btn_run, !busy);
    gtk_widget_set_visible(ctx->progress, TRUE);
    if (busy) {
        ctx->iterations = -1;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->progress), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ctx->progress), "...");
    } else {
        gchar *text = (ctx->iterations >= 0)
                      ? g_strdup_printf("%d it.", ctx->iterations)
                      : g_strdup("--");

        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->progress), 1.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ctx->progress), text);
        g_free(text);
    }
}

static void on_fue_iteration(int k, double f, gpointer data) {
    FueContext *ctx = data;
    gchar *text = g_strdup_printf("it. %d", k);

    ctx->iterations = k;
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ctx->progress), text);
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(ctx->progress));
    g_free(text);
}

/* El mensaje corto, y el largo en el globo */
static void say(FueContext *ctx, const char *brief, const char *full) {
    gtk_label_set_text(GTK_LABEL(ctx->status_label), brief);
    gtk_widget_set_tooltip_text(ctx->status_label, full ? full : brief);
}

/* A la consola, que es donde esta todo lo que el motor escribio */
static void go_to_console(FueContext *ctx) {
    if (ctx->notebook != NULL)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(ctx->notebook), ctx->console_page);
}

static void on_fue_done(const EngineResult *r, gpointer data) {
    FueContext *ctx = data;

    run_busy(ctx, FALSE);
    if (engine_wrote_results(r)) {
        char  *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
        const char *name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
        Convergence c = { CONV_NONE, -1, -1.0, NULL, NULL };

        if (workspace != NULL && name != NULL && *name) {
            gchar *file = g_strdup_printf("%s.out", name);
            gchar *path = g_build_filename(workspace, file, NULL);

            convergence_of(path, &c);
            g_free(file);
            g_free(path);
        }
        g_free(workspace);
        load_output_to_console(ctx);
        if (c.brief != NULL) {
            gchar *brief = g_strdup_printf("fue finished: %s", c.brief);
            gchar *full  = g_strdup_printf("%s\n\n%s", brief, c.full ? c.full : "");

            say(ctx, brief, full);
            g_free(brief);
            g_free(full);
        } else
            say(ctx, r->message, r->output);
        convergence_clear(&c);
    } else {
        say(ctx, r->message, r->output);
        show_engine_output(ctx, r);
    }
    go_to_console(ctx);       /* todo lo que escribio, a la vista */
}

void on_run_fue(GtkWidget *widget, FueContext *ctx) {
    on_save_inp(NULL, ctx);   /* guarda el .inp actual */
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No workspace folder selected.");
        return;
    }

    {
    const char *args[] = { input_name, NULL };

    if (ctx->running) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fue is already running.");
        g_free(workspace);
        return;
    }
    run_busy(ctx, TRUE);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), "Running fue...");
    if (!engine_run_async(workspace, "fue", args, on_fue_iteration, on_fue_done, ctx)) {
        run_busy(ctx, FALSE);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fue could not be run.");
    }
    }
    g_free(workspace);
}

/*
void on_view_output(GtkWidget *widget, FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No workspace folder selected.");
        return;
    }
    char *filename = g_strdup_printf("%s.out", input_name);
    char *out_path = g_build_filename(workspace, filename, NULL);
    g_free(filename);
    gchar *content = NULL;
    gsize len = 0;
    if (g_file_get_contents(out_path, &content, &len, NULL)) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view));
        gtk_text_buffer_set_text(buf, content, len);
        g_free(content);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Output displayed.");
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Output file not found. Run fue first.");
    }
    g_free(out_path);
    g_free(workspace);
}
*/

void on_view_inp(GtkWidget *widget, FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No workspace folder selected.");
        return;
    }
    char *filename = g_strdup_printf("%s.inp", input_name);
    char *inp_path = g_build_filename(workspace, filename, NULL);
    g_free(filename);
    gchar *content = NULL;
    gsize len = 0;
    if (g_file_get_contents(inp_path, &content, &len, NULL)) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view));
        gtk_text_buffer_set_text(buf, content, len);
        g_free(content);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "INP file displayed.");
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "INP file not found.");
    }
    g_free(inp_path);
    g_free(workspace);
}


void on_quit(GtkWidget *widget, FueContext *ctx) {
    gtk_widget_destroy(ctx->main_window);
    gtk_main_quit();
}

/* En file_io.c, añadir al final (o en un lugar adecuado) */

/* Carga un archivo en el text view de la consola */
static void load_file_to_console(FueContext *ctx, const char *filename, gboolean editable) {
    /* Verificar si el archivo existe */
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        gchar *msg = g_strdup_printf("File not found: %s", filename);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), msg);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));
        gtk_text_buffer_set_text(buffer, msg, -1);
        g_free(msg);
        return;
    }
    gchar *content = NULL;
    gsize len = 0;
    if (g_file_get_contents(filename, &content, &len, NULL)) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));
        gtk_text_buffer_set_text(buffer, content, len);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->console_text_view), editable);
        g_free(content);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "File loaded to console.");
    } else {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));
        gtk_text_buffer_set_text(buffer, "Error: Could not load file.", -1);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->console_text_view), FALSE);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Error: Could not load file.");
    }
}


/* Lo que el motor dijo, en la consola: cuando no deja fichero de salida es
 * lo unico que explica por que.                                            */
void show_engine_output(FueContext *ctx, const EngineResult *r) {
    GtkTextBuffer *buf;

    if (!ctx || !ctx->text_view || !r) return;
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view));
    if (r->output && *r->output)
        gtk_text_buffer_set_text(buf, r->output, -1);
    else if (r->message)
        gtk_text_buffer_set_text(buf, r->message, -1);
}

/* Función para cargar el .out en la consola (llamada desde on_run_fue) */

 void load_output_to_console(FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace || !input_name || strlen(input_name) == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Workspace or input name not set.");
        return;
    }
    char *filename = g_strdup_printf("%s.out", input_name);
    char *out_path = g_build_filename(workspace, filename, NULL);
    g_free(filename);
    g_print("Attempting to load output from: %s\n", out_path);
    load_file_to_console(ctx, out_path, FALSE);
    g_free(out_path);
    g_free(workspace);
}

/*
void load_output_to_console(FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace || !input_name || strlen(input_name) == 0) return;
    char *out_path = g_build_filename(workspace, input_name, ".out", NULL);
    load_file_to_console(ctx, out_path, FALSE);
    g_free(out_path);
    g_free(workspace);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), "Output loaded to Console.");
}
*/

/* ========================================================================= */
/* Abre el archivo PDF generado por FUE con el visor predeterminado         */
/* ========================================================================= */
static void open_pdf_file(const char *pdf_path) {
#ifdef _WIN32
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", pdf_path);
    (void)system(cmd);
#elif defined(__APPLE__)
    char *argv[] = { "open", (char *)pdf_path, NULL };
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
#else
    char *argv[] = { "xdg-open", (char *)pdf_path, NULL };
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
#endif
}

void on_view_output(GtkWidget *widget, FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace || !input_name || strlen(input_name) == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Workspace or input name not set.");
        return;
    }
    /* Construir la ruta al archivo PDF: workspace/input_name.pdf */
    char *filename = g_strdup_printf("%s.pdf", input_name);
    char *pdf_path = g_build_filename(workspace, filename, NULL);
    g_free(filename);

    /* Verificar si el archivo existe */
    if (g_file_test(pdf_path, G_FILE_TEST_EXISTS)) {
        /* La ventana propia si el PDF lo dibujo el motor; si no, el visor
         * del sistema (un PDF de pdflatex, por ejemplo).                  */
        if (preview_show(ctx, pdf_path))
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "Graph window.");
        else {
            open_pdf_file(pdf_path);
            gtk_label_set_text(GTK_LABEL(ctx->status_label), "Opening PDF output.");
        }
    } else {
        gchar *msg = g_strdup_printf("PDF file not found: %s", pdf_path);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), msg);
        g_free(msg);
    }
    g_free(pdf_path);
    g_free(workspace);
}

/* file_io.c – añadir al final, después de las funciones existentes */


/* ========================================================================= */
/* La ventana de graficos (src/preview.c) pide esto al programa que la usa   */
/* ========================================================================= */
void preview_open_external(PreviewApp *app, const gchar *path) {
    (void) app;
    open_pdf_file(path);
}

void preview_show_status(PreviewApp *app, const gchar *format, ...) {
    va_list ap;
    gchar  *text;

    if (!app || !app->status_label) return;
    va_start(ap, format);
    text = g_strdup_vprintf(format, ap);
    va_end(ap);
    gtk_label_set_text(GTK_LABEL(app->status_label), text);
    g_free(text);
}
