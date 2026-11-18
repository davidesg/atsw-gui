# Lo que art-python ya aprendió: qué llevar al asistente de ATSW

Estudio del 18-nov-2026, a petición del analista:

> *«Hay muchísimo trabajo de bugs y problemas que ya se ha resuelto en
> art-python y que puede ser útil y ahorrar trabajo para el asistente.»*

Es un estudio **sin código**: inventario, lecciones y prioridades. Continúa
`ESTUDIO-terminal-mcp.md` (sobre todo su §2.2, el inventario de qué va a C y
qué queda en Python) y no reabre ninguna de sus decisiones. Donde aquí se
dice «art BUG-NNNN» es un parte de `art-python/bugs/`; los de este
repositorio se citan sin el prefijo.

**Cómo se ha comprobado.** Tres niveles, y cada afirmación dice el suyo:

- **ejecutado**: con los binarios del paquete (`atsw-cli` y los motores) sobre
  una copia de `docs/ejemplos/jenkins_alavi`, fuera del repositorio;
- **en el código**: leído en `gui/atsw-cli/src/main.c`, `lib/` o
  `mcp/atsw_mcp/src/atsw_mcp/`;
- **no comprobado**: inferido, y dicho así.

**Lo hecho mientras se escribía** (18-nov-2026), sobre la prioridad 1:

- #191, BUG-0001 de atsw-cli: `especifica` rechaza una intervención igual a
  una que el `.inp` ya trae, y una de otra forma en la misma fecha entra con
  el aviso de art BUG-0162. La ayuda de `write_inp` dice que se añaden.
- #192: `suggest_intervention_form` de **art**, con su `rehacer`, por el
  extra Python; art construye la forma en un temporal y el hijo lo estima
  `fue` en C.

Queda abierta una tensión que este estudio señala y que #192 no resuelve: la
madre ya tiene su propia «Sugerir intervención…» en C (`an_sugerir`,
`lib/intervencion`), y donde C y art difieren manda la madre (§5). Ver la
pregunta 2 de §6.

---

## 0. El veredicto, primero

1. **El asistente de ATSW es pequeño a propósito, y está bien que lo sea.**
   `atsw-us` tiene 18 herramientas propias (más `version` y `estado`, comunes
   a los tres servidores) frente a las 51 de art. Lo que le falta no es
   volumen: son **unas pocas piezas que art tuvo que inventar después de
   romperse**, y que el asistente va a necesitar en cuanto se use en serio.
2. **La mayoría de las lecciones caras ya están en C.** Los contrastes
   formales de `lib/formales` citan en su código los partes de art que
   heredaron (art BUG-0025, 0065, 0167, 0235, 0236, 0243…); el `fue` en C
   saca los errores típicos con `fdhess` y no con la semilla del BFGS, con
   lo que toda una familia de partes de art (BUG-0027, 0041, 0060, 0124, 0151,
   0168, 0174, 0202) no se reproduce aquí salvo en los casos de reserva. Lo
   que **no** está es la capa de arriba: cómo se le dice al LLM lo que hay, y
   qué operaciones puede hacer sin equivocarse.
3. **El caso que motivó el estudio se reproduce y su arreglo ya está en C,
   en otra puerta.** `write_inp` (`atsw-cli especifica`) añade
   intervenciones encima de las que el hijo hereda, sin aviso (§1.2,
   ejecutado). La ventana «Sugerir intervención…» de la madre ya rechaza una
   intervención en la misma fecha (`lib/analisis/an_sugerir.c`, por art
   BUG-0162), y `lib/inpdet` ya sabe quitar un determinista
   (`id_quita_det`). Falta llevarlo a `especifica`.
4. **Las tres faltas que más riesgo llevan** son: modificar una intervención
   (no solo añadir), referirse a un suceso **por su fecha** y no por un
   índice de residuos, y poder **construir lo que las herramientas
   recomiendan** (el peldaño 2 de la escalera, la reformulación tras el MEG).
   Las tres tienen la pieza en C y cuestan poco (§4).
5. **Hay dos defectos de rótulo en la Q** que art ya cometió y arregló
   (art BUG-0195, 0213) y que la madre comparte (§3.6, ejecutado y en el
   código). Son de `lib/`, no del motor, y se abren como partes aquí con su
   repro.
6. **Lo que no se debe portar** es lo que art decidió a su modo y la madre a
   otro: donde los dos calculan distinto manda la regla de la madre, y el
   veredicto encadenado de los contrastes se quedó fuera de C a propósito
   (§5).

---

## 1. Por qué este estudio

### 1.1 Los dos tamaños

| | art-python | atsw-us |
|---|---|---|
| herramientas MCP | 51 (`src/art/mcp_server.py`) | 18 propias + 2 comunes |
| partes de defecto | art BUG-0001 … BUG-0253, cinco abiertos (0116, 0186, 0237, 0240, 0253) | los del repositorio, sin uno propio del asistente todavía |
| motor | `fue` en Python (porte) | `fue`, `fuf`, `fug` y `art` en C, por `atsw-cli` |
| registro del recorrido | `guion.json`, con nodos | el manifiesto del proyecto: modelos, padre, razón, `por` |
| lo que no tiene C | — | `boxcox_analysis` e `intervention_ladder`, del extra `[python]`, que llama a la herramienta de art tal cual en un proceso hijo (`extra.py`) |

El extra fija `art-tseries==0.2.3` (`mcp/atsw_mcp/pyproject.toml`). Según el
`fixed_in` de cada parte, ninguno de los cerrados (hasta BUG-0252) se cerró
después de la 0.2.3, así que lo que el extra llama hoy lleva ya esos arreglos.

### 1.2 El caso que lo motivó

Un LLM, trabajando con `atsw-us`, creyó que `write_inp(interventions=[…])`
**sustituía** las intervenciones del modelo; las **añade** a las que el hijo
trae de su padre. Quedó un escalón duplicado, el modelo casi singular y el
hessiano no definido positivo.

**Se reproduce** (ejecutado, sobre la copia del ejemplo, serie anual del
visón): `new_version` de `MINK/m01`, `write_inp(interventions=["impulse
1870"])`, estimar → `m02`; hijo de `m02`, `write_inp(interventions=["step
1870"])`, estimar → `m03`. `atsw-cli especifica` contesta «1 intervención. El
.inp queda con ARMA (4,0)(0,0); fue lo acepta» y el `.inp` lleva las dos.
`m03` estima, la diagnosis dice «mirar», el ω del escalón sale con t = 0,37
y el único aviso es «step 1870 y μ van juntos (r = −0,83)». **Nada dice que
el suceso está contado dos veces.** Es, palabra por palabra, el síntoma que
describe art BUG-0162: «el síntoma es un ω no significativo y una covarianza
casi singular, no un error».

