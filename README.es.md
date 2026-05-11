# gtk_fue 0.9

**Interfaz gráfica GTK+3 para FUE y FUF** — modelización y predicción de series temporales univariantes.

Copyright (C) 2026 A.B. Treadway & D.E. Guerrero  
Licencia: GNU General Public License v2 o posterior.

---

## Índice

1. [Introducción](#introducción)
2. [Requisitos del sistema](#requisitos-del-sistema)
3. [Compilación](#compilación)
4. [Ejecución](#ejecución)
5. [Descripción de la interfaz](#descripción-de-la-interfaz)
6. [Flujo de trabajo](#flujo-de-trabajo)
7. [Estructura de ficheros](#estructura-de-ficheros)
8. [Contacto](#contacto)

---

## Introducción

gtk_fue es una interfaz gráfica GTK+3 para el conjunto de herramientas de
series temporales univariantes:

- **FUE** — Free Univariate Estimation: identificación y estimación por
  máxima verosimilitud exacta de modelos SARIMA con transformaciones Box-Cox,
  componentes deterministas y operadores AR/MA estocásticos.
- **FUF** — Free Univariate Forecasting: predicción probabilística a partir
  de un modelo estimado por FUE.

gtk_fue permite construir, editar y guardar ficheros de especificación de
modelos (`.inp`), ejecutar FUE y FUF, y visualizar los informes de resultados,
todo desde una única ventana. Es multiplataforma: Linux, macOS y Windows.

## Requisitos del sistema

**En tiempo de compilación:**

- Compilador C: GCC ≥ 9
- Cabeceras de desarrollo de GTK+ 3.0 y GLib 2.0
- GNU Make

**En tiempo de ejecución:**

- `fue` — motor de estimación FUE (debe estar en el `PATH`)
- `fuf` — motor de predicción FUF (debe estar en el `PATH`)
- Un visor de PDF (usado por Ver Salida y Forecast)

En Debian/Ubuntu:

```
sudo apt install build-essential libgtk-3-dev
```

## Compilación

```
make
```

El ejecutable se genera en `bin/fue_gui`.

### Compilación cruzada para Windows (estática) desde Linux con [MXE](https://mxe.cc)

```
make CROSS=x86_64-w64-mingw32.static-   # 64 bits
make CROSS=i686-w64-mingw32.static-     # 32 bits
```

Los binarios precompilados `fue.exe` y `fuf.exe` para Windows se encuentran en
`engine/`. El script `create_installer.sh` construye un instalador
autocontenido para Windows a partir de estos binarios.

## Ejecución

```
./bin/fue_gui
```

En Windows, ejecute `fue_gui.exe` desde el directorio de instalación o el
acceso directo del menú Inicio creado por el instalador.

## Descripción de la interfaz

La ventana principal tiene una barra de herramientas y un cuaderno con pestañas.

### Barra de herramientas

| Botón | Acción |
|-------|--------|
| New | Limpiar todos los campos e iniciar un nuevo modelo |
| Open | Cargar un fichero `.inp` o `.pre` existente |
| Save | Escribir la especificación actual del modelo en un fichero `.inp` |
| Run | Guardar el fichero `.inp` y ejecutar FUE |
| View Output | Abrir el informe PDF de estimación |
| Forecast | Ejecutar `fue -f` y luego `fuf`, y abrir el PDF de predicción |
| Quit | Salir de la aplicación |

### Pestañas

| Pestaña | Contenido |
|---------|-----------|
| **Data Input** | Nombre de la serie, frecuencia, rango de observaciones, fichero de datos, directorio de trabajo, factor de reescalado |
| **Box-Cox & Differences** | Parámetros λ y m, diferenciación regular, diferenciación estacional, factores de frecuencia individual |
| **Deterministic Component** | Variables de intervención (cambios de nivel, impulsos, dummies estacionales) con dinámica AR/MA de transferencia |
| **Stochastic Component** | Operadores AR/MA regulares y anuales; operadores AR/MA de frecuencia fija |
| **Console** | Muestra la especificación `.inp` del modelo actual; admite edición en línea y guardado |
| **Forecast** | Cargar y editar un fichero de entrada de predicción, fijar el horizonte, ejecutar FUF, abrir el PDF de salida |

## Flujo de trabajo

1. **Nuevo modelo**: Pulsar *New*, rellenar *Data Input* (nombre de la serie,
   frecuencia, fichero de datos, directorio de trabajo, rango de observaciones).

2. **Especificar el modelo**: Fijar las transformaciones en *Box-Cox &
   Differences*, añadir variables de intervención en *Deterministic Component*
   y añadir operadores AR/MA en *Stochastic Component*.

3. **Estimar**: Pulsar *Save* para escribir el fichero `.inp` y luego *Run*
   para invocar FUE. La pestaña *Console* muestra la especificación del modelo
   tal como la lee FUE.

4. **Ver resultados**: Pulsar *View Output* para abrir el informe PDF de
   estimación.

5. **Predecir**: Pulsar *Forecast* para ejecutar automáticamente `fue -f`
   (que genera `forecast_{modelo}.inp`) y luego `fuf`. El PDF de predicción
   se abre al terminar FUF. Alternativamente, utilice la pestaña *Forecast*
   para cargar cualquier fichero de entrada de predicción manualmente y fijar
   un horizonte personalizado.

## Estructura de ficheros

```
gtk_fue.09/
├── src/                          Ficheros fuente en C
├── include/                      Ficheros de cabecera
├── engine/                       Binarios precompilados de FUE y FUF (Windows)
├── data/                         Ficheros de entrada y series de datos de ejemplo
├── bin/                          Ejecutable compilado
├── Makefile
├── build_windows_static_fue.sh   Script de compilación cruzada con MXE
└── create_installer.sh           Constructor del instalador para Windows
```

## Contacto

David E. Guerrero — davidesg@ucm.es
