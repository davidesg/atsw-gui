# `drvec`: plan hasta una versión beta

*Objetivos, criterios de salida medibles y contingencias. Escrito el 2026-08-17,
sobre lo establecido en `ANALISIS_PRELIMINAR.md` (diagnóstico y siete
correcciones), `ESTUDIO_BVECM_vs_DRVEC.md` (el legado y su artículo) y el estudio
de la literatura de `literature/` recogido en §3.*

---

## 0. Qué significa «beta»

No «que compile y estime». La definición operativa que uso aquí:

> **Beta = alguien que no sea el autor puede usar `drvec` sobre datos propios,
> obtener un modelo VEC interpretable con errores estándar y un rango
> defendible, y saber cuándo no fiarse del resultado.**

De ahí salen cuatro requisitos, y el orden no es negociable:

1. **Fiabilidad numérica** — converge, o dice claramente que no.
2. **Interpretabilidad** — produce las formas BEC/Π y contrastes de exogeneidad
   débil. Sin esto `drvec` no responde a ninguna pregunta económica; da matrices.
3. **Homologación** — reproduce referencias del banco, y las que no reproduce
   están documentadas con evidencia.
4. **Reproducibilidad** — hay una batería de pruebas que falla cuando alguien
   rompe algo.

Cuando escribí esto teníamos parte de (1) y (3), nada de (2) y **nada de (4)**,
que era el riesgo de proceso más grande: todo estaba comprobado a mano. **F0 cerró
(4)** — hay batería (`make test`, 60 comprobaciones) y muerde. Sigue faltando (2)
por completo, y (1) es lo que F1 y F2 empujan.

---

## 1. Punto de partida

**Establecido y verificado:**

- La transformación de Mauricio es fiel al artículo, término a término, y
  coincide con la forma cerrada del AddOn con diferencia 0.000e+00.
- `elf()` (AS 311) validada a 2·10⁻⁸ contra una normal multivariante calculada
  a mano. **No se toca nunca.**
- Siete correcciones aplicadas (§0 de `ANALISIS_PRELIMINAR.md`), todas en
  `src/drvec.c`; el motor está byte a byte intacto.
- `-lrtest` con r = 0 acierta el rango en los dos casos con rango conocido
  (`mink_muskrat` → r = 1; `urca_UKconsumption` → r = 2, igual que `ca.jo`).
- `-fixb2` converge donde el modelo libre se para en termcode 3.

**Abierto:**

- **Termcode 3 residual.** La dirección exactamente plana ya no está, pero el
  optimizador sigue parando ahí en la mayoría de configuraciones.
- **El criterio de |Σ̂| no se cumple en el nivel.** *(Actualizado tras F1.)* Las
  cuatro configuraciones equivalentes ya concuerdan entre sí — dispersión 0.000048
  frente a 0.000225 —, pero concuerdan en ~0.00248 y el objetivo es ~0.00230. La
  banda 0.0022–0.0024 con la que se enunció esto queda corregida en
  `ANALISIS_PRELIMINAR.md` §5.11; el nivel pasa a F2.
- **La columna EML de Mauricio no es reproducible** y no se puede cerrar con el
  material disponible.
- No hay capa interpretable ni siembra desde la suite. *(La batería sí existe
  desde F0.)*
- **La normalización no está verificada**: `drvec` obliga a elegir qué series van
  en el bloque Y₁ y no diagnostica si la elección es apropiada (§3, Mélard).

---

## 2. El invariante que hace seguro todo lo demás

Antes de cualquier fase, esto es lo que permite avanzar sin romper nada. Es la
«puerta diagonal» de `drtran` (`drtran-python/src/drtran/cast.py`), adaptada:

> **Con r = 0 se tiene C̄ = I y H̄ = 0, luego Φ*ᵢ = Fᵢ y Θ* = Θ. Si además se
> activan `-diagar -diagma -diagcov`, el VARMA sobre ∇Y es diagonal, la
> verosimilitud exacta **factoriza**, y el logL conjunto debe ser **exactamente**
> la suma de los univariantes de ML exacta.**

**Verificado el 2026-08-17** sobre `mink_muskrat` (p=2, q=1, caso 1):

```
drvec conjunto, r=0, todo diagonal                      logL = -34.6278
ARMA(1,1) univariante sobre ∇log muskrat                logL = -20.0580
ARMA(1,1) univariante sobre ∇log mink                   logL = -14.5698
                                                  suma = -34.6278
                                            diferencia =   0.0000
```

Dos cosas que salen de aquí, y las dos importan:

- **Es un oráculo del cast.** Si el conjunto y la suma dejan de coincidir, el
  fallo está en `vec_shootx` o en la siembra, **nunca en `elf`**. Es exactamente
  el criterio con el que `drtran` valida el suyo.
- **Fija una convención que no estaba escrita:** `p` es el orden AR del VARMA
  **sobre Ȳ**, así que con r = 0 el orden AR efectivo **sobre ∇Y es p−1**
  (Φ*_p = −F_{p−1}C̄⁻¹H̄ = 0). Comparar contra un ARMA(p,q) univariante en vez de
  un ARMA(p−1,q) da un desajuste de 5.5 unidades que parece un fallo y no lo es.
  *(Me pasó al comprobarlo.)*

La puerta **no** aplica con r ≥ 1: ahí Φ* = C̄Φ̄ no es diagonal ni con las
banderas puestas, porque C̄ acopla. Es un invariante de un peldaño, no general —
pero es el peldaño desde el que se construye todo.

---

## 3. La literatura que condiciona el plan

*Estudiada el 2026-08-17. Cuatro piezas de `literature/`, y dos de ellas cambian
fases de este plan.*

### Mélard, Roy y Saidi (2004) — la construcción alternativa
`literature/TR0444.pdf`. Es el «Mélard et al. (2004)» que Mauricio cita como
única referencia previa para EML de VARMA parcialmente no estacionarios, y frente
al que se posiciona («simpler … does not suffer from nonuniqueness issues and is
not tied to the state-space framework»). Ruta espacio-de-estados: algoritmo de
Shea (1987, 1989), filtro de Kalman, recursiones tipo Chandrasekhar.

**Correspondencia, para el diccionario.** Usan la misma parametrización de rango
reducido: `C = C₁C₂` con `C₂ = [I_{k−d}, C₀]` normalizada. Como `C = −Φ(1) = −Π`
y Mauricio tiene `Π = ΛB′` con `B = [I_r; B₂]`:

> **C₁ = −Λ** y **C₀ = B₂′**.

**Un aviso que toca directamente a nuestro banco.** Escriben, sobre el criterio
de Ahn–Reinsel y Yap–Reinsel: *«Contrarily to what they say, the assumption on
Φ(1) does not imply that Σ Φⱼ = I + C has d unit eigenvalues. There are examples
where that procedure does not work, as Pham, Roy and Cédras (2003) have pointed
out.»* Y como `Π = Φ(1)`, «Π̂ tiene un autovalor nulo» es exactamente «ΣΦⱼ tiene
un autovalor unitario»: **el criterio que dicen que no está implicado**. O sea que
los autovalores de Π̂ que el banco cita de las Tablas 2 y A3 de Mauricio (0.0413 /
1.0602 y 0, 0.7212) son un indicio, **no un criterio de rango fiable**. Refuerza
que el instrumento correcto es el test LR — es decir, F4.

**Y destapa un hueco real de `drvec` para beta.** Ellos evitan la normalización:
usan la descomposición de Pham–Roy–Cédras basada en el **espacio nulo de Φ(1)**
—una tercera transformación, distinta de la C̄ de Mauricio y de la triangular de
Phillips— cuya ventaja es que **no hay que elegir qué variables van en el bloque
Y₁**. `drvec` sí obliga a elegirlas, por el orden de columnas del `.inp`, y **no
ofrece ningún diagnóstico** de si esa normalización es apropiada. Mauricio lo
advierte él mismo (p. 3648) y remite a Luukkonen et al. (1999) y Kurozumi (2005).
Para un programa que va a usar otra persona, una normalización mal elegida es una
trampa silenciosa. → **entra en el plan** (F3, y en los criterios de beta).