Lo que art hizo (art BUG-0162, cerrado en la 0.2.1):

- `rehacer=True` en `suggest_intervention_form` y `guided_intervention`:
  retira la intervención del mismo suceso (`escalera.hereda_del_base`, con
  una ventana de `max(1, n_omega)` períodos), pone la nueva y **dice cuál
  retiró**; si no había nada que retirar, también lo dice;
- sin `rehacer`, si ya hay una intervención en la ventana del suceso,
  **avisa** de que se ha añadido encima, y explica el síntoma; no lo prohíbe,
  porque pueden ser dos sucesos.

Lo que hay en C (en el código):

- `lib/analisis/an_sugerir.c`, la ventana «Sugerir intervención…» de la
  madre, **rechaza** una intervención cuya fecha ya está en el fichero y dice
  cuál («ya lleva … en esa misma fecha»), citando art BUG-0162
  (`DISENO-intervencion.md` §3);
- `lib/inpdet` tiene `id_intervenciones` (las que trae el fichero) e
  `id_quita_det` (quitar un determinista por su línea), que hoy usa
  `lib/formales`;
- **`atsw-cli especifica` no usa ninguna de las dos**: llama a `id_anade`
  directamente.

Es el patrón que art llama «la capacidad está abajo, el nodo no la nombra»
(art BUG-0090, 0162): la pieza existe y la puerta del asistente no pasa por
ella. Es el tipo de hallazgo que este estudio sistematiza.

### 1.3 Lo que art aprendió en general

Tres conclusiones de art que sirven tal cual, porque no son de Python sino de
**hablarle a un LLM a través de herramientas**:

1. **«El motor está sano; lo que falla es el acto de habla.»** Al cerrar 25
   partes seguidos (art BUG-0040 a 0070), ninguno era un estadístico mal
   calculado: unos doce eran presentación que contradice al contenido, cinco
   corrupción silenciosa al cruzar una frontera (un campo que se cae, un
   `len()` que cuenta lo fijado), y el resto capacidades ausentes
   (`art-python/docs/DISENO-lecciones-de-la-replica.md` §1). En el asistente
   de ATSW la estadística es la de la madre, ya cotejada; **el riesgo está en
   la capa delgada**, que es justo la nueva.
2. **«Una herramienta que hay que llamar aparte no existe.»** La
   sobreparametrización de art tuvo cero llamadas hasta que su aviso salió
   dentro de la diagnosis; la marca de un error típico inválido **en su
   sitio** se usó en cuanto existió (mismo documento, §3). Lo esencial va en
   la salida de lo que ya se llama, y lo que no se puede leer se marca donde
   está.
3. **Una conclusión atada a la condición equivocada** (art BUG-0056, 0069,
   0070): arreglar un contraste apagaba el aviso de otro porque el aviso vivía
   dentro de un `if` que no era el suyo. Es lo que hay que mirar en cada
   salida compuesta de `atsw-cli`.

---

## 2. Inventario: art → atsw-us

**La regla del repositorio** (decisión del 15-nov-2026, `docs/TODO.md`): las
herramientas de atsw llevan el nombre y los argumentos de su pareja en art,
drtran o sima, y sólo se inventa un nombre cuando no hay pareja. Los
argumentos de ruta (`inp_path`) pasan a ser nombres del proyecto (`series`,
`model`, `sample`): eso es la adaptación al proyecto, no un desajuste.

### 2.1 Lo que atsw-us tiene hoy

| atsw-us | pareja | por dónde | notas |
|---|---|---|---|
| `guion_map` | art `guion_map` | C (`proyecto ver`) | da modelos, padre y razón; **no da la especificación de cada modelo** (§3.1) |
| `identification_analysis` | art | C (`identifica`, el `art` en C) | `transform «λ,d,D»` y `residuals_of` en lugar de `d, D, lam`; sobre residuos dice «lo que les falta» |
| `new_version` | sin pareja directa (art `record_version` / `confirm_and_estimate`) | C (`proyecto nuevo` / `itera`) | razón obligatoria |
| `write_inp` | drtran `write_inp` | C (`especifica`, `lib/inpdet`) | `arma` sustituye; `add_arma` multiplica; `interventions` **añade**; armónicos y Semana Santa sin repetir los que ya hay |
| `estimate_and_diagnose` | art | C (`estima`) | sólo sobre lo no estimado: «lo estimado no se reescribe» |
| `model_equation_display` | art | C (`ecuacion`) | el bloque del `.out` tal cual, con t; no la ecuación en dos líneas de art |
| `diagnose` | drtran `diagnose` | C (`dictamen`, el de la madre y la ficha) | incluye los pares con \|r\| ≥ 0,7 (la sobreparametrización de art) |
| `formal_tests` | art | C (`formales`, `lib/formales`) | `family` y `underdifferencing` en lugar de `run_meg`, `subdiferenciacion`, `modelo_d_menos_1` |
| `residual_outlier_scan` | art | C (`anomalos`, regla de la madre) | `episodes` en lugar de `omitir`, `threshold`, `motivo` |
| `boxcox_analysis` | art | extra Python | la misma herramienta de art |
| `intervention_ladder` | art | extra Python | la misma, con `at` como índice de residuos (§3.2) |
| `ar_factorization` | art | C (`raices factores`) | |
| `factorized_model` | sin pareja (lo que art BUG-0103 pedía y art no tenía) | C (`raices factorizado`) | |
| `full_report` | art (otro contenido) | C (`ficha`) | la ficha de confianza, no el informe de art |
| `generate_forecast` | art | C (`prever`: `fue -f` y `fuf`) | **sin `horizon`**: el horizonte es el del fichero |
| `guion_annotate` | art | C (`proyecto razon`) | sólo la razón; art tiene `campo` |
| `guion_adopt` | art | C (`proyecto elige`) | |
| `plot` | sin pareja en art (drtran tiene `plot_*`) | `atsw_gui --ver` | abre la ventana de la madre y devuelve en texto lo que enseña |

### 2.2 Lo que falta de art, y cómo vendría

«C existe» quiere decir que la pieza está en `lib/` y la usa una ventana de la
madre, pero `atsw-cli` no la expone. Coste: **bajo** (una suborden sobre lo
que hay), **medio** (orquestación nueva en C o en la capa), **alto** (análisis
nuevo).

