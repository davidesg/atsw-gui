# La escalera, como dato

Entrega de la **fase 3** del estudio de atsw. La pregunta era: *¿qué hace falta
para que subir de univariante a transferencia sea una operación con estado y no
una costumbre?*

La respuesta corta: **falta muy poco para el modelo y falta casi todo para la
sesión** — y, antes que cualquiera de las dos, falta que las puertas que ya
existen cierren, porque hoy la mayoría son párrafos y no cerrojos.

Lo que sigue está medido. Donde dice una cifra, sale de correr el programa.

---

## 0. Lo que está bien, y conviene decirlo primero

El estudio lleva dos fases encontrando grietas. Estas dos no lo son.

**El convenio de signo es el de Box-Jenkins, y es el mismo en los tres
peldaños.** `ν(B) = ω(B)/δ(B)·Bᵇ` con `ω(B) = ω₀ − ω₁B − … − ω_sB^s`: el ω
principal suma y el resto restan. `fue::calcnu` (`fue.c:3369`) hace
`nu[0] = ω₀` y `nu[j] = Σδᵢ·nu[j−i] − ω[j]`; `drtran/src/nlatools.c:728` es la
misma función; `cast.py:87` del puerto hace `omega[0] if lag == 0 else
-omega[lag]`. Verificado sobre la corrida real del m6: la ganancia de
`EP <- EC` sale `0.360712 − (−0.529417) − 0.158782 = 0.731347`, que es lo que
imprime drtran; con el convenio ingenuo saldría −0.0099. **drtran es producto
directo de esa tradición y la respeta.**

**La puerta diagonal se cumple.** Sobre `ES_CPI_m10` + `WTI_ar1`:

    fue por separado:  -7.3917271423 + -760.0326138653 = -767.4243410076
    drtran -0:                                           -767.424341

La diferencia, 7,6e−9, es la precisión con que drtran imprime. La identidad
`logL(conjunta diagonal) = Σ logL(univariantes)` no es una aspiración: se
verifica.

Todo lo demás de este documento es lo que hay alrededor de esos dos aciertos.

---

## 1. Las cuatro puertas: qué afirman y por dónde se pasa

| puerta | qué afirma | dónde vive | ¿cierra? |
|---|---|---|---|
| **diagonal** | el cruce fue→drtran no altera el trozo univariante | `mcp_server.py:588`, sólo en Python | **no**: imprime 🛑 y `load_pre` sigue |
| **ciclo** | la red es un DAG, casteable como VARMA triangular | `topo_sort` (C, `drtran.c:2765`) y `find_cycle` (Python, `network.py:49`) | **en C sí; en Python sólo en las puertas, no en el motor** |
| **óptimo vs especificación** | lo que traes es un óptimo, no una semilla | `_certificado`, `mcp_server.py:409` | **no**: es informativo, y declara legítimas las dos |
| **oráculo TASTE** | la transferencia coincide con el aval externo | `atws/Taste/oracle/battery.py` | **no está enganchado a nada** |

### 1.1 La diagonal certifica la base y nada de lo que se apoya en ella

Estima cada serie por separado, suma, y compara con el ajuste conjunto **de red
vacía** (`links=[]`). Eso certifica el cruce del trozo univariante. Todo lo que
se añade encima —`b, r, s`, el ν(B), la ganancia, la exogeneidad, el cast
empotrado frente al de resta— queda fuera por construcción.

Y hay una demostración ejecutada de que eso importa. En la misma corrida en que
la puerta imprime **✅ Coinciden** y «Todos eran óptimos univariantes», siete
pruebas de extremo a extremo están en rojo, y entre ellas:

    ganancia nu(1):  0.02692  ->  0.02937      (+9 %)

Como λ = 0, esa ganancia **es una elasticidad**: el traslado en porcentaje. Es
el número que alguien publicaría. La puerta está verde y el modelo ha cambiado.

Los siete fallos llevan ahí desde que art cambió de identificación —el fichero
de pruebas se actualizó por última vez el 2026-08-16— y nadie se ha enterado
porque **no hay CI que corra nada** en ninguno de los repositorios.

