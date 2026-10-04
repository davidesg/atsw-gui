/* fake_drvarma.c -- un motor que siempre falla.
 *
 * Sirve para comprobar que el GUI lee el estado de salida del motor y no
 * anuncia un exito cuando el motor no lo tuvo. Es un programa y no un guion
 * de shell porque en Windows GLib/CreateProcess no lanzan guiones.       */
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    fprintf(stderr, "ERROR: fake drvarma always fails\n");
    return 3;
}