| herramienta de art | en atsw-us | cómo vendría | coste | prioridad |
|---|---|---|---|---|
| `suggest_intervention_form` (con `rehacer`, `n_omega`) | no | **C existe**: la ventana «Sugerir intervención…» (`an_sugerir`, `lib/intervencion`, el peldaño 2 de `DISENO-intervencion.md` §2 bis); por la regla de la madre, su forma y no la de art | bajo-medio | **alta** (§3.1) |
| `meg_reformulate` | no | **C existe**: `fm_reformula` (`lib/formales`), el botón «Reformular como estocástica…» | bajo | **alta** (§3.4) |
| `compare_versions` | no | C, leyendo los dos `.out` (`lib/outfile` ya lee ℓ, parámetros y observaciones); sin reestimar (art BUG-0158) | medio | media |
| `test_interventions` (simplificar) | en parte: `diagnose` da la ganancia de un escalón con varios ω | C: lo que falta es proponer la simplificación (art BUG-0123) | bajo | media |
| `verify_optimum` | no | Python sobre C (perturbar y reestimar con `fue`) | medio | baja (el `fue` en C ya da `fdhess`, §3.5) |
| `get_out_report` | en parte (`ecuacion`, `dictamen`) | C; con modo resumido (art BUG-0222) | bajo | baja |
| `extend_sample` | no | la madre tiene muestras; extender es otra operación (art BUG-0173) | medio | baja |
| `update_and_forecast` | no | C (`fuf`) | medio | baja |
| `incident_configurations`, `residual_episodes` | en parte (`anomalos` agrupa episodios) | extra Python, o la regla de la madre | medio | baja |
| `unit_root_analysis` (la tabla d = 0, 1, 2) | en parte: `identification_analysis` da ADF/KPSS del `d` pedido | C (`lib/raices`); el `raices adf` del plan | bajo | baja |
| `seasonal_analysis`, `seasonal_param_analysis` | en parte (la F en `identification_analysis`; la simplificación en `formal_tests`) | C a comprobar (§2.2 del estudio del MCP) | medio | baja |
| `preliminary_outlier_scan` | en parte (`anomalos` sobre un modelo base sin ARMA es lo mismo) | — | — | — |
| `model_histogram` | en parte (`plot(what="histogram")` es de la serie) | `fug -d` | bajo | baja |
| `guion_node`, `guion_abandon`, `guion_diff`, `guion_evidencia`, `export_guion` | no | el manifiesto no tiene nodos (§3.9); `export_guion` sale por P4 | — | a decidir |
| `guided_identification`, `guided_intervention` | no, a propósito | el protocolo vive en las instrucciones | — | — |
| `build_model`, `batch_build` | no, a propósito | decisión 14 del estudio del MCP | — | — |
| `load_data`, `create_inp`, `preview_data`, `series_info` | no, a propósito | los datos entran por la madre (m00) | — | — |
| `save_identification_report`, `sps_dashboard`, `intervention_plot` | no | informes y figuras; la madre tiene sus ventanas | — | baja |

### 2.3 Desajustes de nombre y argumentos

Frente a la regla, y sin contar el paso de rutas a nombres:

- **`generate_forecast` no tiene `horizon`**, que en art y en drtran sí
  existe. Hoy el horizonte es el del `.pre` (24 en el ejemplo, ejecutado).
- **`formal_tests`**: `family="integration"|"seasonality"` y
  `underdifferencing` no son los argumentos de art (`run_meg`,
  `subdiferenciacion`, `modelo_d_menos_1`). Es una decisión defendible —la
  familia agrupa mejor—, pero rompe la regla; o se documenta como excepción o
  se alinea.
- **`residual_outlier_scan`**: `episodes` frente a `omitir` y `threshold` de
  art. El umbral de la madre está calibrado por *n* (2,87 con 62 residuos,
  ejecutado), y por la regla de la madre no se expone; `omitir` sí podría
  llamarse así.
- **`intervention_ladder`**: los argumentos son los de art, incluido `at`
  como índice de residuos; es el desajuste con más riesgo, y no es de nombre
  (§3.2).
- `diagnose`, `write_inp` y `plot` toman el nombre de drtran, que es la
  pareja más cercana: correcto por la regla.

### 2.4 atsw-mt y atsw-sis

Sólo lo claro. `atsw-mt` tiene cinco herramientas (`guion_map`,
`identify_link`, `load_pre`, `estimate`, `open_case`) frente a las veintiuna
de drtran; `atsw-sis`, doce frente a las cuarenta y cuatro de sima. drtran y
sima llevan sus propios registros de defectos, que este estudio **no ha
recorrido**. De art valen para los dos las lecciones transversales de §3.10 a
§3.12 (enumeraciones en el esquema, tablas del motor tal cual, fechas y no
índices, errores que no se tragan). Un estudio como éste sobre drtran y sima
tiene sentido cuando `atsw-mt` y `atsw-sis` crezcan, con el hito de cada
uno.

---

## 3. Las lecciones de art, por tema

En cada tabla: el parte de art, qué pasó, cómo está atsw-us hoy y qué haría
falta. «Hoy» lleva su nivel de comprobación.

### 3.1 Intervenciones

