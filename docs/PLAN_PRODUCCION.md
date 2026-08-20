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
| P1.5 | `--version` imprime una versión y sale 0 | **✔** `drvec 1.0.0-rc1`; la versión definitiva la pone P6 |
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

### Qué se construye (P2)

1. `drvec` imprime `|Σ̂|` (y `log|Σ̂|`) en el `.out`, junto a `Σ`.
2. Se rehacen las cuatro filas con el comando exacto escrito al lado de cada
   una, sobre ficheros que estén en el repositorio; si una configuración exige
   una entrada que ya no existe, se retira de la tabla y se dice.
3. Bloque `SLOW=1` en la batería con el `|Σ̂|` de las configuraciones que queden,
   como valor dorado.
4. Excepciones en `.gitignore` para `benchmark/*.results.txt` y `data/*.out`.

**Criterio de salida de P2:** cada cifra de `HOMOLOGATION.md` tiene al lado el
comando que la produce, ese comando corre sobre el repositorio limpio, y da la
cifra. Comprobado ejecutándolos todos.

---

## 4. La suite: motor, cast y registro de defectos

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

1. **Alinear `matrix`/`imatrix`/`free_matrix`/`free_imatrix`** con la copia
   compartida, cerrar las dos fugas que eso destapa
   (`cond_resid`, `rawmat`), y dejar `nlatools.c` con **cero** líneas de código
   divergentes. Actualizar la tabla de `SUITE_INTEGRATION.md` §5 con el motivo
   medido.
2. **Devolver a la suite** los tres arreglos del lector y el desasignador:
   parche para `drtran/src/fue_pre_reader.c` y entradas en
   `drtran-python/docs/BUGS.md` cerrando BUG-11 y BUG-12 con la referencia al
   commit de `drvec`.
3. **BUG-13**: proponerlo al registro como defecto de `drvarma` con la
   reproducción que `drvec` ya tiene en su batería (el caso sintético de tres
   series con `Q(126) = 110`), y dejar en `drvec` un comentario en `chisq()` que
   diga que está defectuosa y que el programa no la usa.
4. **Medir la identidad de cruce**: establecer si 1.8e−05 es el redondeo del
   formato —escribiendo el mismo `.pre` con más decimales y viendo si baja— o es
   pérdida de precisión de `drvec`. El resultado, sea el que sea, va al registro.
5. **Una nota de arquitectura** que diga qué comparte `drvec` con quién y por
   qué diverge donde diverge, con las cifras de §4.1 y §4.2. Es lo que hoy no
   existe y lo que hace que la deriva se descubra midiendo en vez de leyendo.

**Criterio de salida de P3:**

| # | criterio |
|---|---|
| P3.1 | `diff` de `elfvarma.c`, `drvmlest.c`, `qnewtopt.c` y `nlatools.c` contra `drvarma_v.04.1`: **cero líneas de código**; sólo cabeceras |
| P3.2 | un test de la batería que ejecuta ese `diff` y falla si aparece código divergente |
| P3.3 | `VALGRIND=1 make test` verde **con el layout compartido** |
| P3.4 | BUG-11 y BUG-12 cerrados en el registro de la suite, con parche aplicado a `drtran` |
| P3.5 | la identidad de cruce, medida y explicada |

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

| # | criterio |
|---|---|
| P5.1 | `drvec` prevé en niveles con bandas, y la previsión a un paso coincide con el residuo del ajuste |
| P5.2 | evaluación de origen móvil implementada y comprobada contra un caso cuyo resultado se puede calcular a mano |
| P5.3 | **la tabla VEC contra univariante existe y está publicada, gane o pierda** |
| P5.4 | si pierde en todo el banco, el registro lo dice en la primera página y el programa se describe como lo que entonces es: un estimador de máxima verosimilitud exacta para una clase de modelos, no una herramienta de previsión |

P5.4 no es una cláusula defensiva. Es el punto: la única forma de que este
proyecto sepa lo que tiene es medirlo, y hasta hoy nadie lo ha medido.

---

## 7. Empaquetado y cita

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

### Qué se construye (P6)

1. `LICENSE` (GPL-2.0-or-later, que es lo que el README ya declara) y
   `CITATION.cff` con Mauricio (2006) como referencia del método.
2. Versión en un solo sitio, embebida en el binario, impresa por `--version` y
   en la cabecera del `.out`. Etiqueta `v1.0.0`.
3. `CHANGELOG.md` con el formato de `drtran-python`: lo publicado, y los
   informes completos en el registro.
4. `docs/BUGS.md` propio, o entradas en el del conjunto — a decidir, pero **uno
   de los dos**; hoy los defectos de `drvec` viven repartidos entre el registro
   de desarrollo y comentarios en el código.
5. `make install` con `PREFIX`, y CI que corra `make test` y `VALGRIND=1 make
   test`.
6. Alinear el estado declarado: `docs/README.md` deja de decir «Pre-beta».

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
- **El refactor de `main()`.** 2066 líneas y 36 globales; `drvec.c` pasó de 818
  líneas el 2026-08-16 a 6086 el 2026-08-20. No bloquea el lanzamiento y
  encarece todo lo que venga después, así que va detrás de la etiqueta, no
  delante.
- **La unificación del idioma de la salida.** Hay ~32 mensajes al usuario en
  castellano en un programa con documentación inglesa, y `docs/README.md` cita
  la salida ya traducida («1 df» donde el programa escribe «1 g.l.»). Se arregla
  en P7, después de la etiqueta.

---

## 9. Orden y por qué

**P1 (hecha) → P4 (hecha) → P5 → P2 → P3 → P6 → (P7 idioma) → (P8 refactor).**

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
teniendo que ir antes de la etiqueta, porque no tiene sentido publicar `v1.0.0`
con el motor divergiendo del canónico, con dos defectos del registro resueltos
aquí y abiertos allí, y con dos fugas que la alineación de `nlatools` destapa.

**P6 cierra.** P7 y P8 no cambian ningún número y pueden ir en `v1.1`.

**Y dos órdenes que no seguiría.** Empezar por P6: empaquetar es lo visible y
produciría una versión cuya especificación por defecto no se sostiene. Y dejar
P5 para el final, que es lo que este plan hacía sin darse cuenta al no incluirlo:
un programa que nunca mide si sirve para lo que existe puede pasar todas sus
pruebas y no servir para nada.
