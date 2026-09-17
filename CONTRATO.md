# El contrato de ficheros, tal como ES

Entrega de la **fase 1** del estudio de atsw. No cuenta lo que el contrato
promete: cuenta lo que las implementaciones hacen. Donde las dos cosas no
coinciden, manda esto.

Método: seis implementaciones leídas campo a campo, y una **batería de
conformidad** que pasa un corpus de 120 ficheros por cada escritor y compara
el MODELO que sale con el que entró — no los bytes, porque dos
implementaciones del mismo formato no van a escribir los mismos bytes nunca y
eso no importa. Lo que importa es que quien lo lea después vea lo mismo.

    atws/conformidad/bateria.sh      los ESCRITORES: ¿conservan el modelo?
    atws/conformidad/acuerdo.sh      los LECTORES: ¿entienden lo mismo?
    atws/conformidad/copias.sh       ¿sigue inpcheck del GUI siendo la del motor?
    atws/conformidad/corpus/         los 120 ficheros, y HUECOS.md dice por qué
    atws/conformidad/comparar.py     el juez: fue.load() de los dos y a comparar

Son tres preguntas distintas y hacen falta las tres. `bateria.sh` no puede ver
un fallo de lectura cuando el escritor es fiel; `acuerdo.sh` no puede ver un
escritor que pierde un campo; y `copias.sh` mira lo que ninguna de las dos
mira — que la copia de `inpcheck.c` del GUI siga siendo la del motor, que es
lo único que garantiza que el GUI acepte exactamente lo que el motor acepta.
Ya se desincronizó una vez, en este mismo estudio.

Los **cuatro actores** son las cuatro implementaciones que ESCRIBEN. Las otras
dos no son actores: el lector de Python es el **juez** —es quien lee los dos
ficheros y dice si son el mismo modelo— e `inpcheck.c` es la **puerta** que los
actores en C usan para rechazar lo que no es de su dialecto.

Estado medido hoy, sobre 120 ficheros:

    gtkfue  el GUI en C            101 pasan,  0 fallan, 18 apartados
    pyart   art/_write_inp          49 pasan, 66 fallan,  5 apartados
    pypre   fue/report.write_pre    96 pasan, 19 fallan,  5 apartados
    motor   el propio fue en C      89 pasan, 14 fallan, 17 apartados

Los apartados no son fallos: son ficheros que ese actor rechaza en la puerta
por no ser de su dialecto —12 de fuf y 5 de fug— más, en el caso del GUI, uno
que no le cabe. Que los actores en C aparten 17 o 18 y los de Python 5 **es en
sí una divergencia**: el lector de Python se traga un fichero de fuf y le quita
el horizonte al reescribirlo.

Y las tres cifras de fallo tienen causas distintas, que conviene no mezclar:

| actor | de qué son sus fallos |
|---|---|
| `pyart` | las columnas de los regresores externos, que no escribe (BUG-0187), y los datos truncados a `%.6f` (BUG-0188). Son dos defectos, repartidos por 66 ficheros |
| `pypre` | el horizonte y σ² de los 12 ficheros de fuf, más los parámetros FIJOS cuantizados (§1.4) y los δ pegados (BUG-0020) |
| `motor` | las columnas de los regresores a 6 decimales, los fijos cuantizados, λ a 2 decimales, y el μ fijo que tira |

`gtkfue` y `pyart` **copian**, así que se les exige el fichero entero, valores
incluidos. `pypre` y `motor` **estiman**, así que se les compara en modo
estructura: todo menos los valores que el fichero declaraba LIBRES. Los FIJOS
se siguen comparando — y ahí está el hallazgo de §1.4.

---

## 0. Lo primero: no son cinco implementaciones, son seis

El estudio lateral contó cinco. Falta la que más manda:

| | implementación | dónde |
|---|---|---|
| **E** | el motor en C: lee el `.inp` **y escribe el `.pre`** | `fue-1.14/src/fue.c` (lee 301-919, escribe 1724-2341) |
| **V** | el validador en C | `fue-1.14/src/inpcheck.c`, `fuf-1.09/src/inpcheck.c` |
| **G** | el GUI en C: lee y escribe | `gtk_fue.09/src/file_io.c` |
| **R1** | el lector único de Python | `fue/inp.py::load()` |
| **W1** | el escritor `.pre` de Python | `fue/report.py::write_pre()` |
| **W2** | el escritor de art | `art/pipeline.py::_write_inp()` |

