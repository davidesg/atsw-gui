# Changelog — drvec

Los informes completos están en [`docs/DEVELOPMENT_RECORD.md`](docs/DEVELOPMENT_RECORD.md)
(qué se probó y qué se refutó) y en [`docs/HOMOLOGATION.md`](docs/HOMOLOGATION.md)
(las mediciones). Los defectos, en [`docs/BUGS.md`](docs/BUGS.md). Etiquetas de
publicación: `v*`.

## Sin publicar

### `-xpre`/`-xlink`: entradas exógenas de transferencia en el VEC (MEJORA-2, fase 1)

Híbrido drtran–drvec: funciones de transferencia `ω(B)/δ(B)·Bᵇ` desde series
exógenas al **nivel** de las series del sistema, con el VEC como ruido (forma
«por resta», sintaxis de red de drtran `OUT <- IN b r s`, `r ≤ 2`). X congelada
y en el nivel de la serie (`refactor·BoxCox`, sin restar deterministas); al ir
empotrado no hay retro-pronóstico: se usan las observaciones reales y antes de
la primera X se mantiene en ese valor (anclada, la constante va a `E[W]`; por
eso caso 2 o 3). Se ajusta primero el VEC sin entradas; `ω = 0` lo reproduce al
bit (identidad de cruce, informada) y el LR frente a él es el contraste. Se
rechaza con la ruta `.inp`, `-differenced`, `-warma`, caso 1, los modos, `-f`,
`-estwin`, `-writeinp`/`-writeres`. 14 comprobaciones nuevas (335 en total,
todas pasan; los informes de referencia no se mueven).

### `-fixb2row i v`: contrastes LR sobre una fila de β (MEJORA-1)

`-fixb2` fijaba todo `B₂` o nada. `-fixb2row i v` fija la fila `i` en `v` y
estima las demás; ajusta también el modelo libre (con la misma búsqueda) y da el
LR, χ² con (filas fijadas)·`r` g.l.; con `-alpha`/`-weakex`, el contraste
conjunto. El LR es invariante a la normalización y el Wald no: en el caso que lo
motivó (PPA en un trivariante de precios de Ecuador) el Wald da p = 0,041 o
0,155 según sobre qué serie se normalice, y el LR 6,2131573620 / 6,2131573621 en
las dos. Con `s = 1` coincide al dígito con `-fixb2 v`. Se rechaza con `-fixb2`,
`-warma` y los modos. 18 comprobaciones nuevas (321 en total, todas pasan; los
informes de referencia no se mueven).

### La escala de las series (BUG-43) y las clases MA restringidas en `-lrtest` (BUG-27)

`-lrtest` reescala internamente las series mal escaladas (el LR no cambia; las
logL se dan en unidades originales): rao7 en bruto da ya lo mismo que
reescalado. Un ajuste único avisa (`SCALE`) con el factor. Con una clase MA
restringida, `-lrtest` ya no compara con una tabla que no aplica.

### El ajuste con r = 0 parte de la escalera (BUG-50), y lo que eso cambia

El ajuste directo con r = 0 se quedaba en un óptimo local: en mink-muskrat,
logL −6,3311 desde todos los arranques, frente a −1,0494 subiendo desde la
puerta. `-lrtest` usaba ese L(0): LR(0→1) caso 2 25,62 → 15,06. La afirmación
de homologación «r = 1» para estos datos queda retirada; el bootstrap da
p = 0,475. Las réplicas del bootstrap se ajustan ahora igual que los datos.

### Opciones que se ignoraban y detalles del informe (BUG-45)

Combinaciones sin sentido se rechazan con código 2; `-name` en la ruta `.inp`;
prefijos separados; `-specs` con q = 0; etiquetas de raíces; bloque VEC de
`-warma` en el orden del `.inp`; `-rungs` desde el peldaño inferior; deriva en
la ecuación del caso 3.

### Previsiones en las unidades de cada serie (BUG-36)

En la ruta `.pre` (y con `-interv`) la previsión y la evaluación móvil se
quedaban en las unidades del sistema, `refactor·BoxCox(z) − det`, aunque el
fichero decía «unidades del .inp». Ahora vuelven a z como en drtran: senda
determinista sumada, refactor dividido y Box-Cox invertido, bandas transformadas
por sus extremos y s.e. por el método delta. `-f`/`-estwin` con `-differenced`
se rechazan.

### La evaluación univariante de una semilla `.pre` (BUG-38)

