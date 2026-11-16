/*
 * asistente.c -- el menu Asistente de la madre (docs/ESTUDIO-terminal-mcp.md
 * §5, §6.2 y §7.1).
 *
 * El asistente es un cliente de LLM (Claude Code, Gemini CLI, Codex,
 * CodeWhale) en una terminal, con los MCP de ATSW: atsw-us, atsw-mt y
 * atsw-sis (mcp/atsw_mcp). La madre no lo lleva dentro: lo PREPARA, abre la
 * terminal en la carpeta del proyecto y enseña lo que ha hecho. Lo que el
 * asistente escribe lo ve en vivo (vigila.c) y lo marca (por: asistente).
 *
 * TRES REGLAS, del estudio:
 *
 *   - NINGUNA CLAVE EN EL PROYECTO. Cada cliente guarda la suya en su
 *     configuracion de usuario; un proyecto se comparte, se copia y se
 *     publica. Lo que se escribe aqui son rutas de programas y nada mas.
 *   - LO QUE YA EXISTE NO SE PISA. AGENTS.md es del usuario (un profesor
 *     puede fijar ahi el carril guiado); un .mcp.json o un settings.json que
 *     ya estaban pueden llevar otros servidores. Se escriben si no existen y,
 *     si existen, se enseña el bloque para que lo añada quien sabe que hay.
 *   - LA CONFIGURACION GLOBAL (Codex, CodeWhale) NO SE TOCA: se enseña el
 *     bloque. Esa configuracion no sabe de proyectos; el servidor busca el
 *     proyecto subiendo desde la carpeta donde arranca.
 */

#include <string.h>

#include <glib/gstdio.h>
#ifndef G_OS_WIN32
#include <sys/wait.h>
#endif

#include "atsw.h"
#include "sitio.h"

void barra_pub( Atsw *a, const char *s );

static const char *const SERVIDORES[] = { "atsw-us", "atsw-mt", "atsw-sis" };
#define NSERV G_N_ELEMENTS(SERVIDORES)

/* ------------------------------------------------------------------------ */
/* DONDE ESTA                                                                */
/* ------------------------------------------------------------------------ */

/* La carpeta donde «Instalar el asistente…» lo deja (§6.2): en Windows
 * %LOCALAPPDATA%\ATSW\asistente, en macOS ~/Library/Application Support/
 * ATSW/asistente, y en Linux ~/.local/share/ATSW/asistente.             */
gchar *atsw_asistente_dir( void )
{
#ifdef __APPLE__
    return g_build_filename( g_get_home_dir(), "Library", "Application Support",
                             "ATSW", "asistente", NULL );
#else
    return g_build_filename( g_get_user_data_dir(), "ATSW", "asistente", NULL );
#endif
}

static gchar *en_venv( const char *venv, const char *prog )
{
    gchar *exe = sitio_exe( prog ), *r;
#ifdef G_OS_WIN32
    r = g_build_filename( venv, "Scripts", exe, NULL );
#else
    r = g_build_filename( venv, "bin", exe, NULL );
#endif
    g_free( exe );
    return r;
}

/* Un servidor: el instalado por la madre, el del arbol (mcp/atsw_mcp con su
 * uv sync) y, al final, el PATH. NULL si no esta.                       */
gchar *atsw_asistente_servidor( const char *nombre )
{
    static const char *const arbol[] = {
#ifdef G_OS_WIN32
        "../../mcp/atsw_mcp/.venv/Scripts/%s",
#else
        "../../mcp/atsw_mcp/.venv/bin/%s",
#endif
        NULL };
    gchar *dir = atsw_asistente_dir(), *v = g_build_filename( dir, ".venv", NULL );
    gchar *r = en_venv( v, nombre );

    g_free( dir ); g_free( v );
    if ( g_file_test( r, G_FILE_TEST_IS_EXECUTABLE ) ) return r;
    g_free( r );
    r = sitio_busca( nombre, arbol );
    if ( r && g_file_test( r, G_FILE_TEST_IS_EXECUTABLE ) )
        {
        /* Sin «/../»: la ruta va escrita en la configuracion del cliente. */
        gchar *c = g_canonicalize_filename( r, NULL );
        g_free( r );
        return c;
        }
    g_free( r );
    return NULL;
}

gboolean atsw_asistente_instalado( void )
{
    gchar *r = atsw_asistente_servidor( "atsw-us" );
    gboolean si = r != NULL;

    g_free( r );
    return si;
}

/* La fuente de atsw-gui-mcp, con su uv.lock: en el arbol, o junto al
 * paquete (share/atsw/mcp/atsw_mcp, cuando el paquete la lleve).        */
static gchar *fuente_mcp( void )
{
    static const char *const sitios[] = {
        "../../mcp/atsw_mcp", "../share/atsw/mcp/atsw_mcp", "mcp/atsw_mcp", NULL };
    gchar *yo = sitio_mi_dir();
    guint  i;

    for ( i = 0; yo && sitios[i]; i++ )
        {
        gchar *d = g_build_filename( yo, sitios[i], NULL );
        gchar *l = g_build_filename( d, "uv.lock", NULL );
        gboolean hay = g_file_test( l, G_FILE_TEST_EXISTS );

        g_free( l );
        if ( hay ) { g_free( yo ); return d; }
        g_free( d );
        }
    g_free( yo );
    return NULL;
}

