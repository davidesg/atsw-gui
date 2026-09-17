# DISEÑO — mtram, el GUI de drtran

Qué pantallas tiene, en qué orden, y **por qué cada una cierra la puerta de la
siguiente**.

---

## 0. El límite de TASTE, dicho con precisión

TASTE sí hace función de transferencia, y más de lo que parecía: hasta **12
entradas**, con **δ(B) de verdad**, `b ∈ [0..52]`, `r, s ∈ [0..4]`
(`Ftaste/TASTECTV.PAS:34,57-60,113-133`; `TFMOD.PAS:140,147,165`). Hay una
sesión real conservada en `atws/Taste/TASTE.OUT.tf`.

Pero su propio menú declara el límite:

```pascal
Mes[5] := '    FUNCION DE TRANSFERENCIA CON UN SOLO OUTPUT    ';   /* MENTEC.PAS:480 */
```

**Un output.** Y la diferenciación es única, la de la ecuación: al estimar,
output e inputs sólo reciben Box-Cox, `TransDiff(..., 0, 0, 1, 1, nn)` con
d = D = 0 (`TFEST.PAS:592,609`). Los d/D propios de cada input existen **sólo
para preblanquear al identificar**.

| | TASTE | drtran |
|---|---|---|
| ecuaciones | **1** | red de varias |
| ∇ por serie | uno, el de la ecuación | distinto por serie, con despacho de cast |
| restricciones entre ecuaciones | **ninguna** | `.cns` |
| grafo de dependencia | **ninguno** (dos cadenas de 8 letras) | `.dag` |
| fijo/libre por coeficiente | **no existe** — sólo bajar el orden (`TFMOD.PAS:519`) | bandera por coeficiente |
| estimación | mínimos cuadrados con retroprevisión | ML **exacta** |
| especificación en texto | **no**: `.TSM` es un volcado binario de 3947 bytes | `.inp`/`.pre` |

> **TASTE es una familia de modelos. mtram es un grafo.** Todo lo que sigue
> respeta esa frontera: se rescata el *principio*, no la forma.

---

## 1. Lo que se rescata, y por qué

### 1.1 El método se impone sin prohibir nada — **el hallazgo principal**

El registro de menú de TASTE **no tiene campo `Enabled`** (`MENTEC.PAS:30-50`).
Ninguna opción se grisa jamás. Puedes entrar en Estimación sin haber
identificado (`TASTE.PAS:574-632`: la única guarda es de memoria).

Lo que no puedes es *terminarla*, porque **cada paso te pide por su nombre un
artefacto que sólo el paso anterior sabe fabricar**:

```pascal
UNTIL (ModName[i] <> '') AND (ModName[i] <> 'NULL');                  /* TFID.PAS:111 */
IF NOT (Model_En_Cat(ModName[i])) THEN
   Disp_Error_Msg('EL MODELO "'+ModName[i]+'" NO ESTA EN MEMORIA');   /* TFID.PAS:203-207 */
```

Para identificar una transferencia hay que haber modelado univariantemente cada
entrada. **Eso es la escalera, treinta años antes y sin contrato de ficheros.**

Y el camino correcto es además el de menor resistencia: cada diálogo precarga su
primer campo con la salida del anterior (`STATUS.LastSer` / `STATUS.LastMod`,
`TFID.PAS:147,303`; `TFMOD.PAS:365,602`). Si sigues el orden, ya está tecleado.
Si te lo saltas, tienes que teclear.

**Se adopta entero.** Es la regla de diseño de mtram: *ninguna pantalla se
bloquea; cada pantalla pide lo que la anterior fabrica.*

### 1.2 `[C] CORREGIR` como tercer desenlace, y por bloque

```pascal
st := '[ENTER] PROCESAR     [C] CORREGIR     [ESC] SALIR  ';   /* TASTEGES.PAS:562 */
```

y el bucle es **por input**, no por formulario (`TFMOD.PAS:483-492`): revisas la
entrada 3 sin perder la 1 y la 2. El aceptar/cancelar de hoy perdió esto.

### 1.3 El formulario se despliega según el orden

Tecleas `s` → aparecen s+1 campos `OMEGA(j)`; tecleas `r` → aparecen r campos
`DELTA(j)`, con la estacionariedad validada en el propio campo
(`UNTIL Abs(DelNum[i,j]) < 0.99`, `TFMOD.PAS:149-177`).

**La forma del formulario ES la especificación.** No una rejilla fija de cuatro
ω y cuatro δ con casillas apagadas.

### 1.4 La lectura pegada al gráfico

