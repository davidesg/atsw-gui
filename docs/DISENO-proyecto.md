# El proyecto

Entrega de la **fase 5** del estudio de atsw. La pregunta era: *¿qué es un
proyecto en atsw, y qué tiene que saber?*

La respuesta corta: **el proyecto no puede ser «una carpeta con varios
guiones», porque eso es exactamente lo que ya hay y es lo que falla.** Tiene que
ser un manifiesto que declare una raíz, nombre las series por referencia, fije
una vez lo que es del conjunto, y dé nombre a los ejes que hoy viven en la
primera letra de un fichero.

Todo lo que sigue está medido sobre el corpus real: 125 guiones, 1673 entradas,
cuatro árboles de casos.

---

## 0. Tres formas que se repiten, y que explican casi todo

El estudio lleva cinco fases. Tres patrones han aparecido en más de una, y
conviene nombrarlos porque el diseño sale de ellos.

### La suite calcula lo que importa y lo imprime

| fase | qué se calcula | dónde acaba |
|---|---|---|
| 3 | el `log` de `build_model`: qué decidió en cada nodo | concatenado en una cadena de retorno, y se tira |
| 4 | el `LastReport` de TASTE: σ², errores típicos, correlaciones entre parámetros | sólo al fichero temporal; si no pulsas `A`, desaparece |
| 5 | **qué modelos han quedado obsoletos porque su serie se movió** | un log de texto que nadie lee |

El tercero está fechado en el log real:

    2026-07-02 11:29  re-estimation queue: DE_CORE

Una afirmación verdadera, precisa y accionable —los modelos de DE_CORE están
obsoletos— escrita en un fichero de texto hace dos meses y medio. El código que
la produce aparece **una sola vez** en todo el árbol: la llamada que la imprime.

### El programa tiene el dato delante al escribir, y no lo escribe

| fase | el programa sabe | y escribe |
|---|---|---|
| 3 | los nombres de las series (los pone en un comentario) | `q[3,2] = free` |
| 4 | que la línea 4 del `.BJD` son las unidades | la lee a una variable llamada `shit` |
| 5 | su propia versión (la imprime en el banner) | nada: el `.out` no la estampa |

Hoy conviven cuatro estados del toolchain en el mismo árbol de trabajo, y un
`.out` no puede decir con cuál se hizo.

### Un campo que la herramienta EXIGE se rellena; uno que OFRECE, no

Medido, y es la regla de diseño de toda la fase:

    nodos con rationale   881 / 924   (95%)   ← guion_node RECHAZA la llamada sin razón
    guiones con analyst     0 / 125   ( 0%)   ← el campo existe y nadie lo puede escribir

No es desidia: `analyst` **no tiene parámetro**. Los tres sitios que construyen
un guion lo fijan al literal `""`. El campo se declara, se serializa, se pinta
en el HTML exportado, y nadie lo rellena nunca.

Es la misma regla que explica las unidades de TASTE (ofrecidas, nunca
capturadas) y el `note` de `registry.yaml` (ofrecido como texto libre,
degradado a prosa).

---

## 1. Lo que ya existe y es bueno

Esto **no** hay que rediseñarlo. Hay que exponerlo.

**El nodo del guion.** Veintinueve campos, y los que importan están bien
pensados: `parent`, `status`, `why_abandoned`, `decision`, `rationale`,
`node.{nodo,decidido,evidencia,alternativas}`, `decided_by`. Modelos y
decisiones viven en **una sola lista intercalada**, porque el orden en que
ocurrieron es información: un nodo después de un modelo es una reformulación.

**`diff_nodes`.** Compara dos recorridos emparejando **por nombre de nodo, no
por posición**, con visitas numeradas cuando un nodo se repite, y devuelve
valor + razón + evidencia + decisor de cada lado con un veredicto. Es la
primitiva que un proyecto necesita para contrastar dos series o dos carriles.
**No hay que rehacerla: hay que llamarla n×m.**

**La integridad por sha, y su semántica.** `linaje_dudoso` distingue tres
motivos, ordenados de peor a menos malo: *el padre es otro* (la huella no
cuadra: el árbol dibuja un enlace que no existe), *el fichero ha cambiado* (no
es reproducible desde el disco) y *sin contrastar*. Y una huella vacía
significa **no consta**, nunca *cuadra*. Esa distinción es exactamente la que
hace falta.

**La jerarquía de la policy.** `Policy` / `DefaultPolicy` / `ClaudePolicy`, con
la regla escrita: *declarado gana a inferido, siempre*; *el silencio significa
«usa la regla», no «no hay media»*. Ya es una policy declarable. Sólo le falta
un origen que no sean `**kwargs`.