Una media fija se evaluaba como 0, los factores AR/MA anuales y de frecuencia
fija se perdían o se truncaban, y los factores `ifadf` se ignoraban. Afectaba a
la identidad de cruce, a la semilla de `Σ` y al certificado de optimalidad.
Comprobado contra la verosimilitud exacta de statsmodels.

### Las fechas de la muestra y de los residuos (BUG-37)

La cabecera y el diagnóstico de residuos fechaban la muestra desde la primera
observación cruda, un periodo antes de la primera estimada: todas las fechas del
diagnóstico (máximos, mínimos, atípicos) salían un periodo adelantadas.

### El lector `.pre` rechaza los ficheros mal formados (BUG-39)

`MAXSTR` valía 80 dentro del lector (el de `main.h`), así que una línea larga
desplazaba el fichero: un `.pre` con λ = 1 se leía con λ = 0. Ahora tiene su
propia longitud de línea, y un `ifadf` vacío, una línea de muestra incompleta o
una serie truncada o con `NA` dan error con mensaje en vez de un cuelgue o una
estimación con ceros.

### Combinaciones de opciones que caían o mentían (BUG-30, BUG-31)

`-warma` con `-alpha`/`-weakex` y `-differenced` con `-matest`/`-artest` se
rechazan con un mensaje que dice por qué. `-lrtest -fixb2 -bootstrap` con M ≥ 3
ya no cae: el bootstrap rehace el `B₂` fijo del rango bajo H0.

### `-m 2` es la ML exacta sin truncar, y la puerta certifica sin truncamiento (BUG-47)

`-m 2` se etiquetaba «Conditional (Approximate)» y sólo apaga el truncamiento de
la sucesión `ξ`: las dos son ML exacta, y la cabecera ya lo dice. La puerta
comparaba contra `xitol` como tolerancia y suspendía un par independiente
correcto con θ = 0,9/0,95 (hueco 1,8e-3); ahora evalúa los dos lados sin
truncar y exige 1e-6. Las estimaciones no cambian.

### La condición de rango es la del Teorema 3 (BUG-46)

`G` era `σ_min(Λ⊥′Θ(1)B⊥)`, más fuerte que el teorema: negaba rangos correctos
(el contraejemplo, 0,0017 → 0,262). Ahora es `σ_s(Λ⊥′Θ(1))`. Remedida en el
banco, ya no hay hueco que justifique el umbral 0,2: queda como convención, y el
informe distingue G = 0 («DENIES THE RANK») de G bajo el umbral («NEAR»). `-specs`
retiene además el χ² cuando un peldaño tiene una raíz MA en el círculo unidad
(BUG-49). Las estimaciones no cambian.

### El contraste de rango: la tabla del caso 1, y los LR negativos

- **BUG-24.** El caso 1 (sin término determinista) se leía contra la tabla de
  `urca` con `ecdet = "none"`, que aún ajusta una constante libre: 14,90 al 5 %
  con `M−r = 2` donde corresponde 11,22 (MacKinnon–Haug–Michelis, 1999). El
  tamaño al 5 % pasa del 1,0 % al 3,5 %.
- **BUG-26.** Un LR negativo ya no se anuncia como «un ajuste no convergió»: la
  verosimilitud exacta de `W` lleva su estado inicial estacionario y no está
  anidada en `Λ = 0`; bajo H0 queda ≈ log T por debajo de la de Johansen. Se lee
  como «H0 no rechazada» con su nota, y el `-bootstrap` conserva esas réplicas
  (antes tiraba ~40 % de la masa nula): tamaño 7 % al 5 %. La tabla lleva la
  advertencia de que es la del LR condicional.

### La escalera, como punto de partida por defecto

Con `r ≥ 1` el ajuste parte ahora de donde dice el convenio de la suite: la
**puerta** (`r = 0`, todo diagonal, certificada contra la suma de los
univariantes), los peldaños por debajo del rango cada uno desde el anterior, y el
cruce del rango por la ruta (B) — `Λ` y `B₂` perfilados con el resto sujeto al
óptimo de abajo —, que antes era la opción `-seedgate` y estaba descartada por una
medida hecha con BUG-32 dentro (sujetaba el óptimo de abajo en las coordenadas
equivocadas). `-noladder` vuelve al arranque frío; una semilla explícita también.
Los demás arranques de la búsqueda se siguen probando y la tabla dice cuál gana.

