# DISEÑO — la cara de drtran_gui, la de fue_gui

fue_gui estrenó en la 0.1.4 una barra con iconos propios, en español, con
texto y tres menús (#40), y una diagnosis con la nota encima de los botones
(#39). drtran_gui no tiene nada de eso: ni barra, ni iconos, ni atajos. Cada
página lleva su fila de botones, y *Estimar* sólo está en la 5.

El alumno sube por la escalera de fue a drtran. Si lo que es común —abrir,
estimar, ver el modelo, diagnosticar, prever— está en el mismo sitio, con el
mismo icono y el mismo atajo, sólo tiene que aprender lo nuevo: la red, la CCF,
la exogeneidad.

**Lo que este documento NO cambia:** las siete páginas numeradas. Son el
protocolo de mtram (`DISENO-mtram.md` §3, `DISENO-flujo.md` §2), y los botones
de cada página actúan sobre lo que la página enseña: *Añadir a la red* es de la
Identificación y *Estrella* de la Red. Subirlos a una barra los separaría de
su objeto.

---

## 1. Qué se comparte y qué no

| | va a la barra | se queda en su página |
|---|---|---|
| acciones globales | Estimar · Ver modelo · Diagnosis · Prever | — |
| acciones de una página | — | Abrir y quitar series, ↑ ↓, Operadores, Ventana · Añadir a la red, CCF, Ecuación · los enlaces, Estrella · Libre, Fijar, Compartir · Opciones, Detener, Criterio · ◀ serie ▶, Ir a… |

Una acción va a la barra si **no depende de la página en que se está**.

---

## 2. La barra

    [Estimar] [Ver modelo ▾] │ [Diagnosis ▾] [Prever ▾]

| botón | clic | menú ▾ |
|---|---|---|
| **Estimar** (Ctrl+E) | lo de la página 5, desde cualquier página | — |
| **Ver modelo** | la página de la ecuación que se está mirando en ◀ serie ▶ (al abrir, la de la salida) | una entrada por ecuación con página, en el orden del recorrido: la salida primero y aguas arriba |
| **Diagnosis** (Ctrl+D) | va a la página 6 | Exogeneidad · Adecuación · Ajuste · Residuos · ─ · Serie y ACF/PACF con el modelo · Histograma · CCF de los residuos con X… (una por entrada) · ─ · Calcular el baseline · Exportar la diagnosis… |
| **Prever** | va a la página 7 y calcula | Calcular · ─ · Ver el gráfico de la previsión (la salida primero, y uno por cada otra serie) · ─ · Guardar esta evaluación · Olvidarla · ─ · Ir a la pestaña |

- **Ver modelo, con una red.** `diagnosis_escribe_paginas` escribe una página
  por ecuación (`_<serie>.pdf`), y la madre ya ofrece «Ver modelo…» (la
  salida) y «Ver la ecuación de X…» (`casos_gui.c`, `menu_corrida`). La barra
  hace lo mismo, y el clic sigue a ◀ ▶: el botón y la diagnosis hablan siempre
  de la misma ecuación.
- Los mismos ayudantes, los mismos iconos de fue (Estimar, Ver modelo,
  Diagnosis, Prever) y el mismo `GTK_TOOLBAR_BOTH`.
- Ctrl+Q sale.
- *Gráficos…*, *Exportar…* y *Calcular baseline* salen de la fila de la página
  6 y entran en el menú de Diagnosis.

**Pendiente de decidir (D1): el grupo de ficheros.** fue_gui empieza por
*Nuevo · Abrir · Guardar*, y en fue el objeto es uno: el `.inp`. En drtran el
modelo son las series, el `.dag` y el `.cns`, y hoy cada uno se abre en su
página. Dos opciones:

- **A** — sin grupo de ficheros. Con proyecto, los abre y los guarda la madre
  (los casos y sus modelos); sin proyecto, cada página sigue con los suyos.
- **B** — *Abrir…* abre un modelo entero (las series, el `.dag` y el `.cns` de
  un modelo o de un caso) y *Guardar* escribe el `.dag` y el `.cns` juntos.

Recomendación: **A**. B pide un formato de «modelo» que hoy sólo existe dentro
de un proyecto (el caso), y duplicaría lo que ya hace la madre.

**Pendiente de decidir (D2): apagar o no.** fue_gui apaga lo que necesita un
`.out` y el globo dice por qué. mtram tiene la regla contraria: «nada se grisa;
cada pantalla pide por su nombre lo que le falta» (`DISENO-mtram.md`, reglas
transversales, 1). Recomendación: **la regla de fue en la barra** —Ver modelo,
Diagnosis y Prever, apagados sin estimación, con el globo diciendo qué falta y
quién lo fabrica— y **la de mtram dentro de las páginas**, como hasta ahora. El
globo cumple el espíritu de la regla: nombra lo que falta.

**El tercer menú de fue, Contrastes**, no tiene aún equivalente: drtran no tiene
contrastes formales. Lo que ocuparía su sitio —sobreajustar, la ganancia y la
respuesta al impulso— existe en el MCP de drtran y no en el GUI. Va a §5,
fuera de este cambio.

---

## 3. La barra de estado

La de fue: el mensaje a la izquierda (recortado con «…»), la barra de avance
sólo mientras se estima («it. k» si el motor la da, en pulso si no) y
**«Modelo: c3»** a la derecha —el modelo del caso, o el fichero sin proyecto—.
Sustituye a la línea `m->estado` y a la barra de avance de la página 5.

Y de paso se arregla el defecto de `TODO.md`: «Baseline listo» lo pisa
enseguida «drtran finished.» (`estima.c`, `on_done`).

---

## 4. Los huecos de lógica

| | hoy | cambio |
|---|---|---|
| H1 | Al abrir un modelo estimado del caso, se queda en «1 · Series» | Va a «6 · Diagnosis», como fue_gui va a la consola, y la barra de estado lo dice: «Modelo estimado: su diagnosis.» |
| H2 | No hay *Ver modelo* en drtran_gui; sólo en la madre | El botón de la barra (§2) |
| H3 | drtran se busca sólo en el PATH (`estima.c` ~238) | `engine_set_finder` y `lib/sitio`, como fue_gui (#37): primero el árbol propio, el PATH de respaldo. `run_gui_tests.sh` deja de necesitar meter el motor en el PATH |
| H4 | La Previsión no tiene gráfico | *Ver el gráfico de la previsión*: `fp_forecast` en `lib/fugplot` (fugdraw, sin gnuplot), con dos paneles —el nivel y la variación anual— y las bandas de ±2 d.t.; la nota de que son TEÓRICAS va en el propio dibujo. Regla 7 de mtram: lo que se ve es lo que va al papel |

---

## 5. Fuera de este cambio

- **Sobreajustar, la ganancia y la respuesta al impulso** en el GUI: piden
  diseño propio, y quizás el tercer menú (*Comprobar ▾*).
- **La página de la red entera:** el DAG dibujado, con los ω y δ estimados en
  cada enlace y las ecuaciones juntas. Con tres o cuatro series es la vista
  del modelo de verdad: las páginas por ecuación no enseñan cómo se conectan.
  Pide diseño (fugdraw no dibuja grafos) y va con la respuesta al impulso, que
  también es de la red y no de una ecuación.
- **Fundir Red y Modelo en «Especificación»** (`DISENO-flujo.md` §5.1): no
  mover dos cosas a la vez.
- **Las pestañas de fue_gui, en inglés** («Data Input», «Stochastic
  Component»): es la incoherencia más visible entre los dos GUIs, y es de fue.
  Va en un cambio aparte, dentro de la decisión de `TODO.md` (todo en español
  por ahora, gettext después).

---

## 6. La estructura: `lib/barra`

«La estructura tiene que hacer imposible la copia, no detectable»
(`DISENO-repositorio.md`). Lo que van a usar dos GUIs vive en `lib/`:

- `gui/fue/src/iconos.c` e `include/iconos.h` → `lib/barra/`. No dependen de
  fue: sólo GTK. `fue_icono` → `at_icono`, `fue_menu_item` → `at_menu_item`.
- Los ayudantes de la barra de `gui/fue/src/main_window.c` (~50-98: botón,
  botón partido, entrada de menú, atajo y `pulsa`), hoy `static`, pasan a
  `lib/barra` con un contexto propio.
- Iconos nuevos para drtran, en el mismo estilo (24×24, `@F`/`@A`): los que
  pidan los menús de §2. Si no hace falta ninguno, mejor.
- De paso, `numeros_con_punto` (idéntico en los dos `main.c`).

`conformidad/copias.sh` ya comprueba que ningún `.c` de `lib/` aparezca también
en `gui/` o `engines/`.

---

## 7. Orden

| paso | qué | se ve | pruebas |
|---|---|---|---|
| 1 | `lib/barra`; fue_gui la usa desde ahí | nada | `gui/fue/tests` pasan sin tocarlas |
| 2 | La barra y la barra de estado en drtran_gui; H1 y H3 | sí | `gui/drtran/tests/test_gui.c`: `boton()` busca también en la barra; las notas de `run_gui_tests.sh` |
| 3 | La página 6 sin *Gráficos/Exportar/Baseline*, con la nota en su renglón (#39); H2 y H4 | sí | las mismas |
| 4 | El manual: `docs/manual/capturas/escenas/05-drtran.txt`, las 14 capturas `drtran-*.png` y MANUAL.md §drtran | — | `escenas.c` ya sabe pulsar elementos de barra |

Cada paso, su PR, con la CI en verde en las tres plataformas.