Importa porque **W1 es copia fiel de E, defectos incluidos**. Sin contar a E,
la mitad de las divergencias de Python parecen invenciones del puerto cuando
son fidelidad al original.

Un GUI nuevo sería el séptimo.

---

## 1. Lo que el contrato promete, y dónde se rompe

> `.inp` es la especificación y sus valores son SEMILLAS; `.out` es el
> registro de la estimación con sus errores típicos; `.pre` es el mismo
> `.inp` con las estimaciones como valores iniciales — un óptimo
> reejecutable. **Invariante: corre fue sobre un `.pre` y los números no se
> mueven.**

### 1.1 La grieta conocida: el invariante es sobre los VALORES, no la curvatura

Arrancando en el óptimo, BFGS para en `niter=0` y devuelve la semilla `2/n`
como covarianza (BUG-0027/0061). Por eso los errores típicos sólo son fiables
en el `.out`. Está documentado y hay tests que lo fijan.

### 1.2 La grieta nueva: el invariante es sobre los valores **a la precisión con que se escriben**

`.pre` cuantiza. Cada campo a la suya:

| campo | E escribe | consecuencia |
|---|---|---|
| coeficientes ARMA | `%.4f` | un θ de 0.99996 vuelve como `1.0000`, en la circunferencia unidad |
| δ de los deterministas | `%.4f` | ídem |
| ω | `%.6f` | |
| λ de Box-Cox | `%4.2f` | λ = 1/3 vuelve como `0.33` |
| μ | `%.6f` | medido: −88.263540 → −88.263548 al reejecutar |
| datos | `%.10f` | |
| columnas de deterministas no estándar | `%lf` = 6 decimales | el regresor externo se redondea **en cada estimación** |

O sea: **el invariante vale para las estimaciones, no para las semillas.** La
resiembra está cuantizada en 1e-4. Reejecutar un `.pre` no devuelve
exactamente el mismo punto; devuelve el punto redondeado a cuatro decimales,
y desde ahí el optimizador vuelve a andar un poco.

### 1.3 La rotura: con dos o más δ, el motor escribe un `.pre` que no puede releer

**Encontrado y arreglado en este estudio.** `fue.c:1975-1983` emitía el salto
de línea *fuera* del bucle de los δ, así que los pares salían pegados:

    0 0 0 0 0 0 0 0 0 0 0 2
    **
    1.8400  1-0.8631  1

Medido sobre ese fichero, antes del arreglo:

    fue sobre el .inp            → 0, escribe el .pre de arriba
    fue sobre su propio .pre     → 2  «the flag ... must be 0 or 1»
    fue.load() sobre ese .pre    → ValueError: invalid literal for int(): '1-0.8631'

El `.pre` dejaba de ser reejecutable, que es el invariante entero. Sobrevivió
porque **ningún fichero de los 115 de `fue-1.14/tests/` ni de los 108 del
corpus tenía un determinista con dos δ** — el caso no estaba ejercitado en
ningún sitio.

Arreglado moviendo el `\n` dentro del bucle, como ya lo hacía la sección de ω
doce líneas más arriba. Verificado: las tres pruebas de arriba dan 0, 0 y
`[1.84, -0.8631]`; el banco de fue-1.14 sigue en 109 corridas y 0 fallos.
Añadido al corpus como `RIPC.3.delta2.inp`.

W1 (`report.py:1164-1170`) reproduce el defecto al byte, porque copia a E.
**Sigue abierto en Python.**

### 1.4 Y la consecuencia que no se había visto: un parámetro FIJO no es una semilla

Ésta la destapó el actor `motor`, que no existía hasta ahora, y es la razón
de peso para arreglar la cuantización.

