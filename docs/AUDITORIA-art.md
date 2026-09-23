# Qué de `art` tiene sentido traer al GUI

Auditoría pedida el 2026-09-24: *«un estudio de todas las funciones que tiene
art y cuáles se pueden importar al gui como funciones con gui. En todos los
casos serían funciones en las gui. No modificar los motores.»*

Contadas: **49 herramientas** en `art/mcp_server.py`, 30 717 líneas de Python
en `src/art/`. Dependencias reales: `numpy` (13 módulos), `scipy.stats` (2),
`matplotlib` (6) y `from fue import TimeSeries` — el enlace Python al motor.

---

## 1. El eje que decide, y no es «¿se puede escribir en C?»

Casi todo se puede escribir en C. La pregunta útil es otra: **de dónde saca
cada función lo que necesita.** Hay cuatro sitios, y el coste y el sentido
cambian por completo entre ellos.

| de dónde sale | ejemplo | coste | ¿toca el motor? |
|---|---|---|---|
| **A.** el motor ya lo escribe en el `.out` | pares de parámetros correlacionados | bajo | no |
| **B.** hay que correr el motor otra vez | contraste RV de un modelo anidado | bajo-medio | no |
| **C.** cálculo estadístico propio | ADF, KPSS, MEG | **alto** | no |
| **D.** el guion y el protocolo | `guion_*`, `guided_*` | — | — |

**El grupo D no va al GUI**, y no por coste: es la **otra encarnación**.
`DISENO-madre.md` §10 decidió que ATSW GUI y ATSW Python gestionan los
proyectos *de forma distinta*. El equivalente del `guion.json` en el GUI ya
existe y es el **manifiesto**: linaje obligatorio, razón opcional, muestras,
elegido. Traer el guion sería reimplementar la otra encarnación dentro de la
ventana — y entonces habría dos registros del mismo análisis.

### El límite honesto de «no tocar los motores»

art no lee el `.out`: usa el **enlace Python**, y de ahí saca los parámetros y
la matriz de covarianzas con toda su precisión. El `.out` imprime con seis
decimales los parámetros y **dos** las correlaciones:

```
     -0.001610  (0.000683) [ 1]
Correlations greater than or equal to 0.7 in absolute value:
```

Para **juzgar** —¿son redundantes estos dos parámetros?, ¿es este residuo
noticia?— sobra. Para **encadenar** un cálculo sobre esos números, puede no
llegar. Eso acota el grupo C más que la dificultad de programarlo: hay cuentas
que en C sólo se podrían hacer bien rehaciéndolas desde los datos, no
leyéndolas del informe.

---

## 2. Lo que ya está en el GUI (no importar)

| art | dónde está ya |
|---|---|
| `create_inp`, `load_data`, `preview_data` | `lib/datos` + `lib/xlsx` + `atsw_genera_inp` |
| `get_out_report` | la pestaña «Output» del editor del `.inp` |
| `estimate_and_diagnose` | `fue_gui` |
| `generate_forecast`, `update_and_forecast` | la pestaña de previsión (`fue -f` + `fuf`) |
| `series_info` | la rejilla y el globo de la serie |
| `model_histogram` | el `.out` lo trae, y `fue` dibuja el `A<nombre>.eps` |
| `compare_versions` (la parte de comparar) | la rejilla: estructura, d.t., Q, p, en columnas |
| `record_version` | el manifiesto, y es automático |

---

## 3. Lo que traería YA, por orden (grupos A y B)

### 3.1 El motor ya contestó y nadie lo lee — grupo A

1. **`overparameterization_analysis`.** El `.out` trae la matriz de
   correlaciones de los parámetros **y la lista de pares con |r| ≥ 0.7 ya
   calculada**. Hoy nadie la mira. Es leer y enseñar: un verdicto en la
   rejilla o en el editor. *Coste: horas.*

2. **`residual_outlier_scan` / `preliminary_outlier_scan`.** El `.out` lista
   los residuos extremos con su fecha y su valor tipificado:
   `| 124  5/2012  -2.15 |`. Falta el umbral que decide cuándo es **noticia** —
   art lo hace depender de *n*, que es lo correcto: bajo especificación
   correcta el máximo de *n* normales crece con *n*. Eso son diez líneas.
   *Coste: horas.*

3. **`model_equation_display`.** La ecuación del modelo estimado en forma de
   operadores. **`lib/equation/eqtran.c` ya lo hace para transferencias**
   (drtran); para el univariante no hay equivalente, y los operadores están en
   el `.out`. Es el hermano pequeño de algo que ya existe. *Coste: un día.*

### 3.2 Correr el motor otra vez y comparar — grupo B