/* ------------------------------------------------------------------------ */
/* EL ANDAMIAJE DEL PROYECTO (§7.1)                                          */
/* ------------------------------------------------------------------------ */

static const char AGENTS_MD[] =
"# Cómo trabajar en un proyecto de ATSW\n"
"\n"
"Este es un proyecto de ATSW (análisis de series temporales, metodología\n"
"Box-Jenkins). Lo abre la madre, `atsw_gui`, y lo trabajan el analista y,\n"
"si se le pide, un asistente con los servidores MCP `atsw-us` (modelos\n"
"univariantes), `atsw-mt` (transferencias) y `atsw-sis` (sistemas VARMA y\n"
"VEC). Este fichero es del usuario: la madre lo escribe una vez y no lo\n"
"vuelve a tocar. Quien quiera fijar un procedimiento lo escribe aquí.\n"
"\n"
"## Lo que hay en la carpeta\n"
"\n"
"- `proyecto.yaml` es el manifiesto: las series, sus modelos, el linaje\n"
"  (de qué modelo sale cada uno), la razón de cada paso, el elegido de cada\n"
"  serie y los casos. **No se edita a mano**: lo escriben la madre y las\n"
"  herramientas de los servidores, que saben fusionar lo que cambie otro\n"
"  programa a la vez.\n"
"- Cada serie tiene su carpeta. Su `datos.csv` es el dato: **no se toca**.\n"
"  El nodo de datos (`m00`) tampoco: de él cuelga todo.\n"
"- Cada modelo es un `.inp` (la especificación), y al estimarlo con fue, un\n"
"  `.out` (el registro y su diagnosis) y un `.pre` (el óptimo, reejecutable).\n"
"  Un `.pre` no se escribe a mano: sólo el programa que estimó puede afirmar\n"
"  un óptimo.\n"
"- Las muestras (por ejemplo, «Pre-Covid») son ventanas de la misma serie.\n"
"  Cada modelo vive en su muestra.\n"
"\n"
"## Cómo se trabaja\n"
"\n"
"- **Con las herramientas de los servidores**, no con órdenes de sistema ni\n"
"  editando ficheros. Reciben series, modelos y muestras por su nombre, nunca\n"
"  rutas, y sólo escriben dentro de esta carpeta.\n"
"- **Cada paso es un modelo nuevo**, hijo del anterior, con su razón. No se\n"
"  reescribe un modelo estimado: se deriva otro. Así el linaje cuenta el\n"
"  recorrido entero.\n"
"- **La razón se escribe siempre**, y dice por qué: «el MA(12) sobra, t =\n"
"  0,4», no «modelo nuevo». Sin razón, el paso no se puede revisar después.\n"
"- **Lo que hace el asistente queda marcado** en el manifiesto (`por:\n"
"  asistente`), y la madre lo enseña. No se finge que lo hizo el analista.\n"
"- **El carril lo pregunta cada servidor** al empezar: guiado (el analista\n"
"  decide cada nodo) o autónomo (el asistente decide, con su razón por\n"
"  escrito). Este fichero no lo elige, salvo que el usuario lo escriba aquí.\n"
"- **Los gráficos son las ventanas de la madre**: la herramienta `plot` las\n"
"  abre y devuelve en texto lo que enseñan.\n"
"\n"
"## Para seguir el análisis\n"
"\n"
"La madre enseña en vivo lo que se hace aquí: el modelo nuevo aparece en su\n"
"lista con la marca [asistente], y «Asistente ▸ Qué ha hecho el asistente…»\n"
"lo lista. El manual de ATSW (Ayuda ▸ Manual) explica cada paso.\n";

/* Una cadena JSON: comillas y barras escapadas (las rutas de Windows). */
static void json_str( GString *g, const char *s )
{
    g_string_append_c( g, '"' );
    for ( ; s && *s; s++ )
        {
        if ( *s == '"' || *s == '\\' ) g_string_append_c( g, '\\' );
        g_string_append_c( g, *s );
        }
    g_string_append_c( g, '"' );
}

/* ATSW_GUI y ATSW_BIN: la madre y los motores de ESTE paquete, para que el
 * servidor use esos y no otros del PATH (§7.1).                        */
static void rutas_paquete( gchar **gui, gchar **bin )
{
    gchar *cli = atsw_programa( "atsw-cli" );
    gchar *yo  = sitio_mi_dir();
    gchar *exe = sitio_exe( "atsw_gui" );

    gchar *b;

    *gui = yo ? g_build_filename( yo, exe, NULL ) : g_strdup( "" );
    b    = cli ? g_path_get_dirname( cli ) : g_strdup( yo ? yo : "" );
    /* Sin «/../»: van escritas en la configuracion del cliente. */
    if ( **gui ) { gchar *c = g_canonicalize_filename( *gui, NULL ); g_free( *gui ); *gui = c; }
    *bin = *b ? g_canonicalize_filename( b, NULL ) : g_strdup( "" );
    g_free( b ); g_free( cli ); g_free( yo ); g_free( exe );
}