### 1.2 El certificado del `.pre` no puede afirmar lo que dice afirmar

`_certificado` compara los coeficientes antes y después de reestimar y declara
«ÓPTIMO» o «ESPECIFICACIÓN» con umbral 1e−3. Es la única pieza de la suite que
responde esa pregunta **por contenido y no por extensión**, y el umbral está
medido, no elegido: un `.pre` genuino se mueve ~3e−5, una especificación entre
14 y 47.

Pero se degrada justo en el caso peligroso: `load_pre` **reestima al entrar**,
así que un `.pre` fabricado se corrige antes de compararse con nada. Está
medido en el registro: la puerta imprimió ✅ con verosimilitudes idénticas sobre
ficheros que fallan el invariante del `.pre` por 13,1.

Todo lo demás decide por el **nombre del fichero**: `art/pipeline.py:193` se
niega a estimar si `path.endswith(".pre")`. Renombra un `.pre` a `.inp` y estima
desde el óptimo sin enterarse.

**Y el único test que guarda el invariante está en rojo por una razón
estructural.** `test_a_genuine_pre_is_a_fixed_point` falla porque el `.pre` se
movió `1.000000000001e-06` contra un umbral de `1e-06`. El `.pre` guarda seis
decimales (§1.2 de `CONTRATO.md`), así que el movimiento mínimo detectable **es**
1e−6: el guardián tiene una tolerancia igual al cuanto del fichero que vigila.
No puede pasar de forma robusta. Es la fase 1 reapareciendo un piso más arriba.

### 1.3 El oráculo recuerda, no mide

`battery.py` ejecuta **TASTE** y compara con los números del campo `esperado` de
cada JSON — números que produjo drtran o fue en una fecha pasada. TASTE es un
binario fijo con entradas fijas, así que la salida de la batería es
esencialmente constante: **un cambio en drtran hoy no la haría fallar**. Pasan
los 20 de 20, y eso no dice nada sobre drtran de hoy.

Importa porque, como la homologación diagonal sólo valida el trozo univariante,
**TASTE es el único aval externo de la función de transferencia** — justo lo que
se añade al subir el escalón. Ese aval está congelado en una constante.

---

## 2. Dos implementaciones de la red, y no están de acuerdo

Lo mismo que la fase 1 encontró con el `.inp`, un piso más arriba. Tres
divergencias, las tres verificadas ejecutando:

| | C | puerto |
|---|---|---|
| token numérico en el `.dag` | **posición** (`strtol` primero, `drtran.c:2630`) | **nombre** primero (`network.py:37`) |
| mayúsculas | `strcasecmp`: `wti` encuentra `WTI` | exacto |
| `b, r, s` negativos | los acepta y **estima** | `ValueError` |

La primera es la grave: el mismo `.dag` con la misma línea de órdenes produce
**dos redes distintas**, en silencio, según quién lo lea. Y `b = −1` —una
entrada que lidera desde el futuro— el binario lo ajusta y devuelve 0.

Hay además dos asimetrías de fondo:

- **El ciclo no está en el cast de Python.** `build_cast_spec` valida rango y
  autolazo pero no llama a `check_acyclic`; medido, ajusta un 2-ciclo y lo
  declara `CONVERGED`. En C `topo_sort` corre siempre. El cerrojo está en la
  puerta y no en el motor, que es donde tiene que estar.
- **La alineación de calendario no está en el C.** BUG-2 se dio por arreglado y
  el arreglo es sólo del puerto. Medido: `M6_EU.pre` con su fecha movida
  catorce años cruzada con `M6_EP.pre` da `Log-likelihood = -658.217748` frente
  a `-626.801209`, salida 0, sin avisos — y el informe imprime `Start: 3 1976`,
  la fecha de la salida, que es justo lo que tapa el desajuste.

---

## 3. El `.cns` no es un fichero: es media frase

Éste es el hallazgo central de la fase.

El `.dag` nombra las series **por nombre**. El `.cns` nombra **por posición**, y
por **dos posiciones distintas e independientes**:

1. el **orden de la línea de órdenes** → `q[i,j]`, `phi_i`, `theta_i`, `mu[i]`;
2. el **orden de las líneas del `.dag`** → `omega{j}`, `delta{j}`.

