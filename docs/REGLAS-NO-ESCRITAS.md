# Las reglas no escritas del formato

Anexo de [CONTRATO.md](CONTRATO.md). Invariantes que el código impone y que no
están en ninguna especificación: romperlos hace que el motor falle, se cuelgue,
o —lo peor— dé resultados silenciosamente malos.

Marcas: **[V]** verificado en ejecución · **[C]** leído en el código, la
lectura es inequívoca · **[?]** inferido, no ejercitado.

Hallazgo transversal que explica casi todo lo que sigue: **`fue.c` hace 115
llamadas a `fscanf` y no comprueba el retorno ni una sola vez.** Un campo que
falta no da error: deja la variable con su valor anterior y el token en el
flujo, y el resto del fichero se lee desplazado. `inpcheck.c` existe como
prepaso que repite las mismas llamadas en el mismo orden para atrapar eso
antes de escribir nada — pero sólo cubre lo que él mismo valida.

---

## I. Corrupción silenciosa

El motor termina, imprime un informe completo, y los números son otros.

1. **[C]** Un regresor externo cuyo nombre coincida con una palabra reservada
   (`step`, `trend`, `cos`…) deja de ser columna y se **genera**:
   `fue.c:368-504` encadena `strcmp` y todo lo demás cae al `else`. El bloque
   de datos espera una columna menos y cada fila se come el primer número de
   la siguiente. `inpcheck.c:251-272` replica la cadena, así que tampoco lo
   ve.
2. **[C]** `easter` **sólo existe si `freq == 12`** (`fue.c:443`). En
   trimestral cae a «no estándar» y exige una columna que no está → el mismo
   desplazamiento. art lo protege en `_make_model` pero **no en la ruta
   encadenada** `_build_arma_on_model` (`pipeline.py:1023-1026`).
3. **[V]** ω y δ cuentan **al revés**: ω recorre `0..Nomega` → `Nomega+1`
   líneas y **siempre** una etiqueta, incluso con 0; δ recorre `1..Ndelta` →
   `Ndelta` líneas y **ninguna** etiqueta si es 0 (`fue.c:527-548`).
4. **[C]** La frecuencia debe ser **1, 4 o 12**. Con otra, los `ifadf` se
   leen, se guardan, se reescriben en el `.pre` **y se ignoran** al construir
   el operador (`fue.c:891-895` frente a `:950-965`). `inpcheck.c:212` acepta
   cualquier entero ≥ 1.
5. **[C]** `fue` trunca la respuesta impulso del determinista racional en **40
   retardos** (`fue.c:2749`) y `fuf` en **20** (`fuf.c:1873`). El mismo modelo
   da un efecto determinista distinto en estimación y en predicción, y la
   diferencia crece cuanto más cerca de 1 esté δ.
6. **[C]** `fuf` usa para las bandas **la σ² que viene del fichero**, no la
   que reestima (`fuf.c:307, 1212` frente a `:1130-1138, 1342`). Reutilizar un
   `forecast_*.inp` de otro modelo cambia las bandas y deja el σ² impreso
   intacto.
7. **[C]** `fue` escribe σ² con `%.10f` (`fue.c:1771`): por debajo de 5e-11 sale
   `0.0000000000`, `inpcheck_fuf` lo acepta y **`fuf` produce bandas de anchura
   cero**. Es la razón mecánica del `refactor = 100`.
8. **[C]** `refactor` es el **segundo** token de la línea de bandas, y las
   cinco implementaciones hacen `if (refactor == 0) refactor = 1`. Escribirlo
   solo, o invertir los campos, da escala 1 en silencio: σ̂ ×100 fuera, μ ×100
   fuera, ℓ/AIC/BIC desplazados en n·ln(100). En drvarma ni siquiera es campo
   del fichero: es `trans_scale = 100.0` compilado.
9. **[V]** Los tres dialectos comparten extensión (§3 de CONTRATO.md).
10. **[C]** En fug la línea de Box-Cox es **posicionalmente ambigua**: 3 campos
    → `λ d D`; 4 → `λ m d D`; 5 → `λ m geom d D`. Quitar o añadir un campo
    reinterpreta la línea entera sin error (`fug/src/inpfile.c:124-139`).
11. **[C]** El `.inp` de fue **no tiene campo `m` de Box-Cox** (cableado a 0.0,
    `fue.c:972`) ni `geom`, `eml`/`aml`, `chk`/`nochk` — son opciones de línea
    de órdenes. **El `.pre` no reproduce la ejecución.**
12. **[C]** Con λ=0 y algún dato ≤ 0, `fue` produce **NaN y sigue**
    (`fue.c:3385`, sin guarda). `fug` avisa y cae a λ=1 — pero el aviso va al
    `.out`.
