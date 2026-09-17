/***************************************************************************
 *   gnuplot_i — C interface to gnuplot                                    *
 *   Based on the gnuplot_i library, licensed under the GNU GPL.           *
 *   Modified for FUF: Windows/MSYS2 support, time series plot functions.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef _GNUPLOT_PIPES_C_
#define _GNUPLOT_PIPES_C_

/*-------------------------------------------------------------------------*/
/**
  @file    gnuplot_i.c
  @brief   C interface to gnuplot.

  gnuplot_i is a C interface library that enables sending display
  requests to gnuplot through C calls. gnuplot itself is an open source
  plotting library also written in C.

  The plot can be displayed in its own window or otherwise saved as an image file
  to disk.

  Example of a minimal program structure:

  @code
    gnuplot_ctrl *handle = gnuplot_init();
    gnuplot_cmd (handle, "set terminal png");
    gnuplot_plot_equation (handle, "sin(x)", "Sine wave");
    gnuplot_close (handle);
  @endcode

*/
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Includes
 ---------------------------------------------------------------------------*/

#include "gnuplot_i.h"
#include <math.h>

/*---------------------------------------------------------------------------
  Defines
 ---------------------------------------------------------------------------*/

/** Maximum amount of characters of a gnuplot command. */
#define GP_CMD_SIZE 2048
/** Maximum amount of characters of a plot title. */
#define GP_TITLE_SIZE 80
/** Maximum amount of characters for an equation y=f(x). */
#define GP_EQ_SIZE 512
/** Maximum amount of characters of a temporary file name. */
#define NAME_SIZE 128
/** Maximum amount of characters of a name in the PATH. */
#define PATH_MAXNAMESZ 4096
/** Error message display. */
#define FAIL_IF(EXP, MSG) ({ if (EXP) { printf("ERROR: " MSG "\n"); exit(EXIT_FAILURE); }})

#ifdef _WIN32
#undef P_tmpdir
#endif
/** Define `P_tmpdir` if not defined; this is normally a POSIX symbol. */
#ifndef P_tmpdir
#define P_tmpdir "."
#endif

#define GNUPLOT_TEMPFILE "%s/gnuplot-i-XXXXXX"
#ifndef _WIN32
#define GNUPLOT_EXEC "gnuplot"
#else
#define GNUPLOT_EXEC "gnuplot.exe"
#endif



/*---------------------------------------------------------------------------
  Function codes
 ---------------------------------------------------------------------------*/

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <shlwapi.h>  // For PathRemoveFileSpec

#define PATH_SEPARATOR ';'
#define DIR_SEPARATOR '\\'


/*-------------------------------------------------------------------------*/
/**
  @brief    Create temporary output pipe with randomised name.
  @param    name Name of the pipe.
  @return   int
 */
/*-------------------------------------------------------------------------*/

int mkstemp (char *name) {
  srand(time(NULL));
  int i;
  char *start = strstr(name, "XXXXXX");

  for (i = 0; i < 6; i++) {
    start[i] = (char)(48 + ((int)rand() * 10 / 32768.0));
  }
  i = open(name, O_RDWR | O_CREAT);
  if (i != -1) {
    DWORD dwFileAttr = GetFileAttributes(name);
    SetFileAttributes(name, dwFileAttr & ~FILE_ATTRIBUTE_READONLY);
  }
  printf("\n Create Temporary file for data: %s\n", name); // Before returning from mkstemp for debugging
  return i;
}
#else
  #define DIR_SEPARATOR '/'
  #define PATH_SEPARATOR ':'
#endif


 // printf("Temporary file: %s\n", name); // Before returning from mkstemp for debugging

/*-------------------------------------------------------------------------*/
/**
  @brief    Find out where a command lives in the PATH.
  @param    pname Name of the program to look for.
  @return   pointer to statically allocated character string.

  This is the C equivalent to the 'which' command in Unix. It parses
  the PATH environment variable to find out where a command lives.
  The returned character string is statically allocated within this function,
  i.e. there is no need to free it.
  Note that the contents of this string will change from one call to the next,
  similar to all static variables in a function.

  The input character string must be the name of a command without
  prefixing path of any kind, i.e. only the command name. The returned
  string is the path in which a command matching the same name was found.

  Examples:

  @code
    gnuplot_get_program_path("hello") returns "."
    gnuplot_get_program_path("ls") returns "/bin"
    gnuplot_get_program_path("csh") returns "/usr/bin"
    gnuplot_get_program_path("/bin/ls") returns NULL
  @endcode

 */
/*-------------------------------------------------------------------------*/

