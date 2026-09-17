# fug 1.14 — motor gráfico propio (fugdraw)

La 1.14 nació como prototipo sobre la 1.13 y ya es completa: **todos** los
gráficos se dibujan con el motor propio y fug no necesita gnuplot ni pdflatex.
El detalle está en el `ChangeLog` (sección 1.14).

## Arquitectura

| fichero | qué hace |
|---|---|
| `src/fugdraw.c/h` | Motor vectorial. Graba cada figura como flujo de contenido PDF y la escribe como **EPS** y como página de **PDF** comprimida con Flate. |
| `src/fd_metrics.h` | Anchos de Helvetica, Helvetica-Bold, Times y Symbol, generados desde los AFM de Adobe por `tools/gen_metrics.py`. |
| `src/fugplot.c/h` | Los gráficos de fug (`-a -b -c -d -e`) con su diseño, y la maquetación del PDF. |
| `src/fug.c` | Lectura del `.inp`, transformaciones, `.out` y orden de los gráficos. |
| `gui/src/preview.c` | Ventana de gráficos del GUI: relee el flujo de contenido y lo dibuja con Cairo. |

## Muestras (`samples/`)

Comparaciones con la versión gnuplot (1.13): arriba gnuplot, abajo el motor propio.
- `comparacion_mensual.png`, `comparacion_trimestral_anual.png`: gráfico `-c`
- `comparacion_serie_a.png`: `-a`
- `comparacion_acf.png`: `-b` con 39, 15 y 9 retardos
- `comparacion_histograma_media_desv.png`: `-d` y `-e`
- `IPCM_set_nativo.pdf` frente a `IPCM_set_gnuplot_pdflatex.pdf`: conjunto de identificación

## GUI

El GUI gtk_fmg está en `gui/` y se compila con fug (`make`). Su ventana de
gráficos (`gui/src/preview.c`) dibuja los EPS/PDF de fugdraw con Cairo y los
guarda como PDF, EPS, PNG o SVG, o los imprime. Ver README.

## Pendiente (opcional)

- Incrustar las fuentes (PDF/A para imprenta).
- La hoja completa de identificación y la CCF de GraphMaker.