13. **[C]** **El `.pre` no registra si el optimizador convergió.** El motivo de
    parada va al `.out`. Y con semillas no estacionarias `fue.c:1191-1204` no
    estima: escribe `.out` y `.pre` con las semillas verbatim y sale con 3. El
    `.pre` resultante es indistinguible de uno bueno.
14. **[C]** En `fuf` las columnas de un regresor externo se leen sólo para
    `1..nobs` y valen **cero sobre todo el horizonte** (`fuf.c:880-887, 329`).
    El formato no tiene sitio donde poner su futuro.
15. **[C]** drtran compara series **por índice**: sólo `nobs != nobs` es error
    (`drtran.c:4287-4291`); `begyear`, `begtime` y `freq` no se comparan nunca.
    El puerto lo rechaza y trae el caso medido: dos series desfasadas 66 años,
    «proponía b=18 en serio».
    *Resolved 2026-09-26: the C rejects it too (`lib/fuepre`,
    `fuepre_check_alignment`: same frequency and same last date).*
16. **[C]** En drtran el bloque de datos **se rellena con ceros si es corto** y
    la lectura devuelve ÉXITO (`fue_pre_reader.c:613-648`). Y como lee con
    `fgets`+`sscanf("%lf")`, dos valores en una línea pierden el segundo.
    *Resolved 2026-09-26 (the first half): a short data block is an error in
    `lib/fuepre/fue_pre_reader.c`. The second half stands: that section also
    carries the non-standard deterministic variables as extra columns.*
17. **[?]** Los `ifadf` se componen **multiplicativamente** con `(d, D)`:
    declarar la misma raíz dos veces sobrediferencia en silencio.
18. **[C]** En un `.pre` **anual** una intervención fechada lleva **un** campo
    de fecha (`fue_pre_reader.c:187`); escribir dos reinterpreta el primero
    como el año y tira el segundo → intervención fuera de muestra (regla 22).
19. **[C]** En el `.dag`, un token numérico se resuelve como **posición** en el
    C (`drtran.c:2629-2640`) y como **nombre** en el puerto
    (`network.py:38-46`). El mismo `.dag` da enlaces distintos.
20. **[C]** Una línea del `.cns` que el parser no reconozca como `NAME = …` se
    **salta sin decir nada** (`drtran.c:3239`, `continue`). Una restricción que
    el analista cree puesta, no lo está.
21. **[C]** Las banderas deben ser **literalmente 1** — salvo la de `mu`, que
    libera con cualquier valor distinto de cero (`drtran.c:4283`).
22. **[C]** Una intervención **fuera de muestra** da un regresor idénticamente
    nulo con coeficiente **libre** (`fue.c:381-437` + `:532`). Hessiano
    singular, errores típicos de millones, cero diagnóstico.
23. **[V]** art pierde la columna de todo regresor externo que reescribe →
    art **BUG-0187**.
24. **[V]** Ni el `.inp` ni el `.pre` pueden expresar una **media fija no
    nula**: los cuatro escritores hacen `if (libre) "v 1" else "0"` y tiran el
    valor. Los dos lectores sí la aceptan.
25. **[V]** art lee la media por `getattr(model, "mu")` y el atributo es
    `mu0` → art **BUG-0189**. *Si sólo se arregla una cosa de esta lista, es
    ésta: es un error de una palabra y falsifica el guion.*

## II. Corrupción de memoria, cuelgues y abortos

26. **[C]** `NT = 10` deterministas no estándar (`fue.c:84`), y
    `nstdet += 1; det[nstdet] = i` **sin cota** (`:501`). `inpcheck` admite
    1000: el fichero pasa y el motor escribe fuera del bloque.
27. **[C]** G declara `It[50]`, `Arr[20]`, … `Data[2000]` **consecutivos en la
    misma unidad de traducción** (`model_globals.c:7-27`) y el lector escribe
    sin comprobar, mientras `inpcheck` admite 1000 deterministas y `nobs` hasta
    10 000 000. Un `.inp` de 2001 observaciones pasa la puerta y escribe encima
    de punteros que luego se liberan.
28. **[C]** El **AR(1) fijado en cero** de art (`pipeline.py:641-663`) evita un
    SIGSEGV medido: 178 de 4 505 `.inp` del corpus sin ningún factor. Hay una
    **segunda copia** (`:1262-1264`) que ignora P y Q y mete el relleno
    habiendo operador estacional.
29. **[C]** AR estacional libre **+** MA estacional libre aborta el estimador.
    La guarda está sólo en el carril autónomo (`policy.py:484-486`); la
    superficie MCP no la tiene.