/* Los servidores en el formato de cada cliente. */
typedef enum { F_CLAUDE, F_GEMINI, F_CODEWHALE, F_CODEX } Formato;

gchar *atsw_asistente_bloque( int formato )
{
    GString *g = g_string_new( NULL );
    gchar   *gui, *bin;
    guint    i;

    rutas_paquete( &gui, &bin );
    if ( formato == F_CODEX )
        {
        for ( i = 0; i < NSERV; i++ )
            {
            gchar *s = atsw_asistente_servidor( SERVIDORES[i] );

            /* Comillas simples en TOML: la ruta de Windows con una barra. */
            g_string_append_printf( g, "%s[mcp_servers.%s]\ncommand = '%s'\n"
                                    "env = { ATSW_GUI = '%s', ATSW_BIN = '%s' }\n",
                                    i ? "\n" : "", SERVIDORES[i], s ? s : SERVIDORES[i],
                                    gui, bin );
            g_free( s );
            }
        g_free( gui ); g_free( bin );
        return g_string_free( g, FALSE );
        }

    g_string_append( g, formato == F_CODEWHALE ? "{\n  \"servers\": {\n" : "{\n  \"mcpServers\": {\n" );
    for ( i = 0; i < NSERV; i++ )
        {
        gchar *s = atsw_asistente_servidor( SERVIDORES[i] );

        g_string_append( g, "    " );
        json_str( g, SERVIDORES[i] );
        g_string_append( g, ": { \"command\": " );
        json_str( g, s ? s : SERVIDORES[i] );
        g_string_append( g, ", \"args\": [], \"env\": { \"ATSW_GUI\": " );
        json_str( g, gui );
        g_string_append( g, ", \"ATSW_BIN\": " );
        json_str( g, bin );
        g_string_append( g, " }" );
        /* Gemini corta a los 10 minutos por defecto: el bootstrap del rango
           tarda mas (§7.1).                                            */
        if ( formato == F_GEMINI )    g_string_append( g, ", \"timeout\": 600000" );
        if ( formato == F_CODEWHALE ) g_string_append( g, ", \"disabled\": false" );
        g_string_append_printf( g, " }%s\n", i + 1 < NSERV ? "," : "" );
        g_free( s );
        }
    g_string_append( g, "  }" );
    if ( formato == F_GEMINI )
        g_string_append( g, ",\n  \"context\": { \"fileName\": \"AGENTS.md\" }" );
    g_string_append( g, "\n}\n" );
    g_free( gui ); g_free( bin );
    return g_string_free( g, FALSE );
}

/* Claude Code: permite las herramientas de los servidores y niega las de
 * escribir y ejecutar (§7.5, la segunda capa).                         */
static const char CLAUDE_SETTINGS[] =
"{\n"
"  \"permissions\": {\n"
"    \"allow\": [ \"mcp__atsw-us\", \"mcp__atsw-mt\", \"mcp__atsw-sis\" ],\n"
"    \"deny\": [ \"Bash\", \"Edit\", \"Write\", \"NotebookEdit\" ]\n"
"  }\n"
"}\n";

/* Escribe f con texto si NO existe. Lo apunta en hecho o en dejado. */
static void escribe_si_falta( const char *raiz, const char *rel, const char *texto,
                              GString *hecho, GString *dejado, GString *fallo )
{
    gchar *f = g_build_filename( raiz, rel, NULL );
    gchar *d = g_path_get_dirname( f );
    GError *err = NULL;

    if ( g_file_test( f, G_FILE_TEST_EXISTS ) )
        g_string_append_printf( dejado, "%s%s", dejado->len ? ", " : "", rel );
    else if ( g_mkdir_with_parents( d, 0755 ) != 0 ||
              !g_file_set_contents( f, texto, -1, &err ) )
        {
        g_string_append_printf( fallo, "%s: %s\n", rel, err ? err->message : "no pude crear su carpeta" );
        g_clear_error( &err );
        }
    else
        g_string_append_printf( hecho, "%s%s", hecho->len ? ", " : "", rel );
    g_free( f ); g_free( d );
}

/* Las instrucciones de los tres servidores, para los clientes que no las
 * pasan al modelo (§7.1). Se rehace siempre: es nuestro y es generado. */
