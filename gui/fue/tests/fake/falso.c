/* falso.c -- un motor de mentira, para que las pruebas no dependan de que fue
 * o fuf esten instalados. Contesta con el estado que pide su primer argumento
 * y escribe la clase de mensaje que escribe el de verdad. Se compila dos
 * veces, como fue y como fuf (tests/run_tests.sh).
 *
 * ES UN PROGRAMA Y NO UN GUION, y no por gusto: eran dos guiones de sh, y
 * GLib en Windows no puede lanzar un guion -- CreateProcess solo arranca
 * ejecutables --, asi que alli la prueba del lanzador de los GUIs no probaba
 * nada: todos los casos salian "could not read the input file". Un falso en
 * C corre igual en las tres plataformas.                                   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

int main(int argc, char **argv)
{
    const char *caso = argc > 1 ? argv[1] : "";

#ifdef _WIN32
    /* En binario, como escriben los motores de verdad en Linux: en modo
     * texto Windows anade \r a cada linea y la prueba leia "2\r" donde
     * el motor habia dicho "2".                                         */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    if (!strcmp(caso, "ok")) {
        printf("FUE: banner\nCreated ok.pdf\n");
        return 0;
    }
    if (!strcmp(caso, "bad")) {
        fprintf(stderr, "Error in the input file bad.inp, line 2: the file ends too soon\n");
        printf("FUE: banner\n");
        return 2;
    }
    if (!strcmp(caso, "nofile")) {
        fprintf(stderr, "Error opening input file: nofile.inp\n");
        return 1;
    }
    if (!strcmp(caso, "noest")) {
        printf("FUE: banner\n");
        fprintf(stderr, "Warning: the model could not be estimated\n");
        return 3;
    }
    if (!strcmp(caso, "latin1")) {
        /* UN MOTOR NO PROMETE UTF-8. Escribe lo que le llega: un nombre de
         * serie con los bytes con que estaba escrito, la salida de pdflatex
         * o la de gnuplot. Ese byte acaba en el mensaje y el mensaje en una
         * etiqueta de GTK.                                                 */
        fprintf(stderr, "Error en la serie Espa\361a: no cuadra\n");
        return 2;
    }
    if (!strcmp(caso, "crash")) {
        fflush(stdout);
#ifdef _WIN32
        /* Sin el cuadro de "el programa dejo de funcionar", que en la CI se
         * quedaria esperando una mano.                                     */
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, NULL);
#else
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
#endif
        return 99;
    }
    if (!strcmp(caso, "argv")) {
        /* cuantos argumentos llegaron, y cual era el primero */
        printf("%d\n%s%s\n", argc - 1, argc > 1 ? argv[1] : "", argc > 2 ? argv[2] : "");
        return 0;
    }
    if (!strcmp(caso, "iter")) {
        /* lo que escribe el optimizador: la iteracion y la funcion objetivo,
         * todo en una linea y sin salto entre ellas                         */
        char v[16];
        int  k;

        printf("FUE: banner\n");
        for (k = 0; k <= 12; k++) {
            snprintf(v, sizeof v, "0.9%d", k);
            printf("%4d F: %0.10f", k, atof(v));
        }
        printf("\n");
        return 0;
    }
    return 1;
}