Medido: en Dinamarca M = 5, r = 2 el ajuste sube de 832.36 a 858.34; en el banco
de trigo la escalera es mejor arranque que el frío en la mayoría de los casos, no
en todos. Y destapó BUG-49: la media móvil acaba en la frontera de
invertibilidad en casi todo el banco, con `-mafree` y también con los mejores
óptimos de `-marow`. De paso, un `Σ` singular ya no se acepta como óptimo (dos
series colineales daban logL 830 en una iteración).

### Las etiquetas nombran la serie correcta (BUG-18, 19, 20, 23, 28, 29)

drvec lleva dos órdenes de filas —el interno `[Y₁ ; Y₂]` de los parámetros VEC,
que es el del artículo, y el del `.inp`, `[Y₂ ; Y₁]`— y el informe los mezclaba:
`Γ`, `Θ`, `Q`, `Σ`, `P`/`D`, las pruebas de Wald «quién mueve a quién», los
residuos, los ficheros de residuos, la matriz `A` de `-alpha` y `-weakex` en
`-lrtest`. Todo sale ahora en el orden del `.inp`. Sobre un DGP conocido: el
informe decía que x mueve a y cuando es y quien mueve a x; ahora lo dice bien, y
la batería lo comprueba (8q). Una prueba de la batería **permutaba** la `Σ`
impresa para compararla con la banda de previsión: el defecto, compensado
dentro de la batería. El mensaje final nombra el `.out` y no el primer fichero de
entrada.

### La frontera de invertibilidad, diagnosticada (BUG-49)

Cuando la media móvil acaba con una raíz en la frontera —en el banco, casi
siempre—, el `.out` ya no se limita a marcarla con un asterisco: dice que es un
punto restringido y no un máximo interior, que los errores típicos del MA y
cualquier LR que use ese ajuste no tienen su distribución, y **en qué dirección**
está la raíz (el vector nulo por la izquierda de `Θ*(1)`, con sus pesos sobre
`[∇Y₂ ; W]` por nombre) y qué sugiere: sobrediferenciación de las tendencias
comunes si cae en `∇Y₂`, cuasicancelación AR/MA si cae en `W` con una raíz AR
cerca de uno. `-lrtest` marca cada LR construida sobre un ajuste así.

### La media móvil por defecto vuelve a ser libre

Con `q ≥ 1` y sin clase pedida, `drvec` estima otra vez la `Θ` **libre**: el
modelo de Mauricio (2006). Entre el 20-ago y el 23-sep el defecto fue `-marow`,
justificado por el Corolario 6.3, que resultó falso, y atribuido al artículo
BVECM, que no estima esa clase (su programa estima justo la entrada que `-marow`
anula). Lo que hacía atractivo restringir —la `Θ` libre cuesta de estimar desde un
solo arranque— lo resuelve la búsqueda de abajo. `-marow` sigue disponible. Una
cifra medida entre esas dos fechas sin bandera de clase es de `-marow`. Decisión
en `docs/SPECIFICATION_PLAN.md` §11; estudios en `docs/ESTUDIO_MAROW_2026-09-23.md`.
Cierra BUG-48; `-lrtest` vuelve a comparar modelos anidados (BUG-27, en parte).

### La búsqueda (P12)

Hasta ahora cada ajuste era **una** llamada al optimizador desde **un** punto de
partida: el ajuste principal, cada rango de `-lrtest`, cada réplica de los
bootstraps. Ahora es el mejor de varios arranques —la semilla pedida, la fría,
la canónica de Johansen, y la **cadena anidada** (`q = 0` → `-marow` → `-matri`
→ libre, cada uno desde el óptimo del anterior)— más las perturbaciones de
`-multistart`, que `-lrtest` por fin respeta. La cadena hace que una clase más
rica no pueda acabar por debajo de la que contiene. Un arranque que el motor
rechaza se encoge hasta que lo acepta. El `.out` lleva una tabla con cada
arranque, su logL y cómo paró; `-lrtest` marca las filas cuyo ajuste no paró por
el gradiente. El motor no se toca: el motivo de parada se lee donde `report()` lo
escribe.

Lo que movió, y hacia dónde: seis valores dorados de la batería **subieron**
—ninguno bajó; los otros doce no se movieron ni un dígito— y en UKconsumption la
secuencia de rango vuelve a ser 70.13 / 25.51 (las dos LR estaban permutadas
desde el 17-ago). Cierra BUG-25, BUG-33 y BUG-35; mejora BUG-43.