Aportan además errores estándar y un **estudio Monte Carlo de propiedades en
muestra pequeña**: precedente directo de F4.

### Johansen y Swensen (2024) — la aplicación, y reordena F3
`literature/Journal Time Series Analysis - 2023 - Johansen - …pdf`
(JTSA 45:248–268). Restricciones lineales sobre los coeficientes de ajuste α,
combinadas con expectativas racionales exactas. Definen tres modelos:

| | restricción | nº de parámetros de αβ′ |
|---|---|---|
| **H(r)** | α, β libres | `pr + r(p−r)` |
| **H₁(r)** | **α = Aψ**, A conocida p×s de rango s | `sr + r(p−r)` |
| **H₂(r)** | α = (a, a⊥φ), a conocida p×m | `mp + (r−m)(2p−r)` |

Dos lecturas que cambian el plan:

1. **H(r) es exactamente la parametrización de `drvec`.** Con p = M: `Mr + r(M−r)`
   = Λ (M·r) + B₂ (s·r). Confirmación independiente de que `drvec` parametriza el
   modelo canónico, y de paso da los grados de libertad de cualquier test:
   H₁(r) frente a H(r) son **(M−s)·r**.
2. **La exogeneidad débil es un caso particular de H₁(r)** (A selecciona las filas
   no nulas). Así que F3 **no** debe implementar «tests de exogeneidad débil» ad
   hoc, sino **α = Aψ con A suministrada por el usuario**, que subsume la
   exogeneidad débil, los vectores de ajuste conocidos (H₂) y las restricciones de
   expectativas racionales exactas.

**Y aquí `drvec` está inusualmente bien colocado**, mejor de lo que argumenté
antes: **Λ está *en* su vector de parámetros**, así que imponer α = Aψ es
sustituir M·r entradas libres por s·r y calcular Λ = Aψ dentro de `vec_shootx` —
el mismo tipo de cambio que `-fixb2`, local y barato. En coordenadas BVECM α es
*derivada*, así que la misma restricción exigiría optimización con restricciones a
través de la aplicación inversa. **Es el argumento más fuerte a favor de la
parametrización de `drvec` que ha aparecido en todo el estudio.**

**Posicionamiento que sale de aquí.** Johansen estima H(r), H₁(r) y H₂(r) por
regresión de rango reducido (Anderson, 1951) sobre un **VAR condicional**. Lo que
`drvec` aportaría es **las mismas clases de restricción bajo ML exacta y con
componentes MA**. Eso es una frase defendible sobre para qué sirve el programa.

Su Ejemplo 1 es consumo / renta del trabajo / renta del capital (Campbell 1987,
hipótesis de la renta permanente): la familia valor-presente, que es donde encaja
la aplicación de convergencia y ley de un precio único.

### Trenkler (2004) — arregla la parte más débil de F4
`literature/trenkler.pdf`. Aproxima las distribuciones asintóticas de tests de
cointegración de sistemas por una **Gamma** cuyos parámetros salen de
**superficies de respuesta**, con lo que *«can be easily used to derive arbitrary
p-values or percentiles»*.

Eso sustituye mi tabla de tres columnas indexada por M−r —que no tiene entrada
para el caso 3 y no da p-valores— por **p-valores arbitrarios calculables**.

**Con una salvedad que hay que respetar:** sus estadísticos son la familia
Saikkonen–Lütkepohl **con ajuste previo por los términos deterministas**, no
literalmente la traza ni el λ-max de Johansen. Lo que transfiere es la *técnica*;
para estadísticos tipo Johansen la fuente correcta es **MacKinnon, Haug y
Michelis (1999)**, que es justamente la que cita Mauricio.

### T28297-2 — material de aplicación
Capítulo de tesis sobre otras relaciones de cointegración (variables I(1) e I(2),
ratios de nominales). No es metodológico; se retiene como fuente de casos.

---

## 4. Fases

Cada fase declara objetivo, criterio de salida **medible**, y contingencia.

### F0 — Red de seguridad *(prerrequisito de todo)* — **HECHA el 2026-08-17**

**Objetivo.** Convertir en script lo que hoy se comprueba a mano.

**Lo entregado:** `tests/run_tests.sh` (y `make test`), 37 comprobaciones en
cuatro bloques — estructurales (el recorrido consume exactamente `npar` en **21**
configuraciones; esta cifra decía 19, y contadas al ejecutar el script de F0 son
21), invariantes sin referencia externa (monotonía de logL en r por
anidamiento; el ajuste restringido no puede batir al libre), la **puerta
diagonal** de §2, y valores de oro etiquetados explícitamente como *líneas base
de regresión, no respuestas correctas*.

**Y se comprobó que la batería muerde**, reintroduciendo a propósito cuatro
fallos ya corregidos:

| mutación | fallos que levanta |
|---|---|
| signo de Λ en Φ̄₁ (núcleo de la transformación) | **13** |
| la salida ignora `-diagma` (el fallo de §4.1) | 4 |
| B₂ traspuesta en `vec_shootx` (el **estimador**) | 1 |
| B₂ traspuesta en la **impresora** | **0** ← no lo caza |

El último es un hueco real y está anotado en el propio script: desde el arreglo
de §4.2 las dos impresoras comparten una copia, así que una transposición sólo
en la impresora es invisible desde la salida. Lo que sí guarda el orden que
importa —el del estimador— es el valor de oro de M=5, r=2, donde s=3 y r=2 y por
tanto una lectura traspuesta cambia la verosimilitud. Hizo falta añadir ese caso:
con s=1 (todos los que tenía) transponer es un no-op.

*Nota de método:* los valores de oro sólo valen para la entrada exacta con que se
midieron. El fixture de UK se escribe con `%.10f` y una copia con `%.8f` da un
logL distinto en la sexta decimal — me pasó, y por eso el fixture se genera
dentro del test.

- `tests/run_tests.sh`: el barrido de configuraciones (tres casos deterministas,
  banderas diagonales sueltas y combinadas, M=2 y M=5, r=0..3, `-levels` y
  `-differenced`, `-lrtest`, `-fixb2` con y sin valor), comprobando que no
  aparece `ERROR output` / `ERROR init_guess`.
- **Números de oro**: los logL documentados, con tolerancia declarada.
- **El test de la puerta diagonal** (§2) automatizado.
- `valgrind` sobre las rutas degeneradas (`r=0` con `p=1`, banderas diagonales).

**Salida.** `make test` verde; mover un número documentado hace fallar.

**Contingencia.** Si el test de la puerta necesita `drvarma`/Python y eso
complica la ejecución, se fijan los dos logL univariantes como constantes
declaradas en el script (son estables y están medidos aquí), anotando que son
valores de referencia y no recalculados. Se pierde independencia y se gana que
el test exista — el trato correcto en esta fase.

---

### F1 — Σ endurecida — **HECHA el 2026-08-17, con una contingencia ejercida**

**Objetivo.** Cerrar el criterio de |Σ̂|, copiando tres cosas de `drtran`
(`cast.py:build_sigma`): `var_i = exp(x_i)`, chequeo de definida positiva, y
sembrar las razones de varianza del dato en vez de en 1.

**Lo aplicado: dos de las tres.** La siembra por razones y el chequeo PD. La
tercera, `exp()`, **se probó y se revirtió con evidencia**, y eso contradice el
supuesto con el que escribí esta fase («si no mejora la convergencia se conserva
igualmente: la positividad garantizada es estrictamente mejor»). **La premisa era
falsa: `exp()` no es gratis.**

```
-differenced -case 3   antes de F1  0.04 s, 45 iter
                       con exp()    NO TERMINA en 90 s
M=5 r=2                con exp() y siembra en correlación:  NO TERMINA
```

Bisecando, ninguno de los dos cambios cuelga por separado: es la combinación. La
reparametrización logarítmica manda al optimizador a regiones donde cada
evaluación de la verosimilitud es lentísima. Y además **pierde verosimilitud**:
frente a la variante sin `exp()`, quedaba peor en tres de cuatro configuraciones.