Para un parámetro **libre**, escribir `%.4f` es defendible: el valor es una
semilla y volver a arrancar desde el valor redondeado casi siempre lleva al
mismo óptimo. Para un parámetro **fijo no lo es en absoluto.** Un parámetro
fijo no es una estimación: es parte de la **especificación**. El analista lo
pinchó en un valor y el modelo es ése. Redondearlo no re-siembra nada —
**cambia el modelo**.

Medido, sobre ficheros reales del corpus:

    R.1_4.inp      AR(1) FIJO en 0.941176  ->  0.9412        (16/17)
    R.2_2.inp      AR(1) FIJO en 0.941176  ->  0.9412
    RIPC.3.1.inp   AR(1) FIJO en 0.944444  ->  0.9444        (17/18)

Y con los demás campos que son especificación y no semilla:

    lambda 1/3 = 0.333333   ->  0.33      (`%4.2f`)
    cos 1.5                 ->  cos 2     (el motor redondea; G truncaba a 1)
    operador f-fija k = 3.5 ->  k = 4     (el motor redondea; G truncaba a 3)
    mu FIJO en -88.717979   ->  0         (se tira entero, §2)

Le pasa **al motor y a `write_pre` por igual**, porque el segundo es copia del
primero. El GUI ya no: usa `inp_format()`, el mínimo de decimales que relee
idéntico.

**De aquí sale la regla nueva del contrato**, en §6.

---

## 2. Qué se pierde en un ida y vuelta

Verificado en ejecución, no supuesto.

| qué | E | G | W1 | W2 |
|---|:-:|:-:|:-:|:-:|
| nombre del regresor externo | ✓ | ✓ | ✗ `non-standard` | ✗ `custom` |
| columnas de datos del regresor | ✓ 6 dec. | ✓ exacta | ✓ exacta | **✗ no las escribe** |
| μ FIJO no nulo | ✗ | ✓ *(arreglado)* | ✗ | ✗ |
| λ de Box-Cox | ✗ 2 dec. | ✓ | ✗ | ✗ |
| datos de la serie | ✓ 10 dec. | ✓ exacta | ✓ 10 dec. | ✗ 6 dec. |
| `cbands` | ✓ | ✗ | ✗ | ✗ |
| frecuencia `number` (serie sin fechar) | ✓ | ✗ | ✗ | ✗ |
| armónicos y frecuencias fijas no enteras | ✗ redondea | ✓ *(arreglado)* | ✗ redondea | ✗ |
| comentarios de cabecera y secciones comentadas | ✗ | ✗ | ✗ | ✗ |
| horizonte y σ² (dialecto de fuf) | — | ✗ (no carga) | ✗ | ✗ |

**Ninguno de los cuatro ida y vuelta es la identidad.** El μ fijo lo destruían
los cuatro escritores con la misma forma

    if (media libre) escribe "valor 1";  else escribe "0";

y tiran el valor, no sólo la bandera. Los dos lectores, en cambio, **sí**
aceptan un μ fijo no nulo: el formato puede expresarlo y era sólo el escritor
el que no lo escribía. **G ya lo escribe**; los otros tres, no.

### 2.1 Lo que convierte una pérdida en corrupción

Dos parejas, cada una inofensiva por separado:

- **W2 no escribe las columnas** + **R1 rellena con ceros la columna que
  falta** (`inp.py:502`) = un regresor externo de 68 observaciones vuelve
  idénticamente nulo, con su ω estimable, sin un solo aviso. Medido sobre
  `R.2_5.inp`. Reportado: art **BUG-0187**, fue **BUG-0017**.
- **E redondea las columnas a 6 decimales** + se reestima en cada vuelta = el
  regresor se degrada un poco en cada paso del ciclo.

Fuera de Python el mismo fichero mutilado es peor todavía: `fue` 1.14 lo
rechaza limpiamente, pero `fue` 1.13 lo lee en silencio inventándose
observaciones.

---

## 3. Los tres dialectos que comparten extensión

`.inp` es tres formatos distintos. **La línea que los distingue es la que va
justo detrás de la línea de fecha**, y `inpcheck.c` no mira la etiqueta sino
**cuántos números hay y si son enteros** (`count_numbers`, `inpcheck.c:126-143`):

