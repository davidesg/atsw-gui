# INVENTARIO — lo que hay hoy, antes de diseñar la interfaz madre

Fase 6. Antes de proponer arquitectura, los hechos: **quién carga datos, quién
escribe qué, dónde se pierden las rutas y qué gestión de proyecto ya existe.**

Se hizo con cuatro sondas sobre los tres repositorios y **se comprobó a mano
todo lo que aquí se afirma**. Donde una comprobación contradijo al estudio, se
dice.

---

## 1. La entrada de datos: **cuatro lectores, y discrepan**

| | dónde | un fichero de 2 columnas |
|---|---|---|
| fug | `gui/fug/src/data_load.c:117` | **aplana**: todos los campos de todas las filas en un vector |
| fue | `gui/fue/src/data_handling.c:11` | **columna 1**; el resto a `DataMat` |
| drvarma | `engines/drvarma/gui/drvarma_gui.c:198` | multivariante, con `setlocale` |
| drvarma viejo | `engines/drvarma/old/drvarma_gui.200426.c:148` | copia anterior, sin `setlocale` |

Comprobado leyendo las dos funciones: `collect_values` (`data_load.c:87-113`)
hace `g_array_append_val` por **cada token de cada línea**; fue hace
`Ts.data[i] = DataMat[1][i]` (`data_handling.c:69-72`).

> **El mismo fichero cargado en fue y en fug da dos series distintas.** No
> parecidas: una entrelaza las columnas y la otra se queda con una.

Y divergen en más: fug admite cabecera, `;` y coma decimal; fue usa
`fscanf("%lf")` y falla con las tres. Un fichero con cabecera carga en fug y no
en fue.

Es el patrón conocido del repositorio —`fugdraw` tuvo cuatro copias,
`preview.c` dos— pero **peor**: aquéllas eran idénticas y se factorizaron sin
riesgo; éstas discrepan, así que unificarlas **obliga a decidir cuál acierta**.

**Ninguno de los cuatro deduce la frecuencia ni la fecha del fichero.** Salen
de un combo de la ventana, con defaults distintos: fug `freq=1`
(`gui/fug/src/main.c:83`), fue y drvarma `freq=12`, año 2000
(`main_window.c:209,228`). El dato de cuándo empieza una serie no viaja con la
serie: lo teclea quien la carga.

`gui/drtran` **no lee datos crudos, y es deliberado** (`src/main.c:9-11`):
aceptar un CSV se saltaría el escalón univariante. Es la única de las tres
puertas que está cerrada a propósito.

---

## 2. La salida: tres programas escriben en el directorio actual

**Comprobado ejecutando**, no leyendo. Con los `.pre` en `dt/` y lanzando desde
el directorio de arriba:

```
$ drtran dt/M6_EP.pre … -n dt/m6_net.dag
$ ls dt/*.out   →  NADA
$ ls *.out      →  M6_EP_M6_EI_M6_EU_M6_EC_M6_EA_M6_P.out
```

El `.out` **no** aparece junto a los datos: aparece donde se lanzó. `base_name()`
(`drtran.c:186-194`) descarta el directorio al componer el nombre por defecto.

Y dos fallos del mismo tipo en fue, también reproducidos:

```
$ fue caso/X
Warning: can not write Acaso/X.eps          ← el gráfico de residuos SE PIERDE
Created caso/X.pdf                            (y el PDF se crea igual)

$ fue caso/X -f 4
Error opening file: forecast_caso/X.inp     ← "forecast_" + la ruta entera
```

El prefijo se antepone a la **ruta completa**, no al nombre. El de `-f` aborta y
se ve; el del EPS **sólo avisa y sigue**, así que el informe sale sin el gráfico
de residuos y nadie se entera. `fug` esquivó esto haciendo `chdir` al
directorio del `.inp` (`fug.c:305-321`); fue, fuf y drtran no.

**Colisiones.** `fue X` y `fue X -f` escriben el mismo `X.out`/`X.tex`/`X.pdf`,
y la segunda corrida pisa la primera sin dejar `.pre`. `drtran` sin `-m` sobre
las mismas series da el **mismo nombre** aunque cambien la red o el `.cns`: dos
modelos distintos, un solo fichero.

Y lo mismo en lo que acabo de escribir yo: `gui/drtran` usa nombres **fijos** en
una caché única (`modelo.out`, `red.dag`, `modelo.cns`, `residuos.txt`). **No
caben dos modelos.** Es el defecto de TASTE —una sola ranura `RESIDUOS`,
`TASTECTV.PAS:475`— reaparecido por la puerta de atrás. Diagnosis y Previsión lo
esquivan guardando copias **en memoria** (`Calcular baseline`, `Guardar esta
evaluación`), que se pierden al cerrar.