char *gnuplot_get_program_path(char *pname) {
  int i, j, lg;
  char *path;
  static char buf[PATH_MAXNAMESZ];
  const char *extensions[] = {".exe", ".com", ".bat", ".cmd", ""};

  // --- Phase 1: Check explicit GNUPLOT_PATH environment variable ---
  char *gnuplot_env = getenv("GNUPLOT_PATH");
  if (gnuplot_env != NULL) {
    // Trim quotes (common in Windows env vars like "C:\Program Files\gnuplot")
    char trimmed_path[PATH_MAXNAMESZ];
    strncpy(trimmed_path, gnuplot_env, sizeof(trimmed_path));
    if (trimmed_path[0] == '"' && trimmed_path[strlen(trimmed_path)-1] == '"') {
      trimmed_path[strlen(trimmed_path)-1] = '\0';
      memmove(trimmed_path, trimmed_path+1, strlen(trimmed_path));
    }

    // Check if path points directly to executable
    for (int k = 0; k < sizeof(extensions)/sizeof(extensions[0]); k++) {
      snprintf(buf, sizeof(buf), "%s%c%s%s",
               trimmed_path, DIR_SEPARATOR, pname, extensions[k]);
      if (access(buf, F_OK) == 0) {
        // Extract directory (remove executable name)
        char *last_slash = strrchr(buf, DIR_SEPARATOR);
        if (last_slash) *last_slash = '\0';
        //PathRemoveFileSpecA(buf);  // Windows-specific helper
        return buf;
      }
    }
  }

  // --- Phase 2: Original PATH search logic (modified for Windows) ---

  /* Check current directory with extensions */
  for (int k = 0; k < sizeof(extensions)/sizeof(extensions[0]); k++) {
    snprintf(buf, sizeof(buf), ".%c%s%s", DIR_SEPARATOR, pname, extensions[k]);
    if (access(buf, F_OK) == 0) {
      strcpy(buf, ".");
      return buf;
    }
  }

  /* Search through PATH */
  buf[0] = '\0';
  path = getenv("PATH");
  FAIL_IF(path == NULL, "PATH environment variable not set");

  for (i = 0; path[i];) {
    /* Extract directory from PATH */
    for (j = i; path[j] && path[j] != PATH_SEPARATOR; j++);
    lg = j - i;

    /* Handle directory */
    strncpy(buf, path + i, lg);
    buf[lg] = '\0';
    if (lg == 0) strcpy(buf, ".");

    /* Append directory separator */
    lg = strlen(buf);
    if (lg >= PATH_MAXNAMESZ - 1) continue;  // Skip if too long
    buf[lg++] = DIR_SEPARATOR;
    buf[lg] = '\0';

    /* Check each extension */
    int found = 0;
    for (int k = 0; k < sizeof(extensions)/sizeof(extensions[0]); k++) {
      char full_path[PATH_MAXNAMESZ];
      snprintf(full_path, sizeof(full_path), "%s%s%s", buf, pname, extensions[k]);
      if (access(full_path, F_OK) == 0) {
        found = 1;
        break;
      }
    }

    if (found) {
      /* Remove trailing separator */
      lg = strlen(buf) - 1;
      if (lg > 0 && buf[lg] == DIR_SEPARATOR)
        buf[lg] = '\0';
      return buf;
    }

    /* Move to next path entry */
    i = j;
    if (path[i] == PATH_SEPARATOR) i++;
  }

  /* Command not found */
  FAIL_IF(1, "Command not found in PATH variable or GNUPLOT_PATH");
  return NULL;
}


/*-------------------------------------------------------------------------*/
/**
  @brief     Open a gnuplot session, ready to receive commands.
  @return    Newly allocated gnuplot control structure.

  This opens a new gnuplot session, ready for input.
  The struct controlling a gnuplot session should remain opaque and only be
  accessed through the provided functions.

  The session must be closed using gnuplot_close().
 */
/*--------------------------------------------------------------------------*/

gnuplot_ctrl *gnuplot_init (void) {
  gnuplot_ctrl *handle;


#ifndef _WIN32
#ifndef __APPLE__
  FAIL_IF (getenv("DISPLAY") == NULL, "Cannot find DISPLAY variable");
#endif
#endif
#ifndef _WIN32
  FAIL_IF (gnuplot_get_program_path("gnuplot") == NULL, "Cannot find gnuplot in your PATH, check `which gnuplot`");
#endif
  FAIL_IF (gnuplot_get_program_path(GNUPLOT_EXEC) == NULL, "Cannot find gnuplot in your PATH, check `which gnuplot`");


  /* Structure initialization: */
  handle = (gnuplot_ctrl *)malloc(sizeof(gnuplot_ctrl));
  handle->nplots = 0;
  gnuplot_setstyle(handle, "points");
  handle->ntmp = 0;
  handle->gnucmd = popen(GNUPLOT_EXEC, "w");
  if (handle->gnucmd == NULL) {
    free(handle);
    FAIL_IF (0 == 0, "Error starting gnuplot");
  }

  /* Set plot dimensions (should be handled elsewhere, but just to get things going) */
  int width = 900;
  int height = 400;

  /* Set terminal output type */
#ifdef _WIN32
  gnuplot_setterm(handle, "windows", width, height);
#elif __APPLE__
  /* Determine whether to use aqua or x11 as the default */
  if (getenv("DISPLAY") == NULL || (getenv("USE_AQUA") != NULL && strcmp(getenv("USE_AQUA"), "1") >= 0))
    gnuplot_setterm(handle, "aqua", width, height);
  else
    gnuplot_setterm(handle, "x11", width, height);
#else
  /* The default is wxt, but this requires wxWidgets to be installed (need a test for that) */
  gnuplot_setterm(handle, "wxt", width, height);
#endif
  return handle;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Print contents of gnuplot control handle to screen.
  @param    handle    Gnuplot session control handle

  This is for debugging purposes only.
 */
/*--------------------------------------------------------------------------*/

void print_gnuplot_handle (gnuplot_ctrl *handle) {
  //fprintf(gnucmd);   /* Pipe to gnuplot process. For debugging purposes only. */
  printf("Temporary files: %d\n", handle->ntmp);    /* Number of temporary files in the current session. */
  printf("Active plots: %d\n", handle->nplots);     /* Number of currently active plots. */
  printf("Plotting style: %s\n", handle->pstyle);   /* Current plotting style. */
  printf("Terminal name: %s\n", handle->term);      /* Save terminal name (used by `gnuplot_hardcopy()` function). */
  //char to_delete[GP_MAX_TMP_FILES][GP_TMP_NAME_SIZE];   /* Names of temporary files. Only relevant for multiplots. */
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Close a gnuplot session previously opened by gnuplot_init()
  @param    handle    Gnuplot session control handle.
  @return   void

  Kills the child process-ID (PID) and deletes all opened temporary files.
  It is mandatory to call this function to close the handle, otherwise
  temporary files are not cleaned from memory and the child process might
  survive and unable to be reused.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_close(gnuplot_ctrl *handle) {
  FAIL_IF(pclose(handle->gnucmd) == -1, "Cannot close communication to gnuplot");

  if (handle->ntmp) {
    for (int i = 0; i < handle->ntmp; i++) {
      #ifdef _WIN32
      // Windows-specific deletion with proper cleanup
      const char *fname = handle->to_delete[i];

      // 1. Clear read-only attribute if set
      DWORD attrs = GetFileAttributesA(fname);
      if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_READONLY) {
          SetFileAttributesA(fname, attrs & ~FILE_ATTRIBUTE_READONLY);
        }
      }

      // 2. Retry deletion with delay
      int retries = 3;
      while (retries-- > 0) {
        if (DeleteFileA(fname)) break;

        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
          // Wait for process to fully release handles
          Sleep(100);  // 100ms delay
        } else {
          break;
        }
      }

      // 3. Verify deletion
      if (GetFileAttributesA(fname) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Failed to delete Check %s: Error %lu\n",
                fname, GetLastError());
      }
      #else
      remove(handle->to_delete[i]);
      #endif
    }
  }

  free(handle);
}