| dialecto | esa línea contiene | prueba |
|---|---|---|
| **fue** | nº de deterministas | 1 número, entero |
| **fuf** | horizonte + σ² | 2 números, el segundo no entero |
| **fug** | λ [m [geom]] d D | 3 a 5 números, no todos enteros |

Tiene que ser numérica y no de etiqueta porque `gtk_fmg` escribe la línea de
Box-Cox **sin el prefijo `**`**, así que buscar la palabra «Box-Cox» falla en
los ficheros de fug reales.

**Quién los confunde:**

- **G ya no**: pasa por `inp_ok_to_load` → `inpcheck` antes de cargar. Antes
  reventaba el montón (`malloc(): corrupted top size`).
- **R1 confunde fue con fuf por diseño**: `load()` lee los dos y cuelga el
  horizonte y σ² en `_fuf_horizon`/`_fuf_sigma2`. No es un fallo —es
  deliberado— pero significa que **W1 y W2 tiran esos dos campos al
  reescribir**, y como son atributos privados, mi propio comparador estaba
  ciego a la pérdida hasta que lo arreglé.
- **R1 con fug: ni rama ni diagnóstico.** Revienta con
  `ValueError: invalid literal for int(): '77.9667'` — y ese número es **un
  dato de la serie**, o sea que el parser ya había derivado varias secciones
  antes de morir. El mensaje apunta al sitio equivocado.
- **Los lectores desnudos** (`fue.c` sin `inpcheck`, `fue` 1.13) los confunden
  en silencio. Es lo que motivó `inpcheck.c`.

**Y medido**, que es lo que faltaba. `conformidad/acuerdo.sh` pregunta a los
dos lectores, fichero a fichero, si lo aceptan y de qué dialecto. Sobre los 120
del corpus:

    118 de acuerdo
      1 lo rechaza sólo el C     (hueco_11nonstd: una diferencia de CAPACIDAD)
      0 lo rechaza sólo Python
      0 de dialecto distinto
      1 leído distinto           (hueco_nombre_num)

O sea: **la clasificación de dialectos no está rota.** Python sabe cuál es
cuál — cuelga el horizonte en `_fuf_horizon` — y coincide con `inpcheck.c` en
los 120. Lo que está mal es más estrecho de lo que parecía: (a) Python **sabe**
el dialecto y no lo **impone**, y sus escritores tiran los campos de fuf; y (b)
el mensaje con el que rechaza un fichero de fug es inútil.

Los dos desacuerdos son de otra clase, y valen los dos:

- **Capacidad.** `hueco_11nonstd.inp` lo rechaza el C (11 regresores externos,
  y el motor tiene sitio para 10) y Python lo acepta — legítimamente, porque su
  puerto sólo enlaza los núcleos numéricos, no el `fue.c` que tiene el vector.
  Es una asimetría real de la escalera: **un modelo que art puede ajustar y el
  motor en C no**.
- **Lectura.** `hueco_nombre_num.inp` —una serie anual llamada `2020`— lo
  aceptan los dos y **leen cosas distintas**: el C empieza en 1766 y Python en
  2020, y Python pierde además el nombre. 254 años de diferencia en el origen
  contra el que se fechan todas las intervenciones (fue BUG-0022).

Ese segundo es el que justifica que `acuerdo.sh` exista aparte de la batería.
**La batería no puede verlo por construcción**: el escritor en C reescribe el
fichero byte a byte igual, así que el juez —que es uno de los dos lectores—
compara su propia lectura equivocada consigo misma y dice «iguales». Cuando el
escritor es fiel, un fichero mal leído se reescribe igual de mal.

**Propuesta:** dejar la extensión como está —el ecosistema entero la usa— y
adoptar la prueba de `inpcheck.c` como la definición normativa del dialecto.
No hay que portarla entera: Python ya clasifica bien. Lo que hay que hacer es
**que Python la use para diagnosticar**, de modo que un fichero de fug diga
«esto es de fug» en vez de morir sobre un dato de la serie. Marcar el dialecto
dentro del fichero rompería 100+ ficheros existentes para resolver un problema
que ya está resuelto.

---

## 4. Las reglas que no están escritas en ninguna parte

