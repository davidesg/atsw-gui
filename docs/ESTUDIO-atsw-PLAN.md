# ATSW — plan del estudio grande

Borrador para revisar. Sale de un reconocimiento lateral hecho el 2026-09-16
sobre art-python, drtran, drtran-python, los motores en C, gtk_fue, el
oráculo y TASTE. **No es el estudio: es el plan del estudio.**

## FASE 1 — CERRADA (2026-09-16)

Entregado: **`CONTRATO.md`** (el contrato tal como es, las divergencias y la
propuesta para P1), **`REGLAS-NO-ESCRITAS.md`** (52 invariantes que el código
impone y ninguna especificación menciona) y **`conformidad/`** (tres bancos:
`bateria.sh` para los escritores, `acuerdo.sh` para los lectores, `copias.sh`
para que la copia de inpcheck del GUI siga siendo la del motor).

Lo que cambió respecto al plan:

- **No eran cinco implementaciones, eran seis.** El motor también ESCRIBE el
  `.pre`, y `write_pre` es copia fiel suya, defectos incluidos.
- **La corrección de fondo: un parámetro FIJO no es una semilla.** La bandera
  del fichero dice cuál es cuál, y ningún escritor la miraba. Eso reescribió la
  regla que este mismo documento había propuesto.
- **Ocho arreglos en gtk_fue** (63 → 101 de 120), **tres en los motores** —los
  δ pegados, la cota de deterministas que destruía el montón— y **nueve bugs
  reportados** con repro en art (0187-0189) y en fue (0017-0022).

**P1 respondido:** C ejecutada de verdad —dos implementaciones atadas por el
corpus— y A (`libatswinp`) como obra de fondo. Razonado en `CONTRATO.md` §6.

---

## FASE 3 — CERRADA (2026-09-17)

Entregado: **`DISENO-escalera.md`**.

Dos peldaños salieron **sólidos**, los dos verificados numéricamente y no
leídos: el convenio de signo Box-Jenkins —drtran es producto directo de esa
tradición y la respeta en los tres peldaños— y la identidad diagonal
(`−767.4243410` frente a `−767.424341`).

Lo que cambió respecto al plan:

- **El `.cns` tiene DOS índices posicionales, no uno**, y ninguno está en un
  fichero: el orden de la línea de órdenes y el orden de las líneas del `.dag`.
  Medido: permutar dos series mueve ℓ en 35,6 y cambia de signo una covarianza;
  mover una línea del `.dag` liga la transferencia de una serie a la media móvil
  de otra. Y lo escribe el propio `drtran -g`.
- **mtram no invoca el binario en C**: es un puerto completo. El problema de las
  seis implementaciones de la fase 1 tiene aquí su gemelo.
- **Las puertas verdes no bastan**, y hay demostración ejecutada: ✅ en la
  diagonal y la ganancia movida un 9% en la misma corrida. Siete pruebas de
  extremo a extremo llevan en rojo desde agosto y no hay CI en ningún repo.
- **El tercer peldaño no existe**: mtram → sima no tiene fichero, ni puerta, ni
  continuidad. La escalera son dos peldaños y una recomendación verbal.

**Propuesta central:** el fichero de sesión `.trn` no hay que inventarlo — ya
está escrito como prosa en la cabecera del `.out`. Hay que escribirlo como dato,
con roles y hashes, y que drtran sepa leerlo. Y el recorrido no merece un guion
propio: extender el de art, que está probado.

Reportados: **BUG-17** y **BUG-18** en drtran-python, y reabierta la mitad en C
de **BUG-2**.

---

## FASE 4 — CERRADA (2026-09-17)

Entregado: **`ESTUDIO-taste.md`**, con las seis decisiones que son la entrada de
la fase 5.

Lo que más valió no fue el código sino **los artefactos**: los ficheros de
1991-1993 dicen cosas que el código no dice.

- **El argumento del contrato `.inp`/`.pre`, medido en TASTE.** El modelo
  `USCONS`, misma especificación exacta, guardado en dos sitios: `0.89770
  -0.16262 -0.45259 0.89369` en el `.TSM` y `0.89710 -0.16273 -0.45249
  0.89377` dentro del `.AT0`. Dos corridas del mismo modelo y nada dice cuál es
  cuál. TASTE tenía un solo vector que era semilla Y estimación, y la
  estimación lo pisaba en sitio en cada iteración.
- **El estado de transformación era TRANSITORIO** —se deshace al salir—, no una
  propiedad guardada. Decisión: es estado de la VISTA.
- **El `.AT0` llevaba dentro manejadores del sistema operativo.** 149 253 bytes
  fijos, 97% vacío, con memoria sin inicializar. Es el argumento concreto de
  «apuntar, no contener».