También: el manejador de errores de GSL ya no aborta el programa (devuelve el
error, y cada llamada lo comprueba); `-fixb2 v` y `-seedb2 v` siembran el resto
del vector desde el `W` que se va a ajustar.

## 0.10 — 2026-08-24

### La entrada por `.pre`, que es la del conjunto (P9)

```sh
drvec s1.pre s2.pre ... sM.pre p q r [opciones]
```

Un modelo univariante por serie, como `drtran`. De cada `.pre` se toma la serie
con su `w = refactor·BoxCox(z)`, sus términos deterministas —**restados**, con
las fechas de esta muestra— y su calendario; los ficheros se alinean **por
fecha** y se usa la intersección. Antes había que dar tres pasos a mano:
exportar a un `.inp`, `-interv` y `-seed`.

Certificada por dos identidades, no por parecido: reproduce la ruta `.inp`
**byte a byte** desde `ESTIMATION SUCCESSFUL`, y con una determinista en juego
coincide **exactamente** con `-interv` sobre los mismos modelos. `-name NAME`
nombra los productos (`-m` está tomado: es el método de estimación).

### El idioma (P7)

`src/drvec.c` entero en inglés: 363 comentarios y ~40 mensajes. Hecho contra un
invariante —quitar los comentarios de las dos versiones tiene que dar ficheros
idénticos byte a byte— porque traducir 1 200 líneas dentro de 7 600 es como se
mueve una línea de código sin querer. `tools/check_language.py` lo mantiene,
sólo sobre `drvec.c`: los ficheros vendorizados son de su dueño.

### Sistema de versiones y de defectos (P6, ampliada)

- `docs/VERSIONS.md`: el razonamiento de la versión sale del código y pasa a
  donde un lector lo necesita, con la política, las condiciones para llegar a
  1.0, y la regla de que un defecto **encontrado** no retrasa una versión y uno
  **tapado** sí.
- `tools/check_version.sh` y `tools/check_bugs.py`, los dos en la batería: el
  número tiene que decir lo mismo en seis sitios, y cada defecto tiene que tener
  número, estado y **coste medido**.

### El `.out`, con la estructura del conjunto (P10)

El `.out` de `drvec` no era un fichero de resultados: era un ensayo. De 283
líneas del caso canónico, más de la mitad eran prosa explicativa — por qué una
χ² no es un contraste, qué costó BUG-10 en el programa hermano, qué dice Mélard
sobre los autovalores de Π. Todo eso está además en la documentación, que es
donde va.

Ahora sigue a `drvarma`, que es el programa multivariante del conjunto y con
quien `drvec` comparte motor: cabecera `clave : valor`, bloques separados por
la línea de `=` de 61, y **una tabla de parámetros** con `Estimate`,
`Std.Error`, **`t-stat`**, **`p-val`** y códigos de significación — antes los
errores estándar iban pegados a la matriz, sin `t`, sin `p` y sin poder leerse
en columna. Seis secciones: parámetros, modelo VEC, diagnosis de cointegración,
Wald, diagnosis de residuos y previsión.

De la prosa quedan **cuatro avisos de una línea**, marcados con `!`, que son los
que cambian cómo se lee un número. 283 → 237 líneas.

Verificado sacando todos los literales numéricos de 22 informes antes y después
y exigiendo que el nuevo contenga al viejo. Destapó dos defectos: la ruta
`-warma` perdía la diagnosis, las raíces y las cifras del ajuste.

### Ganancia de largo plazo, retardo medio, y BUG-17 (P11)

`drvarma` acumula la respuesta del sistema estacionario y llama a la suma
ganancia de largo plazo. En niveles esa suma **diverge**: `G_k` no se apaga,
converge al efecto **permanente**. Los dos programas calculan lo mismo de todas
formas, y conviene ver por qué: la respuesta en niveles de `drvec` es la
acumulación de la de `drvarma`, así que sus **incrementos** son lo que `drvarma`
llama respuesta, y la ganancia y el retardo medio son sus mismas fórmulas
aplicadas a ellos.

Con su certificado: `β'Y_t` es estacionario, luego **ningún choque puede moverlo
permanentemente**, o sea `β'·gain = 0` exactamente. Da `1.2e-10`.