**`objetivo` y su puerta.** Un enumerado validado que **rechaza lo desconocido**
en vez de degradar en silencio. Es el modelo de validación que debe usar todo
campo del manifiesto.

**Los detectores de que el registro se sabe incompleto.**
`modelos_sin_registrar` (nació de un caso con 13 modelos en disco y 9 en el
registro, **y el modelo final entre los que faltaban**) y
`entradas_que_no_cuadran`. Escalan a proyecto cambiando la carpeta que barren.

**`registry.yaml`.** Procedencia como campos estructurados —instituto, fuente,
código en el origen, base— y notas con evidencia fechada. Es lo mejor que hay.

---

## 2. Lo que falta, medido

### El alcance del guion no es la serie: es el DIRECTORIO

Ésta es la corrección de fondo de la fase. La ruta del guion se deriva así:

```python
d = dirname(output_path)
if exists(d + "/guion.json"):  return d + "/guion.json"     # ← quien escriba aquí, cae dentro
return d + f"/{serie}_guion.json"
```

La rama que **reutiliza** se prueba **antes** que la que nombra por serie. De
modo que si un proyecto dirige varias series a un directorio común, la segunda
se apunta al guion de la primera, sin aviso, con `series` diciendo el nombre de
la primera.

Y el nombre de la serie no está declarado: se deriva de dos maneras
incompatibles según qué herramienta cree el fichero —del `.inp` o **del nombre
del fichero**— con un *fallback* al literal `"serie"`. Resultado medido:

    series == "serie"   25 de 125 guiones

El literal se filtra a los nombres de las figuras: `figs/serie_v10__3c518e69.png`.

### Las rutas no sobreviven al viaje

    entradas con inp_path   911
    rutas ROTAS             387   (42%)
    rutas de Windows        109   C:\Users\mtgp2\...

**El fichero que debería ser el registro portátil del caso es precisamente el
que no lo es.** Absolutizar al escribir no bastó: hay guiones escritos en otra
máquina, por otro analista, con rutas que aquí no significan nada.

### El tramo multi-serie es el único que no deja rastro

`batch_build` recibe n `.inp`, un directorio y un `objetivo` común. Es lo más
parecido a un proyecto que existe. **No tiene parámetro de guion y no registra
nada.** La herramienta que más se parece a un proyecto es la que menos apunta.

### Lo que es del conjunto se pasa por llamada y se evapora

`objetivo` veta una ruta estacional **para todas las series de un sistema**, con
una razón de conjunto: *«las series de un sistema tienen que llevar el mismo
tratamiento estacional o sus órdenes de integración no son comparables»*. Hoy es
un argumento por llamada. El proyecto no puede afirmar «todas mis series se
modelizaron con objetivo = multivariante», ni un lector comprobarlo.

### Los ejes del proyecto viven en la primera letra de un fichero

Caso real, un solo directorio, una sola serie, **tres cadenas paralelas**:

    IPC_ES_m00…m07w    muestra 2002-2019
    IPC_ES_x00…x07v    muestra 2002-2023, datos del Excel del usuario
    IPC_ES_y00…y08     muestra 2002-2023, datos descargados del INE

**Muestra y vintage son dos ejes de proyecto, y viven en la primera letra.** Sus
`.out` conviven en el mismo `work/`. El nodo `"muestra"` existe en el corpus y
**no está en la lista de nodos canónicos**, así que la máquina de comparar lo
empuja al final como desconocido.

### Y el radio de daño de un defecto hay que calcularlo a mano

El `IPC.xlsx` del usuario tenía **quince valores seguidos erróneos**. El README
que lo documenta concluye:

> *«Todo análisis de IPC_ES con la muestra extendida desde `IPC.xlsx` —run3 a
> run9_DS **y lo que SF_MEG haya tomado de él**— modelizó esos valores.»*

Ese «*y lo que SF_MEG haya tomado de él*» es el resumen del problema: **ningún
guion referencia el registro, ni el fichero de datos, ni su vintage**, así que
la pregunta «¿qué hay que reestimar?» es arqueología manual — y su respuesta
acabó en prosa dentro de un README.

### El veredicto del ejercicio de reconstrucción

Se reconstruyó un caso real desde cero, como si llegara nuevo:

    ~45 %  en ficheros, fiable y legible por máquina
    ~30 %  sólo como prosa en Markdown
    ~25 %  sólo en la cabeza del analista

