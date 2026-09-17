# FUG 1.15 — Free Univariate Graphics

Gráficos de alta resolución (EPS y PDF) para la identificación de modelos
univariantes de series temporales: serie estandarizada, acf/pacf, histograma y
gráfico media–desviación típica, para cualquier combinación de transformación
Box-Cox y diferencias regulares y estacionales.

(c) José Alberto Mauricio, 1995-2002 (rutinas numéricas);
Arthur B. Treadway y David E. Guerrero, 2009-2026. Licencia GPL v2 o posterior.

La versión 1.13 unificó en **una sola fuente** las versiones de Linux, Windows
y macOS. La 1.14 **dibuja sus gráficos ella misma** (motor `fugdraw`): escribe
los EPS y el PDF directamente, sin gnuplot, sin pdflatex, sin pipes ni ficheros
temporales. fug es un único ejecutable sin dependencias, y la salida es
idéntica byte a byte en Linux, Windows y macOS. La 1.15 **comparte el `.inp`
con fue**: lee el `.inp` de fue (usa la serie y no toca el modelo) y escribe
sus resultados como `X_fug.out` y `X_fug.pdf`, para no pisar los de fue. El
`ChangeLog` detalla la historia de versiones.

El GUI **gtk_fmg** (carpeta `gui/`) forma parte del mismo árbol y se compila a
la vez que fug. Muestra los gráficos en su propia ventana, que los guarda como
PDF, EPS, PNG o SVG y los imprime.

## Requisitos

- **fug**: ninguno. No llama a ningún programa externo ni necesita bibliotecas.
  Funciona sin pantalla (ssh, cron, WSL). Los gráficos usan las fuentes
  estándar de PDF/PostScript (Helvetica, Times, Symbol), que todos los visores
  e impresoras tienen; los EPS se pueden incluir en LaTeX como siempre.
- **gtk_fmg**: GTK 2 (Debian/Ubuntu: `sudo apt install libgtk2.0-dev`). Si no
  está, `make` compila solo fug y lo avisa.

## Compilación

```sh
make                 # fug y gtk_fmg (gtk_fmg solo si está GTK 2)
make fug             # solo el motor
make gui             # solo el GUI
make check           # 161 comprobaciones de regresión de fug
make check-gui       # prueba completa del GUI (necesita pantalla o xvfb-run)
sudo make install    # copia fug y gtk_fmg en /usr/local/bin (PREFIX=... para otro sitio)
make windows         # desde Linux: fug.exe y gtk_fmg.exe para Windows
```