Sin `exp()` la diagonal puede irse a negativo, así que **el chequeo PD deja de
ser un extra redundante y pasa a ser la guarda que sostiene la parametrización**.
Cuesta una Cholesky y no toca la geometría.

**Efecto medido** (mink–muskrat p=2 q=1 r=1, y los dos sistemas grandes):

| configuración | antes de F1 | después | |
|---|---|---|---|
| `-case 1` | −8.0795 | **3.6856** | +11.77 |
| `-case 2` | 6.4679 | 6.4786 | +0.01, y **converge** |
| `-case 3` | 5.6105 | 6.5140 | +0.90 |
| `-case 2 -diagar` | 0.8239 | −2.5420 | **−3.37** |
| `-case 2 -diagma` | 0.8927 | 0.8817 | −0.01 |
| `-case 2 -diagcov` | 0.5817 | 0.5696 | −0.01 |
| `-case 2 -fixb2` | 5.2137 | 5.4717 | +0.26 |
| `-case 2 -fixb2 0` | −7.4652 | −8.4835 | −1.02 |
| **Denmark M=5 r=2** | 749.5833 | **828.8447** | **+79.26** |
| UK M=3 r=2 | 570.22971 | 570.22971 | = |

Gana mucho donde predecía la teoría —Denmark mezcla logaritmos con tipos de
interés, así que las razones de varianza importan— y en el caso 1, que era el
peor. Pierde en `-diagar`, y eso hay que decirlo.

**Criterio de salida: NO cumplido en el nivel, cumplido en la dispersión.**

```
                      antes de F1     después
niveles caso 2         0.002461       0.002461
niveles caso 3         0.002569       0.002460
antiguo  caso 2        0.002344       0.002482
antiguo  caso 3        0.002459       0.002508
                       ---------      ---------
dispersión             0.000225       0.000048   <- 4.7x más apretada
media                  0.002458       0.002478
objetivo                      ~0.00230
```

*(Las dos columnas medidas sobre el mismo fixture, el de `%.10f` que genera la
batería, con el binario de HEAD para la columna «antes». La primera versión de
esta tabla daba 0.002373 y 0.002483 en las filas «antiguo» porque venían de una
copia a `%.8f` del `.inp` — la misma trampa de precisión que ya está anotada como
nota de método en F0, y que aquí volvió a morder.)*

Las cuatro configuraciones son el mismo modelo salvo reparametrización, y ahora
**concuerdan entre sí casi cinco veces mejor**; pero concuerdan en ~0.00248, un
8 % por encima del objetivo. **Y hay que decir lo que la tabla también muestra:**
antes de F1 el layout antiguo en caso 2 estaba en 0.002344, *más cerca* del
objetivo que cualquier valor de ahora. La dispersión se cerró hacia arriba, no
hacia el objetivo. Eso es exactamente la contingencia prevista: **se reformula y
el objetivo de nivel pasa a F2 (siembra)**, que era la otra causa candidata.

*Y de paso hubo que corregir el criterio mismo*: la banda 0.0022–0.0024 que
había fijado citaba a Chan & Wallis como apoyo **aunque su 0.00246 cae fuera de
ella**. El objetivo nítido es la búsqueda global sobre el mismo modelo y los
mismos datos (~0.00230); Chan & Wallis calibra magnitud, no es objetivo. Ver
`ANALISIS_PRELIMINAR.md` §5.11.

**Añadido a la batería** (37 → **41 comprobaciones**): un invariante que sigue
justo lo que esta fase pretendía — que |Σ̂| coincida entre los dos layouts del
mismo modelo, dentro del 5 % — y el layout antiguo con q=1 en los tres casos
deterministas, que es donde apareció el cuelgue. Y el arnés tiene ya **timeout
por corrida**, porque el cuelgue de `exp()` lo habría colgado en vez de
reportarlo; ese era un hueco de F0.

**Nota honesta de cobertura:** por mutación, quitar el chequeo PD **no levanta
ningún fallo**. Ningún caso del banco lleva a Σ no definida positiva, así que el
chequeo es un seguro sin ruta de prueba. Está anotado en el script.

---

### F2 — Siembra desde la suite (`fue` / `.pre`)

**Objetivo.** Que `drvec` deje de arrancar en frío. Es el punto flaco real y la
razón de que `drvec` esté hoy fuera de la arquitectura de la suite.

El diseño de la suite es un óptimo binivel con los ficheros como interfaz
(`drtran-python/docs/LADDER_AS_OPTIMISATION.md`): **ART** identifica y emite
`.inp`; **fue** estima y emite `.pre`, que es *«un óptimo en forma
re-ejecutable»*; el consumidor lee los `.pre` y **sólo ensambla**.

#### F2.0 — Lo estudiado, y dónde está *(estudiado el 2026-08-17)*

Esta fase se abrió derivando cosas que ya estaban escritas en la suite. Queda
aquí para no repetirlo. Hay **dos implementaciones de cada pieza**, `fue` y
`drtran` en C y en Python; **para `drvec`, que es C puro, la referencia es la de
C.**

| fuente | qué aporta |
|---|---|
| `atws/fue/fue/docs/FILE_CONTRACT.md` | la **gramática autoritativa** de `.inp`/`.out`/`.pre`, campo a campo. Declara que ante una discrepancia manda el parser (`fue/src/fue/inp.py`, que reproduce el orden de lectura de `fue-1.13.1/src/fue.c` §3.0–3.7) |
| `atws/fue/fue/docs/CAST.md` | qué es el cast, su contrato de empaquetado, sus modos de fallo y el problema de estado global (§9) |
| `drtran/BRIDGE_DESIGN.md` | **el precedente directo en C**: `fue` → `drvarma` por lectura de `.pre`, con su criterio de validación y la nota de escala numérica |
| `drtran/src/fue_pre_reader.c` | el lector de `.pre` en C, ya factorizado (601 líneas). En `fue` el lector vive **dentro** de `src/fue.c` y no es enlazable |
| `drtran-python/src/drtran/pre.py` | qué campos hace falta validar, y la regla de escala medida (`check_scale`) |

#### F2.1 — Procedencia del código traído *(verificado, no supuesto)*

Se **reutiliza** el lector de `drtran`, no se escribe uno nuevo:

- `src/fue_pre_reader.c` y `include/fue_pre_reader.h`, copiados de
  `drtran/src/` y `drtran/include/`. **Un único delta**: comentar
  `#include "drtran.h"`, que no se necesita — comprobado que el lector sólo usa
  `Tusmodel`, `Tseries` y los allocators de `nlatools`, cuyos nombres coinciden
  en los dos proyectos. `diff` contra el original cabe en tres líneas.
- `struct Tusmodel` copiada **byte a byte** de `drtran/include/main.h` a
  `include/main.h`.
- `struct Tseries` gana `numbering` y `refactor`, los dos campos que `drtran`
  añadió para FUE. **Cambio inerte**: `Tseries` no se referencia en ningún `.c`
  de `drvec` (0 apariciones en los cinco), así que el motor no se entera.
- `Easter()` **ya estaba** en `src/nlatools.c:679`; sólo faltaba el prototipo en
  `main.h`. No se ha traído código de más.
- Comprobado que compila sin errores ni declaraciones implícitas, y que sus
  únicos símbolos externos son `DateToObs`, `ObsToDate`, `Easter`, `vector`,
  `ivector`, `matrix` y `stderr` — todos del motor de `drvec`.

**Riesgo asumido, dicho en voz alta:** es una copia, así que puede derivar del
original. La alternativa —enlazar contra `drtran`— acoplaría dos programas que
son independientes por diseño. El original queda citado arriba; si el `.pre`
cambia de formato, los dos ficheros hay que revisarlos a mano.

