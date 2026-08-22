# Defectos de `drvec`

**La numeración es la del conjunto.** `fue`, `drvarma`, `drtran` y `drvec`
comparten motor y cast, y por eso comparten una sola serie de números: `BUG-1` a
`BUG-13` están en `drtran-python/docs/BUGS.md`, y de ellos `drvec` toca los
10 a 13. Los que empiezan aquí siguen esa serie, **no** una propia, para que un
número no signifique nunca dos cosas. (`fue` lleva además su propia numeración
de cuatro cifras, `BUG-0005`, `BUG-0010`, `BUG-0012`, `BUG-0018`, y no se
confunde con ésta.)

Cada entrada dice **qué es, cómo se encontró y qué costó**. Lo que costó es la
parte que no se puede omitir: un defecto sin coste medido no se puede priorizar
ni cerrar.

---

## BUG-14 — `xitol` sin inicializar en la evaluación de origen móvil

**Estado: arreglado el 2026-08-22** (`src/drvec.c`, `rolling_eval`).

`rolling_eval` monta su propia `struct Tvarma` en la pila y la llena con
`vec_shootx`, que rellena todos los campos **menos `xitol`**: esa tolerancia la
pone cada sitio que usa la estructura, y hay una docena de sitios que lo hacen.
Éste no. `elf` recibía entonces como tolerancia de truncamiento del vector `ξ`
lo que hubiera en esa palabra de la pila.

**Cómo se encontró.** Pasando `valgrind` sobre los caminos nuevos de P6:
60 saltos condicionales sobre valor sin inicializar, todos en `cxi`
(`elfvarma.c:773`), con el origen en la asignación de pila de `rolling_eval`.
No lo habría encontrado ninguna comprobación de resultados: el programa no
fallaba ni daba `nan`.

**Qué costó.** Es la ruta de la evaluación **fuera de muestra**, o sea la de la
medición que decide el número de versión del programa (`HOMOLOGATION.md` §4t).
Con el arreglo se volvieron a medir las tres columnas de §4t —`q=1` por defecto,
`q=0` y `q=1 -mafree`, nueve casos cada una— y **reproducen la tabla publicada
dígito a dígito**. Así que el coste medido es **cero** y §4t se sostiene: el
valor que había en la pila caía dentro del rango en que la truncación no muerde.
Eso no lo hace inocuo — era comportamiento indefinido, y el siguiente compilador
o la siguiente pila podían haberlo movido sin avisar.

**La lección.** La estructura tiene un campo que ninguna función de llenado
llena. Mientras siga así, cada nuevo sitio que la use es otra ocasión para el
mismo fallo. Lo que lo cierra de verdad es que `vec_shootx` lo ponga; queda
anotado para el refactor de P8, porque tocarlo ahora movería todas las cifras
del registro por la puerta de atrás.

---

## BUG-15 — el `refactor` del `.pre` no se deshacía en `-interv`

**Estado: arreglado el 2026-08-21** (`src/drvec.c`, `subtract_interventions`).

Los `.pre` que conectan `fue` con el resto de la suite llevan las series
reescaladas ×100 por norma —mejora el condicionamiento del optimizador—, y el
factor viaja en el propio fichero. `subtract_interventions` leía los términos
deterministas del `.pre` y los restaba de los niveles **sin dividir por el
factor**, así que restaba cien veces lo que debía.

**Cómo se encontró.** Estimando un VARMA-VEC sobre IPC_ES, IPC_DE e IPC_FR a
partir de los `.pre` de sus modelos con estacionalidad determinista.

**Qué costó.** El análisis de rango de los tres IPC daba `r = 1` donde es
`r = 0`: una relación de cointegración inventada por un factor de escala. Con el
arreglo, `r = 0` con p de bootstrap 0.313, y Johansen coincide bajo la misma
especificación.

**La lección, que ya estaba escrita.** `drtran-python/docs/PORTE.md` y
`ARCHITECTURE_MCP.md` dicen «nunca codificar el factor de reescalado a mano — la
suite lleva tres defectos registrados por eso». Éste era el cuarto caso del
mismo modo de fallo ya nombrado.

---

## Encontrados y arreglados antes de la 0.9

Sin número porque se arreglaron dentro del desarrollo y su informe completo está
en el registro, no aquí. Se listan porque son los modos de fallo que este
programa tiene, y el que los conozca los buscará antes:

| qué era | dónde está contado |
|---|---|
| el recorrido del vector de parámetros se desalineaba en la impresora y publicaba una `Θ` que nadie había estimado | `DEVELOPMENT_RECORD.md` §4.1 — de ahí viene el bloque **estructural** de la batería, y de ahí la regla de que los índices se anoten **en el recorrido que ya hay** |
| con `-multistart` no había estimación final, y los residuos se quedaban sin calcular: la diagnosis salía con `Q = nan` y «los residuos parecen ruido blanco» | `src/drvec.c`, junto al arreglo — la peor forma posible de equivocarse, porque el mensaje era tranquilizador |
| el bloque de multi-arranque realojaba la estructura VARMA con la primera asignación viva: 1080 bytes perdidos | de ahí el bloque `VALGRIND=1` de la batería |
| doble liberación (`SIGABRT`) al centralizar la limpieza: `main` ya liberaba `datamat` e `Y2_levels`, y con `nobs` en vez de `alloc_nobs` | latente bajo `-estwin`, que es justo donde `nobs` se recorta |
| el `.pre` de prueba se leía una fila desplazada: el bloque determinista lleva **dos** líneas de banderas, no una | misma firma que `BUG-11`, y del lado de `drvec` |

---

## Lo que se vigila, y no es un defecto

Está en `DEVELOPMENT_RECORD.md` §10, que es la lista viva. Lo que más importa
tener a mano al leer una salida:

- **`termcode 3`.** El optimizador se para en «last global step failed» con
  frecuencia. Explicado y medido, no eliminado: `docs/CONVERGENCE.md`.
- **La frontera de invertibilidad en la clase libre** (`-mafree`): ahí los
  errores estándar no están definidos en la dirección que liga. En la clase por
  defecto **no puede alcanzarse** (Corolario 6.3).
- **Los valores críticos del contraste de rango** son asintóticos: sobre un
  proceso con `r = 1` a `n = 120` aciertan el 68 % de las veces y sobre-rechazan
  el 30 %. `-bootstrap` sube a 78 %/20 %; no lo arregla.
- **La elección del bloque `Y₁`** es del usuario y no se comprueba. La alarma de
  normalización avisa cuando la relación apenas lo involucra.