Y el reparto está **invertido respecto a la dificultad**: lo que el sistema
guarda impecablemente —los 216 datos, los 13 parámetros— se recalcula en ocho
segundos. Lo que no guarda —por qué esos 216, cuál de los trece modelos vale, y
que los directorios de agosto son ejercicios calificados y no ciencia— es lo
único irrecuperable.

### El aviso que vale por todo el ejercicio

El reconocimiento dedujo de los ficheros que `mNN` era `ARMA(p,q)`: encaja
perfectamente con los tres ficheros del caso piloto. **Es falso** — lo
desmiente la escalera larga, donde `m09` tiene un factor AR de orden 2 y `m10`
de orden 1: el paso *baja* el orden. Es un contador secuencial.

Lo grave no es el error. Es que con acceso total, todos los ficheros delante y
una hora de trabajo, la convención se puede deducir **mal** y nada la desmiente.
Y en el mismo árbol `_m01` significa dos cosas distintas para la misma serie.

> **El número de versión tiene que ser un campo, no un trozo del nombre que
> cada lector reinterpreta.**

---

## 3. El manifiesto

Un fichero, `proyecto.yaml`, en la raíz del proyecto. Declarativo, versionable,
legible — coherente con el resto de la suite, que es de ficheros.

```yaml
schema_version: 1
id: SF_MEG
titulo: "Inflación del área euro — la asimetría confirmatoria"
creado: 2026-07-02
analista: "D.E. Guerrero"
razon: |
  Doce IPC nacionales, headline y core, para contrastar si la reformulación
  estocástica de la estacionalidad mejora la predicción fuera de muestra.

# 1. La raíz. Todo lo demás es relativo a ella.
raiz: .

# 2. Las series, POR REFERENCIA al registro. No se repite el nombre.
registro: empirical/data/registry.yaml
series: [ES_CPI, ES_CORE, DE_CPI, DE_CORE, FR_CPI, IT_CPI, NL_CPI, IE_CPI]
excluidas:
  FR_CORE: "UNUSABLE_FOR_SEASONALITY — ver la nota del registro"

# 3. Lo que es del CONJUNTO, declarado una vez.
policy:
  objetivo: univariante
  umbrales:
    ventana_episodio: 2          # el analista tiene que poder moverlo
  razon: |
    Univariante porque el ejercicio es de predicción por serie. Si pasara a
    multivariante habría que reestimarlas todas con el mismo tratamiento
    estacional.

# 4. Los EJES de variación, con nombre. Dejan de vivir en la primera letra.
ejes:
  muestra:
    m: {desde: 2002-01, hasta: 2019-12, razon: "pre-COVID y pre-crisis energética"}
    x: {desde: 2002-01, hasta: 2023-11}
    y: {desde: 2002-01, hasta: 2023-11}
  vintage:
    x: {fuente: "IPC.xlsx del usuario", estado: DEFECTUOSO,
        razon: "15 valores erróneos, 2022-09 a 2023-11"}
    y: {fuente: registro, descargado: 2026-07-01}
  carril: [guiado, autonomo]

# 5. El ROL de cada directorio. Es la pregunta que hoy no tiene respuesta.
directorios:
  empirical/cases:        {rol: autoritativo, razon: "el veredicto que va al paper"}
  empirical/sfmeg:        {rol: benchmark, califica_contra: empirical/cases}
  empirical/realizaciones: {rol: benchmark, califica_contra: empirical/cases}
  empirical/run9_DS:      {rol: archivo, nota: "réplica con otro LLM"}

# 6. El instrumento con que se hizo.
instrumento:
  fue: "1.14"
  fuf: "1.09"
  art: "0.2.1 @7d59ae1"
```

### Las cinco propiedades que lo justifican

1. **Una raíz declarada, y todo relativo a ella.** Mover un proyecto pasa a ser
   mover una carpeta. Hoy el 42% de las rutas están rotas y 109 son de otra
   máquina.
2. **Las series por referencia, no por cadena repetida.** Hoy el id de la serie
   está en cuatro sitios sin sincronizar: el registro, el nombre del CSV, el
   prefijo del fichero y el token interno del `.inp` — más un quinto,
   `Guion.series`, que en 25 de 125 casos dice `"serie"`.
3. **Lo del conjunto, una vez.** `objetivo` y los umbrales dejan de evaporarse,
   y el proyecto puede **afirmar** con qué criterio se hizo todo.
4. **Los ejes con nombre.** Muestra, vintage, carril y réplica pasan a ser
   dimensiones consultables. Entonces «¿qué hay que reestimar si el vintage `x`
   estaba mal?» es una consulta, no arqueología.
