# Lo que viene

Los pendientes vivían repartidos por las tablas «Orden» de cada diseño, que
está bien para leer un diseño y mal para saber qué toca. Esto los junta.

---

## LA PRIMERA VERSIÓN

**Qué entra** (decisión del analista, 2026-10-04): **fue y drtran con sus
GUIs**, y fug y fuf con ellos, y la madre. drvec sigue en las pruebas pero
**no** va en la primera versión; drvarma tampoco.

**Dónde estamos.** Compila y pasa sus pruebas —motores y GUIs conducidos—
en Linux, macOS y Windows (`docs/PRUEBAS.md`). Falta:

| | qué | notas |
|---|---|---|
| | **la ronda en máquinas reales** | un Windows y un Mac de usuario, sin entorno de desarrollo: `PRUEBAS.md` §5 |
| | **el paquete de Windows** | DLL de GTK/GSL/zlib y `share/` (esquemas compilados, iconos) junto a los `.exe`, o estático. fue_gui y drvarma_gui ya usan `share/` si está al lado |
| | **el paquete de macOS** | `.app` o Homebrew; firma, o Gatekeeper no lo abre |
| | la consola en Windows | `lib/engine` lanza con `g_spawn`: comprobar que no asoma una consola al estimar |
| hecho | el gestor de proyectos multivariantes | los **casos**: ver abajo |

### Los casos: la madre multivariante — HECHO (2026-10-04/05)