static gboolean instrucciones( const char *raiz, GString *fallo )
{
    GString *t = g_string_new( "# Las instrucciones de los servidores de ATSW\n\n"
        "Generado por la madre (Asistente ▸ Preparar el proyecto para el asistente…) "
        "con `--instrucciones` de cada servidor. Para los clientes que no pasan al "
        "modelo las instrucciones del servidor. Se rehace cada vez: no lo edites.\n" );
    gchar   *f;
    guint    i;
    gboolean ok = TRUE;

    for ( i = 0; i < NSERV; i++ )
        {
        gchar *s = atsw_asistente_servidor( SERVIDORES[i] ), *out = NULL;
        gchar *argv[] = { s, (gchar *) "--instrucciones", NULL };
        gint   st = 0;

        if ( s && sitio_lanza_y_espera( raiz, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL,
                                        &out, NULL, &st, NULL ) && st == 0 && out )
            g_string_append_printf( t, "\n---\n\n## %s\n\n%s\n", SERVIDORES[i], out );
        else
            { ok = FALSE;
              g_string_append_printf( fallo, "%s --instrucciones no respondió.\n", SERVIDORES[i] ); }
        g_free( s ); g_free( out );
        }
    f = g_build_filename( raiz, "instrucciones_servidor.md", NULL );
    if ( !g_file_set_contents( f, t->str, -1, NULL ) ) ok = FALSE;
    g_free( f );
    g_string_free( t, TRUE );
    return ok;
}

/* PREPARA EL PROYECTO para los clientes de la mascara (AS_*). Devuelve en
 * informe lo hecho, lo dejado como estaba y los bloques que hay que pegar.
 * 0 si se escribio todo lo que se podia.                               */
int atsw_asistente_prepara( Atsw *a, unsigned clientes, GString *informe )
{
    char     raiz[PR_RUTA];
    GString *hecho = g_string_new( NULL ), *dejado = g_string_new( NULL ),
            *fallo = g_string_new( NULL );
    int      rc;

    if ( !a->hay || pr_raiz( a->p, raiz, sizeof raiz ) != 0 )
        { g_string_append( informe, "No hay proyecto abierto." ); return 1; }

    escribe_si_falta( raiz, "AGENTS.md", AGENTS_MD, hecho, dejado, fallo );
    if ( clientes & AS_CLAUDE )
        {
        gchar *b = atsw_asistente_bloque( F_CLAUDE );

        escribe_si_falta( raiz, "CLAUDE.md", "@AGENTS.md\n", hecho, dejado, fallo );
        escribe_si_falta( raiz, ".mcp.json", b, hecho, dejado, fallo );
        escribe_si_falta( raiz, ".claude/settings.json", CLAUDE_SETTINGS, hecho, dejado, fallo );
        g_free( b );
        }
    if ( clientes & AS_GEMINI )
        {
        gchar *b = atsw_asistente_bloque( F_GEMINI );
        gchar *rel = g_build_filename( ".gemini", "settings.json", NULL );

        escribe_si_falta( raiz, rel, b, hecho, dejado, fallo );
        g_free( b ); g_free( rel );
        }
    if ( instrucciones( raiz, fallo ) )
        g_string_append_printf( hecho, "%sinstrucciones_servidor.md", hecho->len ? ", " : "" );

    g_string_append_printf( informe, "En %s:\n\n", raiz );
    if ( hecho->len )  g_string_append_printf( informe, "Escritos: %s.\n", hecho->str );
    if ( dejado->len ) g_string_append_printf( informe, "Ya estaban y no se han tocado: %s. "
                                               "Si les faltan los servidores de ATSW, "
                                               "añade el bloque de abajo.\n", dejado->str );
    if ( fallo->len )  g_string_append_printf( informe, "\nNo se pudo:\n%s", fallo->str );
    g_string_append( informe, "\nNingún fichero lleva claves: cada cliente guarda la suya "
                              "en su configuración de usuario.\n" );

    /* LOS BLOQUES: el de los que ya estaban, y el de los globales. */
    if ( ( clientes & AS_CLAUDE ) && strstr( dejado->str, ".mcp.json" ) )
        {
        gchar *b = atsw_asistente_bloque( F_CLAUDE );
        g_string_append_printf( informe, "\n— Claude Code, en .mcp.json:\n\n%s", b );
        g_free( b );
        }
    if ( ( clientes & AS_GEMINI ) && strstr( dejado->str, "settings.json" ) &&
         strstr( dejado->str, ".gemini" ) )
        {
        gchar *b = atsw_asistente_bloque( F_GEMINI );
        g_string_append_printf( informe, "\n— Gemini CLI, en .gemini/settings.json:\n\n%s", b );
        g_free( b );
        }
    if ( clientes & AS_CODEX )
        {
        gchar *b = atsw_asistente_bloque( F_CODEX );
        g_string_append_printf( informe, "\n— Codex: su configuración es global. Añade al "
                                "final de ~/.codex/config.toml:\n\n%s", b );
        g_free( b );
        }
    if ( clientes & AS_CODEWHALE )
        {
        gchar *b = atsw_asistente_bloque( F_CODEWHALE );
        g_string_append_printf( informe, "\n— CodeWhale: su configuración es global. En "
                                "~/.codewhale/mcp.json:\n\n%s", b );
        g_free( b );
        }

    rc = fallo->len ? 1 : 0;
    g_string_free( hecho, TRUE ); g_string_free( dejado, TRUE ); g_string_free( fallo, TRUE );
    return rc;
}

/* ------------------------------------------------------------------------ */
/* LA TERMINAL DEL SISTEMA (§5)                                              */
/* ------------------------------------------------------------------------ */

/* La orden que abre la terminal en dir. NULL si no hay ninguna. Aparte
 * para la prueba: comprueba la orden, no abre ventanas en la CI.        */