- **El menú no impone orden, y hace bien.** No tiene campo de «habilitado»: el
  protocolo está en el grafo de dependencias entre artefactos. *No grisar el
  botón — hacer que el paso pida por nombre algo que sólo el anterior sabe
  fabricar.*
- **La tabla publicable no se escribió porque los datos nunca se capturaron.**
  `TABLA` declarado y nunca usado; la línea de unidades del `.BJD` se lee a una
  variable llamada `shit`; 21 líneas de cabecera reservadas y jamás usadas; y
  grabar destruye las que había. El orden de la solución es el inverso.

---

## FASE 5 — CERRADA (2026-09-17)

Entregado: **`DISENO-proyecto.md`** — el manifiesto `proyecto.yaml`, los
cambios en lo que ya hay, y quién escribe qué.

**El proyecto no puede ser «una carpeta con varios guiones»: eso es
exactamente lo que ya hay y es lo que falla.**

Lo que cambió respecto al plan:

- **El alcance del guion no es la serie: es el DIRECTORIO.** La derivación
  prueba primero si existe un `guion.json` genérico y lo reutiliza, sea cual
  sea la serie. Y el nombre de la serie no se declara: se deriva de dos maneras
  incompatibles, con fallback al literal `"serie"` — en 25 de 125 guiones.
- **Medido: el 42% de las rutas del linaje están rotas** (387 de 911), y 109
  son rutas de Windows de otra máquina. El fichero que debería ser el registro
  portátil del caso es el menos portátil que hay.
- **Los EJES de variación son la pieza que faltaba en el plan.** Muestra,
  vintage, carril y réplica no son metadatos: son dimensiones, y hoy viven en
  la primera letra del nombre de un fichero (`m*` 2002-2019, `x*` Excel del
  usuario, `y*` descarga INE).
- **El rol de cada directorio, también.** Sin él, dos documentos del mismo
  árbol dan veredictos opuestos sobre la misma serie y la respuesta de cuál
  manda está en una plantilla de corrección que empieza diciendo «no se enseña
  a los analistas».
- **`batch_build`, el único tool multi-serie, es el único que no registra nada.**

Y la regla de diseño de toda la fase, medida: **un campo que la herramienta
EXIGE se rellena; uno que OFRECE, no.** `rationale` es obligatoria → 881/924
(95%). `analyst` es opcional y ni siquiera tiene parámetro → 0/125 (0%).

---

## FASE 2 — CERRADA E IMPLEMENTADA (2026-09-17)

Entregado: **`DISENO-editor.md`** y el editor **funcionando en gtk_fue**
(`0498367`), con prueba conducida desde el codigo (`tests/test_editor.c`).

**El fallo tenia nombre, y eran dos funciones casi homonimas haciendo lo
contrario**: `on_save_inp` (el Save de la barra) escribe desde el MODELO y
`on_save_inp_clicked` (el Save de la consola) desde el BUFFER — y `on_run_fue`
llamaba al primero. La secuencia Edit → Save → Run reescribia el fichero desde
las pestanas y la edicion desaparecia sin decir nada.

Las cuatro decisiones:

- **Guardar RECARGA.** Un solo dueno en cada instante: el texto mientras se
  edita, el modelo en cuanto se guarda. Bloquear las pestanas era la
  alternativa y se descarto: es un modo, y la fase 4 midio que los modos son
  peores que las dependencias.
- **Validar antes de guardar**, por la misma puerta que usa el motor. Si no
  vale, el fichero que habia no se toca, se dice el motivo con su linea, y
  sigue editable para corregir.
- **El `.pre` se puede editar y al guardar sale un `.inp`.** El `.pre` no se
  sobrescribe nunca. Es donde el contrato deja de ser una frase de un documento
  y pasa a ser lo que ves al pulsar un boton.
- **La terna se avisa, no se borra.**

53 comprobaciones 0 fallos; corpus de conformidad 102 pasan 0 fallan.

---

## LAS SEIS FASES: CINCO DE ESTUDIO Y UNA IMPLEMENTADA

    fase 1   CONTRATO.md + REGLAS-NO-ESCRITAS.md + conformidad/   el contrato
    fase 2   DISENO-editor.md + el editor en gtk_fue              el editor
    fase 3   DISENO-escalera.md                                   la escalera
    fase 4   ESTUDIO-taste.md                                     TASTE
    fase 5   DISENO-proyecto.md                                   el proyecto

**Fase 6 — la interfaz madre y el GUI de drtran — ABIERTA (2026-09-17).**
Su documento es `DISENO-mtram.md`: las siete pantallas, qué fabrica cada una y
qué pide a la anterior. Lo que sigue ya no es estudio: es diseño de programa.