/*
void gnuplot_close (gnuplot_ctrl *handle) {
  FAIL_IF (pclose(handle->gnucmd) == -1, "Cannot close communication to gnuplot");
  if (handle->ntmp) {
    for (int i = 0; i < handle->ntmp; i++) {
#ifdef _WIN32
      int x = remove(handle->to_delete[i]);
      if (x) printf("Cannot delete %s: error number %d\n", handle->to_delete[i], errno);
#else
      remove(handle->to_delete[i]);
#endif
    }
  }
  free(handle);
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Send a command to an active gnuplot session.
  @param    handle    Gnuplot session control handle
  @param    cmd       Command to send, same as a printf statement.

  This sends a string to an active gnuplot session to be executed.
  This function is the fallback option: if a certain functionality is not
  supported by one of the functions, it is in most cases possible to send
  the required gnuplot commands using this function.

  The command syntax is the same as printf.

  Examples:

  @code
    gnuplot_cmd(h, "plot %d*x", 23.0);
    gnuplot_cmd(h, "plot %g * cos(%g * x)", 32.0, -3.0);
  @endcode

  Since the communication to the gnuplot process is run through a standard Unix
  pipe, it is only unidirectional. This means that it is not possible for this
  interface to query an error status back from gnuplot.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_cmd (gnuplot_ctrl *handle, char *cmd, ...) {
  va_list ap;
  char local_cmd[GP_CMD_SIZE];

  va_start(ap, cmd);
  vsprintf(local_cmd, cmd, ap);
  va_end(ap);
  strcat(local_cmd, "\n");
  fputs(local_cmd, handle->gnucmd);
  fflush(handle->gnucmd);
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Change the plotting style of a gnuplot session.
  @param    handle      Gnuplot session control handle
  @param    plot_style  Plotting-style (character string)
  @return   void

  The provided plotting style is one of the following character strings:
  - lines
  - points
  - linespoints
  - impulses
  - dots
  - steps
  - errorbars (superseded by xerrorbars and xyerrorbars since version 5.0)
  - boxes
  - boxerrorbars
 */
/*--------------------------------------------------------------------------*/