Ninguna de las dos está en ningún fichero. La primera vive en un **comentario**
del propio `.cns`:

    # ORDEN de series en la linea de comandos (para las posiciones q[i,j]):
    #   1=EP  2=EI  3=EU  4=EC  5=EA  6=P

Medido sobre el m6, permutando **dos series** de la línea de órdenes:

| | ℓ | q[3,2] | q[5,2] | q[5,4] |
|---|---|---|---|---|
| orden correcto | **−1697.613401** | 0.1326 * | −0.1085 | **−0.2020 \*** |
| permutado | **−1733.212791** | 0.1646 *** | 0.0159 | **+0.0117** |

Y moviendo **una línea de sitio** en el `.dag`, con `m6_net_prod.cns`:

| | ℓ | qué es `omega1` | a qué se liga |
|---|---|---|---|
| `.dag` original | **−1729.016382** | `EP <- EI` | `theta_2` = MA de **EI** ✓ |
| líneas reordenadas | **−1733.043403** | `EP <- EC` | `theta_2` = MA de **EI** ✗ |

La restricción dice «el numerador de esta transferencia se factoriza con la MA
de su propia entrada» —la forma m6 de Mauricio—. Reordenada, factoriza la
transferencia de **EC** con la media móvil de **EI**. Los dos salen con 0, con
idéntico recuento de parámetros, y **la red impresa es la misma**, porque la red
sí va por nombre.

**Y lo escribe el propio programa.** `drtran -g` escribe el `.dag` con nombres y
el `.cns` con números — y pone los nombres al lado, en un comentario:

    q[3,2] = free      # EI - EU : r(0) = +0.336

El programa tiene los nombres delante en el instante de escribir el fichero y
escribe el número. La información está; el formato no la sostiene.

Reportado como **BUG-17** en `drtran-python/docs/BUGS.md`. Es el mismo defecto
que BUG-2 —*emparejar por índice en vez de por identidad*— un piso más arriba:
allí el índice era la observación, aquí es la serie.

---

## 4. Dónde vive el estado de una sesión

| pieza | dónde vive hoy | ¿sobrevive? |
|---|---|---|
| los modelos univariantes | los `.pre` | **sí** — lo único plenamente persistido |
| qué series y **en qué orden** | la línea de órdenes; como prosa, la cabecera del `.out` | no |
| la red | `_LINKS` en memoria; el `.dag` sólo si se escribió a mano o con `-g` | no |
| las restricciones | el `.cns`, si existe; **la ruta no se guarda** | a medias |
| el certificado diagonal | `_DIAG` / `_DIAG_FIT` en memoria | no |
| enlaces probados **y descartados**, y por qué | en ningún sitio | no |
| el ajuste (x, ℓ, hessiano, e.t.) | `_FITS` en memoria | no |
| diagnosis, sobreajuste, calibración | texto de una llamada | no |
| el recorrido y sus razones | en ningún sitio | no |

Son seis diccionarios (`mcp_server.py:267-272`) y ahí está todo lo que mtram
recuerda. Lo demás —identificaciones, podas, diagnosis, calibración y sus
razones— se calcula, se imprime y se descarta en la misma llamada.

**Y la higiene de esos seis está rota.** `load_pre` invalida `_LINKS` y `_FITS`
y deja vivos `_TABLES`, `_DIAG` y `_DIAG_FIT`. `_DIAG` es la verosimilitud
diagonal, que alimenta el contraste «¿se gana la transferencia su sitio?»
(`mcp_server.py:1806`). Recargar el mismo nombre de caso con otras series lo
mide contra la base anterior. Lo delicado: el código **ya prevé que ese valor
falte**, y entonces avisa; que esté rancio produce un número con sus grados de
libertad y su p, indistinguible de uno bueno. Reportado como **BUG-18**.

**Y el canal está cortado del lado de mtram:** no lee un `.dag` en ningún sitio,
no escribe ninguno, y `set_network` exige índices numéricos. Ni el modelo vuelve
a entrar por donde salió.

---