**Y la copia ya ha divergido, por un arreglo.** El lector de `drtran` lee mal
**todo `.pre` anual**: se salta la sección de factores de la diferencia anual,
que los dos escritores de `fue` emiten siempre. Arreglado aquí y **declarado
como BUG-11 en `drtran-python/docs/BUGS.md`**, con la medida y la reproducción.
El lector de `fue` (`fue.c:819-832`) sí tiene la rama que a `drtran` se le
perdió al extraerlo, así que el fallo no viene del código de Mauricio.

#### F2.2 — El contrato de ficheros, y las seis cosas que atan a `drvec`

Todo esto sale de `FILE_CONTRACT.md` y **no es negociable**, porque el formato no
avisa cuando se incumple:

1. **El parser es posicional y no valida nada.** La regla es de Treadway (manual
   de DRVUS, 2001): el programa *«no interpreta los comentarios […] solamente lee
   los números que espera encontrar en la posición correcta»*. Las líneas que
   empiezan por `**` son separadores: su texto es indiferente, **su presencia no**.
   Reordenar, quitar un separador o meter una línea en blanco no da error: da otro
   modelo, o un cuelgue. Corolario: **un `.inp` que parsea no es un `.inp`
   correcto**, y por eso ninguno se escribe a mano — lo escribe un programa (§5).
2. **`drvec` escribe `.inp`, nunca `.pre`.** El `.pre` es una *afirmación de
   optimalidad* y sólo la puede hacer quien estimó; el fichero no lleva marca de
   autor, así que uno fabricado es indistinguible aguas abajo de uno legítimo.
   `drtran.write_inp` existe justamente por esto. El invariante del `.pre` es
   comprobable: se re-ejecuta `fue` sobre él y los números no se mueven.
3. **ASCII puro al escribir.** BUG-0010 de `fue`, abierto: los ficheros escritos
   por el C en un sistema Latin-1 rompen el parser de Python con
   `UnicodeDecodeError`. Los fuentes del motor de `drvec` **son Latin-1**, así que
   esto es una trampa preparada: el escritor de `.inp` no debe emitir un solo byte
   fuera de ASCII, ni en los comentarios ni en los nombres de serie.
4. **Cabecera anual.** Con `freq = 1` el parser lee `nobs, <ignorado>, begyear,
   nombre` y fuerza `begtime = 1`; el año es el último token numérico antes del
   nombre. Nuestro banco es anual (`mink_muskrat` empieza en 1851), así que es
   exactamente el caso frágil — y el que tuvo el BUG-0018.
5. **Las seis secciones ARMA van siempre**, aunque el recuento sea `0`; no se
   pueden omitir. Y la notación es **factorizada**: `2 1 1` son *dos* factores de
   primer orden `(1−φ₁B)(1−φ₂B)`, mientras `1 2` es *un* AR(2). Para que los
   coeficientes del `.pre` mapeen directamente sobre φ₁…φ_k hay que pedir
   `1 k` — un solo factor de orden k. Cada coeficiente es un par `valor flag`,
   con `flag = 1` estimar y `0` fijar.
6. **μ es la media de la variable ya diferenciada**, y la semilla importa: en
   BUG-0012 un μ₀ = 2.5 contra una serie de media 17.06 deja a `fue` parado en la
   frontera del AR, **6.86 de log-verosimilitud** por debajo del óptimo publicado,
   mientras que desde cualquier valor entre 6 y 20 llega en siete iteraciones. La
   lección es general y aplica a la nuestra: sembrar μ de la media de la
   diferenciada.

#### F2.3 — El cast: qué aplica a `drvec` y qué no

De `CAST.md`:

- **El cast es un puntero a función que se pasa a `est()`**, que no sabe qué es un
  modelo: *«the cast is replaceable by construction»*. Esto reencuadra la frase
  con la que abrí esta fase: `vec_shootx` **ya es** un cast de esa familia, con la
  misma firma. `drvec` no está fuera de la arquitectura de la suite; está dentro y
  lo que le falta es el otro extremo, la siembra.
- **El orden de empaquetado de `x[]` es un contrato público que no está escrito en
  ningún sitio ejecutable**: si cambiara, nada protestaría y cada consumidor
  calcularía otro modelo en silencio. La pregunta abierta nº 5 de `CAST.md` es si
  ese orden debería ser comprobable por máquina — y **`drvec` ya lo comprueba**
  desde F0 (el bloque estructural: el recorrido consume exactamente `npar` en 24
  configuraciones). Eso es algo que `drvec` puede devolverle a la suite.
- **Estado global, §9:** el cast lee modelo, serie y datos de globales de módulo,
  así que no es reentrante y **no pueden estar dos vivos a la vez**. Confirma R3 y
  fija el diseño: **no se llama al cast de `fue` en ejecución**; se leen sólo los
  números del `.pre`. Es lo que hace el C de `drtran`.
- **`elf` concentra la escala**: quien lea `qq` como si fuera Σ obtiene la forma
  bien y la magnitud mal. Es exactamente lo que `drvec` estableció por su cuenta
  en §3.6 y F1, y `BRIDGE_DESIGN.md` §10 llegó al mismo `Q[1,1] = 1`.
  **Convergencia independiente de tres programas sobre la misma decisión.**
- El cast de `fue` aplica el **giro de invertibilidad** de un MA(1) con |θ| > 1
  (θ → 1/θ), lo que deja al optimizador sin restringir a cambio de un pliegue en
  la superficie (mecanismo del BUG-0005, óptimo espurio). `drvec` no hace nada
  parecido, y por tanto un θ del `.pre` viene ya del lado invertible.

#### F2.4 — La escala: el riesgo numérico concreto

`BRIDGE_DESIGN.md` lo mide y `pre.py:check_scale` lo convierte en regla:

- `qnewtopt/cdgrad` usa un paso de diferencias finitas
  `eta^(1/3)·max(|x|, 1.0)` ≈ **6·10⁻⁶ absoluto**. Con varianzas de orden 10⁻⁵ el
  paso es mayor que el propio parámetro, lo empuja a negativo, el cast devuelve
  `ifault = 1` y el gradiente que sale es basura.
- Medido en la suite: la misma serie con Δlog ~0.002 **no converge en 2 minutos**
  y con Δlog ~0.2 converge en 23 iteraciones y un segundo. Banda cómoda: |w|
  típico entre 0.01 y 100, objetivo ~1. El `refactor` de `fue` (consejo de
  Treadway, mayo de 2001, sobre la norma del gradiente) es la palanca, y **hay que
  multiplicar, no dividir** — con la salvedad que `check_scale` añade: subir la
  escala de una serie ya grande la empeora.
- **Por qué `drvec` está protegido en su propio Σ:** la escala está concentrada en
  `sigma2` y el bloque de covarianza lleva razones O(1) con var₁ = 1. Eso es lo
  que dejó F1, y explica de paso por qué `exp()` sobraba.
- **Pero los `.inp` que `drvec` escriba sí necesitan `refactor`**, porque ahí es
  `fue` quien estima varianzas directamente. Para `mink_muskrat` el ∇log tiene
  |w| mediano ~0.2, que da `refactor = 10` por la regla de `check_scale`.

#### F2.5 — La cadena, y el álgebra que hay que respetar

```
B₂ preliminar (OLS estático, ya está en init_guess)
   -> construir Ȳ = (∇Y₂ ; W)          <- observable una vez B₂ está fijado
      -> drvec escribe un .inp por componente   (ASCII, refactor, ARMA(p−1,q))
         -> ART identifica / fue estima          ->  un .pre por componente
            -> drvec lee los .pre y arma la semilla:
                 Θ                             <- de los .pre
                 razones de Σ, Λ, F            <- de la regresión condicional
                 B₂                            <- OLS, o congelado con -fixb2
```

*(Este diagrama decía «Θ, F propias y razones de Σ ← de los .pre». **Las dos
últimas no son posibles**, y saberlo salió de leer el formato y el álgebra, no de
probar: el `.pre` **no lleva σ²** —la varianza de innovaciones sólo aparece en
ficheros `fuf`— y el AR está **sobredeterminado**, porque los univariantes dan
Φ*_k para k = 1..p mientras el modelo sólo tiene F_1..F_{p−1} y Φ̄_p = −F_{p−1}C̄⁻¹H̄
queda determinada. Así que lo único que un `.pre` puede sembrar es Θ — que es,
justamente, lo único que arrancaba en cero.)*

