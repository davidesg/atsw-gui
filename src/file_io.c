/* file_io.c */
#include "file_io.h"
#include "fue_globals.h"
//#include "fue_core.h"
#include "model_spec.h"
#include "utils.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "forecast_tab.h"   // para acceder a los widgets de Forecast

#ifdef _WIN32
#include <windows.h>
#endif

/* ========================================================================= */
/* Helper to write the .inp file using the current model (Ts, Tm, etc.)     */
/* ========================================================================= */
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
    fprintf(f, "%s\n", Ts.name ? Ts.name : "");
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
                default: type_str = "unknown";
            }
            fprintf(f, "%s", type_str);
            if (It[i].type < 6) {
                if (Ts.freq > 1)
                    fprintf(f, " %d %d\n", It[i].period, It[i].year);
                else
                    fprintf(f, " %d\n", It[i].year);
            } else if (It[i].type >= 6 && It[i].type <= 8) {
                if (It[i].type == 8)
                    fprintf(f, "\n");
                else
                    fprintf(f, " %d\n", It[i].freq);
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
        for (int i = 0; i < NumAr2f; i++) fprintf(f, " %d", Ar2f[i].freq);
        fprintf(f, "\n**");
        for (int i = 0; i < NumAr2f; i++) {
            fprintf(f, "\n%.6f", Ar2f[i].op_parameter);
            fprintf(f, " %d\n**", Ar2f[i].op_fixed);
        }
    } else fprintf(f, "\n");

    fprintf(f, "** Number and frequencies of regular MA(2) operators with fixed frequency:\n");
    fprintf(f, "%d", NumMa2f);
    if (NumMa2f > 0) {
        for (int i = 0; i < NumMa2f; i++) fprintf(f, " %d", Ma2f[i].freq);
        fprintf(f, "\n**");
        for (int i = 0; i < NumMa2f; i++) {
            fprintf(f, "\n%.6f", Ma2f[i].op_parameter);
            fprintf(f, " %d\n**", Ma2f[i].op_fixed);
        }
    } else fprintf(f, "\n");

    /* Mean */
    fprintf(f, "** Mean parameter (mu):\n");
    if (Tm.Imu) fprintf(f, "%.6f 1\n", Tm.mu);
    else fprintf(f, "0\n");

    /* Box‑Cox and differences */
    fprintf(f, "** Box-Cox lambda, m. Regular differences and complete annual differences:\n");
    fprintf(f, " %2.2f", Tm.boxlam);
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
    for (int i = 1; i <= Ts.nobs; i++) {
        fprintf(f, "%lf\n", Ts.data[i]);
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
int nstdet = 0, *det = NULL;
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
      printf( "\nError opening input file: %s\n", inputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
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

      Ts.name = g_strconcat( series_name, NULL );
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
             det[nstdet] = i;
             fgets( dumstrg, MAXSTR, inputv );
             }
          }

   /* [3.2.1]: Allocate workspace for omegas and deltas:                     */

      Tm.Nomega = ivector( 1, Tm.NdetVar );
      Tm.Omega  = (double **)malloc((size_t)Tm.NdetVar * sizeof(double *)) - 1;
      Tm.Imega  = (int **)malloc((size_t)Tm.NdetVar * sizeof(int *)) - 1;
      Tm.Ndelta = ivector( 1, Tm.NdetVar );
      Tm.Delta  = (double **)malloc((size_t)Tm.NdetVar * sizeof(double *)) - 1;
      Tm.Ielta  = (int **)malloc((size_t)Tm.NdetVar * sizeof(int *)) - 1;

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
      for ( i = 1; i <= Tm.NdetVar; i++ )
          fscanf( inputv, "%d", &Tm.Ndelta[i] );
	It[ i - 1 ].ar_order = Tm.Ndelta[i];
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
	      It[ i - 1 ].ma_fixed[j]     = Tm.Ielta[i][j];
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
      Tm.Ar1 = (double **)malloc((size_t)Tm.NumAr1 * sizeof(double *)) - 1;
      Tm.Ia1 = (int **)malloc((size_t)Tm.NumAr1 * sizeof(int *)) - 1;
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
      Tm.Ar2 = (double **)malloc((size_t)Tm.NumAr2 * sizeof(double *)) - 1;
      Tm.Ia2 = (int **)malloc((size_t)Tm.NumAr2 * sizeof(int *)) - 1;
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
      Tm.Ma1 = (double **)malloc((size_t)Tm.NumMa1 * sizeof(double *)) - 1;
      Tm.Im1 = (int **)malloc((size_t)Tm.NumMa1 * sizeof(int *)) - 1;
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
      Tm.Ma2 = (double **)malloc((size_t)Tm.NumMa2 * sizeof(double *)) - 1;
      Tm.Im2 = (int **)malloc((size_t)Tm.NumMa2 * sizeof(int *)) - 1;
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
      Tm.Ar1f  = (double **)malloc((size_t)Tm.NumAr1f * sizeof(double *)) - 1;
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
      Tm.Ma1f  = (double **)malloc((size_t)Tm.NumMa1f * sizeof(double *)) - 1;
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
    Ts.name = NULL;
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
#ifdef _WIN32
static gboolean run_fue_win32(const char *inp_base, const char *workdir) {
    char *full_path = g_find_program_in_path("fue.exe");
    if (!full_path) {
        g_print("fue.exe not found in PATH.\n");
        return FALSE;
    }
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", full_path, inp_base);
    g_free(full_path);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    wchar_t *wcmd = g_utf8_to_utf16(cmd, -1, NULL, NULL, NULL);
    wchar_t *wdir = g_utf8_to_utf16(workdir, -1, NULL, NULL, NULL);
    BOOL success = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                                  NULL, wdir, &si, &pi);
    g_free(wcmd);
    g_free(wdir);
    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exit_code == 0;
    }
    return FALSE;
}
#endif