| art BUG | qué pasó en art | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0162 | todos los constructores hacían *append*; reformular dependía de acordarse del `.pre` anterior | **reproducido** (§1.2, ejecutado) | en `especifica`: el control de `an_sugerir` (misma fecha) y, mejor, el de art (una ventana); un `--quita "step 10 2008"` sobre `id_quita_det`; y en `write_inp` un `rehacer` o un `remove_interventions` |
| 0102 | el registro decía menos intervenciones de las que tenía el fichero | `guion_map` no dice qué lleva cada modelo (ejecutado): el LLM no ve lo que el hijo hereda sin pedir la ecuación | que `guion_map` dé la estructura y los deterministas de cada modelo (`fueout_estructura` e `id_nombres` ya existen) |
| 0079, 0093, 0204 | la forma que la escalera recomendaba (un escalón con varios ω) no se podía construir desde la superficie | **no se puede** desde `write_inp`: `especifica` llama a `id_anade` con `nomega = NULL`, todo de un ω (en el código) | aceptar la cuenta de ω (la ventana de la madre ya lo hace con `nesc_de`); sin ello `intervention_ladder` puede recomendar un peldaño 2 que nadie construye |
| 0161, 0163 | el denominador δ estaba en todo el motor y ninguna superficie lo construía; con δ no se publicaba la ganancia | `id_anade` escribe «sin denominador» (en el código) | sólo si se decide el peldaño 3 (`DISENO-escalera.md` §6: «el tercer peldaño no existe») |
| 0086 | la escalera arbitraba escalón contra impulso por AIC | lo hace art en el extra, ya arreglado; las instrucciones lo dicen («el AIC no arbitra») | nada |
| 0087, 0096 | la regla de Treadway con el umbral de los anómalos (3) en vez de 2; vecinos activos ignorados | en el extra, arreglado (`umbral_vecino = 0` → 2,0); en C, la comprobación de Treadway sobre el hijo está **pendiente** (`DISENO-intervencion.md` §5, paso 5) | portar ese paso: es `lib/outfile` + `lib/anomalos`, y es lo que diría que la forma resolvió el suceso |
| 0150, 0249 | la escalera se estimaba sin las intervenciones del base; la calibración sobre un modelo que ya tiene órdenes no ve si los fabricó el anómalo | el nodo 7 de las instrucciones calibra sobre el base **sin ARMA** (en el código): bien. Después de los órdenes, `residual_outlier_scan` sobre un modelo con ARMA tiene el ciego de art BUG-0249 | una línea en las instrucciones: con ARMA, los órdenes pueden estar fabricados; calibrar sobre el base |
| 0076, 0136 | la ganancia se leía como permanente con un impulso, donde es cero por construcción | `dictamen` sólo pregunta la ganancia a un escalón con varios ω (ejecutado: «no aplica» con un ω) | nada |
| 0066 | la función de transferencia con el signo crudo, cuando `fue` resta | `ecuacion` dice el convenio («los operadores de retardo RESTAN») (ejecutado) | nada |
| 0182 | art ofrecía la rampa como una forma más; en autónomo fijó para siempre una tendencia | `especifica` acepta `ramp` (`pide` en `lib/inpdet`, en el código); sólo la instrucción del carril autónomo la prohíbe | rechazarla (o avisar) cuando la pide el asistente, y poner la prohibición en la descripción de `write_inp`, no sólo en el autónomo |
| 0123 | «no hay simplificación posible» cuando un escalón con ganancia nula es un impulso de un orden menos | no hay simplificación de intervenciones | proponerla en `diagnose`, junto a la ganancia que ya da |
| 0201 | la escalera informaba un AIC y un ω que no eran los del ajuste final, con el signo cambiado | en el extra, arreglado | nada; pero la forma la construye otro (`write_inp`) y se estima en C: el ω final es el del `.out` en C, no el de la escalera en Python (§5) |

### 3.2 Fechas e índices: el desfase de los residuos

Es la familia que más veces volvió en art: **seis sitios** distintos
convertían un índice de residuos en una fecha sin el desfase de la
diferenciación.

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0030, 0067 | la intervención autónoma caía `d + D·s` períodos antes | en C los anómalos salen **con fecha** (ejecutado: «1870»), del `.out` | nada en `anomalos` |
| 0172, 0185 | el desfase no contaba las raíces estacionales sueltas (`ifadf`) | no comprobado en C con un modelo reformulado tras el MEG | una prueba con un modelo con `ifadf` |
| 0218 | los anómalos iban sin fecha, con un número de observación que cambia con `d` | resuelto en C por construcción | nada |
| 0140, 0144 | las figuras decían «observación 61» con el calendario en la mano | la madre dibuja con fechas | nada |
| — | **`intervention_ladder(at=…)`** pide la observación «1-based, en los residuos» | ni `anomalos` ni su JSON dan ese índice; sólo la fecha (ejecutado) | **que `at` sea una fecha** («10 2008», como en `write_inp`) y la capa la traduzca con las fechas de los residuos del `.out` (`res_fecha`, que `lib/outfile` ya lee). Pedirle al LLM la cuenta es reabrir art BUG-0030 en la puerta nueva |
| 0171 | la fecha declarada se comparaba como texto («03/2022» frente a «3/2022») | las fechas de `write_inp` son «periodo año» y las juzga `fue` | si se añade una comparación de fechas (el control de §3.1), compararlas como números |

### 3.3 Identificación y orden de integración

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0016, 0023, 0056, 0197, 0210, 0226, 0227 | la tabla ADF/KPSS hablaba con voz de política («usa d = 2»), saltaba dos diferencias, o daba dos recomendaciones de `d` en la misma salida | `identification_analysis` sólo da ADF y KPSS del `d` pedido, sin recomendar (ejecutado); las instrucciones: «ADF y KPSS apoyan, no deciden» | nada; no portar la recomendación de `d` |
| 0052, 0057 | sobre residuos la lista dice un **incremento**, y la llamada que se imprimía **sustituía** el ARMA; luego se contaban operadores fijados | `identifica --residuos` dice «lo que les falta»; el nodo 8 manda `write_inp arma=…`, que **sustituye** (en el código). Con un base sin ARMA da igual; en una segunda vuelta, no | que la instrucción del nodo 8 diga: sobre un modelo con ARMA, `add_arma` (o el total); y que `--arma 0,0,0,0` quita el AR, que sí lo hace (ejecutado; art BUG-0250 no se reproduce) |
| 0044, 0048 | el ruido blanco no entraba en la papeleta, y luego entraba sin filtro | el `art` en C lo propone (ejecutado: «white noise», primero, sobre los residuos de `MINK/m01`) | nada |
| 0095, 0049 | candidatos dispersos «sólo en B^k», con la etiqueta del completo | las instrucciones lo prohíben («nunca se capa un AR…»); si el `art` en C los propone, **no comprobado** | comprobarlo |
| 0192, 0194, 0198, 0238, 0240 | el identificador no ve el airline, un AR(6), el ciclo de la rata almizclera; y art BUG-0240 (abierto) mide que **el `art` en C** acierta el 18 % de un ARMA(1,1) señal más ruido frente al 64 % de art-python | el `art` en C, sobre ∇ ln de la rata almizclera, propone MA(2), AR(2), AR(3)…; ni el AR(6) del ejemplo (ejecutado) | nada en el asistente: es el identificador. Las instrucciones ya dicen que la propuesta es evidencia, no orden; conviene que digan también que un AR alto se mira en la FAP aunque no esté en la lista |
| 0228, 0233, 0239 | el ΔAICc de una identificación ligera ordenaba candidatos; empates declarados que no eran los de la ficha | la salida da similitud, peso y AICc por candidato (ejecutado); el empate lo resuelven las instrucciones estimando | nada |
| 0002 | KPSS pasaba por encima de un ADF que rechaza con fuerza | no hay decisión de `d` en la herramienta | nada |