Tres cosas que hay que hacer bien y que ya están resueltas sobre el papel:

- **Coordenadas.** El MA que ve un univariante de Ȳ es Θ̄(L) = C̄Θ(L)C̄⁻¹, no Θ.
  Con Θ̄ⱼ diagonal salida de los univariantes, **Θⱼ = C̄⁻¹Θ̄ⱼC̄** es exacto y
  calculable con lo que `vec_shootx` ya construye. Sembrar los θ del `.pre`
  directamente en Θ sería un error silencioso en cuanto r ≥ 1.
- **Órdenes.** `p` es el orden AR **sobre Ȳ**, así que con r = 0 el AR efectivo
  sobre ∇Y es **p−1** (§2). El `.inp` de cada componente pide ARMA(p−1, q), y
  pedir ARMA(p, q) mete un desajuste de 5.5 unidades que parece un fallo y no lo
  es. Ya mordió una vez.
- **El `.pre` no da Λ.** Los univariantes no llevan acoplamiento entre ecuaciones
  y Λ *es* el acoplamiento: una semilla puramente univariante daría Λ ≈ 0, el peor
  sitio posible. Por eso la cadena mantiene la regresión condicional para Λ. Es la
  misma decisión que `x0_from_pre` de `drtran`, que **arranca las transferencias
  en cero a propósito**.

#### F2.6 — Qué es exactamente el hueco de hoy

Conviene tenerlo medido antes de tocar nada: el arranque en frío de `drvec` no es
tan frío como decía esta fase. `init_guess` ya siembra B₂ por OLS estático, Λ y
F_i por regresión condicional y Σ de los residuos de esa regresión. **Lo único
que arranca en cero exacto es Θ** (`src/drvec.c:366–369`). Ése es el hueco que
F2 llena, y el resto del beneficio esperado es refinamiento.

**Salida.** El logL sembrado ≥ el de arranque en frío en **todas** las
configuraciones del banco, y la puerta diagonal (§2) sigue cuadrando a 1e-9.

#### F2.7 — Resultado: **el puente está construido; la siembra NO cumple**

*(Medido el 2026-08-17/18.)*

**Lo entregado y funcionando:**

- El lector de `.pre` de `drtran`, reutilizado, **con un fallo suyo arreglado**
  (F2.1) — y el puente `src/fue_bridge.c` con las cuatro funciones que el motor
  de `drvec` no aportaba.
- `-writeinp` (un `.inp` por componente de Ȳ) y `-writeres` (uno por residuo de
  la regresión condicional): ASCII puro, cabecera anual correcta, las seis
  secciones ARMA, la sección de diferencia anual, `refactor` por la regla de la
  suite y **μ siguiendo el caso determinista** (caso 1 ninguna, caso 2 sólo las
  de W, caso 3 todas). Esto último no estaba y era un error: dejar que `fue`
  estimara una media que el modelo conjunto no puede representar devuelve una θ
  condicionada a algo que no existe.
- `-seed` (ruta de residuos) y `-seedybar` (ruta de componentes de Ȳ).
- Batería 41 → **45 comprobaciones**, con los dos fallos que mordieron durante la
  fase convertidos en test: que lo escrito sea ASCII puro y que lleve la sección
  de diferencia anual.

**Las dos rutas, y por qué una está mal fundada.** La cadena que esta fase
preveía —univariantes de los componentes de Ȳ— **no sirve para sembrar Θ**: el
marginal univariante de un componente de un VARMA **no es Θ̄_ii**, porque
marginalizar mezcla AR y MA e infla los órdenes. Vale para `drtran`, donde cada
serie *es* su bloque y el único acoplamiento es la transferencia; no vale para el
VEC, donde C̄ y Λ acoplan densamente. La ruta bien fundada son los **residuos de
la regresión condicional**: e_t = Θ(L)A_t, cuyo marginal por componente es MA(q)
**exactamente**. Y las dos están en coordenadas distintas — Θ̄ = C̄ΘC̄⁻¹ en la
primera, Θ directamente en la segunda—, lo que es un error silencioso esperando:
la misma θ en la coordenada equivocada da otro modelo sin que nada proteste.

**La medida** (mink–muskrat p=2 q=1 r=1; `fue` estimando cada componente):

| configuración | frío | sembrado, residuos | sembrado, Ȳ |
|---|---|---|---|
| `-case 1` | 3.6856 (t3) | −10.7679 (t3) **−14.45** | −9.4504 (t3) −13.14 |
| `-case 2` | 6.4786 (**converge**) | 6.4461 (t3) −0.03 | 4.4108 (cv) −2.07 |
| `-case 3` | 6.5140 (t3) | 5.4576 (cv) −1.06 | 6.8901 (t3) **+0.38** |
| `-case 2 -diagma` | 0.8817 (t3) | 0.6450 (t3) −0.24 | 0.8927 (cv) +0.01 |
| `-case 2 -diagcov` | 0.5696 (cv) | 0.5900 (cv) **+0.02** | −0.9650 (cv) −1.53 |
| `-case 2 -fixb2` | 5.4717 (cv) | 6.5484 (t3) **+1.08** | 6.5389 (cv) +1.07 |

*(cv = el optimizador converge; t3 = para en termcode 3. Nótese que la siembra
cambia también **eso**: el caso 2 pasa de converger a pararse, y el caso 3 al
revés.)*

**Criterio de salida: NO cumplido**, y no por poco: la ruta fundada empeora en 4
de 6 configuraciones. El resto del banco no dice nada porque UK y Denmark van con
q = 0, así que no hay bloque MA que sembrar.

**Y el hallazgo que sí vale la fase.** En el caso 1, mover Θ de cero exacto a
diag(0.0108, 0.0611) —una perturbación de centésimas en dos parámetros— cuesta
**14.45 unidades** de log-verosimilitud en el punto donde para el optimizador. Que
eso no es un fallo de la siembra lo prueba una identidad que ahora está en la
batería: **con θ = 0 en los `.pre`, `-seed` reproduce el arranque en frío bit a
bit** (3.6856397544 en el caso 1, 6.4786201604 en el caso 2). Es decir, la
fontanería —lector, expansión de factores, coordenadas, orden del vector— está
bien, y lo que se está midiendo es la superficie. Es la evidencia más fuerte
recogida hasta ahora a favor de R1: **el problema de `drvec` no es de dónde
arranca, sino por dónde puede caminar.**

**Lo que esto le hace al plan.** La premisa con la que escribí F2 —que `drvec`
está «fuera de la arquitectura de la suite» y que arranca «en frío»— era falsa en
las dos mitades: `vec_shootx` ya es un cast de la familia de `est()`, y el
arranque ya sale de los datos salvo Θ. La fase se cierra con el puente hecho y
utilizable —que es lo que permite que ART y `fue` entren en el flujo de `drvec`—
y con la siembra **medida y rechazada como palanca de convergencia**. El objetivo
de nivel de |Σ̂| que F1 aplazó aquí **sigue sin resolverse**, y ya no hay motivo
para esperar que la siembra lo resuelva.

**Cobertura del arreglo del lector.** Por la ruta de estimación ese fallo es
invisible —la siembra sólo lee el bloque MA, que va *antes* de la sección
corrompida—, así que por mutación levantaba **cero** fallos. Se ha cerrado con un
arnés propio, `tests/pre_probe.c`: enseña lo que `read_fue_pre` ve (nobs, freq,
`refactor`, primera, segunda, penúltima y última observación) y la batería lo
compara **contra lo que el fichero dice**, sacado con `awk` del propio `.pre`. No
hay número de oro que mantener, así que la comprobación no envejece. Con el fallo
restaurado levanta **2 fallos**.

#### F2.8 — Los contratos de la escalera, y qué medía F2.7 en realidad

*(2026-08-18. Esta sección corrige el encuadre de F2.7, no sus números.)*