- **Windows desde Linux**: `sudo apt install gcc-mingw-w64-x86-64` para
  `fug.exe` (estático, solo depende de `KERNEL32.dll` y `msvcrt.dll`; para 32
  bits `make windows MINGW=i686-w64-mingw32-gcc`). `gtk_fmg.exe` necesita GTK 2
  para Windows compilado con [MXE](https://mxe.cc)
  (`make MXE_TARGETS=i686-w64-mingw32.static gtk2` en la carpeta de MXE);
  `make windows MXE=/ruta/a/mxe` (por defecto `~/Dropbox/SRC/mxe`). Sin MXE,
  `make windows` hace solo `fug.exe`.
- **Qué se copia a Windows**: `fug.exe`, `gtk_fmg.exe`,
  `gspawn-win32-helper.exe` y `gspawn-win32-helper-console.exe`, los cuatro en
  la misma carpeta (GLib necesita los dos últimos para ejecutar fug). No hace
  falta instalar nada más.
- **Windows (MSYS2)**: `pacman -S mingw-w64-x86_64-gcc make` y después
  `make fug` en la terminal "MSYS2 MinGW 64-bit".

## El GUI (gtk_fmg)

`gtk_fmg` lee los datos (un fichero de texto con un valor por línea, o un
`.inp` de fue o de fug) y ejecuta fug con las opciones de cada botón. Usa el
fug que está junto a él (los dos se compilan e instalan juntos); la variable
de entorno `FUG` permite elegir otro.

El `.inp` es el de fue y lo comparten los dos programas. El GUI pasa a fug la
transformación de la ventana (λ, m, d, D) con `-B`, así que no necesita
reescribir el `.inp`:
- si no existe, lo crea como `.inp` de fue sin modelo;
- si ya tiene la misma serie, lo deja como está (también si tiene un modelo
  de fue);
- si tiene un modelo de fue de **otra** serie, pregunta antes de sustituirlo
  (Cancelar lo conserva; mejor usar otro nombre de entrada).

Los gráficos se abren en la **ventana de gráficos** del GUI, que los dibuja
con Cairo a partir del mismo flujo de contenido que fug escribe en el EPS o el
PDF (idénticos a como los muestra Ghostscript):
- **Save As**: PDF, EPS, PNG (300 ppp) o SVG. El PDF y el EPS los escribe
  fugdraw, igual que fug; del conjunto de identificación el PDF guarda todas
  las páginas y EPS, PNG y SVG la página que se ve.
- **Print**: diálogo de impresión del sistema, apaisado si el gráfico lo es.
- **External Viewer**: abre el fichero con gv (o el visor del sistema).
- Páginas: botones o teclas RePág/AvPág, Inicio y Fin. Esc cierra, Ctrl+S
  guarda, Ctrl+P imprime.
- Si fug vuelve a hacer un gráfico que ya está abierto, su ventana se
  actualiza en lugar de abrirse otra.

Los ficheros de texto (`.inp`, `.out`) se abren con gedit (Bloc de notas en
Windows).

## Uso

```
fug input [one | set r a] [opciones]

  input        fichero de entrada (la extensión .inp es opcional; puede llevar
               carpeta: los resultados se escriben junto al .inp)
  one          gráficos de la serie con las diferencias del .inp (por defecto)
  set r a      conjunto de identificación: 0..r diferencias regulares y
               0..a diferencias anuales

  -a  serie        -b  acf/pacf        -c  serie + acf/pacf     -d  histograma
  -e  media-desviación (serie en nivel)
  -h  (set) incluye la serie original (lambda = 1) y su media-desviación
  -l n  retardos acf/pacf      -m n  observaciones por grupo (media-desviación)
  -f x  escala acf/pacf        -g n  parámetros ARMA (grados de libertad de Q)
  -x [min max paso]  estima lambda de Box-Cox por máxima verosimilitud
  -B l m d D  lambda y m de Box-Cox, diferencias regulares y anuales (en lugar
              de las del .inp)
  -v  versión                 --help  ayuda
```

Los indicadores se pueden combinar (`-ce`). Ejemplo: `fug PU set 2 1 -c -h -e`.

Resultados: `input_fug.out` (texto), un EPS por gráfico (`d1D1lnX.eps`,
`acf_…`, `hist_…`, `m_dt_…`) y, con `-a` o `-c`, `input_fug.pdf` con los gráficos
`-a`, `-c` y `-e` en páginas A4 (apaisadas para series mensuales o de más de
200 datos). Si el PDF del conjunto de identificación (`set`) no se puede
escribir, fug termina con código 3.

## Fichero de entrada (.inp)

fug lee **el `.inp` de fue**, el mismo que escriben gtk_fue y ART. Usa la
serie, la línea Box-Cox (`lambda d D`), los factores individuales de la
diferencia anual y la escala de la acf/pacf (si no se da `-f`); el modelo (variables
deterministas, operadores AR/MA, media) se salta, leído exactamente como lo lee
fue 1.13.1. Las variables deterministas no estándar ocupan una columna cada una
tras la serie, y fug las salta. Un `.inp` sin modelo:

```
(5 líneas libres)
** Frequency of time series: either 1(A), 4(Q) or 12(M):
 12
** Number of observations and starting date of time series:
 216  1 2002 IPCM
** Number of deterministic variables (including seasonal components):
0
** Number and orders of regular AR operators:
0
   (… annual AR, regular MA, annual MA y los AR(2) y MA(2) de frecuencia fija,
      todos 0)
** Mean parameter (mu):
0.000000 0
** Box-Cox lambda, regular differences and complete annual differences:
 0.000000 1 1
** Individual factors of the annual difference (starting at freq 0.0):
 0 0 0 0 0 0 0
** ACF/PACF bands (0 Automatic) and reescaling factor:
 0.000000 1.00
** Time series (stochastic and non-standard deterministic variables):
77.700000
...
```

fug también lee el `.inp` de sus versiones anteriores (1.01 – 1.14):

```
(5 líneas libres)
** Frequency of time series: either 1(A), 4(Q) or 12(M):
 12
** Number of observations and starting date of time series:
 216  1 2002 IPC
Box-Cox lambda, m. Regular differences and complete annual differences:
 0.00 0.00  1  1
** Individual factors of the annual difference (starting at freq 0.0):
   0 0 0 0 0 0 0
** Time series:
77.7
...
```

Los distingue por la línea que sigue a la fecha: el número de variables
deterministas (un entero) en el de fue, la línea Box-Cox en el antiguo, que
admite los tres formatos que han existido:

| valores | significado                   | versiones                   |
|---------|-------------------------------|-----------------------------|
| 3       | `lambda d D`                  | FUG 1.01 – 1.08 (`PU.inp`), y el `.inp` de fue |
| 4       | `lambda m d D`                | FUG 1.09, 1.12, gtk_fmg ≤ 1.14 |
| 5       | `lambda m geom d D`           | FUG 1.10 – 1.12.01, pyfug   |

- `m` es una constante que se suma a los datos antes de transformarlos (en el
  `.inp` de fue no existe: se da con `-B`).
- `geom = 1` normaliza la transformación con la media geométrica.
- Con `lambda = 1` no se transforma la serie.
- Los factores individuales de la diferencia anual (un 1 para cada factor que
  se quiera aplicar, de la frecuencia 0 a la de Nyquist) se aplican además de
  `d` y `D`.

## Ejemplos

`examples/`:
- `PU.inp`: mensual, formato antiguo de 3 valores.
- `IPCM.inp`, `IPCQ.inp`, `IPCA.inp`: la misma serie en frecuencia mensual,
  trimestral y anual.
- `IPCM.txt`: los datos mensuales sin cabecera, para cargarlos en el GUI.

## Estructura

| carpeta | contenido |
|---|---|
| `src/` | fug: lectura, transformaciones, `.out` (`fug.c`, `diagnose.c`, `delop.c`, `nlutils.c`), motor gráfico `fugdraw.c` y gráficos `fugplot.c` |
| `gui/` | gtk_fmg: ventana principal y diálogos (`main.c`, `callbacks.c`), datos y `.inp` (`data_load.c`), ejecución de fug (`fug_run.c`) y ventana de gráficos (`preview.c`, que usa `src/fugdraw.c`) |
| `src/inpfile.c` | lectura del `.inp` (de fue y de fug) y escritura del `.inp` de fue sin modelo; lo usan fug y el GUI |
| `tests/` | `run_tests.sh` (fug), `data/` (`.inp` de fue de prueba) y `gui/` (prueba del GUI) |
| `examples/`, `samples/`, `tools/` | ejemplos, muestras de gráficos, generador de métricas |