### 3.4 Estacionalidad y modelos MEG

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0035, 0053, 0034 | `meg_reformulate` no escribía el `.inp`, no entraba en el linaje, y lo encadenado encima perdía el testigo de frecuencia fija | **no hay `meg_reformulate`**. `write_inp` no sabe poner un factor suelto de la diferencia anual (`ifadf`); sólo `transform «λ,d,D»` con D = 1 entero | exponer `fm_reformula` como suborden, con el hijo en el linaje (`pr_deriva`), como hace el botón de la madre. Sin ella, un MEG que clasifica una frecuencia como estocástica no tiene camino, y el LLM puede tomar la diferencia anual entera |
| 0019, 0010 | se recomendaba podar armónicos antes de que existiera el modelo; podar uno anulaba todo el barrido del MEG | las instrucciones: «el MEG va antes de podar armónicos» (en el código); `atsw-us` no tiene herramienta de podar | nada |
| 0005 | el armónico de Nyquist entraba con `n_harmonics = 0` | `--armonicos` sólo añade lo pedido; `alter` sólo con «todos» y `s` par (en el código) | nada |
| 0097, 0170 | Semana Santa existía en el motor y no en la superficie; al encadenar se ignoraba | `write_inp(easter=True)` sobre cualquier hijo, y no repite el que ya hay (en el código) | nada |
| 0196, 0223 | la estacionalidad se contrastaba en datos anuales (una F con 0 g.l.) | en anual no sale la F (ejecutado) | nada |
| 0054 | la alarma de estacionalidad residual se leía sobre residuos que no son ruido blanco | `identifica --residuos` puede dar la F sobre los residuos; con datos mensuales, **no comprobado** | si la da, condicionarla a la diagnosis, o decirlo |
| 0206 | la F HAC rechazaba de más | el C usa la F HAC con el arreglo de art (`engines/art/src/seasonal_detection.c`, en el código) | nada |
| 0209 | tras decidir «sin estacionalidad» se proponían candidatos estacionales | no comprobado | comprobarlo con una serie desestacionalizada |
| 0183, 0186 | el objetivo viajaba como texto libre; el LLM reformulaba las tres opciones y perdía «multivariante» (0186, **abierto**: el protocolo solo no basta) | «ninguna herramienta conoce el objetivo: la regla la llevas tú» (instrucciones) | es exactamente la situación que art BUG-0186 dice que no se sostiene: que el objetivo se apunte (p. ej. en la razón del primer modelo o como campo del manifiesto) y que lo pida una herramienta, no sólo la prosa |

### 3.5 Sobreparametrización, errores típicos y el hessiano

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0027, 0041, 0060, 0124, 0151, 0202 | los errores típicos eran la semilla del BFGS (total o en parte), y se imprimían como válidos | el `fue` en C los saca con `fdhess` y dice de dónde salen («Standard errors: fdhess», ejecutado); la reserva es el BFGS cuando el hessiano no es definido positivo o el óptimo está en la frontera (`drvmlest.c`, en el código) | **marcarlo en su sitio**: `ecuacion` copia la línea, pero `dictamen` no la mira (en el código). Un «bfgs (fdhess: the Hessian is not positive definite)» debería poner «mirar» en la Estimación, con su porqué: es el síntoma del caso de §1.2 |
| 0168 | el aviso de las SE publicaba una magnitud de sesgo que nadie calcula, y el LLM la repetía | no hay tal aviso | no inventarlo |
| 0174 | el asistente reestimó desde semillas a cero «para arreglar las SE» | `write_inp arma=…` escribe el ARMA «desde cero» (salida de `especifica`) | una frase en las instrucciones: no se reestima desde cero para verificar; si hace falta, `verify_optimum` (§2.2) |
| 0061 | la covarianza leída de un `.pre` perdía pares enteros | `dictamen` lee el `.out` (en el código) | nada |
| 0230 | «adoptar este modelo» para modelos sobreparametrizados | `dictamen` pone «mirar» con \|t\| < 2 y con pares \|r\| ≥ 0,7, y dice cuál (ejecutado) | nada |
| 0248 | avisos que no correspondían al modelo («normal en un AR(2) complejo» sobre un ARMA(1,1)) | el texto de los pares es neutro («puede sobrar uno, o puede ser la forma del modelo», ejecutado) | nada |

### 3.6 Diagnosis: la Q y sus rótulos

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0195 | dos Ljung-Box con el mismo rótulo y veredictos opuestos: el escaneo de anómalos no descontaba los parámetros ARMA | **se reproduce**. Sobre `MINK/m01`, AR(4): `dictamen` da «Q(5)=5.3 p=0.383» (5 g.l.: 9 retardos menos 4) y `anomalos` «Q(9) con 5.3 (p = 0.809)», con los 9 retardos como grados de libertad (ejecutado; `chisq_cola( qc, c.nlags )` en `main.c` y en `an_anomalos.c`) | parte en este repositorio, con repro; que la Q de `anomalos` descuente los ARMA y diga el retardo y los g.l. por separado |
| 0213, 0074 | «la Q falla en el retardo 12» se leía como estacional; el rótulo daba retardos por g.l. | `dictamen` rotula **Q(g.l.)** y, cuando falla, dice «donde peor es en el retardo %d» **con los grados de libertad** (en el código, `bloque_autocorrelacion`): con un AR(4) que falle en el 12 diría «retardo 8» | en el mismo parte: el retardo y los g.l., cada uno con su nombre |
| 0166, 0077 | los g.l. restaban los deterministas o los coeficientes fijos | los da el motor (la columna DF del `.out`, ejecutado) | nada |
| 0231, 0075 | la diagnosis sólo citaba el último retardo | `dictamen` da la escalera entera (en el código) | nada |
| 0105, 0130 | un solo \|z\| > 3 declaraba insostenible un modelo aprobado; el umbral viajaba sin su criterio | `anomalos` calibra por *n* y lo dice («tan improbable como un extremo aislado de \|z\| ≥ 2.87», ejecutado) | nada |
| 0036, 0042, 0106, 0211 | dos y tres predicados de adecuación con el mismo nombre, que se contradecían | uno: `dictamen` es el de la ficha y el de la ventana (en el código) | mantenerlo: cualquier herramienta nueva que diga «adecuado» llama a `fi_dictamen` |
| 0214 | el JB de la diagnosis no era el del `.out` | se lee del `.out` | nada |
| 0237 (**abierto**) | la puerta de adecuación rechaza 5 de 21 modelos verdaderos: muchos contrastes al 5 % sin corrección | `dictamen` toma el peor p de la escalera, con dos umbrales (en el código): el mismo problema de multiplicidad | seguir lo que decida art; no es urgente porque la madre dice «mirar», no «no» |
| 0062 | un MA no invertible salió de 45 iteraciones sin una palabra | `dictamen` no tiene bloque de raíces (en el código); si `fue` en C lo impide al estimar, **no comprobado** | comprobarlo; si no lo impide, un bloque «Operadores» en `dictamen` |
| 0220, 0051 | se comparaba el AIC de modelos con distinta `d` (o distinto `ifadf`) | no hay herramienta que compare; el LLM puede comparar ℓ de dos `ecuacion` | una frase en las instrucciones, y que `compare_versions`, si se porta, se niegue entre `d`, `D`, `ifadf`, λ o muestras distintas |

