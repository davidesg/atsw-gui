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
| hecho | **el paquete de Windows** | instalador (Inno Setup, por usuario) y zip: `packaging/windows/`. Sin firmar: SmartScreen avisa |
| hecho | **el paquete de macOS** | `ATSW.app` en `.dmg`, arm64 e Intel, macOS 14+: `packaging/macos/`. Firma ad hoc: Gatekeeper avisa |
| | firmar los paquetes | Apple Developer ID + notarización; certificado de firma de código en Windows |
| hecho | **el manual del alumno** | `docs/manual/`: el caso IPC ← WTI, con capturas y vídeo generados desde el programa |
| hecho | la consola en Windows | asomaba una con cada motor que lanzaba un GUI (y el vistazo congelaba la madre): `lib/sitio` lanza con `CreateProcessW` y `CREATE_NO_WINDOW`, y la CI de Windows lo comprueba. `gui/atsw/BUGS.md` BUG-0001 |
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

## CASO DE ESTUDIO: el gestor de previsiones

Decisión del analista (17-oct-2026): **se estudia a fondo como caso propio, y
no se empieza hasta cerrar drtran_gui con el sistema como está.** El plan está
en `DISENO-gestor-previsiones.md`; lo que se aprendió de los programas viejos,
en `ESTUDIO-taste-prevision.md`.

**El problema.** No hay nada para gestionar modelos de cara a ejercicios de
previsión consecutivos:
- llega un dato más;
- se prevé desde el nuevo origen con el modelo vigente;
- la previsión se guarda tal como se hizo;
- cuando llega lo previsto, se mide el error;
- de vez en cuando se reestima y se sigue con el modelo nuevo.

En fue se hace a mano (el `.pre`, la añada, las banderas a 0 y fuf), pero con
varios modelos y meses se pierde el rastro de qué modelo, qué dato y qué
previsión.

**Decidido:**
- **Las añadas de datos las decide el analista.** En modo **SPS** se guarda
  el registro de previsiones y se puede ir adelante y atrás por los orígenes.
  En modo **evaluación** de modelos no se guarda el histórico.
- **Los errores los calculan los motores**: fuf en su columna ERR, drtran con
  `-C`, drvarma en el `.recursive`. El gestor los lee y no hace otra cuenta.
- **El modelo se congela**: no se reestima con cada dato, tampoco
  «reestimando la misma ventana». Los parámetros se leen del modelo vigente.
- **Univariante**: fuf ya prevé con todo fijo y la σ² de la estimación.
  Comprobado: las mismas previsiones con las banderas a 0.

**Abierto:**
- **Multivariante**: hace falta un **fuf multivariante** que prevea con los
  parámetros congelados de una corrida. drtran y drvarma con `-estwin`
  vuelven a estimar la ventana.
  - Comprobar primero si un `.cns` con todo fijo ya basta en drtran.
  - Si no, decidir entre una opción de los motores y un programa aparte.
- **La reestimación**: ¿el gestor la propone (cada k orígenes, o cuando los
  errores a un paso se salen de la banda) o es siempre a mano? ¿Alargar la
  ventana o volver a la identificación?
- **El primer alcance** (¿univariantes primero?) y **el sitio** (¿pestaña de
  la madre o programa aparte?).
- **Las unidades de la columna ERR de fuf, por comprobar.** En
  `CPI_USA_model_AR` vale miles (3370, −8988…) con σ² = 3148 (d.t. ≈ 56), y
  las variaciones parecen multiplicadas dos veces por el refactor (−23,4 por
  un −0,23 %). El gestor va a leer esa columna: hay que aclararlo antes, con
  su `BUG-NNNN-repro/` si es un fallo.

## La interfaz en inglés y en otros idiomas

**Decisión del analista, 2026-10-15.** De momento todo en español: la barra y
los menús de fue_gui pasaron al español con sus iconos propios (la barra:
Nuevo, Abrir, Guardar, Estimar, Ver modelo, y Diagnosis ▾, Contrastes ▾ y
Prever ▾ con su menú). **Más adelante**, una interfaz en inglés y en otros
idiomas para todo ATSW. Quedan en inglés, hasta entonces, las pestañas y los
campos de fue_gui («Data Input», «Stochastic Component»…), que vienen del
motor. Cuando se haga, gettext (GLib ya lo trae) y un catálogo por idioma;
los textos de hoy están escritos en el código, sin marcar.

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