gchar **atsw_terminal_orden( const char *dir )
{
    GPtrArray *v = g_ptr_array_new();
    gchar     *p;

#ifdef G_OS_WIN32
    /* Windows Terminal si esta (viene en Windows 11); si no, cmd. Nunca un
       script de PowerShell (§3.1).                                    */
    if ( ( p = g_find_program_in_path( "wt.exe" ) ) != NULL )
        {
        g_ptr_array_add( v, p );
        g_ptr_array_add( v, g_strdup( "-d" ) );
        g_ptr_array_add( v, g_strdup( dir ) );
        }
    else
        {
        /* «start» abre la ventana: el que lanza la madre va suelto y sin
           consola (lib/sitio), y un cmd sin consola no se ve.         */
        g_ptr_array_add( v, g_strdup( "cmd.exe" ) );
        g_ptr_array_add( v, g_strdup( "/c" ) );
        g_ptr_array_add( v, g_strdup( "start" ) );
        g_ptr_array_add( v, g_strdup( "/D" ) );
        g_ptr_array_add( v, g_strdup( dir ) );
        g_ptr_array_add( v, g_strdup( "cmd.exe" ) );
        g_ptr_array_add( v, g_strdup( "/k" ) );
        }
#elif defined(__APPLE__)
    (void) p;
    g_ptr_array_add( v, g_strdup( "open" ) );
    g_ptr_array_add( v, g_strdup( "-a" ) );
    g_ptr_array_add( v, g_strdup( "Terminal" ) );
    g_ptr_array_add( v, g_strdup( dir ) );
#else
    {
    static const char *const otras[] = { "x-terminal-emulator", "gnome-terminal", "konsole",
                                         "xfce4-terminal", "xterm", NULL };
    const char *t = g_getenv( "TERMINAL" );
    guint i;

    p = ( t && *t ) ? g_find_program_in_path( t ) : NULL;
    for ( i = 0; p == NULL && otras[i]; i++ ) p = g_find_program_in_path( otras[i] );
    if ( p == NULL ) { g_ptr_array_free( v, TRUE ); return NULL; }
    g_ptr_array_add( v, p );        /* se abre en dir: es su carpeta de trabajo */
    }
#endif
    g_ptr_array_add( v, NULL );
    return (gchar **) g_ptr_array_free( v, FALSE );
}

static void on_terminal( GtkMenuItem *mi, Atsw *a )
{
    char    raiz[PR_RUTA];
    gchar **argv;
    GError *err = NULL;
    long    pid = 0;

    (void) mi;
    if ( !a->hay || pr_raiz( a->p, raiz, sizeof raiz ) != 0 )
        { barra_pub( a, "Abre un proyecto antes." ); return; }
    argv = atsw_terminal_orden( raiz );
    if ( argv == NULL )
        { barra_pub( a, "No encuentro una terminal: pon su nombre en la variable TERMINAL." ); return; }
    if ( !sitio_lanza_suelto( raiz, argv, &pid, &err ) )
        {
        gchar *t = g_strdup_printf( "No pude abrir la terminal: %s", err ? err->message : "?" );
        barra_pub( a, t );
        g_free( t );
        g_clear_error( &err );
        }
    else
        {
        gchar *ag = g_build_filename( raiz, "AGENTS.md", NULL );

        /* UNA LINEA SOBRE QUE ESCRIBIR (§5): la madre no lanza el cliente. */
        barra_pub( a, g_file_test( ag, G_FILE_TEST_EXISTS )
            ? "Terminal abierta en el proyecto. Escribe el nombre de tu cliente "
              "(claude, gemini, codex, codewhale) para empezar."
            : "Terminal abierta en el proyecto. Antes de lanzar el cliente, "
              "Asistente ▸ Preparar el proyecto para el asistente…" );
        g_free( ag );
        }
    g_strfreev( argv );
}

/* ------------------------------------------------------------------------ */
/* LOS DIALOGOS                                                              */
/* ------------------------------------------------------------------------ */

/* Un texto largo en una ventana, seleccionable para copiar los bloques. */
static void enseña_texto( Atsw *a, const char *titulo, const char *texto )
{
    GtkWidget *d = gtk_dialog_new_with_buttons( titulo, GTK_WINDOW(a->ventana),
                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                       "_Cerrar", GTK_RESPONSE_CLOSE, NULL );
    GtkWidget *sc = gtk_scrolled_window_new( NULL, NULL );
    GtkWidget *tv = gtk_text_view_new();

    gtk_text_view_set_editable( GTK_TEXT_VIEW(tv), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(tv), TRUE );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR );
    gtk_text_view_set_left_margin( GTK_TEXT_VIEW(tv), 8 );
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(tv) ), texto, -1 );
    gtk_container_add( GTK_CONTAINER(sc), tv );
    gtk_widget_set_size_request( sc, 720, 420 );
    gtk_box_pack_start( GTK_BOX(gtk_dialog_get_content_area( GTK_DIALOG(d) )), sc, TRUE, TRUE, 0 );
    gtk_widget_show_all( d );
    gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
}