5. **El rol de cada directorio.** Es la pregunta que costó media hora de
   confusión en la reconstrucción: dos documentos con veredictos opuestos sobre
   la misma serie y la misma muestra, y la respuesta —cuál manda— en una
   plantilla de corrección que empieza diciendo *«no se enseña a los
   analistas»*.

### Lo que el manifiesto NO contiene

Ni datos, ni parámetros, ni el recorrido. Eso ya está en la terna y en los
guiones, y está bien. El manifiesto **apunta**. Es la decisión de la fase 4
—*contiene la identidad y las relaciones; apunta al contenido*— aplicada un
piso más arriba.

---

## 4. Los cambios en lo que ya hay

Pequeños, y cada uno cierra un agujero medido.

| cambio | por qué |
|---|---|
| **`schema_version` en el guion** | hoy no lo tiene, y la retrocompatibilidad se apoya en «no reescribas con un instrumento viejo» — regla que nada puede comprobar porque no hay nada que comparar |
| **`series_id` declarado**, resuelto contra el registro | hoy se deriva de dos maneras incompatibles, con *fallback* al literal `"serie"` |
| **`analyst` escribible** | el campo existe, se pinta en el HTML, y no tiene parámetro: 0 de 125 |
| **rutas relativas a la raíz del proyecto** | 42% rotas, 109 de Windows |
| **`guion_path` obligatorio** en `confirm_and_estimate` y `build_model` | el propio código argumenta *«lo que es opcional no se hace»* y luego lo deja opcional |
| **`batch_build` escribe guion** | es el único tool multi-serie y el único que no registra |
| **el `.out` estampa la versión del motor** | `fue` la imprime y no la guarda; conviven cuatro estados |
| **el registro gana `freq`, `units`, `vintage`, `window`** y `status` como enumerado | hoy todo eso vive en el texto libre de `note`, y `status` aparece en 1 de 12 entradas |
| **validación que falla ruidosamente** | hoy un typo en `source` produce «skipped» y la serie desaparece del run sin error |

Y uno que no es un campo sino una consecuencia: **la cola de reestimación deja
de ser un `print`.** Con las series por referencia y el vintage como eje, «estos
datos cambiaron ⇒ estos modelos están obsoletos» es una consulta sobre el
manifiesto. Hoy se calcula bien y se escribe a un log.

---

## 5. Quién lo escribe

**Los dos, con la regla de `guion_node`.** La herramienta que fabrica el
artefacto escribe su entrada, y la que no puede dar la razón no registra.

- **art-mcp** escribe las entradas de serie y de modelo, como ya hace con el
  guion. Lo que cambia es que las escribe **relativas a la raíz** y **referidas
  al registro**.
- **El GUI** escribe lo mismo cuando es él quien fabrica el `.inp`. Es la
  decisión P2 del plan —*el GUI en paralelo, sólo por ficheros*— y el
  manifiesto es exactamente el fichero por el que se encuentran.
- **La persona** escribe lo que sólo ella sabe: el rol de cada directorio, la
  razón del proyecto, los ejes y sus motivos. Eso **no** lo puede inferir una
  herramienta, y es el 25% que hoy está en la cabeza.

La prueba de que la regla funciona ya está medida: `rationale` es obligatoria y
está en el 95% de los nodos; `analyst` es opcional y está en el 0%.

---

## 6. Estado de la fase 5

**Cerrada.** Entregado este documento.

Lo que cambió respecto al plan:

- **El alcance del guion no es la serie: es el directorio**, y que coincida con
  una serie es costumbre, no mecanismo. Eso descarta la idea de «un guion más
  grande» y obliga al manifiesto.
- **El manifiesto sí, la base de datos no** — se confirma la lectura del plan, y
  por una razón nueva: todo lo que hay que arreglar es *identidad y referencia*,
  y eso se arregla mejor declarando que indexando.
- **Los ejes de variación son la pieza que faltaba en el plan.** Muestra,
  vintage, carril y réplica no son metadatos: son dimensiones, y hoy viven en la
  primera letra de un nombre de fichero.
- **El rol de los directorios también.** Sin él, dos documentos del mismo árbol
  dan veredictos opuestos sobre la misma serie y nada dice cuál manda.

Queda la **fase 2** (el editor, que esperaba al contrato) y la **fase 6** (la
interfaz madre y el GUI de drtran). Las cinco fases de estudio están hechas; lo
que sigue es diseño de programa.