/* ========================================================================= */
/* Callbacks for toolbar buttons                                            */
/* ========================================================================= */
void on_save_inp(GtkToolButton *btn, FueContext *ctx) {
    sync_all_from_ui(ctx);
    save_inp_file(ctx);
}

void on_run_fue(GtkWidget *widget, FueContext *ctx) {
    on_save_inp(NULL, ctx);   /* guarda el .inp actual */
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No workspace folder selected.");
        return;
    }

#ifdef _WIN32
    if (!run_fue_win32(input_name, workspace))
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Failed to run fue.exe.");
    else {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fue finished.");
        load_output_to_console(ctx);   /* Cargar el .out en la consola */
    }
#else
    char *full_path = g_find_program_in_path("fue");
    if (!full_path) full_path = g_strdup("./fue");
    char *cmd = g_strdup_printf("cd \"%s\" && \"%s\" %s", workspace, full_path, input_name);
    int ret = system(cmd);
    g_free(cmd);
    g_free(full_path);
    if (ret == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fue finished.");
        load_output_to_console(ctx);
    } else
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fue failed.");
#endif
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

/* Callback para el botón "Edit .inp" */
void on_edit_inp_clicked(GtkButton *button, FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace || !input_name || strlen(input_name) == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Workspace or input name not set.");
        return;
    }
    char *filename = g_strdup_printf("%s.inp", input_name);
    char *inp_path = g_build_filename(workspace, filename, NULL);
    g_free(filename);
    load_file_to_console(ctx, inp_path, TRUE);
    gtk_widget_set_sensitive(ctx->save_inp_button, TRUE);
    g_free(inp_path);
    g_free(workspace);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), "Editing .inp file. Click 'Save .inp' to save changes.");
}

/* Callback para el botón "Save .inp" */
void on_save_inp_clicked(GtkButton *button, FueContext *ctx) {
    const char *input_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace || !input_name || strlen(input_name) == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Workspace or input name not set.");
        return;
    }
    char *filename = g_strdup_printf("%s.inp", input_name);
    char *inp_path = g_build_filename(workspace, filename, NULL);
    g_free(filename);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->console_text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *content = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    FILE *f = NULL;
#ifdef _WIN32
    wchar_t *wpath = g_utf8_to_utf16(inp_path, -1, NULL, NULL, NULL);
    if (wpath) {
        f = _wfopen(wpath, L"w");
        g_free(wpath);
    }
#else
    f = fopen(inp_path, "w");