static const struct { const char *rotulo, *prog; unsigned bit; } CLIENTES[] = {
    { "Claude Code — .mcp.json y CLAUDE.md en el proyecto", "claude", AS_CLAUDE },
    { "Gemini CLI — .gemini/settings.json en el proyecto",  "gemini", AS_GEMINI },
    { "Codex — configuración global: se enseña el bloque",  "codex", AS_CODEX },
    { "CodeWhale — configuración global: se enseña el bloque", "codewhale", AS_CODEWHALE } };

static void on_prepara( GtkMenuItem *mi, Atsw *a )
{
    GtkWidget *d, *caja, *l, *c[G_N_ELEMENTS(CLIENTES)];
    unsigned   m = 0;
    guint      i;

    (void) mi;
    d = gtk_dialog_new_with_buttons( "Preparar el proyecto para el asistente",
            GTK_WINDOW(a->ventana), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Preparar", GTK_RESPONSE_OK, NULL );
    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l),
        "¿Con qué cliente vas a trabajar? Se escribe <b>AGENTS.md</b> (cómo se trabaja "
        "en un proyecto de ATSW) si no existe, y la configuración de cada cliente.\n"
        "<small>Lo que ya existe no se toca. Ningún fichero lleva claves.</small>" );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_label_set_max_width_chars( GTK_LABEL(l), 60 );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );
    for ( i = 0; i < G_N_ELEMENTS(CLIENTES); i++ )
        {
        gchar *p = g_find_program_in_path( CLIENTES[i].prog );

        c[i] = gtk_check_button_new_with_label( CLIENTES[i].rotulo );
        /* Marcados los que estan instalados; Claude Code si no hay ninguno. */
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c[i]), p != NULL );
        if ( p == NULL )
            {
            gchar *g = g_strdup_printf( "«%s» no está en el PATH: se puede preparar igual.",
                                        CLIENTES[i].prog );
            gtk_widget_set_tooltip_text( c[i], g );
            g_free( g );
            }
        g_free( p );
        gtk_box_pack_start( GTK_BOX(caja), c[i], FALSE, FALSE, 0 );
        }
    {
    gboolean alguno = FALSE;
    for ( i = 0; i < G_N_ELEMENTS(CLIENTES); i++ )
        alguno |= gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(c[i]) );
    if ( !alguno ) gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c[0]), TRUE );
    }
    gtk_box_pack_start( GTK_BOX(gtk_dialog_get_content_area( GTK_DIALOG(d) )), caja, TRUE, TRUE, 0 );
    gtk_widget_show_all( d );
    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_OK )
        for ( i = 0; i < G_N_ELEMENTS(CLIENTES); i++ )
            if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(c[i]) ) ) m |= CLIENTES[i].bit;
    gtk_widget_destroy( d );
    if ( m == 0 ) return;

    {
    GString *inf = g_string_new( NULL );
    int      rc  = atsw_asistente_prepara( a, m, inf );

    barra_pub( a, rc == 0 ? "Proyecto preparado para el asistente."
                          : "Proyecto preparado, con algo que no se pudo: mira el informe." );
    enseña_texto( a, "El proyecto, preparado para el asistente", inf->str );
    g_string_free( inf, TRUE );
    }
}

/* «Comprobar el asistente…»: lo instalado, sus versiones y los clientes. */
gchar *atsw_asistente_informe( Atsw *a )
{
    GString *t = g_string_new( "LOS SERVIDORES\n\n" );
    gchar   *dir = atsw_asistente_dir(), *uv = g_find_program_in_path( "uv" );
    guint    i;

    for ( i = 0; i < NSERV; i++ )
        {
        gchar *s = atsw_asistente_servidor( SERVIDORES[i] ), *out = NULL;
        gchar *argv[] = { s, (gchar *) "--version", NULL };
        gint   st = 0;

        if ( s && sitio_lanza_y_espera( NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL,
                                        &out, NULL, &st, NULL ) && st == 0 && out )
            g_string_append_printf( t, "  %-9s %s   (%s)\n", SERVIDORES[i], g_strstrip( out ), s );
        else
            g_string_append_printf( t, "  %-9s no está instalado\n", SERVIDORES[i] );
        g_free( s ); g_free( out );
        }
    g_string_append_printf( t, "\n  Se instala en %s\n  uv: %s\n", dir, uv ? uv : "no está" );

    g_string_append( t, "\nLOS CLIENTES (en el PATH)\n\n" );
    for ( i = 0; i < G_N_ELEMENTS(CLIENTES); i++ )
        {
        gchar *p = g_find_program_in_path( CLIENTES[i].prog );
        g_string_append_printf( t, "  %-10s %s\n", CLIENTES[i].prog, p ? p : "falta" );
        g_free( p );
        }
    g_string_append( t, "\n  Cada cliente se instala con su propio instalador; la clave "
                        "la guarda él, nunca el proyecto.\n" );

    if ( a->hay )
        {
        char raiz[PR_RUTA];

        if ( pr_raiz( a->p, raiz, sizeof raiz ) == 0 )
            {
            static const char *const f[] = { "AGENTS.md", ".mcp.json", ".gemini/settings.json",
                                             "instrucciones_servidor.md" };
            g_string_append( t, "\nEL PROYECTO\n\n" );
            for ( i = 0; i < G_N_ELEMENTS(f); i++ )
                {
                gchar *x = g_build_filename( raiz, f[i], NULL );
                g_string_append_printf( t, "  %-26s %s\n", f[i],
                                        g_file_test( x, G_FILE_TEST_EXISTS ) ? "está" : "no está" );
                g_free( x );
                }
            }
        }
    g_free( dir ); g_free( uv );
    return g_string_free( t, FALSE );
}