## 5. Lo que propongo

### 5.1 Que las puertas cierren (antes que nada, y es barato)

La lección está escrita en el propio banco de pruebas de art
(`test_contrato_de_ficheros.py:28-32`):

> *«una propiedad que sólo se sostiene si todo el mundo se acuerda no es una
> propiedad del sistema: es una costumbre. Lo que la convierte en propiedad es
> que la operación prohibida falle ruidosamente.»*

No se ha aplicado aquí. Tres cambios pequeños:

1. **La diagonal marca el caso.** Que `_diagonal_gate` deje `certificado:
   False` en `_SPECS`, y que `estimate` / `set_network` / `build_model` se
   nieguen sobre un caso no certificado salvo `force=True` explícito. Hoy
   imprime 🛑 y la sesión continúa.
2. **`check_acyclic` al cast**, junto a `check_alignment`, que ya está ahí
   (`cast.py:512`). Una línea. Hoy la librería ajusta sistemas simultáneos.
3. **Invalidar los seis diccionarios a la vez** en `load_pre` (BUG-18).

### 5.2 Que el `.cns` nombre por nombre

Admitir nombres en los dos índices, reusando `series_index`, que es la función
que el `.dag` ya usa:

    q[EA,EI] = free
    omega_EP_EI[1] = omega_EP_EI[0] * theta_EI[B^1]

Aceptando también la forma numérica, los `.cns` existentes siguen valiendo. Y
cerrar en el mismo viaje la divergencia de `series_index`: que el C resuelva
por nombre primero, como el puerto.

### 5.3 El fichero de sesión: `.trn`

**No hace falta inventarlo: ya existe, escrito como prosa en la cabecera del
`.out`.** Modelo, la lista ordenada de `.pre` con su papel, la red, el fichero
de restricciones, la frecuencia, la muestra. Todo lo necesario para repetir la
corrida, dentro de un informe de 2098 líneas que ningún programa vuelve a leer.

Lo que propongo es escribir eso mismo **como dato**, y que drtran sepa leerlo:

    # atsw session 1
    output      M6_EP.pre   sha256:a1b2…
    input       M6_EI.pre   sha256:c3d4…
    input       M6_EU.pre   sha256:…
    network     m6_net.dag
    constraints m6_net.cns
    cast        embed
    window      1..64
    certified   diagonal  logL=-767.424341  files=optimum  2026-09-17

Con eso:

- **el orden deja de estar en un comentario**: está en el fichero, con roles;
- **los hashes hacen imposible** usar un certificado contra otros datos — un
  `.pre` reeditado (que por el convenio ya es un `.inp`) invalida la sesión en
  vez de contaminarla en silencio;
- **el certificado diagonal se persiste** en vez de vivir en un diccionario, que
  es la causa de BUG-18;
- y `drtran -i sesion.trn` reabre exactamente lo que se corrió.

Tres campos más de los que hay hoy: la lista con roles, el cast, y la ventana.
Como **fichero de modelo**, `.dag` + `.cns` + esto es el 100%.

### 5.4 Lo que eso todavía NO es: el recorrido

Como **fichero de sesión** sigue siendo el 20%, y por una razón que no se
arregla con campos: **es el resultado, no el camino**. Un `.dag` dice qué cuatro
enlaces quedaron; no dice que se probaron once, cuáles se podaron por
exogeneidad y cuál se cayó porque el retardo no era verosímil. El `.cns` dice
qué tres covarianzas se liberaron; no dice que liberarlas cuesta la
descomposición de la varianza y que se decidió a sabiendas.

Eso es exactamente lo que `guion.json` guarda en art —grafo de versiones, nodos
de decisión con razón y evidencia obligatorias, integridad por sha— y en drtran
**no existe**: ni `guion.py`, ni `record_version`, ni `compare_versions`. Las
~15 herramientas `guion_*` de art no tienen contraparte en las 22 de mtram.

Y la materia prima ya está: el `log` de `build_model` (`mcp_server.py:3024-3203`)
recorre los nodos N0/N2/N3/N4/N5/N6 diciendo qué decidió en cada uno. Se
concatena en una cadena de retorno y se tira.