Inventario largo aparte (52 reglas). Las que cambian una decisión de diseño:

**Estructurales — el formato es POSICIONAL aunque no lo parezca.**
`FILE_FORMATS.md` dice «las líneas `**` son etiquetas para humanos; lo que
importa es el orden». Cierto para R1, **falso para el C**: el escritor del
`.pre` reabre el `.inp` y se posiciona contando **once** `fgets` a pelo
(`fue.c:1734-1746`). Una línea en blanco de más en la cabecera desincroniza
los dos lectores y el `.pre` sale con nombres de determinista basura, sin
error. La regla real: **exactamente una línea de etiqueta y una de valores por
sección**, y la cabecera son **cinco** líneas.

**`fue.c` hace 115 llamadas a `fscanf` y no comprueba el retorno ni una vez.**
Un campo que falta no da error: deja la variable con su valor anterior y el
token en el flujo. `inpcheck.c` existe para atrapar eso *antes* de escribir
nada — es un prepaso que repite las mismas llamadas en el mismo orden. Es la
pieza más valiosa del C y la mejor candidata a ser la definición del formato.

**Topes que `inpcheck` no comprueba:**
- `NT = 10` deterministas NO estándar (`fue.c:84`), sin cota al llenar el
  vector; `inpcheck` admite 1000. Un fichero de 11 pasa y el motor escribe
  fuera del bloque.
- G tiene arrays estáticos consecutivos (`It[50]`, `Arr[20]`, `Data[2000]`) y
  `inpcheck` admite `nobs` hasta 10 000 000. Un `.inp` de 2001 observaciones
  pasa la puerta y escribe encima de punteros que luego se liberan.

**Semánticas que cambian el modelo sin decirlo:**
- Un regresor externo **llamado como una palabra reservada** (`step`, `trend`…)
  deja de ser columna y se genera: el bloque de datos entero se corre.
- `easter` sólo existe si `freq == 12`; en trimestral cae a «no estándar» y
  exige una columna que no está.
- La frecuencia debe ser 1, 4 o 12: con otra, los `ifadf` se leen, se guardan,
  se reescriben **y se ignoran** al construir el operador.
- `fue` trunca la respuesta impulso del determinista racional en 40 retardos y
  `fuf` en 20: **el mismo modelo da un efecto determinista distinto en
  estimación y en predicción.**
- Una intervención fechada **fuera de muestra** da un regresor idénticamente
  nulo con coeficiente libre. Hessiano singular, errores típicos de millones,
  cero diagnóstico.
- En `fuf`, las columnas de un regresor externo valen **cero sobre todo el
  horizonte**: el formato no tiene sitio donde poner su futuro.

**Y el `refactor = 100`, con su razón mecánica:** `fue` escribe σ² con `%.10f`
(`fue.c:1771`). Con datos en logaritmos sin reescalar σ² vive en 1e-6…1e-10, y
por debajo de 5e-11 se escribe `0.0000000000` — `inpcheck_fuf` lo acepta
(sólo prohíbe negativos) y `fuf` produce **bandas de anchura cero**.
Multiplicar la serie por 100 multiplica σ² por 10 000 y lo saca del agujero.
El otro lado está medido en `drtran-python/pre.py:110-134`: con `refactor=1`
el optimizador no converge en 2 minutos; con 100 converge en 23 iteraciones y
un segundo, porque el paso de diferencias finitas es ~6e-6 absoluto.

**El `.pre` no registra si el optimizador convergió.** El motivo de parada va
al `.out`. Y con semillas no estacionarias `fue.c:1191-1204` **no estima**,
escribe `.out` y `.pre` con las semillas verbatim y sale con 3 — un `.pre`
estructuralmente indistinguible de uno bueno. La regla «los errores típicos
sólo del `.out`» es más fuerte de lo que parecía: **sin el `.out` no se sabe
siquiera si el `.pre` es un óptimo.**

**El `.pre` no reproduce la ejecución.** `geom`, `eml`/`aml` y `chk`/`nochk`
son opciones de línea de órdenes, no campos del fichero; `m` de Box-Cox está
cableado a 0.0 en fue. Dos `.pre` idénticos pueden venir de dos estimaciones
distintas.