La fase 4 dejó a TASTE como diseño. La 6 lo usa, y marca el límite: **TASTE es
una familia de modelos —un solo output, un solo operador de diferenciación, sin
fijo/libre, sin restricciones— y mtram es un grafo de ecuaciones.** Se rescata
el principio (ninguna pantalla se bloquea; cada paso pide por su nombre lo que
sólo el anterior fabrica) y se deja atrás la forma.

---

## DECIDIDO (2026-09-17) — el repositorio del lado C

- **`atsw-gui`**: un monorepo con **los GUIs y sus motores**. La mitad en
  Python no se toca. Razonado en `DISENO-repositorio.md`.

  El criterio en una linea: **cada divergencia que aparecio en las seis fases
  tiene la misma causa, el mismo fichero viviendo en dos arboles** — y una de
  ellas, `inpcheck_fue.c`, divergio dentro de una sola sesion de trabajo. La
  estructura tiene que hacer imposible la copia, no detectable.

  Por que monorepo y no submodulos: **el corpus de conformidad cruza los
  programas**, y esa prueba no tiene sitio en un repo por programa.

  El nombre: `atsw` ya es la familia (1.4.0 en PyPI). `atsw-gui` quita la
  ambiguedad sobre que trae `pip install atsw`.

- **La mudanza toma la historia de LOCAL, no de los remotos.** Cuatro de los
  cinco repos tienen trabajo sin subir, y dos estan en ramas sin upstream:
  fue (29 commits, sin upstream), fuf (25, sin upstream), gtk_fue (23 sin
  subir), drvarma (3 sin subir). Son 182 commits; `git subtree` los trae.

---

## DECIDIDO (2026-09-17) — fug y el repositorio

- **La rama buena de fug es `fug-1.14-proto`**, motor y GUI. Habia dos GUIs de
  fug, los dos en GTK2, los dos vivos y los dos tocados en septiembre:

        gtk_fmg.11/src                    2519 lineas, sin preview   13-sep
        atws/fug/fug-1.14-proto/gui/src   3765 lineas, CON preview   15-sep

  Han divergido de verdad —`data_load.c` en 218 lineas, `fug_run.c` en 138—,
  asi que **`gtk_fmg.11` pasa a ser archivo**. El GUI que hay que portar a GTK3
  es el de `fug-1.14-proto/gui`, y son ~74 sitios de API vieja, 50 de ellos en
  un solo fichero: medio dia.

- **`atws/` bajo git** (2026-09-17, `527f882`, local, sin remoto). Con lista
  blanca: se ignora todo y entran solo los seis documentos del estudio y
  `conformidad/`. 142 ficheros, 856K.

  El arbol completo son 908 MB con GSL dentro, diez repositorios anidados, los
  manuales, los zips de Windows **y un fichero de codigos de recuperacion de
  PyPI en texto plano**. Un `git init` a secas se lo habria llevado todo.

---

## DECIDIDO (2026-09-17) — drvarma

- **drvarma entra en la escalera y cumple el contrato**: tiene que **leer y
  validar un `.pre`**, como hacen fue y drtran. Hoy no lee ninguno — carga las
  series originales sin transformar y vuelve a empezar—, que es por lo que la
  fase 3 concluyó que «la escalera son dos peldaños y una recomendación
  verbal». **Es una fase posterior**, no entra en las que quedan.
- **drvarma tiene GUI, y se integra a posteriori.**
  `drvarma_source/drvarma_v.04.1/gui/drvarma_gui.c`, ~2.900 líneas, **ya en
  GTK3**, con Johansen y VECM al lado. Carga un fichero de datos separado por
  espacios, escribe el `.inp` de drvarma y lo ejecuta.

  Consecuencia para la fase 6: los GUI existentes son **tres**, no dos —
  gtk_fue (GTK3), gtk_fmg/fug (GTK2) y drvarma (GTK3)— más el de drtran, que
  hay que escribir. Dos de los tres ya están en GTK3.

---

## DECIDIDO (2026-09-16)

- **Orden: 1 → 3 → 4 → 5.** El contrato, la escalera, TASTE y el proyecto.
  **La fase 2 (el editor) se hace después**, con el contrato ya cerrado y
  sabiendo lo que la escalera y el proyecto le van a pedir.
- **P2 = A ahora, B más tarde.** El GUI en C se entiende con la suite **sólo
  por ficheros**: escribe `.inp`, lee `.out`, y no descuadra la terna. Leer y
  escribir el `guion.json` queda para más adelante, cuando el proyecto de la
  fase 5 esté definido — así el GUI se incorpora al recorrido una vez, y no
  dos.