### 3.7 Contrastes formales

Es donde más se ha portado, y se nota. Lo que hay en `lib/formales` cita
expresamente: art BUG-0009 (el testigo de Nyquist no se pisa), 0011 (las
reservas del crítico), 0025 (la diagnosis primero: «no se impide, se dice»),
0055 (el titular), 0065 (la nula de Shin-Fuller), 0167 (el lado d − 1 se
pide, no se ofrece), 0215, 0224, 0232, 0235 (desde d = 2 no se sube), 0236
(un LR negativo no es un estadístico) y 0243 (el testigo es una raíz, no un
factor de orden 1). El DCD estacional (art BUG-0039, 0193) también está
(`FM_DCD_S`).

Lo que **no** está, comprobado en la salida de `MINK/m01`: el lado AR del par
confirmatorio por sobreajuste a AR(p + 1) cuando las raíces son complejas
(art BUG-0068, 0242). El C dice: «el AR no tiene ninguna raíz real que
aislar; una raíz compleja cerca de la unidad es cosa del MEG». Parece una
decisión de la madre y no un olvido (inferido de cómo lo dice); si se quiere el sobreajuste, es
una pregunta para el analista (§6).

Y lo que no se portó **a propósito**: el veredicto encadenado (§5).

### 3.8 Previsión

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0008, 0205 | la banda del 95 % salía 100 veces estrecha en logaritmos; luego, simétrica en nivel (aproximación delta, `wontfix`) | `generate_forecast` da la tabla de `fuf`: nivel y su d.t., sin banda (ejecutado). El LLM puede construir «nivel ± 1,96·d.t.» y repetir art BUG-0205 | una frase en la descripción: con λ = 0 el intervalo se construye en logaritmos; o que `fuf` dé la banda |
| 0007 | `update_and_forecast` reconstruía el modelo sin el factor de reescala | no hay `update_and_forecast` | si se porta, desde el `.pre` con `fuf`, como `prever` |
| 0191 | lo que la previsión usaba no estaba en las dependencias | `prever` es C | nada |
| — | `horizon` | no existe (§2.3) | añadirlo |

### 3.9 Ficheros, semillas y linaje

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0090, 0159, 0164, 0091, 0158 | «nunca se estima desde un `.pre`» se sostenía con un aviso; `get_out_report` reestimaba en vez de leer | `estima` sólo sobre lo no estimado; `ecuacion` y `dictamen` leen el `.out`; `prever` y `ficha --comprobar` corren `fue` sobre el `.pre` en un temporal (en el código) | nada: es una propiedad del sistema, no una costumbre |
| 0014, 0199, 0245, 0252 | al encadenar desde un `.pre`, la `d`, los armónicos o `ar_f_freqs` pedidos se ignoraban en silencio y la cabecera decía lo pedido | `especifica` aplica cada cambio sobre el `.inp` del hijo y dice lo que hizo; si `fue` no lo acepta, no cambia nada (en el código) | nada, salvo el caso de §3.1: decir también **lo que el hijo ya traía** |
| 0099 | se podía encadenar el `.pre` de otra serie | `proyecto itera SERIE PADRE`: el padre es de la misma serie por construcción | nada |
| 0175, 0184 | el linaje era una ruta, no un contenido; si el `.pre` del padre se reescribía, nadie lo veía | lo estimado no se reescribe por `atsw-cli`; los casos guardan el sha de cada `.pre`, los modelos univariantes **no** (en el código, `lib/proyecto`) | guardar el sha del `.pre` del padre al derivar, como ya hacen los casos |
| 0177 | ℓ, AIC y BIC sin la muestra sobre la que se calculan | las muestras del proyecto son ramas aparte | nada |
| 0108, 0176, 0037, 0058 | el padre se infería («la última entrada») y las cascadas de abandono barrían ramas vivas | el padre se declara siempre (`new_version parent`) | nada |
| 0110, 0217, 0226, 0229, 0241, 0251 | quién decidió cada nodo; «coincide con la propuesta» por comparación de cadenas | no hay nodos: `por: asistente` va en cada modelo | ver §6: en guiado el que decide es el analista y el linaje dice «asistente» |
| 0187, 0188 | `_write_inp` truncaba los datos a 6 decimales y perdía columnas de deterministas no estándar | `lib/inpdet` reescribe sólo lo que toca y deja el resto byte a byte; los datos del ejemplo van con 10 decimales (ejecutado); un no estándar no se escribe, se rechaza (en el código) | nada |
| 0001, 0085 | el factor de reescala se perdía al construir un modelo a mano | el `.inp` de la madre lleva 100 (ejecutado) | nada |

### 3.10 El carril autónomo y el guiado

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0180 | el «autónomo» era un auto-ARIMA con paradas del guiado | no hay `build_model`; el autónomo es el LLM en la silla del analista (instrucciones) | nada |
| 0181 | las herramientas de estimación terminaban en ⏸ en cualquier carril | las salidas de `atsw-cli` no paran ni preguntan (ejecutado) | nada; y no añadir paradas a la salida: en guiado, para el protocolo |
| 0179, 0080 | declarar el dominio sacaba la llamada del carril autónomo; el dominio no era alcanzable en guiado | el dominio es el nodo 1 y va a la razón del primer modelo | nada |
| 0015, 0040, 0178 | la regla del índice (λ = 0 en un índice de precios) sólo vivía en un carril; la taxonomía de dominios era binaria; un dominio desconocido se aceptaba en silencio | la regla está en las instrucciones («Índices de precios: λ=0 siempre»); `boxcox_analysis` es el de art | nada; si un día una herramienta recibe el dominio, como enumeración (§3.11) |
| 0208 | en autónomo se dibujaban figuras que nadie mira | el extra borra las figuras de art; `plot` abre la ventana sólo cuando se pide | nada |
| 0094 | la salida del guiado dependía del LLM | `model_equation_display` y `diagnose` son texto fijo; las instrucciones mandan copiar la ecuación tal cual | nada |

