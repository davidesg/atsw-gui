# RELEVO — dónde está el desarrollo de atsw-gui (17-oct-2026)

Este documento es para retomar el trabajo desde otro chat sin haber visto
este. Primero dice dónde está cada cosa y después qué viene. Las decisiones
del analista van marcadas como tales: no se vuelven a discutir.

---

## 1. Estado del repositorio

| | rama | estado |
|---|---|---|
| **v0.1.4** | tag sobre `b70aa06` | publicada (prerelease) con sus 11 ficheros; el vídeo `ATSW-ipc-wti.mp4` se subió a mano |
| **#43** | `drtran-cara` | **fusionado** en main (`37d6590`): la cara de fue_gui en drtran_gui, pasos 1-3 |
| **#44** | `madre-fallos` | **fusionado** en main (`956ecad`): los fallos de la madre y `lib/proyecto` |
| **#45** | `devuelve-univariantes` | **fusionado** en main (`4c9613d`): devolver los univariantes |
| **#46** | `sigma-unidades` | **abierto**: σ en las unidades de la ecuación (§4) |

**Regla de trabajo:** cada paso va en su rama, con su PR y la CI en verde en
Linux, macOS y Windows. **Se fusiona sólo cuando el analista lo dice.** El
clasificador de permisos suele bloquear `gh pr merge`: entonces el analista
lo lanza con `! gh pr merge N --squash`.

`b40.stdout` y `b40rank.stdout`, en la raíz, están sin seguimiento y no son
de este trabajo: no se tocan.

---

## 2. La hoja de ruta (decisión del analista)

1. **drtran_gui primero**: la cara de fue_gui, llenar huecos y cerrar cómo se
   integra en atsw (la madre, los casos). **Está casi cerrado** (§3 y §4).
2. **Después, nuevo hito: drvarma_gui** (§6). No se empieza hasta que drtran
   esté cerrado.

---

## 3. drtran_gui: lo hecho (#43, en main)

El diseño está en **`docs/DISENO-barra-drtran.md`**.

- **`lib/barra`**: los iconos y los ayudantes de la barra de fue_gui, para
  los dos GUIs (`at_icono`, `at_menu_item`, `at_boton`, `at_partido`,
  `at_entrada`, `at_atajo`).
- **La barra de drtran_gui**: Estimar (Ctrl+E) · Ver modelo ▾ │ Diagnosis ▾
  (Ctrl+D) · Prever ▾. Ctrl+Q sale. Los menús se rehacen al abrirlos
  (`gui/drtran/src/barra_gui.c`).
- **Decisiones del analista:**
  - **D1**: sin Nuevo, Abrir ni Guardar en la barra.
  - **D2**: lo que necesita un modelo estimado se apaga con un globo que dice
    qué falta; dentro de las páginas sigue la regla de mtram, «nada se
    grisa».
  - **Las pestañas numeradas se quedan.**
  - **Ver modelo con un DAG**: botón partido. El clic enseña la ecuación que
    se está mirando y la flecha da una entrada por ecuación, la salida
    primero. La página de la red entera va a §5 del diseño, fuera de este
    trabajo.
- **La barra de estado**: el mensaje, la barra de avance mientras corre el
  motor y «Modelo: cNN».
- **H1**: un modelo estimado del caso abre en «6 · Diagnosis».
- **H3**: el motor se busca primero en su árbol y después en el PATH
  (`mtram_hermano`).
- **H4**: el gráfico de la previsión, `fp_forecast` en `lib/fugplot`
  (fugdraw, sin gnuplot). Dos paneles, el nivel y la variación anual, con
  bandas de ±2 d.t. y la nota de que son TEÓRICAS dentro del dibujo.
- **La página 6** se queda sólo con Releer y ◀ ▶. Gráficos, Exportar y
  Calcular baseline están ya sólo en Diagnosis ▾. Los veredictos se parten
  en líneas en vez de recortarse.
- **«drtran finished.» ya no pisa «Baseline listo»**: un contador de avisos,
  `m->avisos`, en `preview_show_status`.

## 4. Integración en atsw: lo hecho y lo abierto