### Dónde estamos (2026-10-14)

**Antes, dos herramientas de identificación que NO son `formal_tests`** —en
art tampoco lo son, y por buenas razones: no contrastan el modelo, ayudan a
elegirlo—. Hechas en `lib/raices`, sin correr el motor, cotejadas con
art-python número a número (`test_raices`, en la batería de `gui/drtran`):

| | qué (en art) | dónde |
|---|---|---|
| hecho | **ADF y KPSS para d = 0, 1, 2** (`unit_root_analysis`): la d de partida, «ADF manda». Los contrastes son los de `engines/art/src/unit_root_tests.c`, compilados tal cual | Identificación, plegado bajo la cabecera |
| hecho | **La factorización del AR** (`ar_factorization`): raíces, amortiguamiento, frecuencia y periodo, ± por el método delta en un AR(2) entero, el aviso de los módulos parecidos (BUG-0103) | Diagnosis → «Factorización del AR…» |

**Los contrastes formales**, lo decidido el 2026-09-26, de más barato a más
caro. Todos reestiman con fue, en DOS ventanas (2026-10-15, lo pidió el
analista): «Orden de integración…» (Shin-Fuller, DCD regular, sobre y
subdiferenciación) y «Estacionalidad (MEG)…» (simplificación estacional, DCD
estacional, MEG, DCD_f; sólo con s > 1). Se abren desde la diagnosis, desde el
menú de la lista y desde la barra de fue_gui (`lib/formales`, `an_formales`). Se corren de uno en
uno sin bloquear, y cada uno se coteja con art-python número a número
(`test_formales`, en la batería de `gui/drtran`, con fue de verdad):

| | qué | coste |
|---|---|---|
| hecho | la simplificación estacional (RV de los armónicos) | una corrida |
| hecho | Shin-Fuller (la raíz real positiva dominante; un AR(p) se parte, BUG-0065) y el DCD regular y estacional | una corrida por contraste |
| hecho | la sobrediferenciación d+1 con testigo y el **par confirmatorio en f = 0** con Shin-Fuller (las reservas del BUG-0011: ley desnuda con deterministas, banda de cuasi-cancelación); la subdiferenciación d−1, **que se pide y no se ofrece** (BUG-0167), con el AR acotado en ρ (BUG-0224) y el reintento del libre (BUG-0236) | dos o tres corridas |
| hecho | **el MEG** (Abraham y Box, 1978): por frecuencia, el modelo S —sin el armónico, con ifadf[f] = 1 y un testigo de sobrediferenciación libre (MA(2) de frecuencia fija, o MA(1) en Nyquist)— y H₀: el testigo en −1. Críticos del par complejo por n (s = 2) y de raíz real en Nyquist (s = 1), de la simulación de SF_MEG. **La reformulación**: «Reformular como estocástica…» deriva S como hijo del modelo (`meg_reformulate`), una frecuencia cada vez. **El DCD_f** del testigo en el reformulado, y el del testigo de Nyquist en θ = −1 (art lo contrasta en +1, como un MA más) | dos corridas por frecuencia |

Las estimaciones de los contrastes se **reinician** como en fue-python
(BUG-0005 de fue): si fue para sin anular el gradiente, otra vez desde donde
paró, hasta tres veces mientras la verosimilitud mejore; y lo que no converge
aun así se marca «dudoso». Los `.inp` que salen de un `.pre` llevan los
coeficientes del `.out` (seis decimales): con los cuatro del `.pre`, un AR(2)
en 1.375800/−0.375801 suma 1 y fue no arranca (RIPC.2 en el MEG).

Quedan fuera, de lo hecho: el DCD de un MA(q) con q > 1 (art tampoco lo
hace); el MEG con la verosimilitud exacta en la frontera, perfilada en una
rejilla, que art recomienda y no usa (el optimizador libre de fue se sesga en
la frontera de orden 2, acumulación 0.82 frente a 0.62), y los críticos del
MEG con media y armónicos en muestra finita (n = 120: 1.63/2.87/5.81), que la
ventana avisa y no aplica; el reintento del BUG-0236 en el DCD regular y el estacional (sí está en
la sobre y la subdiferenciación, como en art); el Shin-Fuller por sobreajuste
cuando el AR no tiene raíz real positiva, y el par de la frontera d−1/d que
cruza dos modelos (BUG-0235). Y de art sigue abierto el fondo del BUG-0011: el
crítico corregido en f = 0 con deterministas.