**Y ese certificado destapó `BUG-17`**, que no es de presentación: `drvec` lleva
**dos órdenes de fila** —el del `.inp` para los datos, `Γ`, `Θ` y las
respuestas; el interno `[Y₁ ; Y₂]` para `Λ` y `B`— y el informe los mezclaba.
Etiquetaba las filas de `Λ` con los nombres del `.inp`, así que en **todo ajuste
con `r < M` nombraba la serie equivocada**, incluidos los contrastes de
exogeneidad débil desde P6.8; y `-weakex i` restringía una serie distinta de la
que se le pedía. Las cifras eran correctas —las filas se contrastaban bien—,
sólo sus nombres estaban cambiados. Corregido, y **§4u del registro queda
marcado** hasta que se vuelva a medir su atribución por serie.

### Respuestas al impulso y descomposición de la varianza, en niveles (P11)

`diagnose.c` trae `impulse_response()` y `variance_decomposition()` ya
enlazadas, y **no se usan**: darían las respuestas de `Ȳ = (∇Y₂', W')'`, que no
es lo que se le pregunta a un modelo cointegrado. Lo que se pregunta es qué hace
un choque a los **niveles**, donde la respuesta se parte en permanente y
transitoria.

Tampoco se deriva nada nuevo: `level_error_map()` ya lleva una innovación al
error de nivel, y la respuesta de `Y_{t+k}` a un choque en `t` **es `G_k`**. El
mismo mapa certificado de las bandas, leído hacia delante, ortogonalizado con
`Σ = P D P'` — que ahora se factoriza en un solo sitio.

Con el certificado que lo ata a la previsión: `s.e.(h)` **es** la raíz de la
suma de respuestas al cuadrado hasta `k = h−1`, comprobado contra la tabla de la
misma corrida y coincidente a `1e-9`.

### La diagnosis que faltaba, y un defecto en el fichero compartido (P10)

Ejecutar el caso de los tres IPC por `drvarma` y por `drvec` y poner las dos
salidas al lado dejó el hueco a la vista: **1665 líneas contra 280**. `drvec`
sólo había vendorizado la rebanada multivariante de `diagnose.c` — Hosking y
Jarque-Bera — y le faltaba **toda la diagnosis por serie**: momentos con sus
fechas, gráfico estandarizado, histograma, ACF y PACF con sus bandas y
Ljung-Box.

Ahora se vendoriza `diagnose.c` **entero**, y la copia de `drvarma` y no la de
`drtran` por una razón que no es preferencia: la de `drvarma` fecha los residuos
y etiqueta las series con `data_freq`, `data_start_year`, `data_start_sub` y
`series_names`, globales que `drvec` ya tiene con esos nombres. Encaja sin un
cambio, que es lo que dice que era la copia correcta. 280 → 1530 líneas.

**Y al enlazarlo apareció `BUG-16`**: `File_HistSer` reservaba sus filas con un
byte de menos y `strcat` escribía el terminador fuera del bloque, en todos los
histogramas que esa rutina ha dibujado. Arreglado en los dos dueños —`drvarma` y
`drtran`— y traído de vuelta. Lo encontró el bloque `VALGRIND=1` de `drvec`, que
es la primera batería del conjunto que pasa valgrind por encima del informe
entero.

### El informe, en la notación de Johansen (P10)

El nombre del modelo pasa a **`VARMA-VECM`**: `VECM` es el acrónimo de los
manuales y el que trae quien llega desde un VAR-VECM. Y el informe entero pasa a
su notación — `alpha`, `beta`, `Gamma(k)`, `Pi = alpha beta'` — porque el
criterio es el **coste de entrada**: el cast a VARMA es lo que hace calculable la
verosimilitud exacta, pero es interior y no tiene por qué salir en el fichero.

El mapa: `alpha = -Lambda`, `beta = B`, `Gamma_k = F_k`. Sólo se movieron el
signo de `alpha` y de `Pi` —con sus `t`; el error estándar y el `p` no cambian—
y el nombre de `sigma_min(alpha_perp' Theta(1) beta_perp)`, de igual valor. El
autovalor de `Pi` pasa de `+0.557` a `-0.557`, que es como se lee: una velocidad
de ajuste.

También sale `(Mauricio 2006)` de la cabecera —lo que un fichero de resultados
declara es el modelo y el algoritmo, no una cita— y con ella se revisa entera.

### El refactor de `main()` (P8)

`main()` llevaba **2 320 líneas**: la línea de órdenes, cinco modos completos, el
multiarranque y el informe entero —793 líneas— a la misma indentación. Ahora
**468**, y nueve funciones: `report_fit`, `parse_cli`, `run_lrtest`,
`run_specs`, `run_ma_ar_test`, `run_multistart`, `run_rungs`, `read_inp_input`
y `run_eval`. Arriba del fichero va un mapa de dónde está cada cosa.