- P1, P3 y P4 esperan a que su fase traiga la recomendación.

---

Premisa acordada: **los GUI en C van en paralelo a la suite atsw de Python.**
No son su futuro ni su sustituto; son la interfaz que enseña las tripas del
proceso, útil para formar analistas, con las limitaciones propias de una
interfaz antigua. El estudio los trata así.

---

## 0. Lo que el reconocimiento lateral ya dejó establecido

Esto no hay que volver a averiguarlo; hay que usarlo.

**El contrato existe, está escrito y está probado.**
`.inp(t−1) → .pre(t−1) → .inp(t) → .pre(t)`. El `.inp` es la especificación y
sus valores son semillas; el `.out` es el registro de la estimación con sus
errores típicos; el `.pre` es el mismo `.inp` con las estimaciones como
valores iniciales — un óptimo reejecutable. Tres reglas: los errores típicos
se leen del `.out` y **nunca** de reejecutar un `.pre`; nunca se escribe un
`.pre` a mano; un `.pre` que se toca vuelve a ser un `.inp`. Hay tests que
fijan cada una (`tests/test_contrato_de_ficheros.py`, `test_bug_0029_*`).

**Y tiene una grieta medida.** El invariante es sobre los VALORES, no sobre
la CURVATURA: arrancando en el óptimo, BFGS para en `niter=0` y devuelve la
semilla `2/n` como covarianza (BUG-0027/0061). Por eso los errores típicos
sólo son fiables en el `.out`.

**El formato tiene un lector y tres escritores.** Lector único:
`fue/inp.py::load()`. Escritores: `fue/report.py::write_pre()`,
`write_out()`, y `art/pipeline.py::_write_inp()` — que dice en su docstring
replicar `gtk_fue file_io.c:write_inp_file()`. Divergen de verdad: cabecera
anual, `%.4f` vs `%.6f` en los deltas, las columnas de los deterministas no
estándar, `custom` vs `non-standard`. Y en C hay otro lector-validador
(`inpcheck.c`) y otro escritor (el GUI). **Son cinco implementaciones del
mismo formato.**

**La escalera no tiene código.** `escalera.py` es otra cosa (la escalera de
Ockham del análisis de intervención). El paso art→mtram→sima vive en el
README de `atsw-suite`, en `FILE_FORMATS.md` y en las instrucciones de los
servidores. Lo único real es el convenio de ficheros: `drtran-python`
`pre.py::load_pre()` delega el parseo en `fue.load()` y sólo valida campos.

**El escalón sí se certifica.** `_diagonal_gate`: `logL(conjunta) == Σ
logL(univariantes)`, y de paso dice si lo que entró era un ÓPTIMO (`.pre`) o
una ESPECIFICACIÓN (`.inp`). El test del ciclo está implementado en los dos
lados (`network.py::find_cycle`, `drtran.c::topo_sort`) y **nombra** el
ciclo. Lo que no hay es traspaso automático a sima: el código rechaza.

**drtran no tiene fichero de entrada propio.** Lee los `.pre`/`.inp` de fue,
uno por serie, hasta 8. Lo suyo son `NAME.dag` (la red) y `NAME.cns`
(restricciones y covarianzas). No escribe `.pre` — hacerlo fue un defecto.

**El oráculo es un banco de regresión, no un decisor.**
`atws/Taste/oracle/battery.py` compara drtran y fue contra TASTE (1987/1993).
Importa porque el criterio normal de homologación de drtran («diagonal ≡ fue
por separado») sólo valida el trozo univariante: **TASTE es el único aval
externo de la función de transferencia**, que es justo lo que se añade al
subir el escalón.

**El PROYECTO no existe.** A nivel de serie y de modelo el modelo de datos es
excelente: el contrato, y `guion.py` (1562 líneas) con grafo de versiones,
nodos de decisión con razón y evidencia obligatorias, e integridad por sha.
Por encima no hay nada: el caso es una carpeta con convención no escrita
(`_m00`, `work/`), el guion es por serie, el enlace ART→drtran→drvarma es
prosa, y la `Policy` se construye en memoria y se tira. Lo único persistente
y declarativo es `registry.yaml` de SF_MEG, y es de series.

**TASTE ya tenía el proyecto, y se llamaba ÁREA DE TRABAJO.** Un fichero
`.AT0` = un caso = todas las series + todos los modelos + el estado de la
sesión (`ARCHS2.Graba_AT`). `HOWREY.AT0` es literalmente un caso guardado: dos
series, dos modelos univariantes y dos de transferencia. Se abría una cosa y
se seguía donde se dejó. **Eso es exactamente lo que atsw no tiene** — y otras
cuatro ideas suyas que hoy tenemos peor o no tenemos:

- **Catálogo de nombres cortos desacoplados de la ruta** (`DataName`,
  `ModlName`): se opera sobre nombres, no sobre rutas. Hoy todo son rutas.
- **La serie lleva su estado de transformación dentro** (`trdf, dr, de, bc,
  lost`, y fechas y estadísticos por duplicado, original y transformada): la
  transformada no es otra serie, es un estado de la misma.
- **Un hueco reservado a los RESIDUOS en el catálogo**: los residuos son una
  serie más, así que todo el menú de datos —tabular, gráfico, histograma,
  ACF, PACF— funciona sobre ellos sin una línea de código especial.
- **La metodología cableada en el menú**: bajo US y bajo TF, los mismos
  cuatro pasos —Identificación, Modelo, Estimación, Previsión—. La interfaz
  enseña el protocolo.

Y lo que TASTE **no** tenía, que es donde la terna de hoy es mejor: el `.TSM`
funde el `.inp` y el `.pre` sin marca que distinga semilla de óptimo, la
reestimación pisa los parámetros anteriores, y el recorrido no se guarda
(sólo `TASTE.OUT`, append-only y curado a mano con una tecla).

**Y el editor de gtk_fue pierde datos.** Comprobado conduciendo el programa:
«Edit .inp» → «Save .inp» → «Run» y la edición desaparece, porque `on_run_fue`
empieza reescribiendo el `.inp` desde el modelo en memoria. Además sólo abre
`<nombre>.inp`, nunca un `.pre`, no valida al guardar, y no recarga lo
editado a las pestañas.

---

## 1. Las cuatro preguntas que mandan

Son decisiones, no averiguaciones. Cada fase del estudio sirve para poder
tomarlas con fundamento, pero conviene tenerlas a la vista desde el principio
porque **cambian la forma de todo lo demás**.

### P1 — ¿Un solo dueño del formato, o cinco?

Hoy el mismo formato se lee y se escribe en cinco sitios. Un GUI nuevo sería
el sexto. Las salidas posibles:

| opción | qué implica |
|---|---|
| **A. Biblioteca C única** (`libatswinp`) que usen los motores y los GUI, y a la que Python llame por cffi | el C manda; Python deja de tener su parser |
| **B. Python manda** y los GUI hablan con él | el GUI deja de ser autónomo; hay que empaquetar Python con el GUI |
| **C. Dos implementaciones, un corpus de conformidad** que las ate | nadie manda; se paga con una batería de ficheros que las dos tienen que leer y escribir igual, byte a byte |

Mi lectura: **C es lo realista y A lo deseable**. El fuf/fue en C ya tienen
`inpcheck.c`, que es un validador campo a campo escrito para eso; y `fug` ya
comparte `inpfile.c`. Pero unificar de verdad obliga a mover el puerto de
Python, que es el que usa art-mcp todos los días.

### P2 — El GUI en C va EN PARALELO a la suite de Python. ¿Hasta dónde se le pide?

Queda fijado como premisa, no como pregunta: **los GUI en C de los motores
son una interfaz antigua que corre en paralelo a la suite atsw de Python.**
Su valor es didáctico — enseñan las tripas del proceso, que es justo lo que
un asistente conversacional esconde — y sus límites son inherentes a serlo.
El estudio no los trata como el futuro de atsw sino como lo que son: el
banco de trabajo del analista que está aprendiendo.

Eso ordena las prioridades del GUI en C: **primero que no mienta** (el
contrato), después que enseñe, y sólo después que sea cómodo. Y deja abierta
una pregunta de grado:

| opción | qué implica |
|---|---|
| **A. Sólo ficheros** — el GUI escribe `.inp` y lee `.out`; el asistente hace lo mismo; se encuentran en el disco | lo que hay hoy. El precio: el GUI no sabe nada del guion, ni del linaje, ni de los nodos de decisión, y puede descuadrar la terna sin enterarse |
| **B. El GUI lee y escribe también el `guion.json`** | el GUI pasa a ser un ciudadano del recorrido: enseña el árbol de versiones y anota por qué se hizo cada cosa. Para enseñar a un analista, esto ES la asignatura |
| **C. El GUI es cliente MCP** | lo convierte en una cara del asistente; potente, muy acoplado, y contradice el «en paralelo» |

Mi lectura: **B**, y por la razón pedagógica más que por la técnica. El
linaje y los nodos de decisión son lo que distingue el método de un
auto-ARIMA; un GUI educativo que no los enseñe está enseñando la mitad. Y es
barato: el `guion.json` es un JSON con esquema estable y ya probado.
**A es el mínimo irrenunciable** (fase 1 y 2); B es fase 4.

### P3 — El GUI de drtran, ¿antes o después de la interfaz madre?