void gnuplot_setstyle (gnuplot_ctrl *handle, char *plot_style) {
  if (strcmp(plot_style, "lines") &&
      strcmp(plot_style, "points") &&
      strcmp(plot_style, "linespoints") &&
      strcmp(plot_style, "impulses") &&
      strcmp(plot_style, "dots") &&
      strcmp(plot_style, "steps") &&
      strcmp(plot_style, "filledcurves") &&
      strcmp(plot_style, "errorbars") &&
      strcmp(plot_style, "boxes") &&
      strcmp(plot_style, "boxerrorbars")) {
    fprintf(stderr, "Warning: unknown requested plot style: using default 'points'\n");
    strcpy(handle->pstyle, "points");
  } else {
    strcpy(handle->pstyle, plot_style);
  }
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Change the terminal of a gnuplot session.
  @param    handle    Gnuplot session control handle
  @param    terminal  Terminal name (character string)
  @return   void

  In gnuplot the terminal type is the output channel to which the plot should be
  displayed on.

  The terminal type should be one of the following character strings:
  - `x11` for Linux, no anti-aliasing (default)
  - `wxt` or `qt` for Linux, with anti-aliasing
  - `aqua` for OSX
  - `wxt` or `windows` for MS-Windows.

  No check is made on the validity of the terminal name. This function
  calls `gnuplot_cmd` with the provided terminal name. If this function is not
  called, then the `x11` terminal type will be used.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_setterm (gnuplot_ctrl *handle, char *terminal, int width, int height) {
  char cmd[GP_CMD_SIZE];

  strncpy(handle->term, terminal, 32);
  handle->term[31] = 0;
  FAIL_IF (width < 0 || height < 0, "Plot size dimensions cannot be negative");
  sprintf(cmd, "set terminal %s size %d,%d", handle->term, width, height);
  gnuplot_cmd(handle, cmd);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Set the axis label of a gnuplot session.
  @param    handle  Gnuplot session control handle.
  @param    label   Character string to use for axis label.
  @param    axis    Character string to identify axis, ie "x", "y" or "z".
  @return   void

  Sets the axis label for a gnuplot session.

  Example:

  @code
    gnuplot_set_axislabel(h, "x", "Time(sec)");
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_set_axislabel (gnuplot_ctrl *handle, char *axis, char *label) {
  char cmd[GP_CMD_SIZE];

  sprintf(cmd, "set %slabel \"%s\"", axis, label);
  gnuplot_cmd(handle, cmd);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Reset a gnuplot session (next plot will erase previous ones).
  @brief    Reset a gnuplot session (next plot will erase previous ones).
  @param    handle Gnuplot session control handle.
  @return   void

  Resets a gnuplot session, i.e. the next plot will erase all previous ones.
  This function can effectively be used to insert a new plot in the same
  window with all options maintained. As such, it can be used to create
  animations.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_resetplot (gnuplot_ctrl *handle) {
  if (handle->ntmp) {
    for (int i = 0; i < handle->ntmp; i++) {
      remove(handle->to_delete[i]);
    }
  }
  handle->ntmp = 0;
  handle->nplots = 0;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Check for standard errors and exit when encountered.
  @param    handle Gnuplot session control handle.
  @return   void

  This is for refactoring purposes only.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_i_error (gnuplot_ctrl *handle) {
  FAIL_IF (handle == NULL, "Gnuplot_i control handle invalid");
  FAIL_IF (handle->nplots > 0, "A gnuplot session is already open and held by another process");
  FAIL_IF (handle->ntmp == GP_MAX_TMP_FILES - 1, "Maximum number of temporary files reached: cannot open more");
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot a 2d graph from a list of points.
  @param    handle    Gnuplot session control handle.
  @param    x         Pointer to a list of x coordinates.
  @param    y         Pointer to a list of y coordinates (can be NULL).
  @param    n         Number of doubles in x (assumed the same as in y).
  @param    title     Title of the plot (can be NULL).
  @return   void

  Plots a 2d graph from a list of coordinates of type double.

  Provide points through a list of x and a list of y coordinates, with the
  following proviso.
  * If y is NULL, then the x-coordinate is the index of the value in the list,
  and the y coordinate is the value in the list.
  * If y is not NULL, then both arrays are assumed to contain the same number of
  values.

  Example:

  @code
    gnuplot_ctrl *h;
    double x[50], y[50];

    h = gnuplot_init();
    for (int i = 0; i < 50; i++) {
      x[i] = (double)(i)/10.0;
      y[i] = x[i] * x[i];
    }
    gnuplot_plot_coordinates(h, x, y, 50, "parabola");
    sleep(2);
    gnuplot_close(h);
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_plot_coordinates (gnuplot_ctrl *handle, double *x, double *y, int n, char *title) {
  int tmpfd;
  char name[NAME_SIZE];
  char cmd[GP_CMD_SIZE];

  /* Error handling: mandatory arguments, already open session, opening temporary file */
  FAIL_IF (x == NULL || (n < 1), "One of the parameters to gnuplot_plot_coordinates() has been misspecified");
  gnuplot_i_error(handle);

  /* Open temporary file for output */
  sprintf(name, GNUPLOT_TEMPFILE, P_tmpdir);
  FAIL_IF ((tmpfd = mkstemp(name)) == -1, "Cannot create temporary file: exiting gnuplot_plot_coordinates()");

  /* Store file name in array for future deletion */
  strcpy(handle->to_delete[handle->ntmp], name);
  handle->ntmp++;

  /* Write data to this file */
  for (int i = 0; i < n; i++) {
    (y == NULL || memcmp(y, y+1, (sizeof(y)-1)*sizeof(y[0])) == 0) ? sprintf(cmd, "%g\n", x[i]) : sprintf(cmd, "%g %g\n", x[i], y[i]);
    write(tmpfd, cmd, strlen(cmd));
  }
  close(tmpfd);

  /* Command to be sent to gnuplot */
  sprintf(cmd, "%s \"%s\" title \"%s\" with %s", (handle->nplots > 0) ? "replot" : "plot", name, (title) ? title : "No title" , handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot a 3d graph (surface plot) from a list of points.
  @param    handle    Gnuplot session control handle.
  @param    x         Pointer to a list of x coordinates.
  @param    y         Pointer to a list of y coordinates.
  @param    z         Pointer to a list of z coordinates.
  @param    n         Number of doubles in x (same for y and z).
  @param    title     Title of the plot (can be NULL).
  @return   void

  Plots a 3d graph from a list of points, passed as arrays x, y and z.
  All arrays are assumed to contain the same number of values.

  Based on `gnuplot_plot_coordinates`, modifications by Robert Bradley 12/10/2004

  Example:

  @code
    gnuplot_ctrl *h;
    double x[50], y[50], z[50];

    h = gnuplot_init();
    for (int i = 0; i < 50; i++) {
      x[i] = (double)(i)/10.0;
      y[i] = x[i] * x[i];
      z[i] = x[i] * x[i]/2.0;
    }
    gnuplot_splot(h, x, y, z, 50, "parabola");
    sleep(2);
    gnuplot_close(h);
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_splot (gnuplot_ctrl *handle, double *x, double *y, double *z, int n, char *title) {
  int tmpfd;
  char name[NAME_SIZE];
  char cmd[GP_CMD_SIZE];

  /* Error handling: mandatory arguments, already open session, opening temporary file */
  FAIL_IF (x == NULL || y == NULL || (n < 1), "One of the parameters to gnuplot_splot() has been misspecified");
  gnuplot_i_error(handle);

  /* Open temporary file for output */
  sprintf(name, GNUPLOT_TEMPFILE, P_tmpdir);
  FAIL_IF ((tmpfd = mkstemp(name)) == -1, "Cannot create temporary file: exiting gnuplot_splot()");

  /* Store file name in array for future deletion */
  strcpy(handle->to_delete[handle->ntmp], name);
  handle->ntmp++;

  /* Write data to this file */
  for (int i = 0; i < n; i++) {
    sprintf(cmd, "%g %g %g\n", x[i], y[i], z[i]);
    write(tmpfd, cmd, strlen(cmd));
  }
  close(tmpfd);

  /* Command to be sent to gnuplot */
  sprintf(cmd, "splot \"%s\" title \"%s\" with %s", name, (title) ? title : "No title", handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot a 3d graph (surface plot) from a grid of points.
  @param    handle    Gnuplot session control handle.
  @param    points    Pointer to a grid of points (rows,cols).
  @param    rows      Number of rows (y points).
  @param    cols      Number of columns (x points).
  @param    title     Title of the plot (can be NULL).
  @return   void

  Plots a 3d graph from a grid of points, passed in the form of an array [x,y].

  Based on `gnuplot_splot`, modifications by Robert Bradley 2/4/2006

  Example:

  @code
    gnuplot_splot_grid(handle, (double*) points, rows, cols, title);
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_splot_grid (gnuplot_ctrl *handle, double *points, int rows, int cols, char *title) {
  int tmpfd;
  char name[NAME_SIZE];
  char cmd[GP_CMD_SIZE];

  /* Error handling: mandatory arguments, already open session, opening temporary file */
  FAIL_IF (points == NULL || (rows < 1) || (cols < 1), "One of the parameters to gnuplot_splot_grid() has been misspecified");
  gnuplot_i_error(handle);

  /* Open temporary file for output */
  sprintf(name, GNUPLOT_TEMPFILE, P_tmpdir);
  FAIL_IF ((tmpfd = mkstemp(name)) == -1, "Cannot create temporary file: exiting gnuplot_splot_grid()");

  /* Store file name in array for future deletion */
  strcpy(handle->to_delete[handle->ntmp], name);
  handle->ntmp++;

  /* Write data to this file */
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      sprintf(cmd, "%d %d %g\n", i, j, points[i * cols + j]);
      write(tmpfd, cmd, strlen(cmd));
    }
    strcpy(cmd, "\n");
    write(tmpfd, cmd, strlen(cmd));
  }
  close(tmpfd);

  /* Command to be sent to gnuplot */
  sprintf(cmd, "splot \"%s\" title \"%s\" with %s", name, (title) ? title : "No title", handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot contours from a list of points.
  @param    handle    Gnuplot session control handle.
  @param    x         Pointer to a list of x coordinates (length = nx*ny).
  @param    y         Pointer to a list of y coordinates (length = nx*ny).
  @param    z         Pointer to a list of z coordinates (length = nx*ny).
  @param    nx        Number of doubles in x-direction.
  @param    ny        Number of doubles in y-direction.
  @param    title     Title of the plot (can be NULL).
  @return   void

  Plots a contour plot from a list of points, passed as arrays x, y and z.

  Based on `gnuplot_splot`, modifications by Robert Bradley 23/11/2005

  Example:

  @code
    gnuplot_ctrl *h;

    h = gnuplot_init();
    int count = 50;
    double x[count*count], y[count*count], z[count*count];
    for (int i = 0; i < count; i++) {
      for (int j = 0; j < count; j++) {
        x[count*i+j] = i;
        y[count*i+j] = j;
        z[count*i+j] = 1000*sqrt(pow(i-count/2, 2)+pow(j-count/2, 2));
      }
    }
    gnuplot_setstyle(h, "lines");
    gnuplot_contour_plot(h, x, y, z, count, count, "Points");
    sleep(2);
    gnuplot_close(h);
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_contour_plot (gnuplot_ctrl *handle, double *x, double *y, double *z, int nx, int ny, char *title) {
  int tmpfd;
  char name[NAME_SIZE];
  char cmd[GP_CMD_SIZE];

  /* Error handling: mandatory arguments, already open session, opening temporary file */
  FAIL_IF (x == NULL || y == NULL || (nx < 1) || (ny < 1), "One of the parameters to gnuplot_contour_plot() has been misspecified");
  gnuplot_i_error(handle);

  /* Open temporary file for output */
  sprintf(name, GNUPLOT_TEMPFILE, P_tmpdir);
  FAIL_IF ((tmpfd = mkstemp(name)) == -1, "Cannot create temporary file: exiting gnuplot_contour_plot()");

  /* Store file name in array for future deletion */
  strcpy(handle->to_delete[handle->ntmp], name);
  handle->ntmp++;

  /* Write data to this file */
  for (int i = 0; i < nx; i++) {
    for (int j = 0; j < ny; j++) {
      sprintf(cmd, "%g %g %g\n", x[nx * i + j], y[nx * i + j], z[nx * i + j]);
      write(tmpfd, cmd, strlen(cmd));
    }
    sprintf(cmd, "\n");
    write(tmpfd, cmd, strlen(cmd));
  }
  close(tmpfd);

  /* Command to be sent to gnuplot */
  gnuplot_cmd(handle, "unset surface");
  gnuplot_cmd(handle, "set contour base");
  gnuplot_cmd(handle, "set view map");
  gnuplot_cmd(handle, "set view 0,0");

  /* Command to be sent to gnuplot */
  sprintf(cmd, "splot \"%s\" title \"%s\" with %s", name, (title) ? title : "No title", handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot a 3d graph using callback functions to return the points.
  @param    handle    Gnuplot session control handle.
  @param    obj       Pointer to an arbitrary object.
  @param    getPoint  Pointer to a callback function.
  @param    n         Number of doubles in x (y and z must be the the same).
  @param    title     Title of the plot (can be NULL).
  @return   void

  Callback:

  void getPoint(void *object, gnuplot_point *point, int index, int pointCount);
    @param    obj     Pointer to an arbitrary object
    @param    point   Pointer to the returned point struct (double x, y, z)
    @param    i       Index of the current point (0 to n-1)
    @param    n       Number of points
    @return   void
 */
/*--------------------------------------------------------------------------*/

void gnuplot_splot_obj (gnuplot_ctrl *handle, void *obj, void (*getPoint)(void *, gnuplot_point *, int, int), int n, char *title) {
  int tmpfd;
  char name[NAME_SIZE];
  char cmd[GP_CMD_SIZE];

  /* Error handling: mandatory arguments, already open session, opening temporary file */
  FAIL_IF (getPoint == NULL || (n < 1), "One of the parameters to gnuplot_splot_obj() has been misspecified");
  gnuplot_i_error(handle);

  /* Open temporary file for output */
  sprintf(name, GNUPLOT_TEMPFILE, P_tmpdir);
  FAIL_IF ((tmpfd = mkstemp(name)) == -1, "Cannot create temporary file: exiting gnuplot_splot_obj()");

  /* Store file name in array for future deletion */
  strcpy(handle->to_delete[handle->ntmp], name);
  handle->ntmp++;

  /* Write data to this file */
  gnuplot_point point;
  for (int i = 0; i < n; i++) {
    getPoint(obj, &point, i, n);
    sprintf(cmd, "%g %g %g\n", point.x, point.y, point.z);
    write(tmpfd, cmd, strlen(cmd));
  }
  close(tmpfd);

  /* Command to be sent to gnuplot */
  sprintf(cmd, "splot \"%s\" title \"%s\" with %s", name, (title) ? title : "No title", handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot a 2d graph using a callback function to return points.
  @param    handle    Gnuplot session control handle.
  @param    obj       Pointer to an arbitrary object.
  @param    getPoint  Pointer to a callback function.
  @param    n         Number of points.
  @param    title     Title of the plot (can be NULL).
  @return   void

  The callback function is of the following form, and is called once for each
  point plotted:

  void getPoint(void *object, gnuplot_point *point, int index, int pointCount);
    @param    obj     Pointer to an arbitrary object
    @param    point   Pointer to the returned point struct (double x, y, z)
    @param    i       Index of the current point: 0 to n-1
    @param    n       Number of points
    @return   void

  Example:

  Here `points` is an array of points:

  @code
    void PlotPoint(void *obj, gnuplot_point *point, int i, int n) {
      Point *p = (Point*) obj;
      point->x = p[i].x;
      point->y = p[i].y;
    }

    int main (int argc, char *argv[]) {
      ...
      gnuplot_plot_obj_xy(handle, points, PlotPoint, pCount, "Points");
      ...
    }
  @endcode

  Alternatively, `PlotPoint` could return values based on a complex formula and
  many sources of information. For example, it could be used to perform a
  Discrete Fourier Transform on an array of complex numbers, calculating one
  transformed point per call.

  Note: always explicitly set all relevant parts of the struct.
  However, the z component for 2D plots can be ignored.
 */
/*-------------------------------------------------------------------------*/

void gnuplot_plot_obj_xy (gnuplot_ctrl *handle, void *obj, void (*getPoint)(void *, gnuplot_point *, int, int), int n, char *title) {
  int tmpfd;
  char name[NAME_SIZE];
  char cmd[GP_CMD_SIZE];

  /* Error handling: mandatory arguments, already open session, opening temporary file */
  FAIL_IF (getPoint == NULL || (n < 1), "One of the parameters to gnuplot_plot_obj_xy() has been misspecified");
  gnuplot_i_error(handle);

  /* Open temporary file for output */
  sprintf(name, GNUPLOT_TEMPFILE, P_tmpdir);
  FAIL_IF ((tmpfd = mkstemp(name)) == -1, "Cannot create temporary file: exiting gnuplot_plot_obj_xy()");

  /* Store file name in array for future deletion */
  strcpy(handle->to_delete[handle->ntmp], name);
  handle->ntmp++;

  /* Write data to this file */
  gnuplot_point point;
  for (int i = 0; i < n; i++) {
    getPoint(obj, &point, i, n);
    sprintf(cmd, "%g %g\n", point.x, point.y);
    write(tmpfd, cmd, strlen(cmd));
  }
  close(tmpfd);

  /* Command to be sent to gnuplot */
  sprintf(cmd, "%s \"%s\" title \"%s\" with %s", (handle->nplots > 0) ? "replot" : "plot", name, (title) ? title : "No title", handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Open a new session, plot a signal, close the session.
  @param    title    Plot title
  @param    style    Plot style
  @param    label_x  Label for X
  @param    label_y  Label for Y
  @param    x        Array of X coordinates
  @param    y        Array of Y coordinates (can be NULL).
  @param    n        Number of values in x and y.
  @return   void

  This function opens a new gnuplot session, plots the provided signal as an X
  or XY signal depending on a provided y, waits for a carriage return on stdin
  and closes the session.

  An empty title, empty style, or empty labels for X and Y may be provided.
  Default values are provided in this case.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_plot_once (char *style, char *label_x, char *label_y, double *x, double *y, int n, char *title) {
  /* Define handle as local variable to isolate it from other gnuplot sessions */
  gnuplot_ctrl *handle;

  /* Some error handling */
  FAIL_IF (x == NULL || n < 1, "One of the parameters to gnuplot_plot_once() has been misspecified");
  FAIL_IF ((handle = gnuplot_init()) == NULL, "Cannot initialize gnuplot handle");

  /* Generate commands to send to gnuplot */
  gnuplot_setstyle(handle, (style == NULL) ? "lines" : style);
  gnuplot_set_axislabel(handle, "x", (label_x == NULL) ? "X" : label_x);
  gnuplot_set_axislabel(handle, "y", (label_y == NULL) ? "Y" : label_y);
  gnuplot_plot_coordinates(handle, x, y, n, title);
  printf("Press Enter to continue\n");
  while (getchar() != '\n') {}
  gnuplot_close(handle);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Plot a curve of given equation y=f(x).
  @param    handle        Gnuplot session control handle.
  @param    equation      Equation to plot.
  @param    title         Title of the plot (can be NULL).
  @return   void

  Plots a given equation. The general form of the equation is y=f(x),
  by providing the f(x) side of the equation only.

  Example:

  @code
    gnuplot_ctrl *h;
    char eq[80];

    h = gnuplot_init();
    strcpy(eq, "sin(x) * cos(2*x)");
    gnuplot_plot_equation(h, eq, "Oscillation", normal);
    gnuplot_close(h);
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_plot_equation (gnuplot_ctrl *handle, char *equation, char *title) {
  char cmd[GP_CMD_SIZE];

  sprintf(cmd, "%s %s title \"%s\" with %s", (handle->nplots > 0) ? "replot" : "plot", equation, (title) ? title : "No title", handle->pstyle);
  gnuplot_cmd(handle, cmd);
  handle->nplots++;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Save a graph as a Postscript file on storage.
  @param    handle      Gnuplot session control handle.
  @param    filename    Filename to write to.
  @param    color       Any character to retain colors, no colors if NULL
  @return   void

  Sets the terminal to Postscript, replots the graph and then resets the
  terminal back to x11. This function supposes that it will be used in
  combination with one of the plotting functions, see example.

  Example:

  @code
    gnuplot_ctrl *h;
    char eq[80];

    h = gnuplot_init();
    strcpy(eq, "sin(x) * cos(2*x)");
    gnuplot_plot_equation(h, eq, "Oscillation", normal);
    gnuplot_hardcopy(h, "sinewave.ps", "color");
    gnuplot_close(h);
  @endcode
 */
/*--------------------------------------------------------------------------*/

void gnuplot_hardcopy (gnuplot_ctrl *handle, char *filename, char *color) {
  if (color == NULL) {
    gnuplot_cmd(handle, "set terminal postscript");
  } else {
    gnuplot_cmd(handle, "set terminal postscript enhanced color");
  }
  gnuplot_cmd(handle, "set output \"%s\"", filename);
  gnuplot_cmd(handle, "replot");
  gnuplot_cmd(handle, "set terminal %s", handle->term);
}

/*--------------------------------------------------------------------------*/

void gnuplot_plot_acf(
    gnuplot_ctrl    *   handle,
    double          *   d,
    int                 n,
    int                 b,
    double                 cmax,
    char            *   title
)
{
    int     i ;
	int		tmpfd ;
	int     bands;
    char    name[128] ;
    char    cmd[GP_CMD_SIZE] ;
    char    line[GP_CMD_SIZE] ;


	if (handle==NULL || d==NULL || (n<1)) return ;

    /* Open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
	sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
    if ((tmpfd=mkstemp(name))==-1) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }

    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;
    /* Write data to this file  */
    for (i=0 ; i<n+1 ; i++) {
		sprintf(line, "%g\n", d[i]);
		write(tmpfd, line, strlen(line));
    }
//    close(tmpfd) ;

 #ifdef _WIN32
  _close(tmpfd);
#else
  close(tmpfd);
#endif

    gnuplot_cmd(handle, "f(x)=0");

    gnuplot_cmd(handle, "bu(x) = %.2f", 2/sqrt(b) );
    gnuplot_cmd(handle, "bl(x) = -%.2f", 2/sqrt(b) );

    /* Command to be sent to gnuplot    */
    //    if (handle->nplots > 0) {
    //    strcpy(cmd, "replot") ;
    // } else {
        strcpy(cmd, "plot") ;
	// }

    if (title == NULL) {
        sprintf(line, "%s \"%s\" with %s", cmd, name, handle->pstyle) ;
    } else {
        sprintf(line, "%s [0:%d][%.2f:%.2f] \"%s\" title \"%s\" with %s ls 5, \
     f(x) notitle ls 2, \
     bu(x) notitle ls 1,\
     bl(x) notitle ls 1 ", cmd, n, -cmax, cmax,  name, title, handle->pstyle) ;
    }

    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
     handle->nplots++ ;
    return ;
}



/*--------------------------------------------------------------------------*/


void gnuplot_plot_ser(
    gnuplot_ctrl    *   handle,
    double          *   d,
    int                 n,
    int                 tmornsop,
    double              AbsMax,
    char            *   title
)
{
    int     i ;
	int		tmpfd ;
    char    name[128] ;
    char    cmd[GP_CMD_SIZE] ;
    char    line[GP_CMD_SIZE] ;


	if (handle==NULL || d==NULL || (n<1)) return ;

    /* Open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
	sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
    if ((tmpfd=mkstemp(name))==-1) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }

    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;
    /* Write data to this file  */
    for (i=1 ; i<n+1 ; i++) {
		sprintf(line, "%g\n", d[i]);
		write(tmpfd, line, strlen(line));
    }
 //   close(tmpfd) ;

 #ifdef _WIN32
  _close(tmpfd);
#else
  close(tmpfd);
#endif

    gnuplot_cmd(handle, "f(x)=0");
    gnuplot_cmd(handle, "j(x)=2");
    gnuplot_cmd(handle, "k(x)=-2");

    /* Command to be sent to gnuplot    */
    if (handle->nplots > 0) {
        strcpy(cmd, "replot") ;
    } else {
        strcpy(cmd, "plot") ;
    }


    if (title == NULL) {
        sprintf(line, "%s \"%s\" with %s", cmd, name, handle->pstyle) ;
    }
    else {
     sprintf(line, "%s [-%d:%d][-%.0f:%.0f]\"%s\" title \"%s\" with %s ls 1, \
     f(x) notitle with l ls 1, \
     j(x) notitle ls 3, \
     k(x) notitle ls 3", cmd, (tmornsop), n-1 ,  AbsMax, AbsMax, name,
                      title, handle->pstyle) ;
    }
/*        sprintf(line, "%s [-%d:%d][-%.0f:%.0f]\"%s\" title \"%s\" with %s 1 31, \ */
    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
    handle->nplots++ ;
//    return ;
}

/*-------------------------------------------------------------------------*/


void gnuplot_plot_histo (
    gnuplot_ctrl    *   handle,
	double			*	x,
	double			*	y,
    int                 n,
    char            *   title
)
{
    int     i ;
	int		tmpfd ;
    char    name[128] ;
    char    cmd[GP_CMD_SIZE] ;
    char    line[GP_CMD_SIZE] ;

	if (handle==NULL || x==NULL || y==NULL || (n<1)) return ;

    /* Open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
	sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
    if ((tmpfd=mkstemp(name))==-1) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }
    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;

    /* Write data to this file  */
    for (i=1 ; i<=n; i++) {
        sprintf(line, "%g %g\n", x[i], y[i]) ;
		write(tmpfd, line, strlen(line));
    }
 //   close(tmpfd) ;
 #ifdef _WIN32
  _close(tmpfd);
#else
  close(tmpfd);
#endif

    /* Command to be sent to gnuplot    */
    if (handle->nplots > 0) {
        strcpy(cmd, "replot") ;
    } else {
        strcpy(cmd, "plot") ;
    }

    if (title == NULL) {
        sprintf(line, "%s \"%s\" with smooth frequency with %s", cmd, name, handle->pstyle) ;

    } else {

        sprintf(line, "%s \"%s\" title \"%s\"  smooth frequency with histeps lw 2.0 , \
        d2(x) notitle  ls 1 ", cmd, name, title ) ;

    }
/*
     } else {

        sprintf(line, "%s \"%s\" title \"%s\"  smooth frequency with histeps lw 2.0 , \
        d2(x) notitle  ls 1 ", cmd, name, title, handle->pstyle) ;

    }
 */
    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
    handle->nplots++ ;
//    return ;
}

/*--------------------------------------------------------------------------*/


void gnuplot_plot_mdt(
    gnuplot_ctrl    *   handle,
    double	*x,
    double	*y,
    int          n,
    char        *title,
    double   AbsMax
)
{
    int     i ;
	int		tmpfd ;
    char    name[128] ;
    char    cmd[GP_CMD_SIZE] ;
    char    line[GP_CMD_SIZE] ;

	if (handle==NULL || x==NULL || y==NULL || (n<1)) return ;

    /* Open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
	sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
    if ((tmpfd=mkstemp(name))==-1) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }
    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;

    /* Write data to this file  */
    for (i=1 ; i<=n; i++) {
        sprintf(line, "%g %g\n", x[i], y[i]) ;
		write(tmpfd, line, strlen(line));
    }
 //   close(tmpfd) ;
  #ifdef _WIN32
  _close(tmpfd);
#else
  close(tmpfd);
#endif

    /* Command to be sent to gnuplot    */
    if (handle->nplots > 0) {
        strcpy(cmd, "replot") ;
    } else {
        strcpy(cmd, "plot") ;
    }

    if (title == NULL) {
        sprintf(line, "%s [-%.1f:%.1f][-%.1f:%.1f] \"%s\" with %s pt 31 lt 1" , cmd, AbsMax, AbsMax, AbsMax, AbsMax, name, handle->pstyle) ;
    } else {
        sprintf(line, "%s [-%.1f:%.1f][-%.1f:%.1f] \"%s\" title \"%s\" with %s  pt 31 lt 1", cmd, AbsMax, AbsMax, AbsMax, AbsMax, name,
                      title, handle->pstyle) ;
    }

    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
    handle->nplots++ ;
//    return ;
}

/*-------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/

void gnuplot_plot_err(
    gnuplot_ctrl    *   handle,
    double          *   d,
    int                 n,
    double                 sigma,
    double                 cmax,
    char            *   title
)
{
    int     i ;
	int		tmpfd ;
    char    name[128] ;
    char    cmd[GP_CMD_SIZE] ;
    char    line[GP_CMD_SIZE] ;


	if (handle==NULL || d==NULL || (n<1)) return ;

    /* Open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
	sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
    if ((tmpfd=mkstemp(name))==-1) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }

    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;
    /* Write data to this file  */
    for (i=0 ; i<n+1 ; i++) {
		sprintf(line, "%g\n", d[i]);
		write(tmpfd, line, strlen(line));
    }
    close(tmpfd) ;


    gnuplot_cmd(handle, "f(x)=0");

    gnuplot_cmd(handle, "bu(x) = %1.1f", 2*sigma );
    gnuplot_cmd(handle, "bl(x) = %1.1f",-2*sigma );

    /* Command to be sent to gnuplot    */
    //    if (handle->nplots > 0) {
    //    strcpy(cmd, "replot") ;
    // } else {
        strcpy(cmd, "plot") ;
	// }

    if (title == NULL) {
        sprintf(line, "%s \"%s\" with %s", cmd, name, handle->pstyle) ;
    } else {
        sprintf(line, "%s [-1:%d][%1.1f:%1.1f] \"%s\" title \"%s\" with %s ls 5, f(x) notitle ls 2, bu(x) notitle ls 1, bl(x) notitle ls 1 ", cmd, n-1, -cmax, cmax,  name, title, handle->pstyle) ;
    }

    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
     handle->nplots++ ;
    return ;
}



/*--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*/

void gnuplot_plot_df(
    gnuplot_ctrl    *   handle,
    double          *   d,
    double          *   d1,
    double          *   d2,
    int                 n,
    int                 L,
    char            *   title
)
{
    int     i;
    int     tmpfd;
    char    name[128];
    char    cmd[GP_CMD_SIZE];
    char    line[GP_CMD_SIZE];

    if (handle == NULL || d == NULL || n < 1) return;

    /* Open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES);
        return;
    }

    /* Create a single temporary file for all data */
    sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
    if ((tmpfd = mkstemp(name)) == -1) {
        fprintf(stderr, "cannot create temporary file: exiting plot");
        return;
    }

    /* Store file name for deletion */
    strcpy(handle->to_delete[handle->ntmp], name);
    handle->ntmp++;

    /* Write combined data to the file */
    for (i = 0; i < n; i++) {
        if (i < L) {
            /* Columns: d[i], NaN, NaN, d[i] */
            sprintf(line, "%g NaN NaN %g\n", d[i], d[i]);
        } else {
            /* Columns: d[i], d1[i], d2[i], NaN */
            sprintf(line, "%g %g %g NaN\n", d[i], d1[i], d2[i]);
        }
        write(tmpfd, line, strlen(line));
    }
    close(tmpfd);

    /* Build gnuplot command */
    if (handle->nplots > 0) {
        strcpy(cmd, "replot");
    } else {
        strcpy(cmd, "plot");
    }

    if (title == NULL) {
        /* Plot only the first column */
        sprintf(line, "%s \"%s\" using 0:1 with %s", cmd, name, handle->pstyle);
    } else {
        /* Plot all four columns with styles */
        sprintf(line,
            "%s \"%s\" using 0:1 title \"%s\" with %s ls 6 pt 31 ps 1, "
            "\"\" using 0:2 title \"%s\" with lines ls 3, "
            "\"\" using 0:3 title \"%s\" with lines ls 3, "
            "\"\" using 0:4 title \"%s\" with points ls 1",
            cmd, name, title, handle->pstyle, title, title, title);
    }

    /* Send command to gnuplot */
    gnuplot_cmd(handle, line);
    handle->nplots++;
}


/*--------------------------------------------------------------------------*/

#endif

/* vim: set ts=4 et sw=4 tw=75 */
