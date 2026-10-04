# Las pruebas

Qué se prueba, cómo se corre, qué se exige en cada plataforma y qué queda
sin probar. Estado a 2026-10-04: la CI pasa en **Linux, macOS y Windows**
(run 37215962338, PR #1).

---

## 1. Las tres capas

| capa | qué responde | dónde |
|---|---|---|
| **baterías de los motores** | ¿el motor da los mismos números que su referencia? | `engines/*/tests/run_tests.sh` |
| **pruebas conducidas de los GUIs** | ¿el GUI hace lo que dice, con el motor de verdad? | `gui/*/tests/`, `engines/drvarma/tests/gui/` |
| **prueba de humo** | ¿el ejecutable de verdad arranca en esta plataforma? | `.github/gui-humo.sh` |

Y aparte, el banco de conformidad del formato (`conformidad/`, ver el
README), que **no** corre en la CI.

### 1.1 Las baterías de los motores

| batería | cómo se compara | notas |
|---|---|---|
| `engines/fue` | contra `tests/golden/`, fichero a fichero | más la equivalencia «sin ARMA = AR(1) fijado a 0» y «sin PATH escribe su PDF» |
| `engines/fuf` | contra `tests/golden/` | más «sin PATH» |
| `engines/fug` | propiedades (formatos de entrada, operadores) | no depende de la libm |
| `engines/drtran` | valores con tolerancia (`check` del guion) | 330 comprobaciones; reproduce a fue por separado |
| `engines/drvarma` | `tests/banco` byte a byte contra 0.4.1; `tests/escalera` | **no bloquea fuera de Linux** (§3) |
| `engines/drvec` | valores de referencia, invariantes, bootstrap | **no bloquea fuera de Linux** (§3) |
| `gui/drtran` (lógica) | contra la salida de drtran; y las pruebas de `lib/` | netfile, slots, proyecto, datos, tabla, anómalos… y las reglas de la madre |

### 1.2 Las pruebas conducidas de los GUIs

Cada GUI se levanta **dentro del proceso de la prueba**: se enlazan sus
fuentes (sin `main`, o con `main` renombrado), se arma la ventana sin
enseñarla, se pulsan sus botones de verdad (`gtk_button_clicked`, las
señales de los selectores, los diálogos contestados desde un temporizador o
grabados) y se comprueba **lo que vería el usuario**: la barra de estado,
las tablas, los textos, los ficheros que deja, y que los gráficos **tienen
tinta** (el área se pinta en una superficie de cairo y se cuentan los
píxeles que no son del fondo). Los motores son los del árbol, de verdad.

| GUI | prueba | comprobaciones | cubre | no cubre |
|---|---|---|---|---|
| fue_gui | `gui/fue/tests/test_operar.c` (+ `test_gui.c`, `test_units.c`, `test_preview.c`) | 219 + 55 | carga de .inp y de datos, diálogos de operadores y deterministas, guardar y estimar, la consola, el ciclo de previsión (fue -f → fuf), ventanas de análisis en un proyecto, errores del motor | salir, imprimir, el visor externo de PDF, derivar/guardar manifiesto desde el análisis |
| gtk_fmg | `gui/fug/tests/run_gui_tests.sh` | 167 | carga .inp/txt/csv/xlsx, todos los botones de gráficos, Save y Save As PNG, `--proyecto`, errores (sin datos, motor ausente, motor que falla) | imprimir, el diálogo interactivo de Save As, la lupa con ratón |
| drtran_gui | `gui/drtran/tests/run_gui_tests.sh` | las 7 pestañas | series, identificación (CCF y su gráfico), red (.dag), modelo (.cns), estimación real, diagnosis, previsión, modo proyecto | Detener (las corridas acaban antes), reordenar arrastrando, tooltips |
| atsw_gui (la madre) | `gui/atsw/tests/run_gui_tests.sh` | — | proyecto nuevo/abrir, datos, series, vistazo (fug y su gráfico), modelos y linaje, el editor (guardar válido/inválido, estimar), lanzar a los hermanos (con un hijo falso en C), muestras | Save As/Print del gráfico, «Sugerir intervención» |
| drvarma_gui | `engines/drvarma/tests/gui/run_gui_tests.sh` | 258 | carga, propiedades, cada opción del modelo comprobada en el .out, estacionalidad, previsión, Johansen/VECM/orden VAR, errores del motor | (no tiene gráficos) |

Sin servidor gráfico se saltan con una línea que empieza por «no hay» y
salen con 0. En la CI de Linux corren dentro de Xvfb; macOS y Windows tienen
escritorio.

### 1.3 La prueba de humo

`.github/gui-humo.sh` lanza cada uno de los cinco ejecutables **tal cual se
construyen**, comprueba que sigue vivo a los 8 s, le hace una captura de
pantalla (queda como artefacto `capturas-<plataforma>`) y lo cierra. Caza lo
que las conducidas no ven: DLL, temas, esquemas de GSettings, el backend
gráfico.

---

## 2. Cómo se corren

    make            construye motores y GUIs
    make check      todas las baterías y las pruebas conducidas

Una sola:

    cd engines/fue  && sh tests/run_tests.sh
    cd gui/fug      && sh tests/run_gui_tests.sh
    cd gui/atsw     && sh tests/run_gui_tests.sh
    cd engines/drvarma && sh tests/gui/run_gui_tests.sh

Las pruebas de fue_gui y de la madre necesitan los motores en el PATH (la CI
los pone: `engines/*/bin` y `engines/fug`). Como en la CI, con limite de
tiempo por batería y sin parar en la primera que falle:

    bash .github/build-all.sh     # DIRS="engines/fue ..." en el entorno
    bash .github/check-all.sh

---

## 3. Lo que se exige en cada plataforma

**En Linux, el byte.** Las referencias se escribieron allí y se exigen
exactas: cualquier cambio en un número es un cambio del programa.

**Fuera de Linux, las cifras.** macOS y Windows dan los mismos coeficientes
en 7-9 cifras pero no los mismos bytes: la libm de cada sistema redondea
distinto (y clang en ARM64 fusiona multiplicaciones y sumas), y el hessiano
numérico (`fdhess`) lo amplifica en los errores estándar. Así que, sólo si
`cmp` falla y la plataforma no es Linux, `conformidad/referencia.sh` llama a
`conformidad/cifras.py`, que exige el texto idéntico y mide cada número con
su vara:

| número | tolerancia |
|---|---|
| estimación | 2e-3 relativo, o el **2 % de su propio error estándar** si es mayor |
| error estándar (entre paréntesis) | 5 % |
| covarianza | 0,05·√(Cᵢᵢ·Cⱼⱼ): la escala de la correlación |
| correlación | 0,05 absoluto |
| coordenadas de un `.eps` | 2 puntos |
| el informe de convergencia (`**** ...`) | no cuenta: es el camino, no el resultado |

**Los casos frágiles.** Los modelos mal definidos —verosimilitud plana,
hessiano mal condicionado— pueden dar en otra plataforma otro punto u otros
errores estándar sin que el programa esté mal. Van en `tests/fragiles.txt`
de su batería, **cada uno con su razón medida**, y fuera de Linux se
informan como `FRAGIL` y no fallan. Hoy, en fue:

| caso | razón |
|---|---|
| `CPI_USA_model`, `CPI_USA_model_AR` | hessiano mal condicionado (σ² ≈ 3 500): errores estándar ±5 %, una covarianza −0,85 / 0,04 |
| `IPC_FR_model` | un error estándar 4,08 / 3,66 (10 %) |
| `RIPC.1` | verosimilitud plana: las estimaciones cambian en la 3.ª cifra, dentro de su error |

**drvec y drvarma no bloquean fuera de Linux.** No van en la primera
versión, y sus referencias no son reproducibles en otra libm: hay modelos con
varios óptimos locales. El caso de libro es drvec `2 0 2 -case 2`:

| plataforma | logL |
|---|---|
| Linux (referencia) | 858,34 |
| macOS | 849,40 |
| Windows | **862,49** — mejor que la referencia |

Corren en las tres y lo que fallan se ve, como aviso. En Linux bloquean.

**No se añade tolerancia en Linux, ni un caso a `fragiles.txt` sin una razón
medida.** Las diferencias entre plataformas en los casos mal definidos son
un hallazgo, no ruido.

---

## 4. Lo que encontraron, para que no vuelva

Fallos de los programas que sólo se veían en otra plataforma, y la regla que
dejan:

| fallo | plataforma | regla |
|---|---|---|
| fue_gui se colgaba en Run (`lib/engine`: el fin de la tubería sólo por `G_IO_HUP`) | macOS | el EOF también cierra |
| fue_gui no veía los mensajes del motor (camino propio con `CreateProcessW`) | Windows | un solo camino, `g_spawn` |
| fuf: errores típicos de la previsión a 0 (`%Lf` con MinGW) | Windows | nada de `long double` en `printf` |
| drtran ignoraba todas las opciones (`getopt` sin reordenar) | macOS | no depender de extensiones de GNU |
| fue y fuf rechazaban ficheros con finales LF (`ftell`/`fseek` en modo texto) | Windows | quien reposiciona, abre en binario |
| ningún fichero era «de este proyecto» (`lib/proyecto` comparaba rutas con `strcmp`) | Windows | `/` y `\` son el mismo separador |
| gtk_fmg se caía al primer botón (`DtDatos`, 2,2 MB, en la pila) | Windows | nada grande en la pila: la de Windows es de 1 MB |
| fue_gui y drvarma_gui pisaban las rutas de GTK con `share/` junto al `.exe` | Windows | sólo si el paquete la trae |
| la madre y gui/fug no encontraban a sus hermanos (`/proc/self/exe`) | macOS, Windows | `lib/sitio` |
| el banco: `\r\n` de Git para Windows en corpus y referencias | Windows | `.gitattributes`: `* -text` |

---

## 5. La ronda en máquinas reales, antes de la primera versión

La CI prueba en máquinas virtuales de GitHub, con MSYS2 y Homebrew
instalados y sin nadie delante. Antes de lanzar hace falta una ronda a mano
en **un Windows y un Mac de verdad**, de usuario, sin entorno de desarrollo.
Lo que la CI no puede ver:

**Windows**

- [ ] **El paquete**: los `.exe` con sus DLL (GTK, GSL, zlib) y `share/`
      (esquemas de GSettings compilados, iconos) al lado, o enlazados en
      estático. Sin eso no arrancan, o abortan al abrir un fichero.
- [ ] **La ventana de consola**: fue_gui lanza ahora los motores con
      `g_spawn`; comprobar que no asoma una consola al estimar.
- [ ] Rutas con espacios y con acentos (`C:\Users\José\Mis datos\`).
- [ ] Ficheros con finales `\r\n` hechos a mano en el Bloc de notas, y LF
      bajados de GitHub o escritos por la suite en Python.
- [ ] Una serie de verdad, del principio al fin, en la madre: proyecto,
      datos, vistazo, modelo, estimar, diagnosis, previsión, drtran.
- [ ] Abrir el PDF del informe (`start`) con el visor que haya.
- [ ] pdflatex ausente: el informe sigue saliendo (fuf lo prueba sin PATH).

**macOS**

- [ ] El paquete (`.app` o Homebrew) y Gatekeeper: un binario sin firmar no
      abre con doble clic.
- [ ] Apple Silicon e Intel, si se reparte binario.
- [ ] El menú, los atajos (⌘ en vez de Ctrl) y los diálogos de fichero
      nativos.
- [ ] La misma serie de principio a fin.

**Las dos**

- [ ] Pantalla de alta densidad (Retina, escalado al 150 % en Windows): los
      gráficos de fugdraw y la lupa.
- [ ] Imprimir un gráfico.
- [ ] Un proyecto creado en Linux abierto en Windows y al revés (el
      manifiesto guarda rutas relativas a su raíz).
- [ ] Los números: los modelos de `fragiles.txt` darán otros errores
      estándar, y eso está bien; cualquier otro modelo debe coincidir con
      Linux en las cifras de §3.
