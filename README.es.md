# FUE 1.13.1

**Free Univariate Estimation** — estimación de modelos de series temporales  
por máxima verosimilitud exacta no condicionada.

Copyright (C) 1995-1996 J.A. Mauricio; 2009-2026 A.B. Treadway & D.E. Guerrero  
Licencia: GNU General Public License v2 o posterior.

---

## Tabla de contenidos

1. [Introducción](#introducción)
2. [Requisitos del sistema](#requisitos-del-sistema)
3. [Estructura de archivos](#estructura-de-archivos)
4. [Compilación](#compilación)
5. [Uso en línea de comandos](#uso-en-línea-de-comandos)
6. [Flujo de trabajo con FUF y gtk\_fue](#flujo-de-trabajo-con-fuf-y-gtk_fue)
7. [Ejemplo](#ejemplo)
8. [Integración con editores externos](#integración-con-editores-externos)
9. [Reportar errores](#reportar-errores)

---

## Introducción

FUE es un programa de línea de comandos para estimar modelos univariantes de
series temporales empleando el criterio de máxima verosimilitud exacta no
condicionada (EML). Sigue la metodología de identificación, estimación y
validación propuesta por Box & Jenkins (1970), con extensiones posteriores.

Existen dos módulos adicionales que completan el taller de análisis univariante:

- **FUF** — previsión (*forecasting*)
- **gtk_fue** — interfaz gráfica GTK+3 para FUE

## Requisitos del sistema

- Compilador C: GCC ≥ 9 (Linux/macOS) o MinGW-w64 (Windows)
- [GNU Scientific Library (GSL)](https://www.gnu.org/software/gsl/) ≥ 2.0
- [Gnuplot](http://www.gnuplot.info/) ≥ 5.0 (para gráficos en alta definición)
- GNU Make

En Debian/Ubuntu:

```
sudo apt install build-essential libgsl-dev gnuplot
```

En macOS (Homebrew):

```
brew install gsl gnuplot
```

## Estructura de archivos

```
fue-1.13.1/
├── src/           archivos fuente (.c)
├── include/       cabeceras (.h)
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

El ejecutable queda en `bin/fue`.

### macOS

```
make
```

Si GSL está instalado vía Homebrew y `pkg-config` no lo encuentra:

```
PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig make
```

### Windows — compilación cruzada desde Linux con MXE

```
make CROSS=x86_64-w64-mingw32.static-    # 64 bits
make CROSS=i686-w64-mingw32.static-      # 32 bits
```

El ejecutable queda en `bin/fue.exe`.

### Otros targets

```
make check       # pruebas de regresión (tests/README.md)
make clean       # elimina objetos y ejecutable
make distclean   # elimina obj/ y bin/
make install     # copia bin/fue a /usr/local/bin
make uninstall   # elimina /usr/local/bin/fue
```

## Uso en línea de comandos

```
fue input [eml|aml] [chk|nochk] [-f [horizonte]]
```

| Argumento           | Descripción |
|---------------------|-------------|
| `input`             | nombre del archivo de modelo-datos (sin extensión `.inp`) |
| `eml`\|`aml`        | máxima verosimilitud exacta \| aproximada (por defecto: `eml`) |
| `chk`\|`nochk`      | comprobar \| no comprobar invertibilidad MA (por defecto: `chk`) |
| `-f [horizonte]`    | genera `forecast_input.inp` para FUF; `horizonte` es el número de períodos a predecir (por defecto: 24) |

FUE genera un archivo de salida `input.out` con resultados en texto y,
si Gnuplot está disponible, un archivo `input.ps` con los gráficos en alta
definición.

Antes de escribir nada, FUE comprueba el `.inp` y, si algo está mal, dice qué
y en qué línea (por ejemplo, si es un fichero de FUF o de FUG en lugar de uno
de FUE). Códigos de salida:

| código | significado |
|---|---|
| 0 | resultados escritos |
| 1 | error en la línea de órdenes, o un fichero no se puede abrir |
| 2 | el `.inp` no es válido: no se escribe nada |
| 3 | el modelo no se puede estimar desde sus valores iniciales: se escriben los resultados, avisando en el `.out` de que no son válidos |
| 4 | error interno durante el cálculo |

Los mensajes de error van a la salida de errores (stderr).

## Flujo de trabajo con FUF y gtk\_fue

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

3. **Calcular las predicciones** con FUF:
   ```
   fuf forecast_mimodelo
   ```
   Lee `forecast_mimodelo.inp` y produce el informe de predicción.

Desde **gtk\_fue**, los pasos 2 y 3 se ejecutan automáticamente al pulsar
el botón *Forecast* de la barra de herramientas.

## Ejemplo

Escriba un fichero de especificación `mimodelo.inp` y ejecute:

```
fue mimodelo
fue mimodelo -f 12
fuf forecast_mimodelo
```

El directorio `src/` de la versión **fue-1.13** incluye el archivo de ejemplo
`examples/PU.1.inp` con datos del IPC de EE.UU. desde enero de 2000 hasta
diciembre de 2008.

## Integración con editores externos

### KILE

1. `Settings` → `Configure Kile...` → `Build` → `New Tool...`
2. Nombre: `FUE`; tipo: `<Custom>`
3. Pestaña **General**: Command = `fue`, Options = `'%S'`
4. Pestaña **Advanced**: Type = `Run in Konsole`; Source Extension = `inp`; Target Extension = `out`
5. Pestaña **Menu**: Add to Build menu = `Other`

## Reportar errores

David E. Guerrero — davidesg@ucm.es