F2.7 midió «¿mejora el logL final si siembro Θ?» y respondió que no. **La
pregunta estaba mal planteada**, y la razón es que la suite ya tiene definido qué
significa sembrar y cómo se certifica — `drtran-python/docs/LADDER_AS_OPTIMISATION.md`
§2.1 y §3 — y yo estaba reinventándolo:

```
   Σ_i logL(serie i)  =  logL(ajuste conjunto DIAGONAL)  ≤  logL(modelo conjunto)
        \___________________________/                        \____________/
             la factorización: prueba el CRUCE                el término extra

   logL(ajuste diagonal)  ≥  logL(EN los valores almacenados)
        con igualdad ⟺ los valores almacenados son los óptimos univariantes
        -> el CERTIFICADO, que cuesta UNA evaluación y ninguna optimización
```

F2.7 sembraba **sólo Θ** y dejaba Λ, F y Σ de la regresión condicional. Ese punto
**no es el óptimo de nadie**, así que no hay certificado que reclamar: medía el
comportamiento del optimizador desde un punto híbrido, no el transporte de un
óptimo. Lo que hacía falta era sembrar **el bloque univariante entero**.

**Y para eso hubo que corregir dos cosas que F2 daba por imposibles:**

- **σ² sí se puede sembrar.** Es cierto que el `.pre` no lo lleva, pero lleva **el
  modelo y los datos**, así que σ² es *derivable*: se evalúa la verosimilitud
  univariante con el mismo `elf` de la suite (`pre_univariate` en `drvec.c`).
  Quedarse en «no está en el fichero» era quedarse en la primera mitad del
  argumento.
- **`r = 0` es ahora una configuración de primera clase.** Antes sólo se llegaba
  a ella por dentro de `-lrtest`, que es justo donde no se puede inspeccionar. Y
  es el **peldaño diagonal**: donde C̄ = I, H̄ = 0, la verosimilitud factoriza y
  viven los dos contratos.

**El contrato se cumple** (mink–muskrat, `2 1 0 -case 1 -diagar -diagma -diagcov`,
sembrando desde los `.pre` que `fue` escribió sobre los componentes de Ȳ):

| | |
|---|---|
| logL conjunta **evaluada** en los valores del `.pre`, sin optimizar | −34.6278402874 |
| suma de las logL univariantes, calculada por `drvec` desde los `.pre` | −34.6278225170 |
| **identidad de cruce** (diferencia) | **1.8·10⁻⁵** |
| logL del **ajuste** diagonal desde esa semilla | −34.6278400462 |
| **certificado** (ajustar − evaluar) | **+2.4·10⁻⁷ ≥ 0** ✔ |

Las dos diferencias son del orden del redondeo del propio formato: **un `.pre`
guarda sus coeficientes con `%.6f`**, y eso es lo que acota lo afilado que puede
ser el certificado. Los dos contratos están ya en la batería.

*Un detalle que costó un signo:* `fue` estima sobre `w = refactor·z`, y una
log-verosimilitud **no es invariante de escala**. Para comparar con la conjunta
hay que devolverle el jacobiano, `logL_z = logL_w + n·log(refactor)`. Sin eso la
identidad de cruce falla por 280.92 en este caso —que es exactamente
2·61·log(10)— y parece un fallo del transporte. Hecha la corrección, las
univariantes del `.pre` salen **−20.057976** y **−14.569847**, que son las
constantes de la puerta diagonal (§2) reproducidas por una tercera vía.

**Y la escalera transporta exactamente un peldaño.** Con el bloque entero
sembrado y r ≥ 1, la semilla **empeora el punto de partida**, mucho:

| configuración | partida en frío | partida sembrada |
|---|---|---|
| `-case 1` (r=1) | −283.33 | −256.56 (+26.8) |
| `-case 2` (r=1) | −8.74 | **−25.95 (−17.2)** |
| `-case 3` (r=1) | −8.66 | **−25.79 (−17.1)** |

No es un fallo: es el álgebra. Con r ≥ 1 el marginal univariante de un componente
de Ȳ no es el bloque diagonal del conjunto, C̄ y Λ acoplan, y Φ̄_p queda
determinada por F_{p−1}, así que el AR está sobredeterminado y la vuelta es una
proyección. **La información univariante llega hasta el peldaño diagonal y no más
arriba** — que es exactamente donde `drtran` pone su puerta de entrada.

**Conclusión de la fase, ya con el marco correcto.** El valor del puente no es
sembrar el modelo cointegrado: es que `drvec` **entra en la escalera** con los
mismos contratos que el resto de la suite, y puede certificar el cruce y la
optimalidad de los ficheros que recibe. Para sembrar el modelo con r ≥ 1 la
información tiene que venir de algo que conozca el acoplamiento —la regresión
condicional, que ya está, o la rejilla concentrada sobre B₂ de la contingencia—,
no de univariantes.

**Sobre la convención de signos del MA**, verificada en la fuente porque es un
sitio donde la suite ya se ha quemado antes:

| eslabón | convención | dónde |
|---|---|---|
| `elf` | `a_t = (w−μ) − Σφ_j(w−μ)_{t−j} + Σθ_j a_{t−j}` ⟹ Θ(B) = I − Σθ_jB^j | `elfvarma.c:300` |
| cast de `fue` | `_unscramble` devuelve los coeficientes de `1 − c₁B − c₂B² …` y se pasan tal cual a `elf` | `forecast.py:143-147`, `cast_us.py:305` |
| `expand_ma_factors` de `drtran` | la misma, negando `work[k]` para obtenerla | `drtran.c:770-772` |
| cast de `drvec` | `armax->theta[k] = C̄·Θ_k·C̄⁻¹` con Θ_k de `x[]` | `drvec.c`, bloque [6] |

Los cuatro coinciden. Y empíricamente: sembrar con **θ negada** empeora el
arranque (−9.0987 frente a −8.5267 con la semilla y −8.7426 en frío), que es lo
contrario de lo que pasaría con el signo invertido.

**Contingencias:**

- Si la ganancia de sembrar Θ no llega para mover el nivel de |Σ̂| heredado de F1,
  se declara medido y se cierra la fase con el criterio que sí cumpla — el mismo
  trato que en F1, no un objetivo movido a posteriori.
- Si `fue` no está disponible donde se ejecute la batería, los `.pre` del banco se
  **versionan** como fixtures (los escribió `fue`, así que la afirmación de
  optimalidad es legítima) y el test se salta la generación.
- Si aun con Θ sembrada Λ sale mal, la alternativa es la rejilla concentrada para
  B₂ (el patrón de `preestimar_parametros` del legado: concentrar Λ en forma
  cerrada para cada B₂ candidato y buscar en rejilla sólo B₂).

---

### F3 — Capa interpretable: restricciones sobre α, formas BEC y Π

**Objetivo.** Que `drvec` responda preguntas económicas. Hoy da Λ̂ y B̂₂ y para.

Tras Johansen y Swensen (§3), el diseño correcto **no** es «tests de
exogeneidad débil», sino la clase general:

> **α = Aψ**, con A suministrada por el usuario (H₁(r)), de la que la exogeneidad
> débil es un caso particular. Y `drvec` está bien colocado: **Λ está en su vector
> de parámetros**, así que imponerlo es sustituir M·r entradas libres por s·r y
> calcular Λ = Aψ en `vec_shootx` — el mismo tipo de cambio que `-fixb2`. Los
> grados de libertad del LR contra H(r) son **(M−s)·r**, explícitos en el artículo.

Y además, para interpretar:

- la triangularización P con Σ = PDP′ (BVECM §4), que descorrelaciona las
  innovaciones y da las ecuaciones desacopladas;
- las formas BEC (`∇Y_t = Λ(B′Y_{t−1} − E[W]) + C(B)∇Y_{t−1} + u_t`) y Π.

`drv_project` lo tiene resuelto para el caso bivariante (`analisis_BEC`,
`calcular_chi2_weakex`, `calcular_chi2_joint`), y `LEGACY_NOTES.md` §2 y §5
recogen las fórmulas.