**No se porta el veredicto encadenado** de art (`describe_formal_tests`, ~950
líneas): es donde viven BUG-0215 y BUG-0232. Cada contraste dirá su número, su
crítico y lo que dice, y un LR no finito saldrá como «no calculable».

### Lo que hará falta

| | qué | notas |
|---|---|---|
| | leer los `.out` que ya traigan MEG | el motor puede estar imprimiendo parte: mirar antes de portar |
| | GSL para las distribuciones | ya es dependencia de `drtran_gui`; de ahí salen los autovalores de `nlatools` |
| | `ar_factorization` primero | es leer el operador AR y sacarle las raíces — dice si hay un AR estacional escondido, y eso **sí** es de la escuela. Es el peldaño barato y el que menos compromete |
| | **las citas del MEG, en el manual del investigador** | la interfaz dice «modelos MEG» (en inglés, «HSM») y no atribuye. Los modelos son de Abraham y Box (1978); los contrastes reúnen a varios autores (HEGY, Gallego y Treadway, entre otros). Hace falta un estudio con citas y atribuciones en el manual del investigador o del usuario, para quien quiera saber más (analista, 18-oct-2026) |
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
- **HECHO — En los casos, «corrida» pasa a «modelo»** y el doble clic sobre un
  modelo del caso lo abre en drtran_gui (2026-10-08, PR #18).
- **HECHO — La página de fue, sin cursiva** (2026-10-08, PR #19).
- **HECHO — La página del modelo en drtran_gui** (2026-10-09): Diagnosis →
  «Gráficos…» → «Serie y ACF / PACF, con el modelo». El gráfico de los
  residuos y debajo TRES ecuaciones: la transferencia en nivel
  (`ln Y = ω(B)/δ(B) B^b ln X + D_t + N_t`), el ruido con sus diferencias y
  la media, y `D_t`, con los armónicos en un sumatorio. Cada d.t. bajo su
  coeficiente; en una fracción la raya llega al más ancho. Va junto al `.out`
  del modelo (`gui/drtran/src/pagina.c`).
  - **HECHO** (2026-10-15): drtran_gui escribe la página de cada ecuación al
    estimar, junto al `.out` (`diagnosis_escribe_paginas`), y la madre la
    enseña con el botón derecho sobre el modelo del caso: «Ver modelo…» (la
    salida) y «Ver la ecuación de X…» (las demás). Borrar el modelo se lleva
    sus páginas.
  - **HECHO** (2026-10-15): abrir un modelo ya estimado del caso carga su
    `.out` en Estimación, Diagnosis y Previsión sin volver a estimar
    (`estima_desde`), y escribe su página si falta.
  - **HECHO** (2026-10-17, era un fallo y no una decisión): el gráfico de
    residuos ponía `σ_w` en las unidades del motor (×refactor: 25 % en el
    IPC, cuando la ecuación dice 0,25 %). La división por el refactor se
    perdió al pasar el gráfico de gnuplot a fugdraw (fue 1.14). Ahora
    `lib/fugplot` recibe las unidades (`FPUnits`) en fue, fug, drtran_gui y
    Anómalos, y dice % sólo en logaritmos. Lo mismo la σ de la ecuación de
    fue con λ ≠ 0 y la tabla de Ajuste de drtran.

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
- **Actualizar el origen de la previsión**: ahora es el caso de estudio de
  arriba, «El gestor de previsiones».

---

## HECHO (22-oct-2026): el arranque, la presentación y el icono

Pedido del analista (18-oct-2026), hecho después del diseño de la madre.

- **La ventana de presentación** (`gui/atsw/src/presentacion.c`). Lleva el
  icono, la versión y los créditos con la redacción del analista. Mientras
  se ve, corre la comprobación de la instalación.
  - Si no falta nada, se cierra sola a los 2,5 s y abre la madre.
  - Si falta algo obligatorio, se queda abierta con el informe: qué falta
    y cómo se arregla en esta plataforma, con «Copiar el informe» y
    «Continuar».
  - `--sin-presentacion` la salta.
- **La comprobación**: los motores (fue, fuf, fug, drtran, art), los GUIs,
  los esquemas de GSettings y los cargadores PNG y SVG de gdk-pixbuf.
  - Se buscan donde los busca la madre al lanzarlos (`atsw_programa`).
  - Lo opcional se dice en gris y no es un error: el SVG, drvarma y drvec
    (aún no van en el paquete) y el asistente (uv).
  - El mismo informe, a mano, en Ayuda ▸ «Comprobar la instalación…».
- **El icono**: el de la web.
  - Dentro de los programas lo dibuja `at_logo` (lib/barra) con cairo, sin
    cargador de SVG. Es el de todas las ventanas de los cuatro GUIs y el de
    «Acerca de».
  - En los paquetes: `lib/windows/atsw.ico` va en cada .exe (utf8.rc) y en
    el instalador; `packaging/macos/atsw.icns`, en ATSW.app. Los genera
    `packaging/icono/hacer.py`.

## PENDIENTE: la presentación pública — el canal de YouTube

Pedido del analista (25-oct-2026): ATSW tiene su propio canal, y los vídeos
de cada análisis van allí, uno por análisis (univariante, transferencia…).
La web y el MP4 de la release se quedan como alternativa.

| | qué | notas |
|---|---|---|
| | **crear el canal** (lo hace el analista) | cuenta de marca «ATSW» desde un Gmail personal (las cuentas de la UCM pueden tener YouTube restringido); handle `@atsw` o, si está cogido, `@atsw-series`; verificar con el teléfono (youtube.com/verify), sin lo cual no hay miniaturas propias |
| | imagen y banner del canal | imagen 800×800 con el icono de ATSW; banner 2560×1440 legible en el móvil; descripción con la web, GitHub y los créditos con la redacción de siempre |
| | la ficha de cada vídeo | título y descripción sacados de las fichas de la web, capítulos con las marcas de tiempo de las escenas, subtítulos en español (.srt) desde el guion de la narración, miniatura 1280×720 |
| | subirlos (el analista, en YouTube Studio) | a mano: la API exige un proyecto de Google Cloud auditado, o lo subido queda privado; primero «oculto» para revisar, luego público; una lista de reproducción en el orden univariante → transferencia |
| | la web y las notas | cada ficha de `web/index.html#videos` incrusta su vídeo de YouTube, con la descarga del MP4 debajo; el enlace al canal en el pie y en las notas de la v0.2.0 |

## PENDIENTE: el descriptivo de la suite — llegar a un consenso

Planteado por el analista (30-oct-2026). «La suite Box-Jenkins-Treadway» era
cierto mientras ATSW era univariante, intervención y transferencia. Con sima
ya no lo es: quien lea «VARMA-VEC» no lo encuentra en Box y Jenkins. La
escalera desde los modelos univariantes sí viene de Jenkins y Alavi y de la
tradición oral de Treadway, pero ellos nunca vieron un VARMA-VEC. Y sima no
siempre sube la escalera: también entra por el vector (Tiao y Box), y usa a
Lütkepohl y a Poskitt. **Hay que aclararlo en las próximas versiones de
ATSW, sin quitarle nada a la autoría de Treadway.**

Criterios de la conversación:

- La etiqueta Box-Jenkins-Treadway se queda donde es literal: **art y
  drtran**, cuyo descriptivo actual es cierto, y la GUI y la web mientras solo
  tengan univariante y transferencia.
- Los nombres de autores no van en el descriptivo de la suite. Cada nombre
  que se pone invita a preguntar por el que falta. Las citas van en la
  documentación de cada método.

Propuesta de partida (sin decidir):

> **ATSW — A Time Series Workshop: from univariate analysis to multivariate
> systems.** Univariate, intervention and transfer-function models in the
> Box-Jenkins-Treadway methodology; VARMA and cointegrated VARMA-VEC systems by
> exact maximum likelihood. Three MCP assistants, one for each rung: art, mtram
> and sima.

| | qué | notas |
|---|---|---|
| | **revisar todos los descriptivos de la suite** | inventario del 30-oct-2026, en las filas siguientes; un texto acordado, en inglés y en español |
| | `atsw` en PyPI | `art-python/atsw-suite/pyproject.toml`: «the Box-Jenkins-Treadway suite (… multivariate VARMA and cointegrated VEC)». Es el caso más claro. Se cambia con una versión nueva de atsw |
| | la cabecera de la suite | `art-python/atsw-suite/README.md` («ATSW — Box-Jenkins-Treadway time series suite» y la sección «Background — a modern Box-Jenkins-Treadway») y `art-python/llms.txt` |
| | este repositorio | `README.md` («la suite Box-Jenkins-Treadway») |
| | la web y la GUI, cuando lleguen los sistemas | `web/index.html` (meta y h1), `gui/atsw/src/main.c`, `gui/atsw/src/presentacion.c`, `packaging/linux/deb.sh`. Hoy son ciertos: ARIMA, intervención y transferencia |
| | sima | sus descriptivos (README, PyPI, instrucciones del MCP), para que digan las dos rutas sin nombrar escuela |

## PENDIENTE: el asistente

- **El asistente: una terminal con un MCP a medida (MTM) de los motores**,
  pedido del 19-oct-2026. Estudio y decisiones abiertas en
  `ESTUDIO-terminal-mcp.md`. En la comprobación de arranque entra como
  opcional.

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

**fue y fuf, los motores** (2026-10-17; cada uno en su `BUGS.md` con su repro)

- **fue BUG-0001**: el `.pre` va redondeado (AR, MA y δ a 4 decimales; ω a
  6), así que no es el óptimo exacto. Estimar sobre él vuelve a iterar, y
  drtran hereda el redondeo al mantener fijos los parámetros del `.pre`.
  `engines/fue/BUGS.md`.
- **fuf BUG-0001**: en el informe de previsión, con λ = 1 las variaciones
  salen ×100, y la columna ERR no está en la escala de σ. El gestor de
  previsiones va a leer esa columna. `engines/fuf/BUGS.md`.

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

**art / ventana de identificación** (los del motor, en `engines/art/BUGS.md`)

- La ventana enseña el AICc (CSS) y el peso de Akaike de **todos** los
  candidatos (`lib/analisis/an_identifica.c`, columnas `C_PESO` y `C_AICC`,
  ~322-338 y ~825). Desde el 2026-10-09, art-python no ordena por el AICc
  y sólo lo enseña como «[empate: ΔAICc=…]» en los candidatos empatados en
  similitud por el primer puesto, que es el único sitio donde decide
  (art-python BUG-0239). Fuera de ahí la columna invita a leer un ranking
  que la lista no sigue.
- Cuando el F no detecta estacionalidad, la ventana vuelve a identificar
  con `--harmonics off` (#17), pero deja `P` y `Q` en 1. El motor sigue
  proponiendo candidatos estacionales: (1,0)(1,0), (0,0)(1,0) y
  (1,1)(1,1) sobre un AR(1), un ARMA(1,1) y un AR(4) mensuales simulados
  (F p = 0,98, 0,64 y 0,42). En art-python, decidido «sin estacionalidad»,
  el paso 4 no propone candidatos estacionales (BUG-0209). Arreglo: pasar
  `-P 0 -Q 0` en esa segunda identificación.

**drvec** (no va en la primera versión)

- **Su MCP es sima**, no un cuarto asistente (decidido el 2026-10-13). sima
  0.1.0 se cierra sin el VEC, con las tres formas de identificación y sus
  ejemplos (publicada el 2026-10-15, en atsw 2.0.0). El VEC entra en sima 0.2.0, con el porte. Falta la regla de paso
  «≥ 2 series I(1) con realimentación → contrastar el rango», en sima y en
  mtram.
- **El porte a Python.** Decidido el 2026-10-11: la primera versión del porte
  **incluye** `-fixb2row`, `-xpre`/`-xlink`, `-xsys`, `-case pre`/`-trend` e
  `-irfboot` (`engines/drvec/docs/VERSIONS.md` §3, 0.11). Antes de fijar el
  oráculo: BUG-41/42 y `lib/diagmv`; el volcado de la LR* de cada réplica del
  bootstrap (`BASE.boot`); goldens propios de cada opción de esa superficie;
  y las lagunas (`-xsys` con q > 0, rango con exógenas o con `-trend`,
  `-xlink` con `-f`), portadas como rechazo. Después, la etiqueta
  `drvec-v0.12` es el oráculo.
- **Las lagunas, prioridad de la versión siguiente** de drvec y de su porte:
  primero en el C, luego portadas. La lista ordenada, en
  `engines/drvec/TODO.md` («PRIORIDAD»): el rango con exógenas, `-xsys` con
  MA, el rango con `-trend`/`-case pre`, la previsión y las bandas con
  exógenas y tendencia, `-beta file`.

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