---

## 3. Las rutas: **la afirmación del estudio es falsa para C**

El plan decía:

> «El catálogo de TASTE operaba sobre nombres cortos; hoy todo son rutas
> absolutas, incluso dentro del `guion.json`. Un caso que se mueve de
> directorio hoy se rompe.»

**En el lado C no se persiste ni una sola ruta.** Comprobado sobre el corpus:

- **`.inp` / `.pre` son autocontenidos.** La serie va dentro y la cabecera lleva
  sólo el nombre corto: `69  3 1976 EP`. (Las barras que aparecen en un grep son
  `ACF/PACF` en una línea de comentario.)
- **`.dag`** usa **nombres cortos**: `EP <- EI  1 0 1`.
- **`.cns`** usa **índices**: `q[5,2] = free`.
- **`.out`** lleva una ruta *documental* —la que se tecleó— y **nadie la
  reparsea**.
- **No hay fichero de sesión ni de preferencias.** Ninguno de los tres GUIs
  guarda nada entre ejecuciones.

Mover un directorio de caso en el lado C **no rompe nada**.

### Lo que sí se rompe, y cuánto

En `guion.json` (lado Python) hay rutas absolutas, pero `_resuelve_ruta`
(`guion.py:922-956`) ya las **rescata por basename** contra la carpeta del
propio guion — para `inp_path` y `out_path`. Porque `CAMPOS_DE_RUTA` es
exactamente eso, y nada más:

```python
CAMPOS_DE_RUTA = ("inp_path", "out_path")      # guion.py:919
```

**`base_pre_path` se queda fuera**, y es el campo del **linaje**. Lo medí sobre
los 131 guiones reales del disco:

```
entradas con base_pre_path:                                194
  el fichero no existe, pero SÍ está junto al guion:        70   ← rescatables, no rescatadas
  no existe ni junto al guion:                             100
```

**170 de 194 apuntan a un fichero que ya no está**, y 70 se arreglarían con el
mismo truco que ya se aplica dos líneas más arriba. La consecuencia:
`infer_parent` (`guion.py:415-419`) no encuentra al padre y cuelga la versión
nueva de la última; `linaje_dudoso` marca «no consta».

> Corrección al plan: **no es que el caso se rompa al mudarse — es que el
> linaje ya está roto donde está.** Y el arreglo es una línea, en el lado
> Python, que es intocable: queda como hallazgo, no como tarea.

### Y el problema real del lado C es otro

No la mudanza: **la identidad posicional**. El `.cns` nombra por índice y el
`.dag` por nombre, así que reordenar o borrar una serie deja los enlaces
apuntando a otra cosa **en silencio** (`lib/netfile/netfile.h:97-114`). Está
mitigado en la interfaz —`net_remap` al arrastrar— pero no en los ficheros.

---

## 4. Qué gestión de proyecto existe ya

**`guion.py` (1562 líneas) es la pieza madura**, y el plan acertaba con ella.
Un grafo de versiones con linaje verificable por SHA, nodos de decisión con
`{nodo, decidido, evidencia, alternativas}` y quién decidió
(`analista+LLM | LLM | heurística`), y `diff_nodes`, que empareja **por nombre
de nodo y no por posición**. Ya tiene escritas `infer_parent`,
`linaje_dudoso`, `descendants`, `abandon`, `safe_ancestor`,
`modelos_sin_registrar`, `export_guion_html`.

> Su alcance real **no es la serie: es el DIRECTORIO**. La ruta se deriva con
> `dirname(output_path)` y reutiliza un `guion.json` existente si lo encuentra.
> Está documentado como defecto en `DISENO-proyecto.md:110-125`.

**`policy.py` (953 líneas)** ya es una *policy* declarable: una tabla de
umbrales (`THRESHOLDS`) que es el único sitio con cortes `|z|`, y once
decisiones con interfaz abstracta. Le falta un origen que no sean `**kwargs`.

**`registry.yaml` NO es «lo que ya hay».** El plan lo daba por existente. Hay
**un solo ejemplar**, en `~/Dropbox/SF_MEG/empirical/data/`, y **ningún código
lo lee**: las únicas referencias en los tres repositorios están en
`DISENO-proyecto.md` y en el propio plan. Es un buen esquema de procedencia
—id, país, instituto, código en el origen, año base, notas con evidencia
fechada— mantenido a mano para un trabajo concreto, y **hoy está huérfano**.

**El oráculo de TASTE existe y está desenchufado.** 20 casos, y `DISENO-escalera.md`
§1.3 ya dice por qué no sirve hoy: compara TASTE con un literal fechado, no con
drtran de hoy, así que 20/20 pasan siempre.