30. **[C]** La línea de `ifadf` debe traer **exactamente** `freq/2+1` enteros o
    el parser de drtran avanza el puntero un desplazamiento **sin
    inicializar** (`fue_pre_reader.c:599-600`, `off` sin asignar).
    *Resolved 2026-09-26: fewer integers is an error with the count.*
31. **[C]** El orden de δ debe ser **≤ 9**: `tran_shootx.c:52` declara
    `real wr[10]` y `chekma` escribe `1..m*q`, dentro de la función que se
    llama en cada evaluación de la verosimilitud.
32. **[?]** En el cast embebido, `m × q` debe quedarse en **32**
    (`tran_shootx.c:522`). El autor arregló exactamente esto para el cast
    sustractivo (`:662-673`) y **no convirtió la rama embebida**.
33. **[C]** En `drtran_win`, ninguna semilla AR puede acercarse a 0.999, para
    **todo** orden (`tran_shootx.c:621`): todo AR(2) de raíces complejas con
    φ₁ > 1 se rechaza sin evaluar. Arreglado en `drtran`; las copias han
    divergido (drvarma BUG-0002, abierto).
34. **[C]** Un `b` negativo en el `.dag` escribe en índices negativos
    (`tran_shootx.c:196-198`). El puerto lo rechaza; el C no.
35. **[C]** `MAX_SLOT 400`: `add_slot` es un **no-op mudo** al llegar al tope
    (`drtran.c:3073`), y después se reserva con el conteo capado y se llena con
    la enumeración sin capar.
36. **[C]** Cabecera: `drtran_win` exige **5 líneas exactas**; `drtran` busca
    una línea que contenga `"requency"` — el único punto del formato donde el
    lector mira lo que *dice* un comentario, y por tanto donde un banner que
    contenga «frequency» secuestra el análisis. La medición está en el
    comentario: un `.inp` de drvec daba `nobs = 1787128427`.
37. **[C]** drvarma: `p = q = 0` es mortal en el motor C; sólo la puerta FFI lo
    rechaza. Y `macheps` sin fijar hace que **`elf` no vuelva** — bucle
    infinito por un global sin inicializar.
38. **[C]** Longitudes: `MAXSTR 90` en fue (ninguna línea de comentario puede
    pasar de 89 caracteres), 200 en drtran, 8191 en drvarma **justo cuando su
    documentación invita a escribir la matriz entera en una línea**. Nombres
    con `%s` sin anchura en varios sitios; la ruta del `.inp` de drvarma debe
    quedarse bajo 80 caracteres.
    *Note 2026-09-26: drvarma's `MAXSTR` is 80 (`main.h`); its `.inp` reader
    uses an 8192 buffer. The `.pre` reader (`lib/fuepre`) no longer uses the
    host's `MAXSTR`: it has its own 512.*

## III. Pérdidas de precisión en el ida y vuelta

39. **[V]** El `.pre` cuantiza: ARMA y δ a 4 decimales, ω a 6, **λ a 2**, datos
    a 10, columnas de regresores a 6. La invariante «los números no se mueven»
    es propiedad de las **estimaciones**, no de las **semillas**. Un θ de
    0.99996 vuelve como `1.0000`, en la circunferencia unidad.
40. **[C]** La frecuencia de un operador de frecuencia fija es un **índice
    armónico entero**, se lee como real (`fue.c:768`) y se escribe como entero
    (`:2195`). Un *k* fuera de `0..s/2` se solapa y nada lo comprueba.
41. **[V]** G escribe `refactor` con `%.2f` y **tira `cbands`**
    (`file_io.c:223`, el `0` es literal). Y no tiene dialecto de fuf: hoy está
    tapado porque `inp_ok_to_load` se niega a cargarlo, no porque el escritor
    sepa escribirlo.

## IV. Contratos posicionales que nadie enuncia

42. **[V]** **El formato es sensible a líneas aunque no lo parezca.** Todo se
    lee con `fscanf`, pero el escritor del `.pre` reabre el `.inp` y se
    posiciona contando **once** `fgets` (`fue.c:1734-1746`). Una línea en
    blanco de más desincroniza los dos lectores y el `.pre` sale con nombres de
    determinista basura, sin error. `inpcheck` valida sólo el primer lector.
    La regla real: **una línea de etiqueta y una de valores por sección**, y la
    cabecera son **cinco** líneas.
43. **[C]** La línea de fecha lleva un campo «nombre de residuos» que **nadie
    escribe**; el lector se recupera por accidente comiéndose el `**`
    siguiente. Consecuencia: **el nombre de la serie debe ser un solo token sin
    espacios**. Y en anual, R1 toma el año como `nums[-1]`: una serie de nombre
    numérico se absorbe como el año y mueve todas las fechas.