Un refactor afirma que **no mueve nada**, así que primero la red que lo
demuestra: `tools/golden.sh` guarda el hash de cada byte de cada informe sobre
24 configuraciones y se verifica después de cada corte. Los 24 siguen idénticos,
y va en la batería (bloque `[8n]`).

Con la red puesta, **BUG-14 queda cerrado de raíz**: `vec_shootx` fija ahora
`xitol`, de modo que olvidarlo ya no es posible. No movió ningún informe.

Batería: 235 → 253, y 263 con `VALGRIND=1`.

## 0.9 — 2026-08-22

Primera versión numerada. **Por qué 0.9 y no 1.0**: el programa no mejora la
previsión de un ARIMA por serie sobre su banco (HOMOLOGATION.md §4t) y su
especificación por defecto cambió el 2026-08-20. Ninguna de las dos es un
defecto que arreglar —son el estado del conocimiento, medidas y escritas—, pero
tampoco son algo sobre lo que poner un 1.0. El razonamiento va escrito junto al
`#define`, en `src/drvec.c`.

### La interfaz (P1)

- Tabla de opciones única: la lista que el parser **acepta** y la que `usage()`
  **enumera** salen de la misma estructura, y la batería compara las dos. Antes
  `usage()` cubría 22 de 33 opciones.
- Validación antes de tocar nada: una opción desconocida o un argumento que no
  es un número se rechazan con el uso y código de salida 2, en vez de estimarse
  en silencio con `p = q = r = 0`.
- `--version`, y la versión en la primera línea del `.out`.

### La especificación por defecto (P4)

- El defecto pasa a ser `Θ = [T₁₁ T₁₂ ; 0 0]` (`-marow`), con `-mafree` para la
  clase libre de antes. En esta clase el Corolario 6.3 hace que la frontera de
  invertibilidad **no se pueda alcanzar**: `det Θ̃(1) = det(I_r − ΣT_k)` y la
  puerta que el motor ya aplicaba **es** la condición de admisibilidad.

### La previsión (P5)

- `-f H`: previsión en niveles con bandas, por un solo mapa de error que lleva
  el error acumulado de `∇Y₂` al bloque `Y₁` a través de `Y₁ = W − B₂′Y₂`.
- `-estwin E`: evaluación de origen móvil fuera de muestra, con los parámetros
  estimados **una vez** sobre `1..E` y congelados.
- El certificado de un paso: la recursión se comprueba contra los residuos que
  devuelve el motor, no contra sí misma.

### La salida (P6)

- **`<base>.forecast`** y **`<base>.recursive`**, el sistema de ficheros del
  conjunto (`drvarma` v.04.1): un fichero por producto, con su cabecera
  reproducible, sin que haya que nombrar la ruta. La previsión lleva ahora
  FECHA en cada fila. `-C FICHERO` pasa a ser una redirección.
- **Bloque de hipótesis por defecto**: Wald sobre la covarianza que la
  estimación ya calcula —exogeneidad débil fila a fila, exclusión de la relación
  de largo plazo fila a fila, y la dinámica corta— sin reestimar nada. `Λ = 0`
  se imprime **declarando que no es un contraste** (bajo esa nula `B` no está
  identificada; el instrumento es `-lrtest -bootstrap`).
- `LICENSE`, `CITATION.cff`, este fichero, `docs/BUGS.md`, `make install`
  con `PREFIX` e integración continua.

### Arreglos

- **`xitol` sin inicializar en la evaluación de origen móvil** (BUG-14): `elf`
  recibía como tolerancia de truncamiento lo que hubiera en la pila. Las tres
  columnas de §4t se volvieron a medir con el arreglo y **reproducen la tabla
  publicada dígito a dígito**, así que la medida se sostiene.
- **`refactor` del `.pre` en `-interv`** (BUG-15): las deterministas se restaban
  sin deshacer el reescalado ×100 de la suite, y con eso el rango medido de los
  tres IPC salía `r = 1` donde es `r = 0`.
- **BUG-13**, la χ² del motor con `df ≥ 30`: la cola salía invertida. Arreglado
  en las cuatro copias de `nlatools.c` y verificado contra GSL.
- `nlatools.c` vuelve a ser idéntico a la copia canónica de `drvarma`: cero
  líneas de divergencia, y con ello cinco fugas menos.