Acordado que hay que hacerlo; lo que está abierto es el orden. Los dos
argumentos, tal como los veo:

| | a favor |
|---|---|
| **Antes** | Es el único caso de uso real que obliga a que la interfaz madre exista: n series, la ventana común, el enlace entre ficheros de distintos modelos. Hacerlo antes descubre los requisitos de la madre en vez de adivinarlos. Y el escalón univariante→transferencia es justo lo que hay que enseñar |
| **Después** | El GUI de drtran necesita seleccionar `.pre` certificados, y eso es gestión de proyecto: sin la madre acabaría con su propio selector de ficheros, que luego habría que tirar. Además hay piezas que se comparten (dibujo, ventana de gráficos, lector del `.inp`) y hacerlas dos veces es lo que ya nos ha pasado con `fugdraw` |

Mi recomendación: **un tercer camino**. Hacer *primero* la factorización
mínima que los dos necesitan —la biblioteca de dibujo y ventana de gráficos,
y el lector/escritor del `.inp`—, que es fase 5a y es obra corta y segura;
después el GUI de drtran como **segundo cliente** de esa biblioteca; y la
interfaz madre al final, ya sabiendo de verdad qué tiene que gestionar,
porque habrá dos programas pidiéndoselo. Hacer la madre primero es diseñar
un gestor para un solo cliente, que es como se diseñan los gestores que no
sirven.

### P4 — ¿Qué es «atsw»: un programa, o una familia?  ▸ **RESUELTA (2026-09-20)**

> **ATSW es un TALLER, y tiene dos encarnaciones:** ATSW GUI (motores en C,
> multiplataforma, para educación y para el analista que quiere control total
> del proceso) y ATSW Python (interfaz de última tecnología conducida por un
> LLM, los mismos motores, **gestión de proyectos distinta**).
> **Distribuciones separadas**: `pip install atsw` no trae el GUI. La familia
> es `atsw`; el programa, `atsw_gui`.
> El razonamiento y sus consecuencias, en `DISENO-madre.md` §10.

El planteamiento original era éste:

`atsw` ya existe como paquete paraguas de Python (1.3.0) que instala fue,
pyfug, art, drtran y drvarma con sus tres asistentes. Si el GUI también se
llama atsw, hay que decidir si es:

- **un programa** (la interfaz madre) que lanza motores y abre vistas; o
- **la familia entera**, y la interfaz madre se llama de otra manera dentro.

No es cosmético: decide si `pip install atsw` tiene que traer el GUI.

---

## 2. Las fases

Cada fase entrega un documento y, donde tiene sentido, una prueba ejecutable.
El orden no es negociable: cada una usa la anterior.

### Fase 1 — El contrato, a fondo  ▸ manda sobre todo lo demás

**Pregunta.** ¿Qué promete exactamente el contrato, qué no promete, y qué
hace falta para que un sexto actor (el GUI) no lo rompa?

**Qué se mira.**
- Las cinco implementaciones, campo a campo, y el inventario de divergencias
  (ya hay cuatro identificadas; hay que cerrarlo).
- Qué se pierde en un ida y vuelta: el nombre del regresor externo, un μ fijo
  no nulo, las secciones comentadas, los comentarios de cabecera.
- Los tres `.inp` de la familia (fue, fuf, fug) que comparten extensión y se
  distinguen por contenido. ¿Se quedan así, o se marcan?
- El `refactor = 100` y el AR(1) fijado en cero que evita el SIGSEGV: qué más
  hay de esta clase, escrito en ningún sitio salvo el código.
- Qué es estructura y qué es semilla, y si conviene separarlos en el fichero
  o dejarlo como está y separarlo sólo en la interfaz.

**Entrega.** `CONTRATO.md`: el contrato tal como ES (no como se cuenta), la
lista cerrada de divergencias, y una propuesta para P1. Más una **batería de
conformidad**: N ficheros que toda implementación tiene que leer y reescribir
igual. Esa batería es lo que convierte el contrato en algo comprobable.

---

### Fase 2 — El editor de fue  ▸ lo que pediste, derivado del contrato

**Pregunta.** ¿Cómo se edita un `.inp` o un `.pre` sin que el sistema quede
mintiendo?

**Qué se decide.**
- **Un solo dueño del modelo en cada momento.** El fallo de hoy es que hay
  dos (el fichero y las pestañas) y gana el que escribe último. O el editor
  recarga a las pestañas al guardar, o las pestañas se bloquean mientras se
  edita el texto. No hay tercera.
- **Validar al guardar** con `inpcheck` (ya está en el GUI) y no dejar
  guardar algo que el motor no podría leer.