---

## 5. Qué es estructura y qué es semilla

Hoy el fichero los mezcla: `0.8827  1` es un valor y una bandera, y el valor
es semilla o estimación según de qué fichero se trate — y **el fichero no lo
dice**. Ésa es la raíz de media docena de las cosas de arriba.

Pero el fichero **sí** dice una cosa, y es la que importa: **la bandera.**

    valor  1     libre     -> es una semilla, o una estimación
    valor  0     fijo      -> es ESPECIFICACIÓN

Un parámetro fijo no es un valor que el optimizador vaya a mover: es parte de
la declaración del modelo, tan estructura como el orden del operador que lo
contiene. Esa distinción está en el fichero desde el principio y **ningún
escritor la mira** (§1.4). Es lo primero que hay que arreglar, y es barato.

El modo estructura de la batería (`comparar.py --estructura`) es esa regla
hecha prueba: se salta los valores libres y compara los fijos. Los tres fallos
de `R.1_4`, `R.2_2` y `RIPC.3.1` salieron de ahí.

No propongo separarlos en el fichero. El formato tiene 30 años de ficheros
escritos y la extensión `.pre` ya lleva esa información. Lo que propongo es
**separarlos en la interfaz**: un editor que enseñe la estructura (órdenes,
tipos, fechas, banderas) en una zona y los valores en otra, y que al tocar un
valor de un `.pre` avise de que lo está degradando a `.inp`. Eso es fase 2, y
sale directamente de aquí.

---

## 6. Propuesta para P1 — ¿un solo dueño del formato, o seis?

**Recomendación: C, ejecutada de verdad — y por el camino, A.**

Es decir: **dos implementaciones atadas por un corpus de conformidad**, que es
lo que ya existe y ahora funciona; y como obra de fondo, extraer `inpcheck.c`
+ un lector/escritor en C a `libatswinp`, que los motores y los GUI usen, y a
la que Python pueda llamar por cffi cuando convenga.

Las razones, todas medidas en este estudio:

1. **B (Python manda) está descartado por los hechos.** El puerto es hoy la
   implementación **menos** fiel: W2 pierde las columnas, trunca los datos,
   pierde el nombre y pierde λ. No puede mandar lo que peor lo hace.
2. **A (C manda) es lo correcto y es caro.** Obliga a mover el puerto, que es
   lo que art-mcp usa todos los días. No se hace de una vez.
3. **C funciona ya, y encuentra lo que nadie había visto.** En dos tandas: ocho
   fallos del GUI (arreglados, 63 → 101), la rotura de los δ en el motor
   (arreglada), la truncación de art, la pérdida de las columnas, la ceguera
   del propio comparador, la pérdida del horizonte de fuf, la frontera del
   −0.0 y la cuantización de los parámetros fijos. Ninguno se había visto en
   años de uso.
4. **El corpus tiene huecos, y ése es el trabajo.** No había ni un fichero con
   dos δ en 223 ficheros de prueba entre los dos bancos, ni uno con la media
   fija, ni uno con un armónico no entero. Cada hueco que se ha llenado ha
   sacado un fallo. **El valor de la batería está en llenar los huecos, no en
   tenerla.**
5. **Y los actores que ESTIMAN encuentran otra clase de cosas.** `motor` y
   `pypre` no comparan un fichero con su copia: comparan un modelo con su
   óptimo. Eso es lo que destapó §1.4, que es la corrección más importante de
   todo el estudio y que ningún actor de copia podía ver.

**Y la regla nueva para el contrato.** La primera versión de este documento la
escribió así, y **estaba mal**:

> ~~Los parámetros pueden cuantizarse —son semillas— pero los datos son la
> observación.~~

Los actores que estiman la desmintieron. La regla correcta separa por la
**bandera**, no por el tipo de campo:

> Un escritor del formato escribe con la **representación más corta que relee
> idéntica** —nunca con un número fijo de decimales— todo lo que es
> **especificación**: los datos y sus columnas, λ, los armónicos, las
> frecuencias de los operadores de frecuencia fija, y **todo parámetro cuya
> bandera diga FIJO**. Sólo los valores marcados como LIBRES pueden
> cuantizarse, porque son semillas y el optimizador los va a mover de todas
> formas.