#endif

    if (f) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), ".inp file saved.");
        gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->console_text_view), FALSE);
        gtk_widget_set_sensitive(ctx->save_inp_button, FALSE);
    } else {
        gchar *msg = g_strdup_printf("Error saving .inp: %s", strerror(errno));
        gtk_label_set_text(GTK_LABEL(ctx->status_label), msg);
        g_free(msg);
    }

    g_free(content);
    g_free(inp_path);
    g_free(workspace);
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
    gtk_widget_set_sensitive(ctx->save_inp_button, FALSE);
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
    gtk_widget_set_sensitive(ctx->save_inp_button, FALSE);
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
    /* Windows: usa el comando start */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", pdf_path);
    system(cmd);
#else
    /* Linux / Unix: usa xdg-open */
    char *cmd = g_strdup_printf("xdg-open \"%s\"", pdf_path);
    system(cmd);
    g_free(cmd);
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
        open_pdf_file(pdf_path);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Opening PDF output.");
    } else {
        gchar *msg = g_strdup_printf("PDF file not found: %s", pdf_path);
        gtk_label_set_text(GTK_LABEL(ctx->status_label), msg);
        g_free(msg);
    }
    g_free(pdf_path);
    g_free(workspace);
}

/* file_io.c – añadir al final, después de las funciones existentes */

void on_forecast_button_clicked(GtkToolButton *btn, FueContext *ctx) {
    const char *base_name = gtk_entry_get_text(GTK_ENTRY(ctx->input_name_entry));
    if (!base_name || strlen(base_name) == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No model name provided.");
        return;
    }
    char *workspace = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(ctx->workspace_file_chooser));
    if (!workspace) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No workspace folder selected.");
        return;
    }

    // 1) Ejecutar fue -f para generar forecast_<base>.inp
    gtk_label_set_text(GTK_LABEL(ctx->status_label), "Generating forecast input file...");
    gchar *fue_cmd = g_strdup_printf("cd \"%s\" && fue \"%s\" -f", workspace, base_name);
    int ret_fue = system(fue_cmd);
    g_free(fue_cmd);
    if (ret_fue != 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fue -f failed.");
        g_free(workspace);
        return;
    }

    // 2) Construir el nombre base del archivo generado (sin extensión)
    char *forecast_base = g_strdup_printf("forecast_%s", base_name);
    // Ruta completa del archivo .inp generado
    char *forecast_inp_filename = g_strdup_printf("%s.inp", forecast_base);
    char *forecast_inp_path = g_build_filename(workspace, forecast_inp_filename, NULL);
    g_free(forecast_inp_filename);
    if (!g_file_test(forecast_inp_path, G_FILE_TEST_EXISTS)) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "forecast_*.inp not generated.");
        g_free(forecast_base);
        g_free(forecast_inp_path);
        g_free(workspace);
        return;
    }

    // 3) Ejecutar fuf sobre forecast_<base>
    gtk_label_set_text(GTK_LABEL(ctx->status_label), "Running FUF forecast...");
    gchar *fuf_cmd = g_strdup_printf("cd \"%s\" && fuf \"%s\"", workspace, forecast_base);
    int ret_fuf = system(fuf_cmd);
    g_free(fuf_cmd);
    if (ret_fuf != 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "fuf failed.");
        g_free(forecast_base);
        g_free(forecast_inp_path);
        g_free(workspace);
        return;
    }

    // 4) Construir ruta del archivo .out generado
    char *forecast_out_filename = g_strdup_printf("%s.out", forecast_base);
    char *out_path = g_build_filename(workspace, forecast_out_filename, NULL);
    g_free(forecast_out_filename);

    if (g_file_test(out_path, G_FILE_TEST_EXISTS)) {
        // Actualizar el estado interno de Forecast con la ruta completa del .inp generado
        set_current_inp_from_path(ctx, forecast_inp_path);
        // Cargar el contenido del .out en el editor de Forecast
        load_file_to_editor(ctx, out_path);
        gtk_label_set_text(GTK_LABEL(ctx->forecast_status_label), "Forecast completed. Output loaded.");
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Forecast finished successfully.");
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Forecast output file not found.");
    }

    // Cambiar a la pestaña Forecast (opcional, buscar por widget)
    GtkWidget *notebook = gtk_widget_get_ancestor(ctx->forecast_editor, GTK_TYPE_NOTEBOOK);
    if (notebook) {
        int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook),
                        gtk_widget_get_parent(ctx->forecast_editor));
        if (page_num >= 0)
            gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);
    }

    g_free(out_path);
    g_free(forecast_inp_path);
    g_free(forecast_base);
    g_free(workspace);
}
