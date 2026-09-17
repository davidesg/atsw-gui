# `lib/` — lo que comparten los programas

Un fichero que hace falta en dos sitios vive **aquí**, no copiado en los dos.

Ésa es la razón de ser de este repositorio, y está medida: cada divergencia que
encontró el estudio tiene la misma causa —el mismo fichero en dos árboles— y
una de ellas, la copia de `inpcheck` del GUI, divergió **dentro de una sola
sesión de trabajo**, dejando al GUI cargando un fichero que le destruía el
montón.

## `fugdraw/` — el motor vectorial

Escribe EPS y PDF comprimido con Flate desde un único *content stream*. **C
puro: no depende de GTK ni de glib.** Su API son veinte funciones — `fd_line`,
`fd_polyline`, `fd_disc`, `fd_text`, `fd_write_eps`, `fd_pdf_open/page/close`…

Estaba en **cuatro copias byte a byte idénticas** —fue, fuf, fug y el GUI de
fue— y las cuatro tenían el mismo md5. Que estuvieran idénticas no era virtud
del diseño: era suerte, la misma que tuvo `inpcheck` hasta que se le acabó.

Lo usan hoy: `engines/fue`, `engines/fuf`, `engines/fug`, `gui/fue`. Y lo
necesitará el GUI de drtran, si drtran adopta `fugdraw` para dibujar la CCF
preblanqueada en vez de EPS de gnuplot — que es lo que haría coherente la
biblioteca, porque entonces `preview.c` la muestra tal cual, con zoom y lupa.

## Cómo se usa

En el `Makefile` del programa:

    LIB_DIR  = ../../lib
    LIB_SRCS = $(LIB_DIR)/fugdraw/fugdraw.c
    CFLAGS  += -I$(LIB_DIR)/fugdraw

## La prueba de que extraerlo no cambió nada

El EPS que dibuja `fue` desde `lib/` es **byte a byte el mismo** que dibujaba
con su copia:

    md5  2e6ea3c55cf9   antes
    md5  2e6ea3c55cf9   despues

Y los tres bancos, idénticos: 109 corridas y 0 fallos en fue-1.14, 53
comprobaciones y 0 fallos en el GUI, y 102 pasan y 0 fallan en el corpus de
conformidad.

## Lo que falta traer aquí

Por orden de facilidad:

| pieza | dónde está | estado |
|---|---|---|
| `fugplot.c` | `engines/fue`, `engines/fug` | **2 copias idénticas** — el siguiente |
| `preview.c` | `gui/fue` (1496), `gui/fug` (1154) | **divergidas**, y en dos toolkits; la de fue es la de fug portada a GTK3 y ampliada con zoom y lupa. Su `previewhost.h` ya dice en su cabecera que la factorización estaba prevista |
| `engine.c`, `outfile.c`, `utils.c` | `gui/fue` | copia única: traerlos es preparación, no desduplicación |
| `inpcheck_*.c` | `gui/fue` + los motores | copias vigiladas por `conformidad/copias.sh` |
| `nlatools.c` | los siete programas | **NO por ahora.** Siete copias vivas separadas entre 13 y 80 líneas. Son primos cercanos —drtran y drvarma difieren en 13 líneas de 1106— pero es el núcleo numérico y tocarlo mueve números. La mudanza tiene que preservar el comportamiento; reconciliar numéricas no lo preserva |