- Ljung-Box **escalonado en la columna derecha de la propia ACF**
  (`TASTE.OUT.tf:353-371`).
- Los retardos estacionales con `=` en vez de `*`, para no tener que contar
  (`PLOTS3.PAS:136-139`).
- Histograma con **% observado contra % esperado** en la misma línea
  (`TASTE.OUT.tf:345-346`).
- Serie con los atípicos marcados en el margen (`>` … `<`) y **tabla de
  tipificados ≥ 2 debajo** (`TASTE.OUT.tf:225,305-316`).
- Ganancia **con su desviación típica** por el método delta (`BACKTF.PAS:894-930`).
- Raíces factorizadas con amortiguamiento, frecuencia y **periodo del ciclo**,
  también de ω(B) y δ(B) (`ESTIMATE.PAS:172,227`).
- **Matriz de correlaciones entre parámetros** — el diagnóstico de
  sobreparametrización más barato que hay (`TASTE.OUT.tf:182-191`).

Esto es contenido para `lib/fugplot` y `lib/equation`, no arquitectura.

### 1.5 Mensajes que nombran el objeto **y la dimensión** del fallo

```pascal
'Nº OBSERVACIONES INPUT 2 DISTINTO DE Nº OBSERVACIONES OUTPUT'   /* TFEST.PAS:577-579 */
'TRANSFORMACION DEL INPUT 2 DISTINTA EN TF Y US'                 /* TFEST.PAS:614 */
'HAY UN MAXIMO DE 7 INPUTS EN MEMORIA'                           /* TFID.PAS:181-182 */
```

Ninguno dice «error de validación». Todos dicen **qué objeto, por su nombre, y
en qué falla**.

### 1.6 El banco de tanteo: `Cálculo · Pesos NU`

Tecleas b, s, r, ω y δ **a mano, sin datos y sin estimar**, y devuelve ν(B) y la
respuesta al escalón acumulada, en la misma línea (`CALCS1.PAS:173-192`):

```
     NU[0] =         0.43763   ESCALON =         0.43763
     NU[1] =        -0.19599   ESCALON =         0.24164
```

Es donde el analista aprende qué significa (b, r, s) **antes** de leer una CCF.
drtran tiene `impulse_response` en el motor; le falta la pantalla.

### 1.7 Diagnosticar es identificar otra vez

`TFEST.PAS:55-62` es literalmente la misma secuencia que `USID.PAS:242-251` con
`'RESIDUOS'` en lugar de la serie. Y los residuos son una entrada más del
catálogo, así que las ocho operaciones de `Datos` funcionan sobre ellos.

Con una corrección obligada, abajo (§2.4).

### 1.8 Y un detalle que nadie explica y funciona

```pascal
IF DataRecord[CatPs[i]]^.cont = 'C' THEN     /* TFEST.PAS:81 */
```

Sólo cruza los inputs **estocásticos**. Se salta las dummies de intervención,
donde la CCF no significaría nada. No avisa: sencillamente no imprime lo que no
vale.

---

## 2. Dónde TASTE no llega, y qué se pone en su lugar

### 2.1 Una cadena no expresa un grafo

El protocolo de TASTE es lineal: `Identificación · Modelo · Estimación ·
Previsión`, cuatro opciones idénticas bajo US y bajo UT
(`MENTEC.PAS:657-737`). Con un output, la cadena *es* el modelo.

Con una red no. Los cuatro pasos siguen valiendo, pero **se aplican por enlace y
por ecuación, no una vez**. Y aparece un nivel que en TASTE no existe: el de la
red entera.

**La respuesta: dos niveles, y el principio de §1.1 en los dos.**

```
NIVEL RED        qué series, qué papeles, qué enlaces, qué restricciones
NIVEL ECUACIÓN   identificar, especificar, estimar, diagnosticar, prever
```

### 2.2 El `.dag` es un documento, no dos cadenas de 8 letras

Lo único que TASTE persiste de la relación entre objetos es `LastSer` y
`LastMod`. mtram necesita el grafo **editable, con detección de ciclos en vivo**:
un ciclo no es un error de sintaxis, es un modelo que no se puede estimar por
recursión y hay que decirlo mientras se dibuja, no al lanzar.

Sin precedente en TASTE. Diseño nuevo.

### 2.3 El `.cns` no tiene análogo de ninguna clase

TASTE no tiene restricciones ni **dentro** de una ecuación: `npar := s+1+r`
(`TFMOD.PAS:519`), todo lo que declaras por orden se estima, y la única forma de
fijar un coeficiente es bajar el orden.

