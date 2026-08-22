# `drvec`: plan hasta una versión de producción

*Objetivos, criterios de salida medibles y contingencias. Escrito el 2026-08-20,
sobre lo cerrado en `PLAN_BETA.md` (F0–F5.1) y sobre una revisión de coherencia
con los programas hermanos de la suite: `drvarma` (el motor), `fue` (el cast),
`drtran` en C (el precedente directo) y `drtran-python` (el porte, y el estándar
de empaquetado del conjunto). Todo lo que aquí se afirma sobre otro programa se
ha medido contra sus fuentes, no supuesto.*

---

## 0. Qué significa «producción»

`PLAN_BETA.md` §0 definió beta como *«alguien que no sea el autor puede usar
`drvec` sobre datos propios y saber cuándo no fiarse del resultado»*. Producción
no es un grado más de la misma escala: cambia el sujeto.

> **Producción = `drvec` puede ser instalado por un tercero, invocado desde un
> script que él no ha escrito, y sus resultados publicados citando una versión.
> Un error de uso se rechaza en vez de estimarse; un fallo de estimación se
> nota desde fuera del programa; y una cifra del registro se puede volver a
> producir desde el repositorio y nada más.**

De ahí salen cinco requisitos, y el orden tampoco es negociable:

1. **La interfaz no miente.** Una opción desconocida se rechaza, un valor fuera
   de rango se rechaza, y un ajuste fallido devuelve un código de salida que lo
   dice. Hoy no se cumple ninguno de los tres.
2. **El registro es reproducible desde el repositorio.** Toda cifra publicada
   tiene un fichero de entrada versionado y un comando que la vuelve a producir.
3. **La suite es una suite.** El motor, el cast y el registro de defectos son
   los mismos objetos en los cinco programas, o la diferencia está escrita y
   justificada.
4. **El programa se puede citar.** Versión, licencia, registro de cambios y
   registro de defectos, al nivel de `drtran-python`.
5. **Lo científico abierto sigue abierto y dicho.** Producción no significa
   cerrado; significa acotado.
6. **El modelo se gana sus parámetros.** Un VARMA-VEC existe para decir algo que
   un univariante no dice. Si no mejora la previsión de un ARIMA univariante
   sobre los mismos datos, no hay producto — hay un estimador correcto de un
   modelo que no sirve. *(Añadido el 2026-08-20, y es el que reordena el plan;
   ver §6.)*

Hoy (1) está hecho, (5) completo, buena parte de (3) sin escribir, y **(6) sin
medir siquiera una vez**.

---

## 1. Punto de partida, medido el 2026-08-20

**Sano y verificado:**

- `make test` verde: **152 comprobaciones, 162 con `VALGRIND=1`**, cero fugas y
  cero errores de memoria; también sobre `-specs`, que es código posterior al
  bloque de memoria de la batería y no estaba cubierto.
- **`-Wall -Wextra` sin un solo aviso** en el código propio (`drvec.c`,
  `fue_bridge.c`, `diagnose_mv.c`). Los 26 restantes son de los dos ficheros
  vendorizados (`nlatools.c` 19, `fue_pre_reader.c` 7) y el Makefile los declara.
- El generador aleatorio es **determinista con semilla fija** en bootstrap y en
  multiarranque, y la escalera de amplitud es monótona en `n`. Criterio de
  producción, ya tomado.
- Rendimiento no es un problema: 0.03 s un ajuste, 0.11 s `-specs`.
- El ejemplo de `docs/README.md` (`-weakex 2` → `LR = 3.589519`, `p = 0.058145`)
  reproduce exacto.
- Todos los enlaces entre documentos resuelven.

**Abierto, y esto es el plan:**

- **La interfaz no valida nada.** §2.
- **Tres de las cuatro filas del criterio 2 de salida de beta no reproducen.** §3.
- **La copia de `nlatools.c` de `drvec` es la única del conjunto que diverge en
  código**, y la razón registrada para no alinearla es cierta de `fue` y falsa
  de `drvec` — medido. §4.
- **No hay versión, ni licencia, ni registro de cambios, ni registro de
  defectos**, mientras los tres paquetes hermanos los tienen. §5.

---

## 2. La interfaz — el bloqueo real — **HECHA el 2026-08-20**

### 2.1 Lo medido

El bucle de opciones (`src/drvec.c:4145-4215`) es una cadena de `strcmp`
**sin rama `else`**. Consecuencias, todas comprobadas sobre `bin/drvec` del
commit `1f12134`:

| entrada | qué hace hoy |
|---|---|
| `-bogusflag`, `-diagcv`, `-multistar 20` | **se ignoran en silencio** y el programa estima otro modelo |
| `p = 0` | **SIGSEGV** en `init_guess` (`drvec.c:2949`, confirmado con AddressSanitizer) |
| `p = -1`, `p = 200` | **SIGSEGV** |
| `drvec fich x y z` | **SIGSEGV** (`atoi` da 0 y se cae por la misma vía) |
| `q = -1` | **SIGSEGV**, tras imprimir `ERROR init_guess: idx=10, npar=6` |
| `q = 60` | se cuelga (> 30 s sin salida) |
| `p = 60` con 61 observaciones | estima, falla con `ifault = 3`… y **devuelve 0 al shell** |
| `-case 0`, `-case 9`, `-m 3` | aceptados en silencio |
| `-multistart -5`, `-multistart abc`, `-bootstrap -1`, `-matest 0`, `-rankadm -1`, `-seedb2 abc` | aceptados en silencio |
| `-weakex 0` | `ERROR: no se pudo abrir (null)` — el mensaje de otra cosa |

Y dos más, del mismo tronco:

- **El código de salida no distingue fallo de éxito.** `ESTIMATION FAILED:
  ifault = 3` sale con `exit(0)`, así que ningún script puede detectarlo. Es
  incompatible con el punto (1) de §0 y con cualquier uso en lote.
- **El `usage()` del binario documenta 22 de las 33 opciones.** Faltan `-alpha`,
  `-bootstrap`, `-multistart`, `-weakex`, `-writeres`, `-writeinp`, `-seedybar`,
  `-eval`, `-interv`, `-fdhess` y `-levels`. El propio texto lo admite y remite
  a `USAGE.md`, que sí las documenta las 33.

### 2.2 Por qué esto es más grave aquí que en un programa cualquiera

La tesis del programa, escrita en `SPECIFICATION_PLAN.md` §8 y en la última
columna de `-specs`, es que **la especificación es el resultado**: un ajuste que
niega el rango al que fue estimado no es un ajuste peor, es el ajuste de otro
modelo. Un `-diagma` mal tecleado hace exactamente eso, y hoy el programa no lo
dice. Un programa que grita cuando `σ_min(Λ⊥′Θ(1)B⊥)` cae por debajo de 0.2 y
calla cuando le pasan una opción que no existe tiene sus alarmas mal repartidas.

### 2.3 La convención de la suite, que es la que hay que adoptar

No hay que inventarla. `drtran` en C usa `getopt` con
`default: usage(argv[0]); return 1;` (`drtran.c:4215`), y el porte la enuncia
como principio en `cli.py`:

> *«Refusing rather than ignoring the option.»*

con tres códigos de salida: `0` éxito y `-h`; `1` línea de órdenes mal formada o
`CliError`; `2` opción reconocida pero no disponible. `drvec` no puede usar
`getopt` tal cual —su vocabulario es de palabras (`-multistart`) y no de letras—,
así que la forma de mínimo delta es conservar la cadena de `strcmp` y **añadir la
rama `else`**, más una tabla de validación de valores.

### 2.4 Qué se construye (P1)

1. Rama `else` que rechaza cualquier argumento que empiece por `-` y no esté en
   la tabla, con el mensaje y el `usage()`, y `exit(1)`.
2. Validación de los tres posicionales: `p >= 1`, `q >= 0`, `0 <= r < M`, y
   rechazo explícito de un no-número donde se espera un entero (`strtol` con
   comprobación de `endptr`, no `atoi`).
3. Validación de los valores de opción: `-case` ∈ {1,2,3}, `-m` ∈ {1,2},
   `-multistart >= 1`, `-bootstrap >= 1`, `-matest`/`-artest >= 1`,
   `-rankadm > 0`, `-weakex` ∈ 1..M, `-seedb2` y `-fixb2 v` numéricos de verdad.
4. **Cota de grados de libertad**: rechazar `npar >= n·M` antes de estimar. Hoy
   `p = 60` sobre 61 observaciones se intenta.
5. `-h`/`--help` → `usage()` por stdout y `exit(0)`; sin argumentos → `usage()`
   por stderr y `exit(1)`.