`docs/DISENO-casos.md` está **entero hecho** con el #45. Lo de antes ya estaba
en main: alta y derivación de casos, hashes, la sección CASOS, puerta
diagonal, conversión de corridas viejas y «Ver modelo…» desde la madre.

**#44, los fallos de la madre:**
- `atsw_relee` copia dentro del `Proyecto` que había y no cambia el puntero,
  porque las ventanas de Ganancia y Sugerir lo guardan.
- `pr_leer` rechaza una versión desconocida (`PR_EVERSION`), un fichero vacío
  (`PR_EVACIO`) y una lista o unas comillas sin cerrar.
- El lector deshace el escape `\"` y `\\`.
- El escritor entrecomilla lo que YAML leería como otra cosa: `[ { * & - …`.

**#45, devolver los univariantes (DISENO-casos §3):**
- **`lib/vuelta`**:
  - `vu_escribe`: un `.pre` más la tabla del `.out` dan el `.inp`.
  - `vu_caso`: hace lo mismo para todas las entradas de un caso, cada una
    como modelo derivado, hijo del que entró, con su razón.
  - `vu_con_transferencia`: dice si el `.out` lleva transferencia.
- **Se asigna por ORDEN, no por nombre**: el orden de los slots de
  `lib/slots` contra las banderas del `.pre`. Es `.inp` y nunca `.pre`.
- **`lib/inpdet`**: `id_det_coefs` y `id_escribe_det_coefs` (en el orden de
  drtran: por variable, omegas y luego deltas), `id_media_lee` e
  `id_arf_cuenta`. `coef_linea` escribe el número más corto que vuelve
  exacto.
- **Dónde está la acción**: en drtran_gui, «Devolver los univariantes…» en
  Ver modelo ▾ (`gui/drtran/src/vuelta_gui.c`); en la madre, la misma
  entrada en el menú del modelo del caso (`casos_gui.c`, `on_c_vuelta`).
- **Validado** en IPC ← WTI: la corrida diagonal devuelve el `.pre`, y fue
  estima los `.inp` al mismo óptimo (logelf −7,4026486474).

**Lo que queda abierto de drtran y su integración:**
- **σ_w, resuelto en el #46**: no era una decisión sino un fallo. «Tiene que
  ser correcto, tanto si se presenta en % como en bruto» (el analista). La
  división por el refactor se perdió al pasar el gráfico a fugdraw (fue
  1.14); `lib/fugplot` recibe ahora `FPUnits` y pone % sólo en logaritmos.
- **`lib/vuelta` y los AR(2) de frecuencia fija: resuelto** (17-oct). Se
  escriben como los MA(2): `id_arf_lee` / `id_escribe_arf`, en el orden de
  `lib/slots`. Va probado con `syn_ARF`.
- **El `.pre` del caso IPC no es exactamente un óptimo**: fue tarda 10
  iteraciones sobre él (el AR va redondeado a 4 decimales). Anotado como
  **fue BUG-0001** (`engines/fue/BUGS.md`, con su repro), sin arreglar. Es el asunto de
  la puerta diagonal y de «si los `.pre` eran óptimos o especificaciones»
  (TODO.md).
- **Posible prueba frágil en la madre**: una pasada de
  `gui/atsw/tests/run_gui_tests.sh` dio «2 fallos»; tres seguidas dieron 0.
  Si vuelve a salir, guardar el log entero.
- **«Pasar a drvarma»** desde un caso con ciclo: va con el hito de drvarma.
- **El paso 4, el manual**: decisión del analista, **el manual y el vídeo no
  se tocan hasta preparar la próxima versión publicada**. Entonces hay que:
  - pasar a la barra la escena `docs/manual/capturas/escenas/05-drtran.txt`,
    que todavía pulsa «Calcular baseline» y «Gráficos…» en la página 6;
  - regenerar las 14 capturas `drtran-*.png` y MANUAL.md §drtran, con la
    barra, los menús, el gráfico de la previsión y la vuelta;
  - corregir el Hosking: el manual dice P(56), p = 0,18 y ahora sale P(52),
    p = 0,099, probablemente por BUG-57 de drtran;
  - regenerar las capturas de fue, que son de antes del #39;
  - grabar el vídeo y subirlo a mano a la release.