**No existe ningún concepto de proyecto ni de caso, en ninguno de los tres
repositorios.** La unidad es la terna `.inp`/`.out`/`.pre`, y el agrupador de
facto es el directorio.

### La prueba de que falta: los guiones sueltos

En `engines/drvarma/cases/` hay tres programas Python escritos a mano
—`forecast_compare.py`, `recursive_compare.py`, `recursive_eval.py`— y otro
homónimo y no emparentado en `engines/drtran/examples/passthrough/`. Hacen lo
que le tocaría a la madre: recorrer casos y comparar resultados entre peldaños.
Con un diccionario a pelo:

```python
ART_FUF = {"IPC_ES": "cases/IPC_ES/work/IPC_ES_m10.fuf.inp",
           "IPC_FR": "cases/IPC_FR/work/IPC_FR_msar.fuf.inp", ...}
```

Ahí dentro hay **tres cosas que deberían ser datos del proyecto**: la
disposición del caso, el convenio de nombres de modelo (`_m10`, `_msar`,
`_m00`) y —la que importa— **cuál de los n modelos de una serie es el
elegido**. Esa decisión hoy no está escrita en ninguna parte salvo en el nombre
de un fichero, repetido en tres guiones con rutas relativas a un cwd concreto.

Y la disposición de drvarma —`cases/<CASO>/<CASO>.inp` + `work/<CASO>_<modelo>.*`—
es la única de las tres que tiene forma de proyecto. **No está documentada en
ningún README**: es convenio emergente.

---

## 5. La exportación: lo que TASTE dejó sin hacer, sigue sin hacerse

`ESTUDIO-taste.md` §5 cierra así:

> Se construyó el instrumento y se dejó sin construir el informe. **Ése es
> exactamente el hueco que la interfaz madre tiene que llenar.**

TASTE declaró `TABLA`, `Graba_Ser_PRN` y `Graba_Ser_TSF` y no escribió ninguno.
Hoy:

**Hay** PDF y LaTeX de informe (fue, fuf, fug, drtran), EPS de todos los
gráficos, «Guardar como» a PDF/EPS/PNG/SVG en el visor, y **un solo CSV**: el de
evaluación recursiva de drtran (`-C`), más el texto tabulado de `-e`.

**No hay** ningún export de **tablas**: ni la de parámetros, ni la de
previsiones, ni la ACF/PACF, ni los residuos de fue/fuf. **Ningún botón
«Exportar» en ninguno de los tres GUIs.** Nada de CSV de resultados, JSON,
Excel ni portapapeles.

Siete pantallas y la única forma de sacar un número es copiarlo de una
etiqueta.

---

## 6. Los requisitos que salen del inventario

No adivinados: cada uno viene de un hecho de arriba.

1. **Una sola puerta de datos.** Un lector, no cuatro, y que decida qué hacer
   con las columnas y la cabecera. Unificar obliga a elegir un comportamiento:
   hay que elegirlo, no promediarlo. (§1)
2. **La frecuencia y la fecha tienen que viajar con la serie**, no salir de un
   combo con defaults distintos por programa. (§1)
3. **Los artefactos van donde está el caso**, no donde se lanzó el programa. Y
   los prefijos van en el nombre, no delante de la ruta. (§2)
4. **El proyecto da nombre a las corridas.** Sin eso no caben dos modelos, y
   comparar dos modelos es justo lo que la suite sabe hacer y TASTE no podía.
   (§2)
5. **La identidad de una serie no puede ser su posición.** (§3)
6. **Exponer `guion.py`, no reinventarlo** — y llamarlo n×m, que es lo que le
   falta. (§4)
7. **El catálogo por nombre corto hay que conectarlo**, porque existe sin
   dueño. (§4)
8. **La tabla publicable es el primer requisito, no el último.** (§5)

---

## 7. Lo que queda por decidir

Sin cambios desde el plan, pero ahora con hechos debajo:

- **Nombres o rutas.** El lado C ya opera con nombres cortos y no tiene el
  problema. La pregunta es si la madre adopta ese modelo o el de rutas.
- **La frontera entre apuntar y contener.** El `.pre` **contiene** los datos, y
  por eso es portátil. ¿La madre apunta a los `.pre` o los agrupa?
- **Qué es «atsw»** (P4): ¿la madre es un programa de la familia, o es la
  familia?
- **Los residuos como serie de primera clase.** Hoy hay tres implementaciones
  de graficarlos; la última la escribí esta semana.

---

**Estado: inventario cerrado.** La entrada de la propuesta de arquitectura.