### 3.11 La forma de los textos que lee un LLM

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0116 (**abierto**) | el cliente recortaba las descripciones al 23 %: lo que decide estaba detrás del corte | descripciones cortas y las instrucciones en 11,8 KB (en el código; el plan de la fase 1b pide menos de 20 KB) | mantener el presupuesto: lo que decide, delante; la doctrina, en las instrucciones. **La descripción de `write_inp` debería decir en su primera frase que las intervenciones se añaden a las heredadas** |
| 0155, 0183 | un parámetro de enumeración sin su enumeración: se aprende por el error | `family` y `what` son `str` y se validan al llamar, con los valores en el mensaje (en el código) | tiparlos como `Literal[...]` para que el esquema los lleve |
| 0112 | un parámetro sin tipo llegaba como cadena | todos tipados (en el código) | nada |
| 0120, 0154, 0156 | el mismo bloque o veredicto dos y tres veces; dos recomendaciones distintas en una salida | `plot` añade a la ventana el texto de `anomalos` o de `dictamen`, una vez | vigilarlo en cada salida compuesta |
| 0059, 0225, 0247 | el Box-Cox perdía el signo de la correlación y se contradecía | es el de art, con los arreglos | nada |
| 0033, 0084, 0253 (**abierto**) | σ con un «%» que sólo vale con λ = 0; la misma σ ×100 en la figura; con λ = 1 los ω en la escala ×100 de `fue` | `ecuacion` imprime σ en la escala del motor y, con λ = 0, «(en logaritmos: ×100 es un %)» (ejecutado: «26.02…»); la σ ya está multiplicada por 100, y la frase se puede leer como «multiplícala otra vez». Con λ = 1, μ y los ω salen en la escala ×100 sin que se diga (inferido de art BUG-0253; no comprobado con una serie en niveles) | decir la unidad sin ambigüedad («26,02 %», o «en la escala del motor: ×100») y, con λ = 1, que los ω y μ están ×100 |
| 0043 | un JB que falla sin anómalos es un síntoma de λ, y la salida mandaba intervenir | no comprobado | una frase en las instrucciones del nodo 2 |
| 0029 | el `.out` existía y nada llevaba a él | `ecuacion` lo cita tal cual | nada |

### 3.12 Lo que se traga en silencio, y la robustez del servidor

| art BUG | qué pasó | atsw-us hoy | qué haría falta |
|---|---|---|---|
| 0160 | cinco avisos del método vivían en un `except: pass` | `cli.extra` traga a propósito el fallo de apuntar el coste («el coste no para el análisis»); `_hazlo` sólo recoge `NoSePuede`, y un `TimeoutExpired` de `atsw_cli` sale como error genérico del servidor (en el código) | que el plazo de `atsw_cli` se convierta en `NoSePuede` con su texto, como ya hace `extra` |
| 0114 | un subproceso colgado bajo el servidor MCP | `stdin=DEVNULL` y plazo en todos los subprocesos (en el código) | nada |
| 0078, 0081, 0111, 0121, 0122, 0126, 0147 | figuras que se pisaban, se abrían tres veces, abrían diálogos en Windows o no se escribían | el extra pone `ART_NO_VIEWER` y un temporal que se borra; las ventanas son de la madre (en el código) | nada |
| 0125, 0191 | lo que la herramienta necesitaba no venía en la instalación | el extra fija versiones y `uv.lock` | nada |
| 0021, 0107 | un `AttributeError` por un campo renombrado tumbaba la herramienta entera | el extra devuelve el motivo («art: …») y el servidor sigue (en el código) | nada |

---

## 4. Prioridades

Ordenadas por **riesgo para el asistente** (un modelo equivocado sin aviso
pesa más que una herramienta que falta), luego por coste, y con preferencia
por lo que ya está en C.

1. **Modificar una intervención, no sólo añadir** (art BUG-0162). En
   `especifica`: el control de `an_sugerir` (misma fecha) llevado a una
   ventana; `--quita` sobre `id_quita_det`; y que la salida liste **las
   intervenciones que el hijo ya traía**. En `write_inp`, la primera frase de
   la descripción. *Riesgo alto, reproducido; coste bajo; todo en C.*
2. **Los sucesos por su fecha** en `intervention_ladder` (art BUG-0030,
   0067, 0172, 0185). La capa traduce la fecha al índice con las fechas de
   los residuos del `.out`. *Riesgo alto: el error cae en el mes de al lado
   sin aviso; coste bajo.*
3. **Poder construir lo que se recomienda.** El peldaño 2 (varios ω) en
   `write_inp`, como la ventana de la madre (art BUG-0079, 0093, 0204); y,
   mejor, `suggest_intervention_form` en C, que es la ventana «Sugerir
   intervención…» sin ventana, con la regla de la madre. *Riesgo alto: hoy la
   escalera puede recomendar algo que el asistente no sabe hacer, y
   improvisará; coste bajo-medio.*
4. **`meg_reformulate`** sobre `fm_reformula` (art BUG-0035, 0053, 0034).
   *Riesgo alto en series estacionales: sin ella el veredicto del MEG no
   tiene camino; coste bajo.*
5. **Los rótulos de la Q** en `anomalos` y `dictamen` (art BUG-0195, 0213):
   un parte aquí con su repro (`BUG-NNNN-repro/`), y el arreglo en `lib/`,
   que cambia también la ventana de la madre. *Riesgo medio: el LLM ve dos p
   para la misma Q y un «retardo» que no es el retardo; coste bajo.*
6. **El hessiano de reserva, marcado en la diagnosis** (art BUG-0027, 0060):
   que `dictamen` lea la línea «Standard errors» y diga «mirar» cuando no es
   `fdhess`. *Riesgo medio: es el síntoma del caso de §1.2; coste bajo.*
7. **`guion_map` con la especificación de cada modelo** (art BUG-0102 y la
   raíz de 0162): estructura y deterministas por modelo. *Riesgo medio: es lo
   que habría evitado el caso de §1.2; coste bajo.*
