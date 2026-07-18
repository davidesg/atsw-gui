# FUF 1.08.2

**Free Univariate Forecasting** — predicción probabilística para modelos SARIMA univariantes.

Copyright (C) 1995-1996 J.A. Mauricio; 2009-2026 A.B. Treadway & D.E. Guerrero  
Licencia: GNU General Public License v2 o posterior.

---

## Índice

1. [Introducción](#introducción)
2. [Requisitos del sistema](#requisitos-del-sistema)
3. [Estructura de ficheros](#estructura-de-ficheros)
4. [Compilación](#compilación)
5. [Uso en línea de comandos](#uso-en-línea-de-comandos)
6. [Flujo de trabajo con FUE y gtk\_fue](#flujo-de-trabajo-con-fue-y-gtk_fue)
7. [Formato del fichero de entrada](#formato-del-fichero-de-entrada)
8. [Ejemplo](#ejemplo)
9. [Contacto](#contacto)

---

## Introducción

FUF es un programa de línea de comandos para el cálculo y presentación
de predicciones probabilísticas a partir de un modelo SARIMA estimado con
FUE. Lee el fichero de predicción generado por `fue -f`, propaga la estructura
ARMA y produce:

- Un fichero de texto con las predicciones puntuales e intervalos de predicción.
- Un informe LaTeX/PDF con el gráfico de predicción.
- Un gráfico diagnóstico de los residuos del modelo.

FUF forma parte del conjunto de herramientas de series temporales univariantes:

- **FUE** — identificación y estimación por máxima verosimilitud exacta
- **gtk\_fue** — interfaz gráfica GTK+3 para FUE y FUF

## Requisitos del sistema

- Compilador C: GCC ≥ 9 (Linux/macOS) o MinGW-w64 (Windows)
- [GNU Scientific Library (GSL)](https://www.gnu.org/software/gsl/) ≥ 2.0
- [Gnuplot](http://www.gnuplot.info/) ≥ 5.0 (gráfico de residuos)
- Distribución LaTeX con `pdflatex` (informe PDF de predicción)
- GNU Make

En Debian/Ubuntu:

```
sudo apt install build-essential libgsl-dev gnuplot texlive-latex-base
```

En macOS (Homebrew):

```
brew install gsl gnuplot
```

## Estructura de ficheros

```
fuf-1.08.2/
├── src/           ficheros fuente (.c)
├── include/       ficheros de cabecera (.h)
├── obj/           objetos compilados (generado por make)
├── bin/           ejecutable (generado por make)
├── Makefile
├── README.md
└── README.es.md
```

## Compilación

### Linux (por defecto)

```
make
```

El ejecutable queda en `bin/fuf`.

### macOS

```
make
```

Si GSL está instalado vía Homebrew y `pkg-config` no lo encuentra:

```
PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig make
```

### Windows — compilación cruzada desde Linux con [MXE](https://mxe.cc)

```
make CROSS=x86_64-w64-mingw32.static-    # 64 bits
make CROSS=i686-w64-mingw32.static-      # 32 bits
```

El ejecutable queda en `bin/fuf.exe`.

### Otros targets

```
make clean       # elimina objetos y ejecutable
make distclean   # elimina obj/ y bin/
make install     # copia bin/fuf a /usr/local/bin
make uninstall   # elimina /usr/local/bin/fuf
```

## Uso en línea de comandos

```
fuf entrada
```

| Argumento  | Descripción |
|------------|-------------|
| `entrada`  | nombre del fichero de predicción (sin extensión `.inp`) |

FUF lee `entrada.inp`, que debe ser el fichero generado por `fue entrada -f`.
Escribe `entrada.out` (resultados en texto) y compila el informe PDF.

## Flujo de trabajo con FUE y gtk\_fue

El flujo de trabajo habitual es:

1. **Estimar** con FUE:
   ```
   fue mimodelo
   ```
   Lee `mimodelo.inp` y escribe el informe de estimación `mimodelo.out`.

2. **Generar el fichero de entrada para la predicción**:
   ```
   fue mimodelo -f [horizonte]
   ```
   Escribe `forecast_mimodelo.inp` con los parámetros estimados, el horizonte
   de predicción (por defecto: 24 períodos) y la varianza de innovación.

3. **Calcular las predicciones**:
   ```
   fuf forecast_mimodelo
   ```
   Lee `forecast_mimodelo.inp` y produce el informe de predicción.

Desde **gtk\_fue**, los pasos 2 y 3 se ejecutan automáticamente al pulsar
el botón *Forecast* de la barra de herramientas o desde la pestaña Forecast.

## Formato del fichero de entrada

El fichero `.inp` que lee FUF es generado por `fue -f` y comparte la misma
estructura que un fichero `.inp` estándar de FUE, con dos añadidos en la
cabecera:

- Un nombre de serie de residuos en la línea de fecha (se usa `A{nombre}`
  por defecto si no está presente).
- Una sección *Forecast horizon and estimated innovation variance* justo
  después de la línea de fecha, con el horizonte `L` y la varianza `σ²`.

Las secciones de operadores ARMA (AR/MA regular, AR/MA anual, AR/MA de
frecuencia fija) tienen el mismo formato y orden que en un fichero `.inp`
estándar de FUE.

## Ejemplo

El directorio `src/examples/` de la versión **fuf-1.08** contiene `PC11.inp`,
un ejemplo de modelo trimestral. Para estimar y predecir:

```
fue PC11
fue PC11 -f 12
fuf forecast_PC11
```

## Contacto

David E. Guerrero — davidesg@ucm.es