Diseño en **`DISENO-casos.md`**, en cuatro pasos, todos en `main` y con la
CI en verde en las tres plataformas (PR #3 a #6):

1. `lib/proyecto`: el **caso** —series en orden, cada una con el modelo con
   que entra y el sha256 de su `.pre`— y sus **corridas**, con linaje, razón
   y una elegida. `pr_borra` no borra un modelo que es entrada de un caso.
2. `drtran_gui --caso C [--corrida c]`, el **alta automática** al estimar
   con series del proyecto, y un caso desfasado deriva uno nuevo en vez de
   tocarse.
3. La madre: la sección **CASOS**, con las entradas y el estado de su
   `.pre`, el árbol de corridas, «Nuevo caso…» con la ventana común, «Abrir
   en drtran», elegir, razón, derivar, borrar y los dos desfases.
4. La conversión de las corridas que drtran_gui registraba como modelos de
   la serie de salida.

Y después (PR siguiente): **la puerta diagonal por corrida** —la conjunta
contra la suma de los `logelf` de los `.out` de fue de las entradas: una
corrida diagonal tiene que cuadrar; con transferencia, se enseña lo que
gana—.

**Lo que queda, fuera de la primera versión:**

- **«Pasar a drvarma»** desde un caso con ciclo: el campo `motor` y la
  acción están diseñados (`DISENO-casos.md` §4), no hechos.
- **Si los `.pre` eran óptimos o especificaciones**, por entrada, como el
  certificado de mtram: hoy la puerta lo delata en la corrida diagonal,
  pero no dice cuál.
- **El recorrido** de drtran —qué enlaces se probaron y por qué se podaron—
  sigue sin guion (`DISENO-escalera.md` §5.4).

### ART entra en el GUI

**Decisión del analista, 2026-10-04:** la última versión de ART está lista
para entrar en el GUI. Falta concretar cuál es esa versión (ART_18.2 está
sin etiquetar; ART_19 se congeló el 2026-10-01) y **cómo** entra —ver las
lecturas de abajo—. Lo que sigue es el contexto para hacerlo bien.

**ART_18 no es una versión de fue.** Es otro programa en C
(`~/Dropbox/SRC/ART/ART_18`, repo `davidesg/art-identifier-c`): un
**identificador automático** de órdenes ARMA/SARIMA —lista corta por AICc
con pesos de Akaike, Hannan-Rissanen, ADF/KPSS, detección estacional y
armónica, ACF/PACF teórica contra empírica— con su propio GUI GTK3
(`art_gui`) y una CLI (`art_cli`). La versión trabajada es la **18.2,
«corrección estadística del identificador»**, sin publicar: su Fase 4
(banco final, `make test`, etiqueta) está abierta.

**La tensión, dicha.** `AUDITORIA-art.md` §5 descartó para el GUI los
auto-ARIMA (`build_model`, `batch_build`): *«Un GUI de control total no
debería tener un botón que decide por ti»*; y `ESTUDIO-atsw-PLAN.md` lo
repite para el uso educativo. En cambio, los contrastes formales (ADF/KPSS,
MEG) **sí** se decidieron traer (2026-09-26, la sección siguiente), y ART_18
los tiene corregidos en C.

**Las formas de entrar**, a elegir:

1. **Traer `art_gui`/`art_cli` como un programa más** del taller, que la
   madre lanza como lanza fug —un asesor que propone, no que decide—.
2. **Traer sólo sus piezas** al GUI de siempre: ADF/KPSS (ya en C y
   cotejados con statsmodels), la detección estacional, la factorización AR.
   Es la línea de la sección siguiente.
3. **Citarlo** en las notas de la versión como el identificador de
   referencia, sin código. (Descartado: la decisión es que entra.)

---

## EL SIGUIENTE PASO: los contrastes formales y el MEG

**Decisión del analista, 2026-09-26.** Traer al GUI `formal_tests` y el MEG:

- **`formal_tests`** — MEG, DCD_f y Shin-Fuller. 1 871 líneas en art.
- **`meg_frequency`** — la frecuencia que el MEG señala.
- **`meg_reformulate`** — reformular el modelo según lo que el MEG dice.

### Por qué esto cierra una pregunta abierta, y no es un encargo más

`AUDITORIA-art.md` §4 los dejó en el grupo C —«lo que se puede pero hay que
decidir antes»— y la razón no era el coste:

> La identificación de esta escuela es **visual** —la ACF, la PACF, la media
> contra la desviación típica, que es lo que fug enseña—. Traerlos al GUI no
> es sólo trabajo: **cambia lo que el GUI enseña**, y el GUI es, por la
> decisión P4, el que se usa para educación y para el analista que quiere
> control total del proceso.

La recomendación de entonces fue: `ar_factorization` sí, **el resto sólo si
decides que el GUI también los enseña**. El analista lo ha decidido: sí.
Queda dicho aquí para que dentro de seis meses no parezca que se coló.

### Lo que hará falta

| | qué | notas |
|---|---|---|
| | leer los `.out` que ya traigan MEG | el motor puede estar imprimiendo parte: mirar antes de portar |
| | GSL para las distribuciones | ya es dependencia de `drtran_gui`; de ahí salen los autovalores de `nlatools` |
| | `ar_factorization` primero | es leer el operador AR y sacarle las raíces — dice si hay un AR estacional escondido, y eso **sí** es de la escuela. Es el peldaño barato y el que menos compromete |
| | decidir **dónde** se enseña | la diagnosis juzga lo que hay; un contraste de raíz unitaria es identificación, no diagnosis. La distinción que el analista hizo con la ganancia vale aquí igual |

**No modificar los motores** salvo defecto, y con el visto bueno — como el de
la división entera del Jarque-Bera.

---

## Pedidos del analista, probando el caso IPC_ES → WTI

- **HECHO — La lista de modelos de la madre, como la quiere el analista**
  (2026-10-07):
  - **Doble clic lanza fue_gui** con el modelo (ya lo hace: `atsw_on_activado`
    → `on_fue`, con el `.pre` si está estimado). Lo que falta: **si el modelo
    ya está estimado, fue_gui tiene que abrir directamente en la consola con
    los resultados** (el `.out`), no en la pestaña de especificación.
  - **Botón derecho sobre el modelo marcado: «Ver el gráfico…»** — los
    residuos con su ACF/PACF y el modelo escrito debajo, el `A<id>.eps` que
    fue deja junto al `.out` (el mismo del `.pdf`), en la ventana de
    gráficos (`lib/preview`), sin pasar por fue_gui. Apagado, con globo que
    diga por qué, si el modelo no está estimado.

## Lo demás, por documento

### `DISENO-anomalos.md`

- **§7.4** — el mismo interruptor de calibrar en el **vistazo**, sobre la
  serie *sin* modelo. Es donde la calibración de verdad decide, porque es
  donde se eligen los órdenes.

### `DISENO-intervencion.md`

- **§5.5** — la comprobación de **Treadway sobre el `.out` del hijo**: leer si
  la forma elegida resolvió el suceso o lo dejó a medias. Cierra el círculo
  —hoy la superposición lo estima *antes*, sin motor— y las piezas están:
  `lib/outfile` más `lib/anomalos`.
- **§5.4** — el peldaño 2 estimado de verdad, para poder comparar con el 1 por
  algo que no sea el AIC.

### `DISENO-muestras.md`

- **paso 6** — renombrar la clave de una serie. El menos urgente.

### `DISENO-prevision.md`

- El **acta** de previsión y la evaluación ex-post sobre una submuestra. Se
  revirtió con aquel intento fallido y sólo está en la conversación.

---

## Fallos conocidos y sin arreglar

Los encontraron las pruebas conducidas y la CI de 2026-10-04; se dejaron
sin tocar porque no bloqueaban las pruebas o porque son del motor. Por
programa:

**fue / fue_gui**

- Al abrir la ventana, Diagnosis, Anómalos y Ganancia salen **encendidos**
  y sin tooltip: `fue_analisis_refresca()` sólo corre cuando cambia el
  nombre de entrada, nunca en `create_main_window` (`gui/fue/src/main_window.c`).
- `--proyecto` ignora la `raiz` del manifiesto (`gui/fue/src/main.c` ~144):
  `pr_ruta` con serie vacía devuelve vacío. Es el mismo fallo que se arregló
  en `gui/fug/src/main.c`.
- En Windows los motores escriben su salida con `\r\n`; la primera línea que
  el GUI enseña en un mensaje de error puede llevar el `\r` (`lib/engine`,
  `first_line`).

**drtran / drtran_gui**

- «Baseline listo» se pisa enseguida con «drtran finished.»: el usuario no
  lo ve (`gui/drtran/src/estima.c`, `on_done`, ~489-507).

**drvarma / drvarma_gui**

- El motor **sale con 0** cuando la estimación falla («ESTIMATION FAILED: AR
  nonstationary»), y el GUI lo anuncia como éxito. Pasa con el VARMA(1,1)
  por defecto sobre niveles.
- El motor aborta en el manejador de errores de GSL con datos diminutos (3×2).
- «Use differenced data» en Select VAR Order no hace nada: sólo cambia una
  etiqueta.
- La conclusión de Johansen se escribe con `strcat` en un búfer fijo de
  2048 bytes: desborda con muchas series.
- Las recargas pierden `base_name` y compañía; `transform_data` no se usa.

**gtk_fmg**

- Con `--proyecto` y un fichero fuera de la raíz, el manejador del selector
  mueve el espacio de trabajo a la carpeta del fichero y se pierde la raíz
  del proyecto (`gui/fug/src/callbacks.c` ~836).
- Al cargar un CSV, su frecuencia, fecha, nombre de cabecera y el aviso de
  columnas sobrantes no aparecen hasta el primer Save o gráfico, y la serie
  toma el nombre del fichero, no el de la cabecera (`callbacks.c` ~850).

**la madre / lib/proyecto**

- `atsw_relee` libera `a->p` mientras las ventanas de Ganancia y Sugerir
  guardan un puntero a él (`an_ganancia.c:80`, `an_sugerir.c:356…`): uso
  tras liberar posible si el manifiesto cambia por fuera con ellas abiertas.
- `pr_leer` acepta `schema_version: 99`, una lista sin cerrar y un fichero
  vacío sin protestar.

**drvec** (no va en la primera versión)

- `copias.sh` señala una copia de `lib/fuepre/fue_pre_reader.c` en
  `engines/drvec/src/`.
- Modelos con varios óptimos locales: la referencia de Linux no es el mejor
  (`PRUEBAS.md` §3).

---

## Higiene, sin prisa

- **`/usr/local/bin` tiene instalaciones viejas** (`fug` 1.14, `fue_gui` de
  mayo). No estorban —los GUIs se buscan primero en el árbol de
  construcción, y por eso `atsw_programa` mira ahí antes que en el PATH—
  pero están, y necesitan sudo para irse.
- **`IPC_ES_m04.inp`** quedó huérfano en el proyecto de pruebas: fichero en
  disco que el manifiesto no conoce, de cuando «Derivar» escribía a medias
  desde fue_gui. La próxima derivación lo sobrescribe.