- **La degradación del `.pre`.** Si se edita un `.pre`, el editor tiene que
  ofrecer guardarlo como `.inp` — que es lo que dice el contrato — y decir
  qué pasa con los hermanos `.pre`/`.out`, que quedan obsoletos.
- **La terna.** Al guardar un `.inp` nuevo, el `.out` y el `.pre` de al lado
  son de otro modelo. Marcarlos, moverlos o borrarlos: hay que elegir.
- **Qué edita el editor.** Texto plano con resaltado y validación, o un
  editor estructurado por secciones. Mi opinión: **texto, con la estructura
  al lado** — el valor de este GUI es enseñar las tripas.

**Entrega.** `DISENO-editor.md` + el editor implementado en gtk_fue, con
prueba conducida desde el código (ya hay banco: `tests/test_gui.c`).

---

### Fase 3 — La escalera como dato  ▸ convertir la prosa en fichero

**Pregunta.** ¿Qué hace falta para que subir de univariante a transferencia
sea una operación con estado y no una costumbre?

**Qué se mira.**
- Qué identifica un peldaño y qué lo certifica (`_diagonal_gate`, el
  certificado óptimo-vs-especificación, el test del ciclo).
- El **fichero de sesión de mtram** que hoy no existe: qué `.pre`, qué red,
  qué restricciones, qué ajuste. Hoy vive en la memoria del servidor.
- `NAME.dag` y `NAME.cns`: ¿son ya ese fichero de sesión, o falta la tercera
  pata (qué `.pre` y en qué orden)?
- El oráculo como puerta del escalón: si TASTE es el único aval externo de la
  transferencia, ¿tiene que pasar por ahí un modelo antes de darse por bueno?

**Entrega.** `DISENO-escalera.md`: el formato de la sesión, qué se guarda y
quién lo escribe, y dónde encajan las dos puertas (diagonal y ciclo).

---

### Fase 4 — TASTE  ▸ cómo se gestionaba un proyecto cuando hubo que inventarlo

**Por qué está aquí.** TASTE (1987/1993) no entra en el estudio como oráculo
—eso ya está resuelto— sino como **diseño**. Lo escribieron los que
inventaron esta manera de analizar series, con las limitaciones de la época,
y resolvieron el problema que hoy no tenemos resuelto: cómo organiza su
trabajo un analista que lleva varias series, varios modelos por serie, y un
recorrido que hay que poder rehacer. Conviene mirarlo antes de diseñar el
proyecto, no después, para no reinventar peor.

**Pregunta.** ¿Qué era una serie, un modelo y un proyecto para TASTE, y qué
de aquello sigue siendo buena idea hoy?

**Lo que el reconocimiento ya trajo** (§0): el área de trabajo, el catálogo
por nombres, el estado de transformación dentro de la serie, el hueco de los
residuos y la metodología en el menú. La fase no tiene que averiguar eso: lo
tiene que **convertir en decisiones**.

**Qué se decide.**
- **El área de trabajo, ¿vuelve?** Un fichero que reanuda un caso entero es
  cómodo y es lo que atsw no tiene. Pero el `.AT0` era un volcado binario que
  no sobrevivía ni al cambio de compilador. La versión de hoy sería un
  manifiesto de texto que **apunta** a la terna, no que la contiene. ¿Dónde
  está la frontera entre apuntar y contener?
- **Nombres o rutas.** El catálogo de TASTE operaba sobre nombres cortos; hoy
  todo son rutas absolutas, incluso dentro del `guion.json`. Un caso que se
  mueve de directorio hoy se rompe. ¿Se adopta el catálogo?
- **La transformada, ¿serie o estado?** TASTE decía estado; atsw dice que la
  receta está en el `.inp`. Las dos tienen razón en algo. Hay que elegir qué
  enseña la interfaz.
- **Los residuos como serie de primera clase.** Idea barata y excelente: el
  GUI podría graficar, tabular y hacer ACF de los residuos con el mismo
  código que usa para los datos. Hoy son herramientas aparte.
- **El menú como protocolo.** Las pestañas de gtk_fue están organizadas por
  *sección del modelo* (Datos, Box-Cox, Determinista, Estocástico); las de
  TASTE, por *paso del método* (Identificación → Modelo → Estimación →
  Previsión). Para un GUI que enseña, la segunda es la buena. ¿Se reorganiza?
- **Y lo que TASTE dejó sin hacer**: el módulo de tabla publicable con
  unidades y fuente (`TABLA`, `Graba_Ser_PRN`, `Graba_Ser_TSF`: declarados y
  nunca escritos). Es justo la capa que una interfaz madre debería cubrir.
  Señal, no casualidad.