**Y un hueco que la literatura destapó y hay que cerrar aquí:** `drvec` obliga a
elegir qué r series van en el bloque Y₁ (por el orden de columnas del `.inp`) y
**no dice nada** sobre si esa normalización es apropiada. Mélard et al. lo evitan
por construcción; Mauricio lo advierte y remite a Luukkonen et al. (1999) y
Kurozumi (2005). Beta necesita, como mínimo, un **diagnóstico** que avise cuando
la normalización elegida sea dudosa — por ejemplo que alguna fila de B̂ implicada
sea numéricamente despreciable, o que el ajuste sea muy sensible a permutar el
bloque.

#### F3.1 — Lo entregado *(2026-08-18)*

**1. `α = Aψ`, la clase general.** `-alpha <fichero>` con A del usuario, y
`-weakex <i>` como atajo para la A que declara exógena débil la ecuación `i` —
la exogeneidad débil **no** se implementa como test propio, porque es el caso
particular de H₁(r) en que A selecciona filas. El LR contra H(r) se reporta con
sus `(M − sa)·r` grados de libertad y su p-valor.

La ventaja estructural que predecía §3 se confirma en el código: Λ está en el
vector de parámetros, así que la restricción es una sustitución dentro de
`vec_shootx` —el mismo patrón que `-fixb2`— y los errores estándar salen del
hessiano. El legado necesitaba método delta con pseudoinversa SVD.

Sobre `mink_muskrat` (p=2, q=1, r=1, caso 2):

| hipótesis | LR | g.l. | p |
|---|---|---|---|
| la ecuación del muskrat no ajusta | 26.573 | 1 | ≈ 0 |
| la ecuación del mink no ajusta | 3.590 | 1 | 0.058 |

Se comprueba el rango de A por su Gram **antes** de estimar: con `A'A` singular
ψ no está identificada.

**2. Π = ΛB′, la matriz de largo plazo**, con sus autovalores. Y con una
advertencia que resultó ser **más fuerte** que la que traía la literatura: aquí
Π tiene rango r *por construcción*, así que sus M−r autovalores nulos están
garantizados y leerlos como evidencia del rango es **circular**. El aviso de
Mélard, Roy y Saidi (que el supuesto sobre Φ(1) no implica lo que esa lectura
supone) sigue valiendo para el caso no restringido. Π es además **invariante a la
normalización**, al contrario que Λ y B, así que es sobre Π sobre lo que hay que
comparar ajustes.

**3. El diagnóstico de normalización** — el hueco que destapó Mélard y que era el
criterio 6c de beta. La medida es libre de unidades: en `W = Y₁ + B₂′Y₂` cada
serie pesa `|coeficiente| · sd(serie)`, y se informa la cuota del bloque Y₁ en
ese peso. Una cuota diminuta dice que la relación no es sobre Y₁ y que la
normalización está forzada.

| caso | cuota de Y₁ | |
|---|---|---|
| `mink_muskrat` | 75.5 % | sin aviso |
| `datasets/synthetic/badnorm.inp` | **0.6 %** | **avisa** |

El segundo es un caso construido a propósito —y versionado— donde dos series
cointegran y la tercera, que es la que va al bloque Y₁, es un paseo aleatorio
independiente. **Una alarma sin un caso donde dispare no es una alarma**, así que
la batería comprueba las dos direcciones: que salta ahí y que calla en un ajuste
bien normalizado. Por mutación, desactivarla levanta 1 fallo.

**4. La triangularización `Σ = PDP′`** (LDL′): `P` unitriangular inferior y `D`
diagonal, de modo que con `A_t = P A*_t` las innovaciones `A*_t` están
incorrelacionadas y el sistema premultiplicado por `P⁻¹` se lee ecuación a
ecuación. Se informa además `D_i/Σ_ii`, la cuota propia de cada varianza de
innovación. **El orden es el de las columnas del `.inp`**, y otro orden da otra
`P`: es la misma clase de decisión silenciosa que la del bloque Y₁, así que la
salida lo dice. El legado sólo lo tenía para el caso bivariante; aquí es general.

*Nota de cobertura:* la comprobación de que `P D P′` reconstruye `Σ` se hace sobre
**M = 3 y no sobre M = 2**, y por una razón que sólo se ve midiendo: con M = 2 el
bucle interno de la LDL′ no llega a ejecutarse, así que una mutación del término
cruzado es invisible. Medido: negarlo levanta 0 fallos en M = 2 y 1 en M = 3.

**F3 queda cerrada** salvo lo que el plan mandó a F4 (valores críticos en muestra
finita).

**Salida.**
1. `-alpha <fichero>` (o equivalente) impone α = Aψ y reporta el LR contra H(r)
   con (M−s)·r grados de libertad.
2. Sobre un caso bivariante, reproducir los α, γ y los p-valores de exogeneidad
   débil de `drv_project` con tolerancia declarada.
3. Un aviso de normalización dudosa que se dispare en un caso construido a
   propósito para ello.

**Contingencia.** Si el método delta para C(B) y Π resulta frágil, se publican las
estimaciones puntuales de la forma BEC **sin** errores estándar, marcando el
hueco: mejor que unos errores estándar que no se sostienen. Lo que **no** se
sacrifica es α = Aψ, porque ahí Λ y su covarianza ya son directas. Y si el
diagnóstico de normalización no se deja construir a tiempo, beta se documenta
diciendo **explícitamente** que la elección del bloque Y₁ es responsabilidad del
usuario y no está verificada — declararlo es aceptable; no mencionarlo, no.

---

### F4 — Test de rango defendible

**Objetivo.** Que el rango que sale de `-lrtest` sea defendible en muestra
finita, no sólo asintóticamente.

BVECM §6.3–6.5 es tajante: bajo H₀ la distribución **no es χ²** y recomienda
**bootstrap paramétrico** — simular N muestras bajo H₀ con los parámetros
estimados, reestimar H₀ y H₁ en cada una, y tomar percentiles empíricos. Razones:
hay componentes MA bajo las dos hipótesis y (en su extensión) el operador de
convergencia añade deterministas que desplazan la distribución.

**Y dos mejoras que salen de la literatura (§3):**

- **p-valores en vez de una tabla de tres columnas.** Trenkler (2004) aproxima la
  distribución asintótica por una **Gamma** con parámetros de **superficies de
  respuesta**, lo que permite p-valores o percentiles arbitrarios. Eso cierra el
  hueco del **caso 3**, que hoy sale con `-` porque no lo tabulé. Salvedad: sus
  estadísticos son los de Saikkonen–Lütkepohl con ajuste previo por deterministas;
  la técnica transfiere, pero para estadísticos tipo Johansen la fuente correcta es
  **MacKinnon, Haug y Michelis (1999)**, la que cita Mauricio.
- **No usar los autovalores de Π̂ como criterio de rango.** Mélard et al. muestran
  que el supuesto sobre Φ(1) **no implica** que ΣΦⱼ tenga d autovalores unitarios
  (Pham, Roy y Cédras, 2003). Es indicio, no criterio; el test LR es el
  instrumento. Conviene anotarlo donde el banco cita esos autovalores.

**Salida.** Reproducir el rango de `ca.jo` en ≥ 4 casos del banco, con los
valores asintóticos **y** con el bootstrap, y que coincidan en esos casos. Los
asintóticos se conservan como opción rápida por defecto, y el caso 3 deja de
salir sin valores críticos.

**Contingencia.** El coste es N × M estimaciones completas y puede ser
prohibitivo. En ese caso: (a) reducir N y **reportar el error de Monte Carlo**
en vez de esconderlo; o (b) limitar el bootstrap al contraste r = 0 frente a
r = 1, que es el que decide si hay cointegración y el único imprescindible.

---

### F5 — Cierre de beta

**Objetivo.** Empaquetar.

- Consolidar la documentación: hoy hay tres estudios (`ANALISIS_PRELIMINAR`,
  `ESTUDIO_BVECM_vs_DRVEC`, este plan) más `LEGACY_NOTES` y `ENCUADRE_ESTUDIO`.
  Beta necesita **un** documento de entrada que diga qué hace el programa y qué
  no, con los estudios como material de apoyo.