44. **[C]** Los bloques de frecuencia fija **anuales no existen**: lector y
    escritor los tienen **comentados** (`fue.c:786-813`, `:844-871`). El `.inp`
    tiene exactamente dos bloques de frecuencia fija.
45. **[C]** Un operador de frecuencia fija guarda **un solo** coeficiente y debe
    ser **estrictamente negativo**: φ₁ se deriva como `2·cos(2πk/s)·√(−φ₂)`
    (`fue.c:2929`), y un valor positivo hace la verosimilitud inevaluable en el
    punto de arranque. art siembra siempre −0.5, así que la regla se cumple por
    accidente.
46. **[C]** Semillas MA(1) con |θ| > 1 **se invierten en silencio**
    (`fue.c:2987`), con un `CAUTION` en el código avisando de que estropea los
    errores típicos. Y el chequeo de invertibilidad compara **la diagonal, no
    el módulo** (`elfvarma.c:528-532`): una raíz compleja de módulo 1.2 pasa.
47. **[C]** Observaciones de más al final se descartan sin decir nada; `nobs`
    manda. En R1, además, es la cabecera «time series» la que le dice al parser
    que falta la sección de bandas: reescribirla hace que **se coma la primera
    observación** y la serie entera se lea corrida un período.
48. **[C]** Toda línea que no empiece por `*` es DATO para R1. El C es más
    estricto y más callado: un `**` de más hace que el `fgets` se lo coma y el
    `fscanf` choque contra la cabecera real. `write_pre` logra el conteo exacto
    con el truco deliberado de **fundir el `**` final de cada bloque con la
    etiqueta siguiente**.
49. **[C]** Contadores del fichero usados sin acotar en Python
    (`inp.py:188-194`, `:335`, `:357`). Y `art/guion.py:846-877` lee los
    deterministas contando líneas sin comprobar que sigan en el bloque: con un
    `n` grande devuelve `"0"` o `"1"` como nombre — y eso alimenta a
    `entradas_que_no_cuadran`, el detector que existe para cazar desacuerdos
    entre guion y fichero. **El auditor se puede hacer mentir con el mismo
    fichero que audita.**
50. **[C]** drtran compara prefijos con `strncmp`: un determinista llamado
    `timeshift` se genera como **tendencia lineal**, en silencio.
    *Resolved 2026-09-26: the keyword is the whole first word, compared with
    `strcmp` as fue does; and only `trend`, since fue never accepted `time`.*
51. **[C]** `s = -1` en el `.dag` es un centinela vivo en el C y un error en el
    puerto.
52. **[C]** drvarma consume los nombres **posicionalmente**: un nombre que
    empiece por `*` desaparece y desplaza todo.

---

## Lo que sí falla limpiamente

`fue`/`fuf` ≥ 1.14/1.09 con `inpcheck`: línea y motivo, salida 2, sin escribir
nada. Topes: 1000 para todo contador y orden; `nobs` ∈ [1, 10⁷]; estación
inicial ∈ [1, freq]; `d`, `D` ∈ [0,100]; **`nobs − d − freq·D ≥ 3`** (la única
comprobación de que queda muestra en las seis implementaciones); horizonte de
fuf ∈ [1,10000]; σ² ≥ 0. Y una regla de nombres que sólo vive ahí: **un
determinista no estándar no puede empezar por dígito, `-`, `.` ni `*`**.

`fue` siempre concatena `.inp` a `argv[1]`: **no se puede ejecutar sobre un
`.pre`**, hay que copiarlo — y copiarlo evapora la regla «nunca estimar desde
un `.pre`», que en el puerto se impone por sufijo.

---

## Estado de la documentación, en una frase

`fue-1.14/README.es.md` **no describe el formato en absoluto**.
`atsw-suite/docs/FILE_FORMATS.md:79-117` es la única especificación, es de
orden de campos, y afirma que «las líneas `**` son etiquetas para humanos; lo
que importa es el orden» — cierto para R1 y **falso para el C** (regla 42).
drtran y los `.dag`/`.cns` no tienen especificación de ninguna clase.

Las mejores fuentes de estas reglas son código, no prosa: **`inpcheck.c`** (que
es la versión escrita de la mitad de todo esto), **`fug/src/inpfile.c`** (el
lector defensivo de referencia) y los mensajes de commit `24dda25`, `a0d3509`
y `ac2feb2` de gtk_fue.09.

---

*Procedencia: reconocimiento en profundidad del 2026-09-16 sobre los seis
lectores/escritores del formato, más drtran, drvarma y el puerto de Python.
Las marcas [V]/[C]/[?] distinguen lo ejecutado de lo leído y de lo inferido.*