Eso choca de frente con el contrato: **un parámetro con bandera de FIJO es
especificación, no semilla**. El editor del `.cns` no puede parecerse a
`TFMOD.PAS`; sí puede heredar su forma desplegable (§1.3) añadiendo la bandera
por coeficiente.

**Y hace falta más que una bandera.** El `.cns` resultó ser un lenguaje de
cinco formas sobre una **tabla de slots** que el modelo genera:

    NOMBRE = free            liberar   (las q[i,j] nacen fijas en cero)
    NOMBRE = 0.5             fijar
    NOMBRE = OTRO            compartir: un grado de libertad en dos sitios
    NOMBRE = [-]a * b        producto — numerador factorizado con MA compartida
    NOMBRE = a + b - c       combinación lineal — un (1−B) fijo impone ν(1)=0

De ahí sale la forma de la pantalla, y es la generalización de §1.3 al caso que
TASTE no tenía: **la pantalla no es un editor de texto, es una vista de la
tabla de slots.** Los nombres no son libres — `omega3[2]` existe o no según el
`.dag` y según lo que traigan los `.pre` — así que teclear a ciegas es el error
más fácil de cometer y el más tonto de diagnosticar. La lista *es* el modelo.

Un hallazgo del camino, y es el contrato en miniatura: en el m6 **las seis
medias están fijas en sus `.pre`**, así que no hay ni un `mu[i]` en la tabla.
No es un olvido: es la única lectura correcta del fichero.

### 2.4 Unos residuos por modelo, no uno global

```pascal
DataName[MNI+1] := 'RESIDUOS';     /* TASTECTV.PAS:475 */
```

**Una sola ranura.** Estimar un segundo modelo pisa los del primero: en TASTE no
se pueden comparar dos modelos. Es la carencia más grave de su diseño y hay que
corregirla al copiar §1.7.

### 2.5 La compatibilidad de operadores ∇

No existe en TASTE porque el operador es uno. En drtran la salida y la entrada
pueden ir diferenciadas por operadores distintos, y eso **cambia el cast**: si el
empotrado ajustara ν·Δ con Δ(1) = 0, la ganancia saldría aniquilada (BUG-8). La
pantalla de Series ya lo enseña y avisa del despacho.

### 2.6 La CCF bidireccional, donde se decide

TASTE **tiene** la CCF bidireccional con una Q por lado, y escribe la leyenda con
los nombres y la flecha:

```pascal
Writeln(..., SerName1:8,' --> ',Sername2:8,' SI k > 0 ');
Writeln(..., SerName2:8,' --> ',Sername1:8,' SI k < 0 ');   /* PLOTS3.PAS:95-97 */
Writeln(' Q(',Lags+1,') = ',chi_test( Lags):7:3,'    k ≥ 0');
Writeln(' Q(',Lags+1,') = ',chi_test(-Lags):7:3,'    k ≤ 0');  /* PLOTS3.PAS:169-171 */
```

Pero su pantalla de identificación UT llama con `UnoDos = 1`, que pone
`down := 0`: **sólo enseña k ≥ 0** (`TFID.PAS:289`). El instrumento de
exogeneidad vive en otro menú, `Datos · C.C.F.`, sobre series sueltas.

> **TASTE tenía la herramienta y no la puso donde se decide.** `lib/ccfplot` sí,
> y `identifica.c` dice además la consecuencia metodológica: si la salida
> antecede a la entrada, el escalón que toca es el VARMA, no éste.

### 2.7 Lo que no se copia por ser de su época

- Un solo canal de mensajes, modal, «pulse cualquier tecla», sin severidades: el
  mismo cuadro para `'SISTEMA INICIALIZADO'` que para `'FACLDLT O.K.'`
  (153 llamadas a `Disp_Error_Msg`).
- Sin p-valores: están **comentados, ocho veces**, una por rama de parámetro
  (`ESTIMATE.PAS:501-541`).
- Sin ayuda: `'NO HAY AYUDA DISPONIBLE EN ESTA VERSION'` (`MENTEC.PAS:265,1261`).

Lo que **sí** se conserva en espíritu es el canal único de salida: todo —tabla,
gráfico, ACF, ecuación, diagnosis— al mismo informe paginable, imprimible,
archivable con una tecla y hoy *greppable* (`BROWSE.PAS:187-211`). `lib/preview`
ya da guardar e imprimir sobre el mismo fichero que se va al papel.

---

## 3. Las pantallas de mtram

Cada una fabrica lo que la siguiente pide. **Ninguna se bloquea.**