- **Registro de homologación**: una tabla de casos del banco, qué reproduce
  `drvec`, con qué tolerancia y con qué fecha.
- **Corregir `benchmark/README.md`** al nivel que quede: hoy ningún caso
  califica como «Verified».
- Revisar los `.inp` de `data/` uno a uno (§8.3 de `ANALISIS_PRELIMINAR`): sólo
  `AL.inp` está identificado como layout antiguo.

**Criterios de salida de beta:**

| # | criterio | estado hoy |
|---|---|---|
| 1 | `make test` verde, con la puerta diagonal dentro | **✔ (F0)** 60 comprobaciones, medidas por mutación |
| 2 | \|Σ̂\| concordante entre las cuatro configuraciones **y** en el nivel objetivo | **parcial (F1)**: concuerdan (dispersión 0.000048, con test), pero en ~0.00248 y no en ~0.00230 → nivel a F2 |
| 3 | Siembra desde `.pre`, con logL ≥ arranque en frío | **✔ en el peldaño diagonal, ✘ por encima (F2.8)**: con r = 0 el `.pre` transporta el óptimo univariante y los dos contratos de la escalera se cumplen (cruce 1.8e-5, certificado +2.4e-7 ≥ 0, los dos con test). Con r ≥ 1 la información univariante no vale: la semilla arranca 17 unidades peor, por sobredeterminación del AR y por el acoplamiento de C̄ y Λ |
| 4 | Formas BEC/Π y exogeneidad débil, con o sin s.e. declarado | **✔ (F3.1)**: Π = ΛB′ con autovalores y la advertencia de circularidad, exogeneidad débil por LR, y la triangularización Σ = PDP′ con test de reconstrucción |
| 5 | Rango correcto en ≥ 4 casos del banco | **✔**: `mink_muskrat` (r=1), UK (r=2, igual que `ca.jo`), y dos sintéticos con rango **conocido por construcción**, r=0 y r=2, los dos recuperados y en la batería. Y medido lo que faltaba: en 20 réplicas con r=1 verdadero el test acierta 16, sobre-rechaza 3 y sub-rechaza 1 |
| 6 | Todo termcode 3 residual **explicado**, no necesariamente eliminado | ✘ |
| 6b | α = Aψ soportado, con LR y grados de libertad correctos | **✔ (F3.1)** con `-alpha`/`-weakex`, g.l. (M−sa)·r, y guarda de rango sobre A |
| 6c | La elección del bloque Y₁ **diagnosticada o declarada** como no verificada | **✔ (F3.1)**: diagnóstico libre de unidades, con caso construido que lo dispara y test en las dos direcciones |
| 7 | Registro de homologación y documento de entrada | **✔**: `docs/HOMOLOGATION.md` (qué reproduce, con qué tolerancia y qué no) y el conjunto de documentación en inglés con `docs/README.md` de entrada |

---

## 5. Restricciones transversales

Valen para todas las fases y no se negocian:

- **`elf()` no se toca.** Es la AS 311 publicada y refereada, validada aquí a
  2·10⁻⁸.
- **El optimizador no se toca.** `drtran/docs/OPTIMIZER_STOPPING_STUDY.md` ya
  probó y rechazó con evidencia tres variantes de `typx`, y la pregunta
  «¿termcode 3 es el óptimo o mal condicionamiento?» está abierta a nivel de
  suite. Todo lo de este plan se consigue por **cast, siembra o
  parametrización**.
- **Nada de cambios que muevan resultados sin declararlo.** El patrón de esta
  sesión (§0 de `ANALISIS_PRELIMINAR`: qué cambio mueve estimaciones y cuál no)
  se mantiene.

---

## 6. Registro de riesgos

| # | riesgo | probabilidad | contingencia |
|---|---|---|---|
| R1 | **El termcode 3 residual es intrínseco.** El artículo BVECM dice que «multicollinearity appears to be natural to CI», y el estudio del optimizador lo deja abierto para toda la suite | alta | No perseguirlo. Emitir una **nota de convergencia** como hace `drtran` (`Fit.convergence_note`) y declararlo en la documentación. Criterio 6 de beta se cumple con *explicado*, no con *eliminado* |
| R2 | **La columna EML de Mauricio no se cierra nunca.** Haría falta el programa original o correspondencia con el autor | alta | Beta se homologa contra el criterio de \|Σ̂\| (§5.11), respaldado por tres fuentes independientes, **no** contra los logL publicados. Ya está así en `benchmark/README.md` |
| R3 | **El cast de `fue` no es reentrante** (`CAST.md` §9) | cierta | Leer sólo los números del `.pre`, nunca llamar al cast en ejecución. Ya incorporado al diseño de F2 |
| R4 | **Los datos del Census Housing no aparecen** | alta | Beta se homologa con un solo caso de Mauricio más los reproducidos con `ca.jo`. Ya está declarado en el banco |
| R5 | **Trampas de convención** entre `drvec`, BVECM y `drv_project`: B₂ = −β, orden de Ȳ invertido en el código del legado, `p` como orden sobre Ȳ y no sobre ∇Y | media, y ya ha mordido dos veces | Están en `ESTUDIO_BVECM_vs_DRVEC.md` §2 y en §2 de este plan. Convertirlas en **un test** en F0, no sólo en documentación |
| R6 | **F3 se desborda** (la triangularización P general para M y r arbitrarios es más trabajo del que parece) | media | Beta con la capa interpretable **sólo para el caso bivariante** (M=2, r=1), que es el del banco y el del legado, y el caso general como post-beta |
| R8 | **Normalización mal elegida.** El usuario decide el bloque Y₁ por el orden de columnas; una elección mala da un modelo equivocado sin avisar | media | Diagnóstico en F3; y si no llega, **declararlo** en la documentación de beta. Luukkonen et al. (1999) y Kurozumi (2005) son las referencias |
| R7 | **Deriva de documentación**: cinco documentos y las correcciones se pisan | media, ya ocurrió en esta sesión | F5 consolida. Mientras tanto, cada corrección se fecha y se dice qué afirmación anterior invalida |

---

## 7. Lo que **no** entra en beta

Declararlo evita que el alcance se estire:

- **El operador de convergencia** (BVECM §5), que es la extensión de
  investigación y el punto donde `drvec` y el artículo convergerían de verdad.
- **El experimento de la parametrización dual** (optimizar en coordenadas VARMA
  y convertir al final). Es la hipótesis más informativa sobre el termcode 3 y
  también la más cara; y la evidencia de esta sesión apunta a que lo que compra
  convergencia es **fijar B₂**, no cambiar de coordenadas.
- **r = M** (proceso estacionario en niveles): no es expresable con este layout
  y para eso está `drvarma`.
- **La capa interpretable general** para M y r arbitrarios (ver R6).
- **Los tests de convergencia** (tipo de convergencia, τ, velocidad) del legado,
  que dependen del operador de convergencia.

---

## 8. Orden recomendado y por qué

**F0 → F1 → F2 → (F3 ∥ F4) → F5.**

F0 primero porque sin batería de pruebas cada fase siguiente es una apuesta: en
esta sesión encontré dos fallos (salida corrida, B₂ traspuesta) que un invariante
automático habría cazado gratis. F1 antes de F2 porque es barato, aislado y copia
práctica establecida — y porque si resuelve el criterio de |Σ̂|, F2 se aborda con
un objetivo más limpio. F2 antes de F3 porque no tiene sentido construir la capa
interpretable sobre estimaciones que no se sabe si son el óptimo. F3 y F4 son
independientes entre sí y pueden ir en paralelo.

**Y una advertencia sobre el orden que no seguiría:** empezar por F3 es tentador
porque es lo visible y lo que da valor económico. Sería un error. La capa
interpretable amplifica lo que haya debajo: si la estimación no es fiable, lo que
produce son p-valores de exogeneidad débil con pinta de resultado.