G la cumple entera desde `575a4bc`, y por eso pasa la batería con 0 fallos. E,
W1 y W2 no la cumplen en ninguno de los dos términos.

---

## 7. Lo reportado por el camino

| registro | id | qué |
|---|---|---|
| art-python | **BUG-0187** | `_write_inp` no escribe las columnas de los deterministas no estándar |
| art-python | **BUG-0188** | `_write_inp` escribe los datos con `%.6f` y los trunca |
| art-python | **BUG-0189** | art lee la media por `getattr(model, "mu")` y el atributo es `mu0`: el guion registra `mu: 0.0` siempre |
| fue (Python) | **BUG-0017** | `load()` inventa una columna de ceros cuando falta la de un regresor externo |
| fue (Python) | **BUG-0018** | `load()` descarta `cbands` y la frecuencia `number` — y con ello ciega a la batería en esos dos campos |
| fue (Python) | **BUG-0019** | el puerto rechaza un `.pre` del motor: φ₂ = `-0.0` falla `< 0`, que en C es `> 0.0` y pasa |
| fue (Python) | **BUG-0020** | `write_pre` pega los pares de δ sin separador — el mismo defecto que el motor, copiado |
| fue (Python) | **BUG-0021** | `write_pre` cuantiza también los parámetros FIJOS: un AR fijado en 0.941176 vuelve en 0.9412 |
| gtk_fue.09 | `a0d3509` | el GUI truncaba los datos al guardar |
| gtk_fue.09 | `ac2feb2` | los deterministas no sobrevivían al ida y vuelta (orden δ, bandera, columnas, nombre) |
| gtk_fue.09 | `f66c88a` | la media fija, el armónico, y la fecha que `trend` no tiene |
| gtk_fue.09 | `575a4bc` | la frecuencia de un operador de frecuencia fija también es real |
| fue-1.14 | `0d09abb` | los δ salían pegados y el `.pre` no lo relee ni el propio motor |

Pendientes de reportar, del inventario de 52 reglas: `NT = 10` sin cota; los
arrays estáticos de G frente a los topes de `inpcheck`; los 40 retardos de fue
contra los 20 de fuf; y la cuantización del `.pre` **en el motor**, que es el
mismo BUG-0021 del otro lado y no tiene registro donde ponerlo.

---

## 8. Estado de la fase 1

**Cerrada.** Lo que pedía el plan —el contrato tal como es, la lista de
divergencias, una propuesta para P1 y una batería de conformidad— está. Y trajo
tres cosas que el plan no pedía porque no sabía que existían: la corrección de
§1.4 (fijo ≠ semilla), el banco de acuerdo entre lectores, y la vigilancia de
las copias.

Lo que **no** se ha hecho, a propósito:

- **No se ha tocado el puerto de Python.** Los nueve defectos encontrados en él
  están reportados con repro y con el arreglo propuesto, y ahí se quedan hasta
  que se decida. Arreglarlos es trabajo de una tarde con la batería delante:
  cada informe dice qué ficheros del corpus tienen que pasar después.
- **No se ha unificado nada.** La propuesta de §6 es C ahora y A como obra de
  fondo; A mueve el puerto, y eso es una decisión, no una tarea.

Lo que queda pendiente y es pequeño:

- **Los huecos que faltan**, al final de `conformidad/corpus/HUECOS.md`: un
  `.inp` con una sección comentada (que va al banco de acuerdo, no al de
  conformidad, porque el motor lo rechaza), y μ fijo en cero frente a μ fijo no
  nulo — para ver si al arreglar los escritores distinguen «no hay media» de
  «la media es cero».
- **Que Python diagnostique el dialecto** en vez de morir sobre un dato de la
  serie. No hay que portar la prueba entera: ya clasifica bien (§3), sólo tiene
  que decirlo.
- **Añadir `copias.sh` y `acuerdo.sh` a lo que se corre antes de un commit** en
  gtk_fue y en los motores. Hoy hay que acordarse.