| # | pantalla | pide (lo fabrica…) | fabrica | hoy |
|---|---|---|---|---|
| 1 | **Series** | un `.pre` por serie — *fue* | papeles, ventana común, compatibilidad ∇ | ✔ |
| 2 | **Red** (`.dag`) | series con papel | el grafo, sin ciclos | ✔ |
| 3 | **Identificación** | salida + entrada con sus `.pre` | (b, r, s) y el veredicto de exogeneidad | ✔ |
| 4 | **Modelo** (`.cns`) | los órdenes propuestos | la especificación, con banderas fijo/libre | ✔ |
| 5 | **Estimación** | un `.cns` completo | el `.out`, y los residuos **de este modelo** | ✔ |
| 6 | **Diagnosis** | residuos | el veredicto, con la batería de identificación | |
| 7 | **Previsión** | un modelo estimado y diagnosticado | las previsiones y su evaluación | |

Estado hoy: **1 a 5 hechas**. Faltan diagnosis y previsión.

De la 2 salió una lección que vale para las que quedan. Al sacar el lector del
`.dag` a `lib/netfile` puse su mensaje de error en español, y la batería del
motor cayó de 304 a 303 porque comprueba `unknown series` **al pie de la
letra**: la salida del motor en inglés es una propiedad declarada del puerto.
La corrección no fue traducir de vuelta, fue que **la librería devuelve el
hecho, no la frase** (`NetError`: qué falló, en qué línea, con qué palabra), y
cada frente lo redacta. El fallo tenía razón, y arreglarlo mejoró el diseño.

La 5 añadió tres reglas propias, porque es la primera que **lanza el motor
como programa**:

- **La orden se ve.** Completa y seleccionable, antes de ejecutarla. Es lo que
  permite reproducir en un terminal lo que el GUI hizo, pegarlo en un guion o
  mandarlo con un informe de error. *Un GUI que esconde la orden convierte cada
  duda en una arqueología.*
- **Se dice QUÉ binario se va a lanzar, y de cuándo.** El motor se busca en el
  `PATH`, y uno instalado hace meses se ejecuta igual de callado que el recién
  compilado: los resultados serían de otro programa y nada lo diría. (Comprobado
  en esta máquina: el `drtran` del `PATH` era de julio y el del repo de hoy.)
- **No se inventan controles que no existen.** TASTE dejaba tocar iteraciones,
  tolerancia y longitud de paso (`TFEST.PAS:295-300`); drtran los tiene clavados
  en `drtran.c:3256-3257`. Se **enseñan** como el criterio de parada que rige, y
  no se ofrece una casilla que no llega a ningún sitio.

Y un principio que la 2 estrena y las siguientes heredan: **lo que el GUI
escribe se le pregunta al motor.** Que los dos *lean* igual lo garantiza
compartir el lector; que lo que mtram *escribe* lo lea drtran hay que
comprobarlo con drtran.

### Reglas transversales

1. **Nada se grisa.** Cada pantalla pide por su nombre lo que le falta y dice
   quién lo fabrica (§1.1).
2. **Todo mensaje nombra el objeto y la dimensión del fallo** (§1.5).
3. **Corregir es un desenlace de primera clase, y por bloque** (§1.2).
4. **El formulario tiene la forma del modelo** (§1.3).
5. **La lectura va pegada al gráfico** (§1.4).
6. **Los residuos son por modelo** (§2.4).
7. **Lo que se ve en pantalla es el fichero que va al papel** — `lib/preview`
   interpreta lo que escribe `fugdraw`; mtram no tiene dibujante propio.

---

## 4. Ficheros citados

`Ftaste/`: `TASTECTV.PAS` (tipos y cupos) · `MENTEC.PAS` (todos los menús, y el
editor de campo) · `TASTE.PAS` (ventana y despachador) · `TFID.PAS`
(identificación UT) · `TFMOD.PAS` (formulario TF) · `TFEST.PAS` (estimación y
diagnosis) · `TFFO.PAS` (previsión) · `TASTEGES.PAS` (`PCS`, `Disp_Error_Msg`) ·
`PLOTS3.PAS` (CCF y bandas) · `ESTIMATE.PAS` (informe y ecuación) ·
`MRQEST.PAS` (optimizador) · `BROWSE.PAS` (el visor) · `CALCS1.PAS` (Pesos NU) ·
`USID.PAS` (barrido univariante).

Artefactos: `atws/Taste/TASTE.OUT.tf` (sesión de transferencia, 429 líneas) ·
`Ftaste/TF.TSM` · `Ftaste/CONSUMO.BJD`.

Ver también `docs/ESTUDIO-taste.md` (TASTE como *diseño*, no como oráculo).