**Recomendación:** no inventar un guion para drtran. Extender el de art, que
está probado y versionado, con nodos de transferencia — el `.trn` de §5.3 es
entonces la *materialización* de un nodo, igual que el `.pre` lo es de uno
univariante. Eso además prepara la fase 5: el proyecto no puede ser por serie si
la escalera cruza series.

### 5.5 El oráculo, que mida

Que `battery.py` ejecute **drtran de hoy** y lo compare con **TASTE de hoy**, en
vez de comparar TASTE con un literal fechado. Es un cambio pequeño y convierte
un registro histórico en el banco de regresión que dice ser — y es el único aval
externo que tiene la transferencia. De paso, que un `.pre` que falta marque el
caso como omitido en vez de matar la batería entera (`battery.py:74-78` hace
`sys.exit`).

---

## 6. El tercer peldaño no existe

*Update 2026-09-26: it exists now. drvarma 5.0 reads and validates the `.pre`
files, with a diagonal gate that stops the program when it fails
(`engines/drvarma/docs/DESIGN-v5-ladder.md`). A cycle in drtran is the end of
its road; the hand-over is the same list of `.pre` files given to drvarma.
The GUI integration is still pending.*

mtram → sima no tiene fichero, ni puerta, ni continuidad de parámetros.
`drvarma` no lee un `.pre` en ningún punto: carga las series originales sin
transformar y vuelve a empezar. El traspaso es prosa en las instrucciones del
servidor.

Mientras eso siga así, **«la escalera» son dos peldaños y una recomendación
verbal**. Conviene decirlo así en la documentación, en vez de dibujar tres.

**Decidido (2026-09-17): drvarma entra y cumple el contrato** — tiene que leer
y validar un `.pre`, como los otros dos peldaños. Es **una fase posterior**, no
de las que quedan, pero deja de ser una pregunta abierta: el tercer peldaño se
va a construir.

Y hay una pieza que no estaba en el reconocimiento: **drvarma tiene GUI**,
`drvarma_source/drvarma_v.04.1/gui/drvarma_gui.c`, ~2.900 líneas y **ya en
GTK3**, con Johansen y VECM. Hoy carga un fichero de datos separado por
espacios y escribe el `.inp` de drvarma — es decir, **entra por debajo del
escalón**, que es justo lo que el contrato tendrá que corregir. Se integra a
posteriori.

---

## 7. Lo reportado por el camino

| registro | id | qué |
|---|---|---|
| drtran-python | **BUG-17** | el `.cns` nombra por posición —y por dos posiciones distintas— y ninguna está en un fichero |
| drtran-python | **BUG-18** | `load_pre` deja vivo el `logL` diagonal al cambiar de series, y es la base del contraste LR |
| drtran-python | **BUG-2** | reabierta la mitad del C: sigue cruzando series con catorce años de desfase, sin avisar |

Pendientes de reportar, medidos aquí y no escritos todavía en ningún registro:
los siete fallos del extremo a extremo (que son un síntoma, no un defecto: hay
que decidir si la referencia cambió legítimamente o si art regresó); `b` negativo
aceptado por el C; el ciclo ausente del cast de Python; y el umbral 1e−6 del
guardián del `.pre`, que es el cuanto del propio fichero.

---

## 8. Estado de la fase 3

**Cerrada.** Entregado este documento. Lo que trajo y el plan no preveía:

- la escalera tiene **dos peldaños sólidos** —el signo BJ y la identidad
  diagonal—, los dos verificados numéricamente y no leídos;
- el `.cns` tiene **dos** índices posicionales, no uno, y el propio programa
  escribe el fichero frágil;
- **mtram no invoca el binario en C**: es un puerto completo, de modo que el
  problema de las seis implementaciones de la fase 1 tiene aquí su gemelo;
- y la demostración ejecutada de que las puertas verdes no bastan: ✅ en la
  diagonal y la ganancia movida un 9% en la misma corrida.

Siguiente según el orden fijado: **fase 4, TASTE** — qué era una serie, un
modelo y un proyecto cuando hubo que inventarlo, y qué de aquello sigue siendo
buena idea.