4. **`verify_optimum`.** Corre `fue` sobre un `.pre` y comprueba que **los
   números no se mueven**. Es literalmente el invariante del contrato, y el
   taller no tiene forma de comprobarlo desde la ventana. Con `lib/engine` y
   `lib/outfile` es: correr, leer los dos `.out`, comparar la tabla de
   parámetros. *Coste: un día.* **Y es el que yo pondría primero de todos**,
   porque verifica una promesa que hoy se cree sin comprobar.

5. **`extend_sample`.** El mismo modelo con más observaciones. Con `datos.csv`
   como dueño del dato y `atsw_genera_inp` con ventana, esto es casi gratis —
   y es **la pareja natural de las submuestras**: estimas en `pre-covid`,
   extiendes, y ves si el modelo aguanta. *Coste: un día.*

6. **`test_seasonal_simplification`** y el contraste RV anidado de
   **`compare_versions`.** Estimar el modelo restringido y comparar
   log-verosimilitudes. El `.out` da la logl; el `.inp` restringido lo escribe
   el editor que ya tenemos. Lo único nuevo es la chi-cuadrado, **que ya está
   en `lib/outfile` (`chisq_cola`)**. *Coste: dos días.*

7. **`suggest_intervention_form`.** Añadir un impulso, escalón o rampa al
   `.inp`, reestimar y enseñar la diagnosis. El editor del `.inp` ya hace dos
   de las tres cosas; falta el diálogo que escribe la intervención en su
   sitio. *Coste: dos o tres días.* Alto valor: la intervención es donde más
   se pelea a mano con el fichero.

---

## 4. Lo que se puede pero hay que decidir antes — grupo C

`unit_root_analysis` (ADF + KPSS), `seasonal_analysis` (F de HAC),
`formal_tests` (MEG / DCD_f / Shin-Fuller, 1 871 líneas), `ar_factorization`
(raíces del operador AR), `meg_frequency`, `meg_reformulate`,
`seasonal_param_analysis`.

**Se puede**: GSL ya es dependencia de `drtran_gui` —de ahí salen los
autovalores de `nlatools`— y trae distribuciones y raíces de polinomios
(`gsl_poly_complex_solve`). `ar_factorization` sería de los primeros.

**Pero hay una pregunta de método antes que de coste**, y no es mía: la
identificación de esta escuela es **visual** —la ACF, la PACF, la media contra
la desviación típica, que es lo que fug enseña—. ADF y KPSS son otra
tradición. Traerlos al GUI no es sólo trabajo: **cambia lo que el GUI enseña**,
y el GUI es, por la decisión P4, el que se usa para educación y para el
analista que quiere control total del proceso.

Mi recomendación: **`ar_factorization` sí** (es leer el operador y sacarle las
raíces: dice si hay un AR estacional escondido, y eso es de la escuela). El
resto, **sólo si decides que el GUI también los enseña.**

---

## 5. Lo que no va al GUI — grupo D

`guion_map`, `guion_node`, `guion_diff`, `guion_abandon`, `guion_evidencia`,
`export_guion`, `guided_identification`, `guided_intervention`, `build_model`,
`batch_build`, `full_report`, `save_identification_report`, `sps_dashboard`,
y los recursos `_r_*`.

Tres razones distintas:

- **El guion es de la otra encarnación** (P4). El GUI ya tiene su registro: el
  manifiesto.
- **Los `guided_*` son un protocolo conversacional**: «un nodo de decisión por
  llamada». Eso es un LLM llevando al analista de la mano. En una ventana, el
  equivalente ya existe y es la ventana: los botones *son* los nodos.
- **`build_model` y `batch_build` son un auto-ARIMA.** El propio art lo marca:
  *«no es el modo autónomo… lo peor de los dos»*. Un GUI de control total no
  debería tener un botón que decide por ti.

`full_report` en HTML lo dejaría fuera por otra razón: el motor **ya escribe**
el `.tex` y el `.pdf`, y `lib/tabla` ya da CSV, TXT y TeX. Un tercer formato
con otro contenido es una tercera versión de la verdad.

---

## 6. Resumen

| grupo | cuántas | veredicto |
|---|---|---|
| ya está en el GUI | 8 | no tocar |
| **A — el `.out` ya lo dice** | 3 | **traer ya**, horas cada una |
| **B — correr y comparar** | 4 | **traer**, uno a tres días cada una |
| C — estadística propia | 7 | `ar_factorization` sí; el resto, decisión de método |
| D — guion, protocolo y automático | 14 | **no van al GUI** |
| instrumentos sueltos del nodo de intervención | 6 | con el 7 (`suggest_intervention_form`) |
| auxiliares internos | 7 | no son herramientas |

**Por dónde empezaría**: `verify_optimum` (comprueba una promesa que hoy se
cree), y después los tres del grupo A, que son horas y están esperando en un
fichero que ya leemos.

Nada de esto toca los motores: o el `.out` ya lo trae, o se corre el motor otra
vez con un `.inp` distinto — que es exactamente lo que el taller sabe hacer.