static void on_comprobar( GtkMenuItem *mi, Atsw *a )
{
    gchar *t = atsw_asistente_informe( a );

    (void) mi;
    enseña_texto( a, "El asistente", t );
    g_free( t );
}

/* «Instalar el asistente…» con uv (§6.2): un Python y atsw-gui-mcp con su
 * uv.lock, en atsw_asistente_dir(). Corre aparte y la barra dice cuando
 * acaba: tarda lo que tarde la descarga.                                */
typedef struct { Atsw *a; gchar *dir; } Instala;

static void al_instalar( GPid pid, gint st, gpointer d )
{
    Instala *x = d;
    gchar   *t;

    gboolean ok;

    g_spawn_close_pid( pid );
#ifdef G_OS_WIN32
    ok = st == 0;
#else
    ok = WIFEXITED( st ) && WEXITSTATUS( st ) == 0;
#endif
    t = ok
        ? g_strdup_printf( "El asistente está instalado, en %s. Ahora: Asistente ▸ Preparar "
                           "el proyecto para el asistente…", x->dir )
        : g_strdup_printf( "La instalación del asistente falló (uv salió con %d). "
                           "Asistente ▸ Comprobar el asistente… dice qué hay.", st );
    barra_pub( x->a, t );
    g_free( t ); g_free( x->dir ); g_free( x );
}

static void on_instalar( GtkMenuItem *mi, Atsw *a )
{
    gchar   *uv = g_find_program_in_path( "uv" ), *src = fuente_mcp();
    gchar   *dir = atsw_asistente_dir(), *venv;
    gchar  **env;
    GPid     pid;
    GError  *err = NULL;

    (void) mi;
    if ( uv == NULL || src == NULL )
        {
        barra_pub( a, uv ? "No encuentro la fuente del asistente (mcp/atsw_mcp)."
                         : "Para instalar el asistente hace falta uv: https://docs.astral.sh/uv/" );
        g_free( uv ); g_free( src ); g_free( dir );
        return;
        }
    g_mkdir_with_parents( dir, 0755 );
    venv = g_build_filename( dir, ".venv", NULL );
    env  = g_environ_setenv( g_get_environ(), "UV_PROJECT_ENVIRONMENT", venv, TRUE );
    {
    gchar *argv[] = { uv, (gchar *) "sync", (gchar *) "--frozen", (gchar *) "--no-dev",
                      (gchar *) "--no-editable", (gchar *) "--project", src, NULL };

    /* SIN SHELL Y CON LA ENTRADA CERRADA, como lib/engine. */
    if ( !g_spawn_async( dir, argv, env, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDIN_FROM_DEV_NULL,
                         NULL, NULL, &pid, &err ) )
        {
        gchar *t = g_strdup_printf( "No pude lanzar uv: %s", err->message );
        barra_pub( a, t );
        g_free( t );
        g_clear_error( &err );
        }
    else
        {
        Instala *x = g_new0( Instala, 1 );

        x->a = a; x->dir = g_strdup( dir );
        g_child_watch_add( pid, al_instalar, x );
        barra_pub( a, "Instalando el asistente con uv: un Python y atsw-gui-mcp. "
                      "La barra dirá cuándo acaba." );
        }
    }
    g_strfreev( env );
    g_free( uv ); g_free( src ); g_free( dir ); g_free( venv );
}

/* ------------------------------------------------------------------------ */
/* QUE HA HECHO EL ASISTENTE (§7.2)                                          */
/* ------------------------------------------------------------------------ */

enum { QH_SERIE, QH_MUESTRA, QH_MODELO, QH_QUE, QH_RAZON, QH_COLS };

/* Las filas: cada modelo que derivo y cada eleccion que hizo. Cuantas. */
int atsw_asistente_pasos( const Proyecto *p, GtkListStore *st )
{
    int i, n = 0;

    for ( i = 0; p && i < p->nm; i++ )
        {
        const PrModelo *m = &p->m[i];
        gboolean        hizo = !strcmp( m->por, "asistente" );
        gboolean        eligio = m->elegido && !strcmp( m->por_elegido, "asistente" );
        GtkTreeIter     it;

        if ( !hizo && !eligio ) continue;
        n++;
        if ( st == NULL ) continue;
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            QH_SERIE,   m->serie,
            QH_MUESTRA, m->muestra[0] ? m->muestra : "completa",
            QH_MODELO,  m->id,
            QH_QUE,     hizo && eligio ? "lo derivó y lo eligió" : hizo ? "lo derivó" : "lo eligió",
            QH_RAZON,   eligio && m->razon_elegido[0] && !hizo ? m->razon_elegido
                        : ( m->razon[0] ? m->razon : "(sin razón)" ),
            -1 );
        }
    return n;
}

