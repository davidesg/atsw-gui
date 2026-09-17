# `netfile` — el `.dag`, la red de transferencias

Una línea por **enlace**:

    SALIDA <- ENTRADA   b r s        # lo que siga a # es comentario

donde `SALIDA` y `ENTRADA` son el nombre de la serie —el del `.pre`— o su
posición en la línea de órdenes. La del m6:

    EP <- EI   1 0 1
    EP <- EC   1 0 2
    EI <- EU   1 0 3
    EU <- EC   2 0 1

`EU` es **salida de `EC` y entrada de `EI` a la vez**. Eso es lo que distingue
una red de una estrella, y es lo que los sistemas de Mauricio son de verdad.

## No es código nuevo

`read_network`, `series_index` y `topo_sort` estaban dentro de `drtran.c`,
`static` y atados a los globales `n_ser`, `n_link`, `lnk[]`, `Ts[]`, `topo[]`.
Aquí reciben sus argumentos y nada más. **El motor sigue llamando a estas mismas
funciones**, así que el GUI y el motor no pueden discrepar sobre lo que es un
`.dag` válido: no es que se parezcan, es que son el mismo código.

## Por qué importa el ciclo

El motor resuelve el sistema por **recursión en orden topológico**: una serie
sólo se puede construir después de todas las que la alimentan. Con un ciclo ese
orden no existe, el sistema es **simultáneo**, y no se puede triangularizar
restando transferencias.

> Un ciclo **no es un error de sintaxis**. Es un modelo que no es de este
> escalón: un sistema simultáneo se estima con `drvarma`, no aquí.

Es el mismo veredicto que da la CCF bidireccional cuando los retardos negativos
salen de la banda (`lib/ccfplot`), dicho sobre la red entera en vez de sobre un
enlace.

El motor sólo dice **que hay** ciclo. `net_cycle` dice **cuál**, cerrado y en el
sentido en que la señal viaja:

    EU -> EI -> EP -> EU

No cambia ningún veredicto: añade el detalle, que es lo que el GUI necesita para
señalar la fila que lo cierra.

## La librería no elige idioma

`net_read` no devuelve una frase: devuelve un **hecho** (`NetError`: qué falló,
en qué línea, con qué palabra). Cada frente lo redacta:

| | quién | ejemplo |
|---|---|---|
| `net_error_en` | el motor | `unknown series in the network file: 'NO_EXISTE'` |
| `por_que()` en `gui/drtran/src/red.c` | mtram | *«línea 1: no tengo cargada ninguna serie que se llame «NO_EXISTE». Cárgala en la pestaña Series, o corrige el nombre.»* |

Esto salió de un fallo real: al mover el lector puse el mensaje en español y la
batería del motor cayó de 304 a 303. La salida del motor en inglés es una
**propiedad declarada del puerto** y la batería la comprueba al pie de la letra.
El fallo tenía razón, y arreglarlo mejoró el diseño.

## Las pruebas

`test_netfile.c`, y dos de ellas merecen nombrarse:

**El orden topológico no se comprueba contra una lista escrita a mano** —eso
sólo diría que hoy sale lo de siempre— sino contra **la propiedad que lo
define**: toda serie aparece después de todas las que la alimentan. Si el orden
fuera otro y siguiera cumpliéndola, seguiría siendo correcto, y la prueba lo
acepta.

**Y lo que el GUI escribe se lo pregunta al motor.** Que los dos *lean* igual lo
garantiza compartir el lector; que lo que mtram *escribe* lo lea drtran hay que
comprobarlo con drtran, y eso hace `gui/drtran/tests/run_tests.sh`: reescribe la
red del m6, la pasa por el motor, y exige que el bloque `Transfer network` del
`.out` sea idéntico.

La batería del motor: **304 PASS, 0 FAIL** después de la mudanza.