8. **Enumeraciones en el esquema y el objetivo apuntado** (art BUG-0155,
   0183, 0186): `Literal` en `family` y `what`; el objetivo del autónomo, en
   el manifiesto o en la razón del primer modelo, y pedido por una
   herramienta. *Riesgo medio en autónomo multivariante; coste bajo.*
9. **La rampa, rechazada o avisada** cuando la pide el asistente (art
   BUG-0182), y la frase del nodo 8 sobre `add_arma` en una segunda vuelta
   (art BUG-0052). *Riesgo medio; coste mínimo.*
10. **`compare_versions` en C, leyendo los `.out`**, que se niegue entre
    diferenciaciones o muestras distintas (art BUG-0158, 0051, 0220, 0177).
    *Riesgo medio: sin ella el LLM compara ℓ a mano; coste medio.*

Después, en este orden: la comprobación de Treadway sobre el hijo (paso 5
pendiente de `DISENO-intervencion.md`), la simplificación de intervenciones
(art BUG-0123), `horizon` en `generate_forecast`, la unidad de σ y de los ω
(art BUG-0033, 0253), el sha del padre en el linaje (art BUG-0175),
`verify_optimum`, `extend_sample` y `update_and_forecast`.

---

## 5. Lo que no conviene portar, o sólo con cuidado

- **Donde C y art-python calculan distinto, manda la madre** (decisión del
  24-oct-2026). Los cinco casos conocidos: los anómalos y sus episodios
  (`lib/anomalos` frente a art), la forma de la intervención (`lib/intervencion`
  frente a art), el veredicto de la diagnosis (la escalera de `lib/dictamen`
  frente a la última Q de art), la entrada de la F estacional, y la
  identificación de órdenes (el `art` en C frente al porte). El asistente ve
  los números que ve el analista en las ventanas; **portar una regla de art
  ahí cambiaría la madre**. Lo que se porta de art en esos cinco es la
  *lección* (avisar, fechar, no contradecirse), no el cálculo.
- **El veredicto encadenado de los contrastes no se portó a C a propósito**
  (art BUG-0215, 0232; `ESTUDIO-terminal-mcp.md` §2.2). `formal_tests` da cada
  contraste con su número y su crítico, y qué hacer lo decide el analista o
  el autónomo con su razón. No hay que traerlo por el extra.
- **La recomendación de `d` de la tabla ADF/KPSS** (art BUG-0016, 0023, 0056,
  0197, 0210, 0226, 0227): art tardó siete partes en domarla. La salida de C
  no recomienda, y así debe seguir.
- **El `guion.json` y sus herramientas** (`guion_node`, `guion_abandon`,
  `guion_diff`, `export_guion`): son de la otra encarnación (P4). Lo que de
  ellas sirva entra en el manifiesto, no como un segundo registro.
- **`build_model` y `batch_build`**: fuera por la decisión 14, y art mismo
  advierte (art BUG-0180).
- **El extra estima con el `fue` de Python** (`intervention_ladder`): su ω y
  su AIC son los del porte, y el modelo que luego construye `write_inp` se
  estima en C. Es la frontera que art BUG-0201 enseña a vigilar: la escalera
  dice qué forma, no qué número; el número es el del `.out` del hijo.
- **Lo que es de Python por el porte y no por el método**: los partes de
  figuras (`_show_fig`, las ventanas en Windows), de dependencias, de
  `refactor` en `fue.Model`, de nombres de atributos (`mu` frente a `mu0`, art
  BUG-0189). Se aprende de ellos el patrón —corrupción al cruzar una
  frontera—, pero no hay nada que portar.
- **No arreglar en el porte lo que está en C, ni al revés** (motores
  homologados): si un parte de art resulta estar también en el motor en C, el
  parte va aquí con prioridad y los dos se mueven juntos.

---

## 6. Preguntas para el analista

1. **¿`rehacer` como en art, o rechazo como en la ventana?** La ventana de
   la madre rechaza una intervención en la misma fecha; art avisa y deja
   hacer (pueden ser dos sucesos) y ofrece `rehacer`. Para el asistente
   propongo lo de art: aviso, `rehacer` explícito, y una ventana de períodos y
   no sólo la fecha exacta. ¿Se cambia también la ventana para que coincidan?
2. **¿El peldaño 2 en `write_inp` o una herramienta propia?** Añadir la cuenta
   de ω a `interventions` es lo mínimo; portar `suggest_intervention_form`
   (la ventana «Sugerir intervención…» sin ventana) es lo completo, y es lo
   que el §2.2 del estudio del MCP ya preveía como «C, a comprobar».
3. **¿Quién decidió cada paso en guiado?** Hoy lo que escribe el asistente
   lleva `por: asistente` aunque lo haya decidido el analista (art BUG-0110
   es exactamente esto). ¿Un argumento `decided_by` en `new_version` y
   `guion_adopt`, o se acepta que `por` diga quién lo ejecutó?
4. **El objetivo del modelo** (art BUG-0186, abierto): ¿un campo en el
   manifiesto, o basta con la razón del primer modelo?
5. **El sobreajuste a AR(p + 1)** cuando el AR tiene raíces complejas (art
   BUG-0068, 0242): la madre no lo hace y lo dice. ¿Se queda así?
6. **Los rótulos de la Q en la madre**: el arreglo de §3.6 cambia la ventana
   de anómalos y el dictamen, no sólo el asistente. ¿Se hace así, por la
   regla de la madre, o se discute antes?
7. **¿Un estudio igual para drtran y sima** cuando crezcan `atsw-mt` y
   `atsw-sis`, con sus propios registros?

---

## 7. De paso

Al leer las instrucciones de `atsw-us` para este estudio salieron dos cosas
que no son de art pero conviene apuntar:

- el nodo de contrastes ofrece el MEG «marcado (experimental)» y lo llama
  «los modelos HSM, Hybrid Seasonal Models» dentro del texto en español. El
  criterio del repositorio es nombrarlos **modelos MEG** en español (HSM en
  una interfaz en inglés) y no presentarlos como experimentales: lo único sin
  publicar son los críticos por simulación;
- las instrucciones dicen que la sobreparametrización «la da diagnose», y es
  cierto en lo que mide art (los pares con \|r\| ≥ 0,7); sobreajustar
  —añadir un parámetro y contrastarlo— es otra cosa y no está en ninguno de
  los dos.

---

**Estado: estudio, sin decisiones.** Las prioridades de §4 esperan a las
preguntas de §6.