## 5. Cómo se trabaja aquí

**Construir y probar:**

    cd gui/drtran && make && sh tests/run_tests.sh && sh tests/run_gui_tests.sh
    cd gui/fue    && make && sh tests/run_tests.sh
    cd gui/atsw   && make && sh tests/run_gui_tests.sh
    sh conformidad/copias.sh     # da «1 copia(s)»: ya estaba en main, no es nueva

**Reglas del repositorio:**
- **«La estructura tiene que hacer imposible la copia»**: lo que usan dos
  programas vive en `lib/`. Por eso `vu_caso` está en `lib/vuelta` y la
  madre y drtran_gui sólo preguntan y guardan.
- **`docs/.gitignore` es una lista blanca**: un documento nuevo necesita su
  línea `!/NOMBRE.md`.
- **`Proyecto` ocupa unos 1,2 MB**: nunca en la pila (Windows). Se usa
  `g_new0` o `static`, y lo mismo con `OdParams` (unos 70 KB).
- **`inp_check_fue` no se copia**: se compila `engines/fue/src/inpcheck.c`
  aparte, con `-Dinp_check=inp_check_fue`, como objeto suelto. Así lo hacen
  `gui/drtran/Makefile`, `gui/fue`, `gui/atsw` y los `run_*tests.sh`.
- **Los motores no se modifican** salvo defecto, y con el visto bueno.

**Las pruebas conducidas de drtran (`gui/drtran/tests/test_gui.c`):**
- `pulsa()` activa también entradas de menú.
- `entrada_diag(m, "texto")` busca en el menú de Diagnosis ▾.
- `pulsa_dialogo(w, &plan)` contesta el diálogo siguiente con
  `plan_vacio(GTK_RESPONSE_OK)`.
- `check_tinta("fichero.eps", …)` mira que un gráfico tenga tinta.

**Versiones:**
- Un tag `v*` en main dispara `.github/workflows/paquetes.yml`, que crea la
  prerelease.
- La web (https://davidesg.github.io/atsw-gui/) lee la última release.
- El MP4 del vídeo no lo hace la CI: se sube a mano.

**Commits y PR:**
- En español, con la línea `Co-Authored-By` que pida el sistema.
- El cuerpo del PR acaba con la línea «Generated with Claude Code».
- `gh pr edit` falla por los Projects clásicos; se usa
  `gh api -X PATCH repos/davidesg/atsw-gui/pulls/N`.

## 6. Lo siguiente: el hito drvarma_gui (decisión del analista)

- **Se diseña con lo aprendido en sima-python** (sima-tseries, el drvarma en
  Python: `~/Dropbox/SRC`, servidor MCP `sima`) y sus ejemplos, empezando
  por `jenkins_alavi`. Es lo mismo que hizo el diseño de drtran_gui desde
  mtram y drtran-python (`docs/DISENO-mtram.md`).
- **Antes de desechar nada, estudiar el GUI actual**,
  `engines/drvarma/gui`. Son unas 5200 líneas, en inglés, con VECM y
  Johansen, y **funciona**. Lo nuevo de verdad es la interacción con la madre:
  los casos con `motor: drvarma` y «Pasar a drvarma» desde un caso de drtran
  con ciclo.
- drtran y drvarma son hermanos: modelos sin retroalimentación
  (transferencia) y con ella (VARMA).
- **Lo primero, un documento de diseño** como `DISENO-barra-drtran.md`:
  - el estudio del GUI actual;
  - el protocolo de sima (N0…);
  - qué se reutiliza de `lib/barra`, `lib/vuelta`, `fp_forecast` y
    `lib/diagmv`;
  - la integración con los casos.
- **Fallos conocidos de drvarma en `TODO.md`**:
  - el motor sale con 0 cuando falla la estimación;
  - «Use differenced data» no hace nada;
  - el `strcat` de Johansen desborda;
  - las recargas pierden `base_name`.

**Antes de empezar**: el analista quiere revisar drtran_gui una vez más
(17-oct-2026) antes de darlo por cerrado.