static void on_que_ha_hecho( GtkMenuItem *mi, Atsw *a )
{
    GtkWidget       *win, *caja, *sc, *tv, *pie;
    GtkListStore    *st;
    GtkCellRenderer *r;
    static const char *const tit[] = { "Serie", "Muestra", "Modelo", "Qué", "Razón" };
    int              i, n;
    gchar           *t;

    (void) mi;
    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }
    st = gtk_list_store_new( QH_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    n = atsw_asistente_pasos( a->p, st );

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_transient_for( GTK_WINDOW(win), GTK_WINDOW(a->ventana) );
    gtk_window_set_default_size( GTK_WINDOW(win), 820, 420 );
    gtk_window_set_title( GTK_WINDOW(win), "Qué ha hecho el asistente" );
    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );
    gtk_container_add( GTK_CONTAINER(win), caja );

    tv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    for ( i = 0; i < QH_COLS; i++ )
        {
        r = gtk_cell_renderer_text_new();
        if ( i == QH_RAZON ) g_object_set( r, "wrap-mode", PANGO_WRAP_WORD, "wrap-width", 360, NULL );
        gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(tv), -1, tit[i], r,
                                                     "text", i, NULL );
        }
    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER(sc), tv );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 0 );

    t = n ? g_strdup_printf( "%d paso%s del asistente, leídos del manifiesto (por: asistente). "
                             "Lo que hace el analista no lleva marca. El recorrido entero, "
                             "en Proyecto ▸ Linaje.", n, n == 1 ? "" : "s" )
          : g_strdup( "El asistente no ha hecho nada en este proyecto." );
    pie = gtk_label_new( t );
    g_free( t );
    gtk_label_set_xalign( GTK_LABEL(pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(pie), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), pie, FALSE, FALSE, 0 );
    gtk_widget_show_all( win );
}

/* ------------------------------------------------------------------------ */
/* EL MENU (§7.1)                                                            */
/* ------------------------------------------------------------------------ */

static void separador( GtkWidget *m )
{
    gtk_menu_shell_append( GTK_MENU_SHELL(m), gtk_separator_menu_item_new() );
}

void atsw_menu_asistente( Atsw *a, GtkWidget *m )
{
    GtkWidget *mi;
    gboolean   inst = atsw_asistente_instalado();
    gchar     *uv = g_find_program_in_path( "uv" ), *src = fuente_mcp();

    mi = atsw_entrada( a, m, "Abrir la terminal del sistema",
                       "La terminal de tu sistema, en la carpeta del proyecto: ahí se "
                       "lanza el cliente (claude, gemini, codex, codewhale).",
                       G_CALLBACK(on_terminal), a );
    if ( !a->hay ) atsw_apaga( mi, "Abre un proyecto antes: Proyecto ▸ Abrir… o Nuevo…." );

    mi = atsw_entrada( a, m, "Preparar el proyecto para el asistente…",
                       "Escribe AGENTS.md (cómo se trabaja en un proyecto de ATSW) y la "
                       "configuración de cada cliente, sin claves. Lo que ya existe no se toca.",
                       G_CALLBACK(on_prepara), a );
    if ( !a->hay )     atsw_apaga( mi, "Abre un proyecto antes: Proyecto ▸ Abrir… o Nuevo…." );
    else if ( !inst )  atsw_apaga( mi, "El asistente no está instalado: Asistente ▸ Instalar "
                                       "el asistente…" );
    separador( m );

    mi = atsw_entrada( a, m, "Qué ha hecho el asistente…",
                       "Los modelos que derivó y los que eligió, con su razón: lo que lleva "
                       "«por: asistente» en el manifiesto.", G_CALLBACK(on_que_ha_hecho), a );
    if ( !a->hay ) atsw_apaga( mi, "Abre un proyecto antes: Proyecto ▸ Abrir… o Nuevo…." );

    if ( inst )
        atsw_entrada( a, m, "Comprobar el asistente…",
                      "Los servidores y su versión, los clientes que hay y si este proyecto "
                      "está preparado.", G_CALLBACK(on_comprobar), a );
    else
        {
        mi = atsw_entrada( a, m, "Instalar el asistente…",
                           "Un Python y atsw-gui-mcp, con uv, en tu carpeta de usuario. No "
                           "toca el PATH ni el sistema.", G_CALLBACK(on_instalar), a );
        if ( uv == NULL )
            atsw_apaga( mi, "Hace falta uv (https://docs.astral.sh/uv/). En Windows: "
                            "winget install astral-sh.uv." );
        else if ( src == NULL )
            atsw_apaga( mi, "Este ATSW no lleva la fuente del asistente (mcp/atsw_mcp)." );
        atsw_entrada( a, m, "Comprobar el asistente…",
                      "Qué falta del asistente y los clientes que hay.",
                      G_CALLBACK(on_comprobar), a );
        }
    g_free( uv ); g_free( src );
}