6. `--version` → nombre, versión y `git describe` embebido, `exit(0)`.
7. **Códigos de salida con significado**: `0` ajuste terminado; `1` error de uso
   o de fichero; `2` el ajuste no se pudo completar (`ifault != 0`, o ningún
   arranque convergió). El termcode 3 **no** es fallo: es una parada explicada,
   y sigue saliendo con 0 y su nota de convergencia.
8. `usage()` completo — las 33 opciones — generado de la misma tabla que valida,
   para que no puedan volver a separarse.

**Criterio de salida de P1** — todo comprobado por la batería:

| # | criterio | estado |
|---|---|---|
| P1.1 | ninguna entrada de la tabla de §2.1 produce SIGSEGV; cada una da mensaje y código | **✔** las cuatro que morían por señal salen con 1 y un mensaje |
| P1.2 | toda opción desconocida se rechaza con `exit(1)`; comprobado con una opción inventada y con erratas de opciones reales | **✔** y con sugerencia: `-diagcv` → «did you mean `-diagcov'?» |
| P1.3 | el conjunto de opciones que `usage()` enumera **es** el conjunto que el parser acepta, verificado comparando las dos listas | **✔** 33 = 33; la lista del `usage` se genera de la misma tabla que valida, y el test la compara contra los `strcmp` del fuente |
| P1.4 | `ESTIMATION FAILED` sale con código 2; un ajuste con termcode 3 sale con 0 | **✔** con una fixture construida al efecto (dos series exactamente colineales, `ifault = 3`) |
| P1.5 | `--version` imprime una versión y sale 0 | **✔** `drvec 0.9` desde el 2026-08-21 |
| P1.6 | ningún valor dorado se mueve: la validación no toca el cálculo | **✔** 190 pasadas, 0 fallos; 200 con `VALGRIND=1` |

### 2.5 Lo construido, y lo que mide

**El diseño, en una frase:** una **tabla** declara qué opciones existen y qué
argumento lleva cada una; una **pasada de validación** la recorre antes de que
nada se asigne; y la cadena de `strcmp` de siempre queda intacta detrás. La
validación **no asigna**, que es lo que garantiza que no puede cambiar por
accidente lo que el programa estima — y en efecto ningún valor dorado se movió.
La misma tabla genera el listado completo del `usage`, de modo que la lista
aceptada y la documentada no pueden volver a separarse.

**La batería creció de 152 a 190 comprobaciones**, en una sección `[0]` nueva.
Y **muerde**, medido por mutación contra el fuente actual, que es la regla 3 del
método:

| mutante | fallos levantados |
|---|---|
| `validate_cli()` convertida en no-op | **16** |
| `p`, `q`, `r` de vuelta a `atoi`, y fuera la cota de grados de libertad | **7** |
| `usage()` enumera sólo las 22 primeras opciones | **1** |
| el código de salida del fallo de estimación, de vuelta a 0 | **1** |

**Dos comprobaciones no muerden, y quedan escritas como tales** en la cabecera
del script: `-weakex 0` (antes de P1 también salía con 1, pero por la vía
equivocada y con el mensaje de otro fallo, `no se pudo abrir (null)`; el test
fija el código, no el mensaje) y `r = -1` (ya se rechazaba, por la comprobación
explícita que había en `main`). Están para que los tres posicionales se
comprueben como conjunto.

**La cota superior de `p` y `q` la pone la muestra, no un número.** Se comprueba
en `npar < nobs · M` una vez leído el fichero: cubre `p` grande, `q` grande y `M`
grande por igual, y es deliberadamente generosa — no es un criterio estadístico,
para eso están el AIC y el BIC que el programa ya imprime, sino la frontera por
debajo de la cual el ajuste no significa nada. `60 1 1` sobre 61 observaciones
declara 246 parámetros contra 122 datos y se rechaza.

**Y la distinción que había que no romper.** El termcode 3 —«last global step
failed to locate a lower point»— **no** es un fallo: es la parada explicada
sobre la que se apoya la mayor parte de lo que este programa publica
(`CONVERGENCE.md`), y sigue saliendo con 0. Convertirla en error habría marcado
como rotos la mitad de los resultados del registro. Hay un test que lo fija.

Igual de importante, lo que **no** debía romperse: `-fixb2` y `-rankadm` llevan
valor opcional, y el criterio con que la validación decide si el siguiente
argumento es el valor tiene que ser **el mismo** que usa el asignador, o una
línea de órdenes legítima deja de funcionar. `-fixb2 -0.5` (un valor negativo,
que empieza por `-`) y `-fixb2 -diagma` (una opción detrás de `-fixb2`) son
válidas y siguen estándolo, con test.

---

## 3. El registro, y lo que no reproduce

El criterio 2 de salida de beta (`PLAN_BETA.md` §F5, tabla; `HOMOLOGATION.md`
§ del `|Σ̂|`) es la concordancia de `|Σ̂|` sobre cuatro configuraciones
equivalentes con `-multistart 60`. Vuelto a medir hoy, calculando `|Σ̂|` a mano
desde la `Σ` que imprime el `.out`:

| configuración | registrado | medido 2026-08-20 |
|---|---|---|
| niveles caso 3 | 0.002347 | **0.002347** ✔ |
| niveles caso 2 | 0.002346 | **0.002372** |
| antiguo caso 2 | 0.002344 | **0.003085** |
| antiguo caso 3 | 0.002358 | **0.002418** |

**No es deriva del código.** Reconstruido el binario en `e6a9431` —el commit que
cierra la beta— da exactamente la misma `Σ` que el de hoy para «niveles caso 2»:
`0.047081 / 0.025008 / 0.063671`. Lo que ha cambiado no es el programa.

La hipótesis para las dos filas «antiguo» es la regla 4 del método
(`DEVELOPMENT_RECORD.md` §0): *un valor dorado sólo vale para la entrada exacta
sobre la que se midió*. `datasets/mauricio/mink_muskrat.inp` se regeneró en
niveles el 2026-08-17, y esas dos filas se midieron con `-differenced` sobre el
fichero **pre-diferenciado**, que ya no existe en el repositorio. Pasar
`-differenced` al fichero de hoy es una lectura distinta, no la misma medida.
Para «niveles caso 2» no hay esa explicación y la diferencia (2.6e-5) es mayor
que toda la dispersión que la tabla reclama (1.4e-5).

Agravantes, los dos estructurales:

- **`drvec` no imprime `|Σ̂|`**, siendo el criterio de homologación declarado del
  programa. Hay que sacarlo a mano de la matriz impresa a seis decimales.
- **La batería no protege ese criterio.** Los valores dorados cubren sólo el
  arranque único; el número sobre el que se declaró cerrada la beta no tiene
  regresión.

Y una tercera, de reproducibilidad pura: `.gitignore` lleva `*.txt` y `*.out`, de
modo que **los 15 `benchmark/*.results.txt`** —la referencia externa de
`urca::ca.jo` que `benchmark/README.md` cita como nivel «Reproduced»— y los
`data/*.out` **no están versionados**. El documento cita ficheros que no viajan
con el repositorio.

### Qué se construye (P2) — **HECHO el 2026-08-20**

1. **✔** `drvec` imprime `|Σ̂|` y `log|Σ̂|` en el `.out` y en la terminal, junto a
   `Σ`. Comprobado contra el cálculo a mano: 0.002461005 frente a 0.002461007,
   que es la precisión de la matriz impresa. *(Y la primera versión de esa línea
   elevaba el determinante al cuadrado, porque `choldcp` ya acumula los
   cuadrados de la diagonal del factor de Cholesky; se vio porque daba
   6.06e−06, que es exactamente el cuadrado del valor del registro.)*
2. **✔** Rehechas las cuatro filas, con el comando al lado. **Tres reproducen a
   la cifra**; la cuarta —«niveles caso 2»— no, y tampoco reproducía desde el
   cierre de beta: el binario reconstruido en `e6a9431` da lo mismo que hoy. Era
   un error de transcripción, corregido en los tres documentos donde vivía. La
   hipótesis anterior —que las dos filas «antiguo» necesitaban un fichero
   desaparecido— **era falsa**: el layout antiguo se deriva del `.csv` igual que
   ya hacía la batería.
3. **✔** Bloque `[8g]`, opt-in con `SLOW=1`: las cuatro cifras y, además, que
   **concuerden** — cuatro valores moviéndose a la vez pasarían una comprobación
   valor a valor.
4. **✔** `.gitignore` con excepciones: entran los 16 `benchmark/*.results.txt`
   —la referencia externa de `urca::ca.jo` que `benchmark/README.md` cita como
   nivel «Reproduced»— y los 11 `data/*.out`. Comprobado que las salidas
   transitorias siguen ignorándose.

**Lo que costó, y es el resultado de fondo:** el criterio queda algo más débil
de lo que estaba escrito — dispersión 0.000028 en vez de 0.000014, nivel 1.9 %
en vez de 1.6 % — y la conclusión cualitativa no se mueve. La causa de que se
perdiera la pista está identificada y cerrada: el programa declaraba un criterio
y no lo emitía, así que alguien tenía que multiplicar a mano una matriz
redondeada a seis decimales, y quien lo hace se equivoca.

**Criterio de salida de P2:** cada cifra de `HOMOLOGATION.md` tiene al lado el
comando que la produce, ese comando corre sobre el repositorio limpio, y da la
cifra. Comprobado ejecutándolos todos.

---

## 4. La suite: motor, cast y registro de defectos — **HECHA el 2026-08-21**

`drvec` es, a nivel de suite, un subproducto de `drvarma` y de `fue`: usa el
motor del primero y el cast del segundo, exactamente como `drtran`. Esa frase
estaba escrita en `SUITE_INTEGRATION.md` §5; lo que sigue es la primera vez que
se **mide**.

### 4.1 El motor: `drvec` es el fiel

Diferencias contra `drvarma_v.04.1/src`, contando sólo líneas que cambian:

| fichero | drvec | drtran |
|---|---|---|
| `elfvarma.c` | **0 — byte a byte idéntico** | 5, todas de cabecera |
| `drvmlest.c` | **0** | 50, con cambio de código |
| `qnewtopt.c` | **0** | 70, con cambio de código |

`drvec` no ha tocado el motor, que es lo que su documentación afirma, y ahora
está comprobado contra la fuente canónica y no sólo contra su propio historial.
**El que ha divergido es `drtran`**, y por razones suyas: expone `opt_iters` y
`opt_termcode` como globales *«para que el programa pueda informar con
honestidad»*, y conserva el `report()` antiguo. `drvec` resuelve la misma
necesidad —la nota de convergencia— por dentro de `report()`, en la versión de
`drvarma`. Son dos soluciones al mismo problema y ninguna está mal; lo que falta
es que esté escrito en algún sitio que son dos.

### 4.2 `nlatools.c`: `drvec` es el único que diverge en código

La cabecera de `drvarma_v.04.1/src/nlatools.c` declara el contrato:

> *«SHARED, Numerical-Recipes-free version: kept byte-identical to
> drtran/src/nlatools.c and to the copy embedded in the Python package. […] If
> you fix something here, fix it there.»*

Medido contra esa copia:

| copia | líneas de diff | de las cuales, código |
|---|---|---|
| `drvarma/csrc/internal/` (paquete Python) | 8 | **0** |
| `drtran/src/` | 13 | **0** |
| **`drvec/src/`** | **65** | **~34** |
| `fue/csrc/internal/` | 846 | 403 (es una copia reducida, otra historia) |

La divergencia de `drvec` está localizada: `vector`, `ivector`, `tensor` y sus
`free_*` **son** los de la copia compartida —eso se hizo en `aeb159c` y
`83e832e`—, pero `matrix`, `imatrix`, `free_matrix` y `free_imatrix` son los de
**`fue`**, carácter por carácter: `calloc(nrh+1)` punteros de fila, sin
desplazamiento de base y sin `- ncl`, y el `free` correspondiente.

O sea: **`drvec` corre el motor de `drvarma` sobre el layout de memoria de
`fue`.** Funciona hoy porque ningún sitio de `drvec` reserva una `matrix` con
cota inferior distinta de 1 —comprobado, no hay ninguna—, que es exactamente la
misma latencia que ya mordió dos veces en la suite: en `tensor(-q+1, ...)` de
`elf` y en el `vector(-nlags, nlags)` de la identificación de `drtran`.

`SUITE_INTEGRATION.md` §5 registra la decisión y su motivo: *«la variante con
desplazamiento usa un layout de fila incompatible, así que las dos no son
intercambiables; el programa univariante las dejó en paz por esa razón»*. El
comentario de `fue` es más explícito todavía: *«matrix/imatrix se dejan COMO
ESTAN a proposito: aplicarles el mismo cambio rompe fue (free(): invalid
pointer) […] Queda por ver por que.»*

**Medido hoy, y cambia la conclusión para `drvec`.** Sustituidas las cuatro
rutinas por las de `drvarma_v.04.1` en una copia del árbol:

- `make test` → **152 pasadas, 0 fallos**. Ningún valor dorado se mueve, la
  puerta diagonal sigue cerrando, el `.pre` sigue leyéndose.
- `VALGRIND=1 make test` → **10 fallos**, y los diez son el mismo hallazgo:
  con el layout compartido `valgrind` **destapa dos fugas reales que el layout
  actual oculta**:

  ```
  1.416 bytes definitely lost — matrix (nlatools.c:402)
                              — init_guess (drvec.c:3011)   [cond_resid]
  1.488 bytes definitely lost — matrix (nlatools.c:402)
                              — main (drvec.c:4299)         [rawmat]
  ```

Es el mismo mecanismo que `SUITE_INTEGRATION.md` §5 ya explica para `vector`:
*«un puntero devuelto en la base de su bloque le parece alcanzable a valgrind
aunque nadie vaya a liberarlo nunca»*. Las dos son de vida de proceso y no hacen
daño en un programa por lotes, pero son reales y hoy son invisibles.

**Conclusión: la razón para no alinear es cierta de `fue` y falsa de `drvec`.**
La incompatibilidad es local al cast univariante, no al motor — que es
precisamente la pregunta que el comentario de `fue` deja abierta. `drvec` puede
alinearse y con ello el conjunto pasa a tener **cuatro copias idénticas de
cinco**, y la quinta con su motivo medido en vez de supuesto.

### 4.3 El cast: `drvec` va por delante, y el registro no lo sabe

`fue_pre_reader.c` de `drvec` es copia de la de `drtran` con 86 líneas de
diferencia. No son deriva: son **tres arreglos** que `drtran` no tiene.

| | qué | estado en el registro |
|---|---|---|
| cabecera libre | el original salta 5 líneas por cuenta; `drvec` busca el separador `frequency`, que es lo que hace el parser autoritativo. Con el otro, un `.inp` de la batería daba `nobs = 1787128427` | no registrado |
| `nobs <= 0` | se para ahí en vez de pedir el vector | no registrado |
| **BUG-11** | la sección `ifadf` la escriben siempre los dos escritores de `fue`; el original la lee sólo dentro del `if`, así que en un fichero **anual** todo lo posterior se desplaza, `refactor` sale de memoria sin inicializar y la serie queda a ceros — con `read_fue_pre` devolviendo **éxito** | **OPEN** en `drtran-python/docs/BUGS.md` |
| `ifadf = NULL` | el original lo dejaba sin inicializar | no registrado |

Y `free_fue_pre` (`fue_bridge.c:249`) es el desasignador de **BUG-12**, también
**OPEN** en el registro y ausente en `drtran`.

Así que dos defectos abiertos del registro de la suite están **resueltos en C
dentro de `drvec`**, y el registro no lo refleja. Para un usuario anual —y el
banco de `drvec` empieza en 1851— BUG-11 no es un detalle: es la diferencia
entre leer la serie y leer ceros.

### 4.4 BUG-13: esquivado, no arreglado

`chisq()` de `nlatools.c` invierte la cola para `df >= 30` (`if (z > 0) prob =
1 - prob;` seguido de `if (z < 0) prob = 1 - prob;`). La copia de `drvec` es
**idéntica a la de `drvarma`**: el defecto sigue ahí. Lo que `42d8351` arregló
fue el p-valor del portmanteau, y lo hizo **saliéndose de la función**:
`diagnose_mv.c:61` define `SUITE_CHISQ_UPPER` como `gsl_cdf_chisq_Q`, y las
cuatro llamadas de `drvec.c` usan también GSL directamente. La decisión está
razonada en el fichero (*«no se toca chisq() en nlatools.c: ese fichero es
motor»*) y es defendible, pero deja dos cosas:

- una **divergencia de método** con la suite, que calcula sus p-valores con la
  `chisq()` de la casa mientras `drvec` los calcula con GSL;
- una **trampa** para el código futuro de `drvec`, que tiene a mano una función
  con el defecto puesto.

### 4.5 Los contratos univariantes

Los dos contratos de la escalera (`LADDER_AS_OPTIMISATION.md` §2.1 y §3) son la
frontera por la que `drvec` entra en la suite, y `drvec` los comprueba solo en el
peldaño diagonal:

| contrato | `drtran-python` | `drvec` |
|---|---|---|
| identidad de cruce: `Σᵢ logL(serie i) = logL(ajuste diagonal conjunto)` | −1.50e−07 | **1.8e−05** |
| certificado de optimalidad: `logL(ajuste) − logL(en los valores guardados) >= 0` | +0.000000 | **+2.4e−07** |

El certificado está al nivel del hermano. **La identidad de cruce está dos
órdenes de magnitud peor**, y la explicación registrada —«el redondeo del propio
formato»— no se ha medido: el `.pre` que lee `drvec` es el mismo objeto que lee
`drtran-python`. Puede ser cierta, y puede ser que se esté perdiendo precisión en
el camino. Hoy es una afirmación, no una medida, y las afirmaciones no medidas
son justo lo que la regla 1 del método prohíbe.

### 4.6 Qué se construye (P3)

1. **HECHO el 2026-08-21.** Adoptadas las cuatro rutinas de la copia canónica.
   **Sin comentarios, `nlatools.c` difiere del canónico en cero líneas**, igual
   que la de `drtran` y la del paquete de Python: cuatro copias, un fichero.

   Destapó **cinco** fugas, no dos: a `cond_resid` y `rawmat` se sumaron
   `alpha_A` y, en `-lrtest` y `-rungs`, `datamat` e `Y2_levels` —que se
   reservan una vez por rango—. Cerradas en `free_case_data()`, llamada desde
   `cleanup_names()`, que es por donde ya pasaban las siete salidas de `main`.

   Y de paso apareció una sexta cosa: `main` liberaba `datamat` e `Y2_levels`
   con `nobs` en lugar de con la dimensión de la **reserva**, que dejaron de ser
   el mismo número cuando `-estwin` empezó a recortar la muestra de estimación.
   Esa liberación ya no está; hay una sola, y usa las dimensiones de quien
   reservó.
2. **HECHO el 2026-08-21** (`drtran d5bf5be`, registro `58e293b`). Devueltos los
   tres arreglos del lector y el desasignador. BUG-11 y BUG-12 **cerrados**.

   Medido sobre un caso mensual real de `examples/`: de **20 bloques
   «definitely lost» a 2**, y de varios del lector a **cero**. Los dos que
   quedan son de `drtran` mismo —`main` y `apply_univariate_model`— y se dejan
   como están, registrados: ampliar el arreglo a las tripas de otro programa no
   es lo que se venía a hacer.

   Y comprobado lo que de verdad importaba: el mismo caso mensual con el lector
   viejo y con el nuevo da una salida **idéntica, cero líneas de diferencia**.
   El arreglo es del caso anual, que es el que estaba roto.
3. **BUG-13** — **HECHO el 2026-08-21**, y más de lo que el plan pedía. En vez
   de comentar que está rota, se ha **arreglado en las cuatro copias a la vez**
   —`drvarma` canónico, el paquete de Python, `drtran` y ésta—, carácter por
   carácter, porque un arreglo divergente en un fichero compartido es peor que
   el defecto. Comprobado antes de tocar nada que **ningún llamante se había
   adaptado**: los ocho escriben `1.0 - chisq(...)`. Verificado contra
   `gsl_cdf_chisq_P` a través del objeto construido: 1.9e−04 con `df ≥ 30`, que
   es el error de Wilson-Hilferty, y precisión de máquina por debajo. Y con
   regresión: `tests/chisq_probe.c` y el bloque `[8h]`, que **vigila código que
   `drvec` no ejecuta** porque los dos programas que sí lo ejecutan no tienen
   batería. Medido que muerde: devolver la segunda corrección levanta dos
   fallos.

   *Lo que queda fuera:* `drvarma_source` no está bajo control de versiones, así
   que sus dos copias quedan arregladas en disco y sin commit; y las versiones
   archivadas `v.01`–`v.04` se dejan como están, a propósito.
4. **HECHO el 2026-08-21, y la pregunta estaba mal planteada.** No es el
   redondeo del formato ni pérdida de precisión: es **la truncación de `ξ`**.
   Medido en la puerta diagonal — `q = 1` da −1.773e−05 con truncación y
   −1.634e−10 sin ella (`-m 2`), y `q = 0` da −1.123e−12 con las dos, porque no
   hay sucesión que truncar.

   Y corrige dos cosas escritas. `SUITE_INTEGRATION.md` §3 atribuía **las dos**
   diferencias al `%.6f` del formato; sólo la segunda —el certificado, que
   evalúa en los valores guardados— lo es. Y este plan comparaba 1.8e−05 contra
   el −1.50e−07 del hermano y llamaba a `drvec` «dos órdenes peor»: los dos
   números nunca fueron comparables —otro modelo, otra `q`, y una truncación que
   sólo uno lleva— y sin ella este lado da 1.6e−10. Es la regla del propio plan
   vuelta contra él: una cifra tomada del documento de otro programa no es una
   medida de éste.
5. **HECHO el 2026-08-21**: [ARCHITECTURE.md](ARCHITECTURE.md), con las cifras
   ya no como tabla de divergencias sino como **una columna de ceros**, y con el
   comando que las vuelve a medir al final. Si una fila deja de ser cero, o
   alguien arregló algo aquí y no allí o al revés — y ninguna de las dos cosas
   se anuncia sola.

6. **HECHO**: `tools/compare_johansen.py` elegía el rango contando cuántas filas
   de la traza rechazan, cuando la secuencia se para en la primera que **no**.
   Coincide mientras los rechazos sean un prefijo —lo habitual, y por eso duró—
   y falla en cuanto una fila posterior rechaza con una anterior que no, que es
   justo el caso de los tres IPC de `HOMOLOGATION.md` §4u: contar daba 1 y la
   secuencia da 0. Daba la respuesta correcta por la razón equivocada.

**Criterio de salida de P3:**

| # | criterio | estado |
|---|---|---|
| P3.1 | `diff` de `elfvarma.c`, `drvmlest.c`, `qnewtopt.c` y `nlatools.c` contra `drvarma_v.04.1`: **cero líneas de código** | **✔** los cuatro a cero |
| P3.2 | un test de la batería que ejecuta ese `diff` y falla si aparece código divergente | **✘ no hecho** — la batería no puede alcanzar `drvarma_source`, que está fuera del repositorio y sin control de versiones. Queda el comando en [ARCHITECTURE.md](ARCHITECTURE.md) §7, que es una comprobación manual y hay que decirlo |
| P3.3 | `VALGRIND=1 make test` verde **con el layout compartido** | **✔** 230, y 235 con `SLOW=1` |
| P3.4 | BUG-11 y BUG-12 cerrados en el registro de la suite, con parche aplicado a `drtran` | **✔** `drtran d5bf5be`, registro `58e293b` |
| P3.5 | la identidad de cruce, medida y explicada | **✔** es la truncación de `ξ`, y corrige dos afirmaciones escritas |

---

## 5. La especificación por defecto — P4 — **HECHA el 2026-08-20**

*Escrito el 2026-08-20, después de `HOMOLOGATION.md` §4q y §4r y del Corolario
6.3 de `DEMOSTRACIONES.md`. Esta fase no existía cuando se escribió el plan: el
plan suponía que la especificación estaba decidida, y `SPECIFICATION_PLAN.md` §8
la había decidido mal.*

### El problema, medido

En el régimen en que vive el banco —medias móviles del tipo `(1 − θB)`, que es
lo que dan las series de precios y poblaciones diferenciadas— **el `Θ` libre no
es recuperable**: a `n = 120` y a `n = 250`, en toda magnitud probada, con
dispersión intercuartílica de 0.4 a 1.5 sobre un parámetro de orden 1, y
`-multistart` lo empeora en vez de mejorarlo (§4q). Ocho de los nueve casos del
banco acaban con una raíz MA clavada en 0.99995, que es el umbral exacto de la
puerta del motor.

Con `Θ` libre no hay modelo VARMA-VEC que pueda competir con un univariante:
cuatro parámetros MA que la verosimilitud no distingue no compran previsión,
la degradan.

### Lo que la teoría da, y es una solución y no un diagnóstico

**Corolario 6.3.** Con las `s` filas inferiores de cada `Θ̃ₖ` nulas —la condición
(6) del Teorema 6, y también la clase más débil de `-marow`—, para todo `q` y
todo `(r, s)`:

```
det Θ̃(1) = det(I_r − Σ T_k)
autovalores(compañera de Θ̃) = autovalores(compañera del bloque r×r) ∪ {0,…,0}
```

Luego `Θ̃(L)` es invertible **si y solo si** lo es su bloque `r × r`, y por el
Corolario 3.1 la condición de rango del Teorema 3 se cumple automáticamente. El
punto de `𝒫 \ 𝒞` que el Teorema 4 dice que la verosimilitud premia y el Teorema
5 dice que **ningún chequeo del motor puede ver, no es alcanzable en esta
clase**: ahí `chekma` sobre `Θ̃` es exactamente `chekma` sobre el bloque `r × r`.
Con `M = 2, r = 1`, la admisibilidad pasa de ser el rango de una matriz `2 × 2`
invisible al motor a ser `|θ_w| < 1`, un escalar que ya comprueba.

**El problema no se diagnostica: se elige una parametrización en la que no
existe.**

### Y funciona

§4r, sobre una WARMA con el bloque `w` identificado: en `θ = +0.9` —coeficiente
positivo, raíz en 1.11, pegada al círculo, el caso duro— `-mawarma` da sesgo
0.050 e IQR 0.272, interior, `G = 0.96`; el libre se queda **en la puerta** con
`G = 0.45` y el doble de dispersión. `G` no baja de 0.93 en ninguna celda de la
clase estructurada.

### Qué se construye (P4)

1. **La clase del Teorema 6 pasa a ser el modelo por defecto.** El `Θ` libre
   queda como diagnóstico y como término de comparación, no como especificación
   de la que publicar.
2. **Re-medir el registro entero bajo el nuevo defecto**, y etiquetar cada cifra
   existente con la parametrización a la que pertenece. Esto es lo caro y es
   inevitable: un defecto que se mueve mueve todos los números.
3. Los valores dorados de la batería, re-medidos o re-etiquetados.
4. `-rankadm` se queda como está: instrumento de medida sobre la clase libre. En
   la clase estructurada no hace falta, y decirlo es parte del cambio.
5. El aviso de la terminal cambia de sentido: ya no avisa de que el ajuste niega
   su rango, sino de que se está usando la clase libre, que es donde eso puede
   pasar.

**Criterio de salida de P4:**

| # | criterio | estado |
|---|---|---|
| P4.1 | el defecto es la clase del Teorema 6, y `make test` verde con los dorados re-medidos | **✔** 201 comprobaciones, 211 con `VALGRIND=1`, 0 fallos |
| P4.2 | ninguna cifra del registro sin la parametrización a la que pertenece | **✔** nota al principio de `HOMOLOGATION.md` y línea `MA :` en la cabecera de todo `.out` |
| P4.3 | ningún ajuste por defecto puede alcanzar `det Θ(1) = 0`, comprobado por una prueba que lo intente | **✔** bloque [8c]: el defecto llega a la frontera en **0 de 5** casos del banco y `-mafree` en **4 de 5**, o sea que la comprobación muerde |
| P4.4 | el `.out` reporta el bloque `r × r` y su raíz, que es ahora la condición entera | **✔** con la nota del Corolario 6.3 al lado de las raíces |

### Lo construido, y lo que costó

**El defecto.** Con `q ≥ 1` **y `r ≥ 1`** y sin bandera de clase, se estima
`-marow`. La condición sobre `r` no estaba en el plan y es necesaria: en `r = 0`
no hay bloque `W`, el «bloque `r × r`» es `0 × 0` y anular las `s = M` filas
inferiores anularía `Θ` entera. Es lo que `SPECIFICATION_PLAN.md` §9 ya decía
—las clases restringidas colapsan en `r = 0`— y es también donde viven los dos
contratos de la escalera. Se resuelve en un solo sitio, `ma_struct_on()`, que
consulta el `r` **del ajuste en curso** y no el de la línea de órdenes: `-lrtest`
recorre `r = 0..M−1` y necesita una sola clase en todos ellos, que es la libre.

**`-mafree`** devuelve el defecto anterior, y es la trigésima cuarta opción.

**Tres sitios más que el plan no había previsto**, y los tres son la misma
lección — cuando el defecto se mueve, todo lo que *transporta* parámetros entre
ajustes tiene que saber a qué clase van:

- `init_guess` escribía el bloque MA según las banderas sin guardia de `r`, y
  con `-lrtest` desalineaba el vector (`ERROR init_guess: idx=6, npar=10`).
- `gate_profile_seed` (`-seedgate`) escribía `M·M` valores de la `Θ` retenida en
  el peldaño `r = 0`; ahora escribe la **proyección** sobre la clase de destino,
  que es la lectura correcta y no un parche: se conservan las entradas que la
  clase lleva y se descartan las que anula.
- La escalera `-specs` y el bootstrap de `-matest` ya salvaban y restauraban las
  banderas, y por eso no hubo que tocarlos.

**La batería pasó de 190 a 201 comprobaciones** (211 con memoria). Ocho valores
dorados se re-midieron y **siete se conservan bajo `-mafree`**, de modo que la
parametrización anterior —la que sostiene todo el registro previo— sigue
protegida contra deriva. Seis pruebas que construían un caso inadmisible ahora
lo piden con `-mafree`: que el defecto ya no pueda producirlo es el resultado, y
se comprueba aparte en [8c].

**Lo que se movió, dicho como manda la regla 2 del método.** Ocho valores
dorados. Por ejemplo `2 1 1 -case 2` pasa de **6.4786** a **2.3040** —menos
verosimilitud con doce parámetros en vez de catorce— con `G` de **0.18** a
**0.39** y la raíz MA de **0.99995** a **1.28**: fuera de la puerta, admisible,
y con `B̂₂` de −0.2405 a −0.1508. Cuál de los dos está más cerca de la verdad no
lo dice la verosimilitud, y es lo que P5 tiene que medir.

---

## 6. La previsión, y el criterio de aceptación que faltaba — P5

*El requisito 6 de §0. Es la fase que cierra el programa, y no existía en este
plan ni en ningún otro documento del proyecto.*

**El hueco, dicho sin rodeos.** `drvec` **no sabe prever**. Cero apariciones de
`forecast` en el fuente; ninguna de sus treinta y tres opciones lo hace; no hay
evaluación fuera de muestra ni comparación contra un univariante. El registro
tiene mil setecientas líneas sobre estimación y **ninguna medida de la única
cosa que decide si el modelo sirve**.

Eso no es un olvido menor. Es muy difícil mejorar la previsión de un univariante
con un modelo multivariante — es el resultado empírico repetido de la literatura
de previsión —, y toda la razón de ser de este programa es la estructura de
corto plazo que un univariante no tiene. Si esa estructura no se puede
parametrizar de forma que se estime con `n = 120`, no hay nada que ganar; y si
se puede (P4), hay que demostrar que se gana.

**El precedente está en la suite** y no hay que inventarlo: `drtran/src/forecast.c`
en C, y en el porte `forecast.py` y `evaluate.py` con `-estwin` —origen móvil,
parámetros fijos, MAE/RMSE/MAPE por horizonte—, cuya propia ayuda lo describe
como *«the only way to decide EMPIRICALLY whether one model forecasts better than
another»*.

### Qué se construye (P5)

1. **Previsión** a horizonte `H` desde el modelo VEC, con sus bandas, en niveles
   — deshaciendo la transformación, que es donde está la dificultad técnica y
   donde `drtran` ya tiene el precedente y **BUG-10 abierto** sobre la varianza
   del nivel: hay que leerlo antes de escribir esto.
2. **Evaluación de origen móvil** (`-estwin E -f H`): estimar una vez en
   `1..E`, mantener los parámetros fijos, avanzar el origen, y acumular error
   por horizonte.
3. **El banco de comparación**: el mismo ejercicio con `fue` univariante serie a
   serie, que es el contrafactual que importa. La suite ya tiene el puente para
   escribir los `.inp` y leer los `.pre`.
4. Una tabla en el registro: RMSE por horizonte, VEC contra univariante, sobre
   los casos del banco, con la especificación seleccionada por el protocolo.

**Criterio de salida de P5, y es el criterio de salida del programa:**

| # | criterio | estado |
|---|---|---|
| P5.1 | `drvec` prevé en niveles con bandas, y la previsión a un paso coincide con el residuo del ajuste | **✔ 2026-08-20**: `-f H`, y el certificado da 3.5e−04 —que es la truncación de `ξ`— y 1.8e−15 con `-m 2`. Más un segundo certificado que el criterio no pedía y que hacía falta: la banda a un paso es la covarianza de la innovación leída en niveles, comprobada contra la `Σ` del vector de parámetros en cinco configuraciones. Ver [FORECAST.md](FORECAST.md) |
| P5.2 | evaluación de origen móvil implementada y comprobada contra un caso cuyo resultado se puede calcular a mano | **✔ 2026-08-20**: `-estwin E -f H`, con el protocolo de la suite (estimar una vez en 1..E, parámetros fijos, origen rodando). El caso a mano es el de un solo origen, donde MAE y RMSE son el mismo número por construcción, y la batería lo comprueba junto con el conteo `n − H − E + 1` |
| P5.3 | **la tabla VEC contra univariante existe y está publicada, gane o pierda** | **✔ 2026-08-20, y PIERDE**: [HOMOLOGATION.md](HOMOLOGATION.md) §4t. Con `q = 1` el VEC pierde en 7 de 9 casos a todos los horizontes, hasta por un factor 2.9; con `q = 0` queda en tablas (6/9, 5/9, 5/9, 4/9) con márgenes de pocos puntos. El contrafactual es el peldaño diagonal del propio programa, que por el Teorema 9 **es** un ARIMA por serie |
| P5.4 | si pierde en todo el banco, el registro lo dice en la primera página y el programa se describe como lo que entonces es: un estimador de máxima verosimilitud exacta para una clase de modelos, no una herramienta de previsión |

P5.4 no es una cláusula defensiva. Es el punto: la única forma de que este
proyecto sepa lo que tiene es medirlo, y hasta hoy nadie lo ha medido.

---

## 7. Empaquetado y cita — **HECHA el 2026-08-22**

El estándar del conjunto ya existe y no es este programa:

| | versión | `pyproject`/licencia | `CHANGELOG` | registro de defectos |
|---|---|---|---|---|
| `fue` | 0.1.11 | sí | sí | sí |
| `drvarma` | 0.1.6 | sí | sí | sí |
| `drtran-python` | 0.2.4, «Development Status :: 4 - Beta» | sí, GPL-2.0-or-later | sí | `docs/BUGS.md` |
| **`drvec`** | **ninguna** | **no hay fichero de licencia** | **no** | **no** |

`drvec` tampoco tiene etiquetas de git, ni `make install`, ni integración
continua, ni `--version`. Y `docs/README.md` dice **«Pre-beta»** mientras
`PLAN_BETA.md` marca sus nueve criterios de salida con ✔: el estado declarado y
el estado registrado no coinciden.

### 7.1 El sistema de ficheros de salida — medido el 2026-08-22

Los dos programas que hay que seguir no dicen lo mismo, y el que hay que seguir
es el que **separa**:

| programa | ficheros que escribe | criterio |
|---|---|---|
| `drv` (`/drv_project`, Phillips triangular) | `<base>.out` y nada más | un solo `fopen` de salida en `main.c:1043`; el `.out` de `AL.2` son 2124 líneas donde el volcado del optimizador, los residuos y los parámetros van seguidos |
| `drvarma` C v.04.1 | `<base>.out`, `<base>.forecast`, `<base>.recursive`, `<base>.volexp`, `<base>.volmov` | un fichero por **producto**, con su propia cabecera reproducible |
| `drvarma` Python 0.1.6 | los mismos, más `<base>_<serie>.html` | `cli.py` los nombra en un solo sitio y anuncia cada uno por `stdout` |
| **`drvec` 0.9** | **`<base>.out`**, y lo demás sólo si el usuario nombra la ruta | `-C FICHERO` (CSV de errores) y `-writeres`/`-writeinp` (prefijo) |

El defecto de `drvec` no es que escriba poco: es que **la previsión y la
evaluación de origen móvil no tienen fichero propio**. La tabla de previsión se
imprime dentro del `.out`, que es el informe de la ESTIMACIÓN, y los errores
origen a origen sólo existen si el usuario se acuerda de pasar `-C`. Eso rompe
dos cosas a la vez:

- **la previsión no se puede consumir**: no lleva fechas, así que quien la lee
  tiene que reconstruir el calendario por su cuenta a partir de la cabecera del
  `.inp`. `drvarma` lleva `sub/año` en cada fila desde su v.04, y `ObsToDate` ya
  está en `drvec` (`src/fue_bridge.c`, usado por `-writeres`): lo que falta es
  llamarlo;
- **la evaluación fuera de muestra es opcional por accidente**. §4t del registro
  —la medida que decide que la versión es 0.9 y no 1.0— se hizo con `-C`
  apuntando a un fichero temporal. Una medida que sostiene el número de versión
  no puede depender de que el usuario recuerde una opción.

`drv` no sirve de modelo aquí, y conviene decir por qué en vez de callarlo: su
`.out` mezcla el volcado del optimizador con el resultado porque nunca tuvo que
alimentar a nada. `drvec` sí: `tools/forecast_vs_univariate.py` y el
Diebold-Mariano leen el CSV de `-C`.

### 7.2 Las hipótesis por defecto — lo que un VEC dice y `drvec` se calla

`drvarma` imprime **siempre** un bloque `JOINT HYPOTHESIS TESTS (WALD)`
(`report.py:_wald_blocks`): último retardo AR, último MA, todos los efectos
cruzados, y para cada variable las dos direcciones (qué la influye y a quién
influye). No hace falta pedirlo.

`drvec` imprime por defecto la diagnosis (Hosking, Jarque-Bera multivariante,
las R(k)) y la condición de rango de Granger — todo sobre los RESIDUOS — y
**ninguna hipótesis sobre las relaciones**, que es de lo que trata el modelo.
Las que tiene existen todas detrás de una opción y **todas exigen reestimar**:

| hipótesis | opción hoy | coste |
|---|---|---|
| rango de cointegración | `-lrtest` (+ `-bootstrap N`) | M estimaciones |
| exogeneidad débil de la variable i | `-weakex i` | una estimación restringida |
| estructura de la media móvil | `-matest N` | N remuestreos |
| dinámica corta triangular | `-artest N` | N remuestreos |
| B₂ conocida | `-fixb2 v` | una estimación restringida |

Falta el escalón barato: la matriz de covarianzas de los parámetros **ya está
calculada** (`est` la devuelve en `cov`, y de su diagonal salen los `sd` que el
`.out` ya imprime junto a Λ y a B₂). Con ella, un Wald sobre un subvector es
aritmética, no una estimación más. Y las hipótesis que un VEC contesta con eso
son exactamente las que dan sentido al modelo:

1. **Λ = 0** — el término de corrección de error, entero. **NO es un contraste**
   y hay que escribirlo así: bajo Λ = 0 la matriz B queda sin identificar
   (problema de Davies), y la χ² no es la distribución de nada. Se imprime por
   referencia y se remite a `-lrtest -bootstrap`, igual que ya se hace con la
   χ² de `-matest`.
2. **Exogeneidad débil, fila a fila: Λ_{i·} = 0**, `r` g.l. Ésta **sí** es una
   χ² estándar (Johansen 1992): con Λ ≠ 0 en conjunto, B sigue identificada.
   Dice qué variables no se ajustan al desequilibrio, que es la pregunta que
   trae a un usuario a un VEC.
3. **Exclusión de la relación, fila a fila: B₂_{i·} = 0**, `r` g.l., χ²
   estándar por la superconsistencia de β. Dice qué variables no entran en la
   relación de largo plazo.
4. **El bloque corto**: último retardo de F y de Θ, los efectos cruzados y las
   dos direcciones por variable — literalmente lo que hace el hermano, sobre
   los mismos parámetros y con la misma lectura.

Las tres primeras son las que `drvarma` no puede tener porque su modelo no
tiene ni Λ ni B: son la parte en que `drvec` es informativo y el hermano no.
Que hoy no salgan por defecto es el hueco.

**Una advertencia que va en el propio bloque**: por defecto `cov` viene del
factor que acumula el BFGS, no del hessiano en el óptimo. `-fdhess` lo sustituye
por el de diferencias finitas AL óptimo, y es el que hay que usar para publicar
un contraste. Se dice ahí y no en la documentación, que es donde nadie mira
cuando está leyendo un p-valor.

### Qué se construye (P6)

1. `LICENSE` (GPL-2.0-or-later, que es lo que el README ya declara) y
   `CITATION.cff` con Mauricio (2006) como referencia del método.
2. Versión en un solo sitio, embebida en el binario, impresa por `--version` y
   en la cabecera del `.out` — **hecho el 2026-08-21**: `0.9`, y va también en
   la primera línea del fichero de resultados, porque un `.out` que no dice con
   qué se produjo no lo reproduce nadie. Etiqueta `v0.9`.

   **Por qué 0.9 y no 1.0**, decidido y no heredado: un número de versión es una
   afirmación sobre lo que hay dentro, y dentro hay dos cosas que un 1.0
   taparía — que el programa **no mejora la previsión de un ARIMA por serie**
   sobre su banco (§4t del registro), y que **la especificación por defecto
   cambió el 2026-08-20**, con ella todas las cifras medidas sobre la anterior.
   Ninguna de las dos es un defecto que arreglar: son el estado del
   conocimiento, medidas y escritas. Lo que no procede es ponerles un 1.0
   encima.
3. `CHANGELOG.md` con el formato de `drtran-python`: lo publicado, y los
   informes completos en el registro.
4. `docs/BUGS.md` propio, o entradas en el del conjunto — a decidir, pero **uno
   de los dos**; hoy los defectos de `drvec` viven repartidos entre el registro
   de desarrollo y comentarios en el código.
5. `make install` con `PREFIX`, y CI que corra `make test` y `VALGRIND=1 make
   test`.
6. Alinear el estado declarado: `docs/README.md` deja de decir «Pre-beta».
7. **El sistema de ficheros de salida del conjunto** (§7.1): `<base>.forecast`
   con FECHAS y bandas, y `<base>.recursive` para los errores origen a origen,
   escritos sin que haya que nombrar la ruta. `-C FICHERO` se queda como
   redirección, no como condición para que la medida exista.
8. **El bloque de hipótesis por defecto** (§7.2): Wald sobre la `cov` que ya se
   calcula — Λ = 0 por referencia y dicho que no es un contraste, exogeneidad
   débil fila a fila, exclusión de la relación fila a fila, y el bloque corto
   del hermano. Con el aviso de `-fdhess` dentro del bloque.

### Lo construido (P6), el 2026-08-22

| | |
|---|---|
| `LICENSE` | GPL-2.0, la copia del hermano `drvarma` v.04.1, que es lo que el README ya declaraba sin fichero detrás |
| `CITATION.cff` | con Mauricio (2006) como referencia del método. Los autores son **la línea de copyright que llevan las fuentes** (`src/drvec.c:22`), no una atribución nueva |
| `CHANGELOG.md` | el formato de `drtran-python`: lo publicado aquí, los informes completos en el registro |
| `docs/BUGS.md` | registro propio, **con la numeración del conjunto**: `BUG-1` a `BUG-13` viven en `drtran-python`, y de ahí siguen `BUG-14` y `BUG-15`. Un puntero cruzado en el otro registro dice que lo siguiente empieza en 16, para que un número no signifique dos cosas |
| `make install` | con `PREFIX` y `DESTDIR`; binario en `bin/`, documentación en `share/doc/drvec`. La CI comprueba que instalar y desinstalar no mienten |
| CI | dos trabajos: `make test`, y `VALGRIND=1 make test` aparte porque tarda varias veces más |
| `docs/README.md` | deja de decir «Pre-beta» y dice 0.9, con las dos razones |

**El sistema de ficheros (P6.7).** `<base>.forecast` con la FECHA de cada fila,
y `<base>.recursive` con los errores origen a origen, los dos escritos sin que
haya que nombrar la ruta. `-C` pasa a ser redirección. Ocho comprobaciones
nuevas en la batería, bloque `[8j]`, incluida la que ata las tres columnas de la
banda entre sí y la que comprueba que la primera fecha continúa el `.inp`.

**Las hipótesis (P6.8).** Bloque Wald por defecto sobre la `cov` que ya se
calculaba. Ocho comprobaciones nuevas, bloque `[8i]`, y la que vale es una
**identidad**: con un solo parámetro el Wald ES el cuadrado del cociente `t` que
el propio `.out` imprime al lado del coeficiente, y se comprueba en los dos
extremos del vector de parámetros — Λ, que está al principio, y B₂, que está al
final. Si el mapa de índices se desalineara —el fallo de §4.1— la igualdad se
rompe en el acto. Por eso los índices se anotan **en el recorrido que ya hay** y
no en uno nuevo.

**Y un defecto encontrado por el camino**, que es lo que suele pasar cuando se
pasa valgrind sobre código que no se había pasado: `BUG-14`, `xitol` sin
inicializar en la evaluación de origen móvil, o sea en la ruta de §4t. Las tres
columnas de §4t se volvieron a medir y **reproducen la tabla dígito a dígito**,
así que el coste medido es cero y el resultado se sostiene — ahora sobre
comportamiento definido. Ver `HOMOLOGATION.md` §4v.

Batería: **235**, y **245** con `VALGRIND=1`.


---

## 8. Lo que **no** entra en producción

Declarado para que el alcance no se estire, y porque nada de esto impide
publicar mientras siga estando escrito con esta claridad:

- **La columna EML de la Tabla 5 de Mauricio (2006)**, no reproducida y no
  cerrable con el material disponible.
- **El termcode 3**, explicado y medido, no eliminado.
- **La calibración en muestra finita del test de rango**: 68 %/30 % por la vía
  asintótica, 78 %/20 % con bootstrap. Mejorado, no arreglado.
- **La frontera de invertibilidad en la clase libre**: ahí toda especificación
  con `q >= 1` se apoya en ella sobre estos datos, y los errores estándar no
  están definidos en la dirección que liga. *En la clase por defecto de P4 deja
  de ser un problema abierto y pasa a ser imposible (Corolario 6.3); queda como
  limitación de la clase libre, que se sigue ofreciendo.*
- **`-marow` contra el modelo libre** con una distribución válida.
- **Los cuatro p-valores en el suelo `1/(B+1)`**, que necesitan `B >= 999`.
- ~~**El refactor de `main()`.**~~ **Hecho el 2026-08-23** (§11): 2 320 → 468
  líneas, nueve funciones extraídas, y la red que dice que no movió nada.
- ~~**La unificación del idioma de la salida.**~~ **Hecha el 2026-08-22** (§10.1):
  `src/drvec.c` entero en inglés, contra un invariante, y con una comprobación
  en la batería para que no se deshaga.

*Los dos tachados se hicieron después de la etiqueta `v0.9`, que es donde el
plan los había puesto. Se dejan escritos porque el resto de esta lista sigue
vigente y porque un plan que borra lo que cumplió no permite juzgar si el orden
era el bueno.*

---

## 9. Orden y por qué

**P1 → P4 → P5 → P2 → P3 → P6 → P7 → P9 → P8. Todas hechas.**

El orden cambió el 2026-08-20, y el motivo es el requisito 6 de §0.

**P4 antes que nada de lo demás**, aunque sea la fase cara. La especificación por
defecto decide qué modelo estima el programa, y por tanto qué significan todas
las cifras del registro. Re-medir el registro (P2) bajo un defecto que va a
cambiar es trabajo tirado; empaquetar (P6) un programa cuya especificación por
defecto se sabe inadecuada es peor que no empaquetarlo. Antes se ponía P4 al
final porque se suponía la especificación decidida; `SPECIFICATION_PLAN.md` §10
explica por qué no lo estaba.

**P5 inmediatamente después**, porque es el criterio que dice si algo de esto
valía la pena, y porque cuanto antes se sepa mejor. Si el modelo estructurado no
mejora al univariante sobre el banco, eso cambia lo que el programa dice ser, y
conviene saberlo antes de invertir en el registro y el empaquetado, no después.

**P2 y P3 después de P5** por la misma razón: son consolidación, y consolidar
sobre una especificación que aún puede moverse es hacerlo dos veces. P3 sigue
teniendo que ir antes de la etiqueta, porque no tiene sentido publicar `v0.9`
con el motor divergiendo del canónico, con dos defectos del registro resueltos
aquí y abiertos allí, y con dos fugas que la alineación de `nlatools` destapa.

**P6 cierra.** P7 y P8 no cambian ningún número y pueden ir en `v1.1`.

**Y dos órdenes que no seguiría.** Empezar por P6: empaquetar es lo visible y
produciría una versión cuya especificación por defecto no se sostiene. Y dejar
P5 para el final, que es lo que este plan hacía sin darse cuenta al no incluirlo:
un programa que nunca mide si sirve para lo que existe puede pasar todas sus
pruebas y no servir para nada.

---

## 10. P7 y P9, y por qué van juntas — **HECHAS el 2026-08-22**

Las dos salieron de la misma revisión y de la misma pregunta: **qué parte de
`drvec` está donde el usuario la ve, y qué parte está donde sólo la ve quien lo
mantiene.**

### 10.1 El idioma (P7)

`src/drvec.c` tenía 1 156 líneas de comentario en castellano dentro de un
programa con documentación inglesa, y `docs/README.md` citaba salida ya
traducida («1 df» donde el programa escribía «1 g.l.»). Las dos cosas son el
mismo defecto: **lo que se puede auditar y lo que no**. Un comentario en un
idioma que el lector no tiene es un comentario que no está, y un documento que
cita una salida que el programa no emite ha dejado de ser comprobable.

Traducido entero el 2026-08-22: 363 comentarios y ~40 mensajes al usuario.

**Cómo, y esto importa más que el resultado.** Traducir a mano 1 200 líneas de
prosa dentro de un fichero de 7 600 es exactamente el tipo de edición que mueve
una línea de código sin que nadie se entere — un `ii++` desplazado dentro del
recorrido de la impresora es §4.1 otra vez. Así que la traducción se hizo contra
un **invariante**: `tools/strip_comments.py` quita todos los comentarios y las
dos versiones, antes y después, tienen que ser **idénticas byte a byte**. Una
traducción que cambie un carácter de código no pasa. Se comprobó en cada uno de
los ocho lotes.

Lo que queda fijo es `tools/check_language.py`, en la batería (bloque `[8k]`),
**sólo sobre `src/drvec.c`**: `nlatools.c`, `fue_pre_reader.c`, `fue_bridge.c` y
`diagnose_mv.c` son del conjunto, copiados de `drvarma` y `drtran`, y
traducirlos aquí sería justamente la divergencia que P3 existe para impedir. Su
idioma es asunto de su dueño. Y el propio comprobador se comprueba: la batería
le pasa un fichero que es castellano de principio a fin y exige que lo rechace,
porque una comprobación que no puede fallar es decoración.

### 10.2 La versión y los defectos, con sistema (P6, ampliada)

**El razonamiento de la versión sale del código.** Vivía en un comentario junto
al `#define`, y es un argumento que el lector necesita **antes** de ejecutar el
programa, no mientras lee sus fuentes. Ahora está en `docs/VERSIONS.md`, que
además dice lo que un registro de versiones tiene que decir y no suele:

- **qué afirma cada número** — aquí no es «cuánto se ha añadido» sino qué puede
  dar por bueno quien lo usa;
- **qué tendría que ser cierto para que se mueva** a 1.0, escrito como condición
  y no como tarea;
- y una regla que este proyecto ya había aplicado sin escribirla: **un defecto
  encontrado no retrasa una versión; un defecto tapado sí.** `BUG-14` se
  encontró el día de la etiqueta 0.9, en la ruta misma sobre la que descansa el
  argumento de la versión. Se arregló, se volvió a medir la tabla que dependía
  de él, y el resultado entró en el registro. Esa secuencia es lo que un número
  de versión puede sostener; lo que no puede sostener es una medida que nadie
  volvió a correr.

**Y los dos registros que se llevan a mano se comprueban**, porque son
exactamente lo que se pudre en silencio:

| | qué comprueba |
|---|---|
| `tools/check_version.sh` | el número dice lo mismo en el `#define`, `CITATION.cff`, `CHANGELOG.md`, `docs/VERSIONS.md`, el binario y la etiqueta. Una publicación cuyo `CITATION.cff` lleva el número anterior no la puede citar nadie |
| `tools/check_bugs.py` | cada entrada tiene número, estado y **coste medido**; los números no se repiten ni invaden el rango que otro registro del conjunto posee; y todo `BUG-N` citado en el árbol está registrado en alguna parte — una referencia a un número que nadie registró no identifica nada |

Los dos van en la batería (bloque `[8l]`) y los dos se comprueban a sí mismos:
se les da un `CITATION.cff` con otra versión y una entrada sin coste, y tienen
que fallar.

### 10.3 La entrada por `.pre` (P9)

**El diagnóstico.** `drvec` comparte la escalera de `fue` y `drvarma` no. Se ve
en las interfaces:

| | entrada | por qué |
|---|---|---|
| `drvarma` | un `.inp` con todas las series | no comparte la escalera: **es** el motor sobre el que la escalera está construida |
| `drtran` | `drtran salida.pre entrada1.pre ...` | sí la comparte: el trabajo univariante se hace en `fue` y llega hecho |
| **`drvec` hasta la 0.9** | **un `.inp`**, y los `.pre` sólo para sembrar | comparte la escalera y su interfaz no lo decía |

Y no era sólo una cuestión de forma. Para partir de modelos de `fue` había que
dar **tres pasos a mano**: exportar las series a un `.inp`, `-interv` para
restar las deterministas, y `-seed` para la media móvil. Tres pasos que hay que
recordar y un formato que rellenar a mano — y el segundo de ellos es donde
apareció `BUG-15`.

**Lo construido.** `drvec s1.pre s2.pre ... sM.pre p q r [opciones]`. De cada
`.pre` se toma la serie, su transformación `w = refactor·BoxCox(z)` —que es el
contrato del formato y literalmente lo que hace `drtran.c:885`—, sus términos
deterministas (restados, con **las fechas de esta muestra**, porque una
determinista es función del tiempo) y su calendario. Los ficheros se alinean
**por fecha** y se usa la intersección. El orden de columnas es el del `.inp`,
y se dice cuál fue a cada bloque.

**Lo que NO hace, y es deliberado.** No siembra la media móvil desde esos
modelos, aunque los tiene delante y sería gratis: está medido que **empeora** el
ajuste con `r ≥ 1` (F2.7 y F2.8), y una ruta que hace en silencio algo medido
como dañino es peor que una que obliga a pedirlo. `-seed` sigue pidiéndolo.

**El certificado.** La ruta nueva no es «otra forma de leer datos»: es una
segunda implementación de la entrada, y una segunda implementación se justifica
con una **identidad**. Sobre mink–muskrat, la ruta `.pre` reproduce la ruta
`.inp` **byte a byte** de `ESTIMATION SUCCESSFUL` en adelante; y con una
determinista en juego coincide **exactamente** con lo que da `-interv` sobre los
mismos modelos, que es lo que dice que las unidades son las correctas —lo que
`BUG-15` era. Las dos están en la batería, bloque `[8m]`, con lo que refusa:
un solo fichero, frecuencias mezcladas, `-interv` (restaría dos veces) y
`-differenced`.

---

## 11. P8, el refactor — **HECHO el 2026-08-23**

Era lo último de la lista y lo que el plan describía como «no bloquea el
lanzamiento y encarece todo lo que venga después». Lo segundo se notó durante
P6, P7 y P9: cada vez que hubo que insertar algo en `main()` hubo antes que
leerlo entero para encontrar dónde.

### El problema, medido

`main()` tenía **2 320 líneas** y llevaba, a la misma indentación: el análisis de
la línea de órdenes, la lectura del `.inp`, cinco modos completos (`-rungs`,
`-specs`, `-lrtest`, `-matest`/`-artest`, `-eval`), el multiarranque y **el
informe entero**, 793 líneas de él.

### La red, primero

Un refactor afirma que **no mueve nada**, y esa afirmación es más fuerte que las
que comprueba el resto de la batería, así que necesita una comprobación más
fuerte. `tools/golden.sh` guarda el hash de **cada byte de cada informe** sobre
24 configuraciones elegidas para tocar todos los modos, y se verifica después de
cada corte. Los 24 siguen idénticos.

Conviene decir lo que esa red **no** es: un invariante dice lo que tiene que ser
cierto en cualquier versión correcta; un hash dorado sólo dice que la salida de
hoy es la de ayer, y vale exactamente lo que valía el día en que se capturó.
Para un cambio cuyo objetivo es que nada se mueva es la única red honesta; para
cualquier otro no sirve. Va en la batería como bloque `[8n]` y tarda 1,2 s.

### Lo cortado

| | líneas | |
|---|---|---|
| `report_fit` | 793 | todo lo que el `.out` dice de un ajuste |
| `parse_cli` | 267 | la línea de órdenes y lo que fija |
| `run_lrtest` | 210 | el contraste secuencial de rango |
| `run_specs` | 155 | la escalera de especificaciones |
| `run_ma_ar_test` | 144 | los dos contrastes con distribución simulada |
| `run_multistart` | 106 | no es un modo: deja un ajuste detrás |
| `run_rungs` | 102 | la escalera bajo el rango |
| `read_inp_input` | 53 | la ruta `.inp`, al lado de `read_pre_inputs` |
| `run_eval` | 32 | la verosimilitud en el punto de partida |

**`main()`: 2 320 → 468 líneas**, y lo que queda se lee como la secuencia que
siempre fue. Arriba del fichero va ahora un **mapa** de dónde está cada cosa,
que es lo que faltaba para poder trabajar en 8 200 líneas sin leerlas.

### Y BUG-14, cerrado de raíz

`vec_shootx` llenaba la estructura menos `xitol`, y por eso `rolling_eval` pudo
olvidarlo. Parchear ese sitio arregló el síntoma y dejó el agujero: mientras el
campo no lo llene nadie, el siguiente sitio lo olvidará también. Ahora lo pone
`vec_shootx`. No mueve ningún número —todos los sitios lo fijan con la misma
expresión y lo fijan **después** de la llamada—, y eso no es un argumento sino
una medida: los 24 informes dorados no se movieron.

Esperó a P8 a propósito. Un cambio sin efecto medible va con los demás cambios
sin efecto medible, donde una sola red los cubre a todos.

### Lo que NO se hizo, y por qué

**Las 36 globales siguen ahí**, agrupadas y documentadas pero no encapsuladas en
una estructura. Son tres cosas: las banderas de opción (lo que el usuario pidió),
los datos (`rawmat`, `datamat`, `Y2_levels` y el calendario) y los tres nombres
de fichero. Cada una la leen varias de las funciones de arriba y la escribe
exactamente una. Pasarlas por seis firmas diría menos que declararlas una vez, y
sería un diff grande cuyo único efecto es un diff grande. Queda dicho en el mapa
del fichero, que es donde un lector lo necesita.