**Entrega.** `ESTUDIO-taste.md`: el modelo de datos y de sesión de TASTE, y la
lista de ideas con veredicto —*la suite de hoy la tiene / la tiene peor / no
la tiene*— y con decisión. Esa lista es la entrada de la fase 5.

---

### Fase 5 — El proyecto  ▸ la entidad que no existe

**Pregunta.** ¿Qué es un proyecto en atsw, y qué tiene que saber?

**Qué se mira.**
- Lo que ya hay y sólo hay que exponer: `registry.yaml` (series),
  la terna (modelos), `guion.json` (linaje y decisiones), `policy.py`
  (criterios), el oráculo (validación).
- Lo que no hay: el objeto que agrupa n series × n modelos; el enlace
  ART→drtran→drvarma como dato; las preferencias persistentes; la
  correspondencia serie↔guion↔ficheros, que hoy se deriva del directorio.
- Si el proyecto es **un fichero de manifiesto** (declarativo, versionable,
  legible) o **una base de datos**. Mi lectura: manifiesto, por coherencia
  con todo lo demás de la suite, que es de ficheros.

**Entrega.** `DISENO-proyecto.md`: el modelo de datos, el manifiesto, y qué
herramienta lo escribe (el GUI, art-mcp, o los dos).

---

### Fase 6 — La interfaz madre, y el GUI de drtran

**Pregunta.** ¿Qué programa es atsw, qué comparte con qué, y cómo se llega
desde él a una función de transferencia?

**Qué se mira.**
- Qué se factoriza de verdad: hoy hay cuatro copias de `fugdraw.c` y dos de
  `preview.c`. La biblioteca compartida (dibujo, ventana de gráficos, lector
  del `.inp`) es la primera pieza de la interfaz madre, no la última.
- Qué necesita un GUI de drtran que el de fue no tiene: n series con una
  marcada como salida, la ventana común, la compatibilidad de operadores,
  la terna (b, r, s) por enlace con la CCF preblanqueada, el editor del DAG
  con detección de ciclo en vivo, el editor de restricciones con las ranuras
  del modelo, y el escalón diagonal como pantalla propia.
- De dónde salen los datos de drtran: **de los `.pre` de fue, no de un CSV**.
  Un GUI que dejara cargar datos crudos se saltaría el escalón univariante.
- GTK3 en todo (gtk_fue ya lo es; el GUI de fug sigue en GTK2).

**Entrega.** `DISENO-atsw.md`: la arquitectura, qué bibliotecas, qué
programas, y el plan de migración desde lo que hay.

---

## 3. Lo que NO voy a estudiar

Para que el estudio acabe:

- **drvarma/sima.** 171 000 líneas. Entra sólo como destino del test del
  ciclo, no como objeto de estudio.
- **La estadística.** No se revisan criterios ni contrastes: es un estudio de
  arquitectura y de contrato.
- **Windows y macOS.** Se anotan las decisiones que los afecten, no se
  prueban.
- **Reescribir el puerto de Python.** Se estudia el contrato que cumple; si
  P1 decide moverlo, eso es obra, no estudio.
- **Ejecutar TASTE.** Se estudia su código y sus ficheros. Hacerlo correr bajo
  DOSBox para ver los menús vivos sería otro trabajo; si de la fase 4 sale
  que hace falta, se decide entonces.

---

## 4. Lo que necesito de ti

1. **Las cuatro preguntas de §1.** P1 y P3 pueden esperar a que su fase traiga
   la recomendación. **P2 conviene decidirla pronto** —cambia lo que el editor
   de la fase 2 tiene que hacer— aunque sea sólo para decir «A ahora, B más
   tarde», que es lo que yo propondría.

2. **El orden.** Propongo:

   ```
   1 contrato ─ 3 escalera ─ 4 TASTE ─ 5 proyecto ─ 2 editor ─ 6 madre + drtran
   ```

   DECIDIDO. El editor se beneficia de esperar: cuando se haga, ya sabremos
   si el modelo se identifica por ruta o por nombre, si hay área de trabajo, y
   qué tiene que guardar al lado del fichero.

3. **La factorización (fase 6a) puede adelantarse.** Sacar `fugdraw` y la
   ventana de gráficos a una biblioteca compartida no depende de ninguna
   decisión de este estudio, es obra corta, y hoy hay cuatro copias de
   `fugdraw.c` y dos de `preview.c`. Se puede hacer en cualquier hueco.

4. **Hasta dónde llega «a fondo».** Mi propuesta: cada fase termina en un
   documento **y en algo ejecutable** —una batería, una prueba, un prototipo—.
   Un estudio que sólo produce documentos no se comprueba a sí mismo, y este
   proyecto ya tiene bastante prosa buena que nadie ejecuta.
