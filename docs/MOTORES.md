# Los motores: cómo se cambia uno

Las reglas de trabajo de los motores de cálculo de la suite (fue, fuf, drtran,
drvarma, drvec) y de sus portes a Python. Están aquí juntas porque hasta
noviembre de 2026 andaban repartidas entre `RELEVO.md`, los diseños y la
memoria de quien trabajaba, y eso costó dos días de cambios cruzados.

- Las pruebas, una a una: `docs/PRUEBAS.md`.
- La arquitectura de cada motor: su `docs/` (drvarma:
  `engines/drvarma/docs/DESIGN-v5-ladder.md`).

## 1. La regla de oro

**El motor en C y su porte a Python son el mismo motor.** Tienen que dar los
mismos resultados. El C está homologado y tiene detrás estudios publicados;
ninguno de los dos se aparta del otro.

De ahí:
- un cambio en un motor empieza en el C y el porte se mueve con él;
- no se publica nada con errores conocidos;
- lo que funciona no se toca: un defecto latente en un motor probado no se
  arregla de paso. Se arregla lo que lo provoca (el escritor de un fichero,
  no el formato; el arranque, no el optimizador).

El optimizador (`qnewtopt.c`, `raxopt`) y la verosimilitud exacta
(`elfvarma.c`) son de J. A. Mauricio y no se modifican
(`engines/drtran/docs/OPTIMIZER_STOPPING_STUDY.md`,
`docs/ESTUDIO-escala.md`).

## 2. El orden de un cambio

1. **El estudio, antes de tocar nada.** Qué hace hoy el motor, a qué afecta
   el cambio (qué baterías, qué hermanos, qué GUI), y qué decisiones son del
   analista. Se enseña y se espera el visto bueno.
2. **El C, con la batería delante.** Si es un defecto: primero el parte con su
   repro, que sale con 1; después el arreglo, y el repro sale con 0.
3. **Todas las baterías que conducen ese motor**, no solo las suyas (sección
   4): `make check` en la raíz.
4. **CI en verde en las tres plataformas antes de fusionar.** No se da la
   orden de fusión con el CI en cola.
5. **Los hermanos en C**, si el cambio toca código compartido.
6. **El porte, rehomologado** contra el C: los mismos números.
7. **La GUI y los asistentes**, lo último.

Los cambios en un motor son deliberados y lentos a propósito. Un cambio de
valor por defecto o de fórmula no entra como efecto lateral de otra cosa.

## 3. Las referencias

Una referencia de una batería (`tests/*/ref`) fija la salida del motor. Se
regenera **solo** cuando el cambio de la salida es deliberado, y el PR dice
qué referencia se mueve, en qué y por qué. «La batería pasa tras regenerar»
no es una comprobación.

El banco de drvarma (`engines/drvarma/tests/banco`) nació como la salida de
la 0.4.1 y se ha regenerado seis veces a propósito: ya no es esa salida.

Lo que no tiene un óptimo bien definido no se fija a seis decimales: un
modelo sobreparametrizado acaba en un punto que depende de la trayectoria y
se mueve con cualquier cambio de semilla. En `tests/rutas` esos casos van
marcados `libre`.

## 4. Qué baterías correr

`make check` en la raíz las corre todas y es lo que se pasa ante cualquier
cambio en un motor. Lo que sigue es para saber qué se está moviendo.

**Quién conduce cada motor desde fuera.** Un cambio en drvarma no lo ven solo
sus baterías:

| motor | lo conducen además |
|---|---|
| drvarma | `gui/sim`, `gui/atsw-cli`, `lib/partida`, `lib/outvarma`, `lib/sistema`, `mcp/atsw_mcp/tests` |
| drtran | `gui/drtran`, `gui/atsw-cli`, `mcp/atsw_mcp/tests` |
| fue | casi todo |

`gui/sim` compila `nlatools.c` y `diagnose.c` de drvarma; `gui/drtran` y
`gui/atsw-cli`, los de drtran. Tocar esos ficheros cambia la GUI.

**Qué motores enlazan cada librería compartida** (de los `Makefile`):

| librería | motores |
|---|---|
| `lib/optim` (la búsqueda en línea) | drvarma, drtran, drvec, fue, fuf |
| `lib/fuedet` (el filtro determinista) | drvarma, drtran, drvec, fue, fuf; y una copia en el porte de fue |
| `lib/diagmv` (la diagnosis multivariante) | drvarma, drtran, drvec |
| `lib/lik` (la verosimilitud, elf o Shea) | drvarma, drtran |
| `lib/fuepre`, `lib/prewhiten` (el lector y el molde univariante) | drvarma, drtran; drvec lleva copia propia del lector |
| `lib/dates` | drvarma, drtran, fue, fuf, fug |

`lib/optim` y `lib/diagmv` no tienen prueba propia: las prueban las baterías
de los motores.

**Las baterías de drvarma:**

| batería | qué | dónde corre |
|---|---|---|
| `tests/banco/banco.sh` | la vía `.inp` (obsoleta), byte a byte | `make check`, CI |
| `tests/escalera/pruebas.sh` | la escalera: la puerta, las rutas, la forma echelon, los números del porte | `make check`, CI |
| `lib/outvarma/run_test.sh` | el lector de la salida | `make check`, CI |
| `tests/rutas/rutas.sh` | las rutas U y V, la forma echelon, los tres caminos del mismo modelo: unas 300 estimaciones, veinte minutos | a mano, **antes y después de todo cambio en el motor** |

## 5. Los hermanos no son iguales

drvarma, drtran y drvec comparten origen, no código. Hoy solo `elfvarma.c` es
idéntico en los tres.

| | drvarma | drtran | drvec |
|---|---|---|---|
| errores estándar por defecto | fdhess | fdhess | BFGS (`-fdhess` a petición) |
| `gradtol` | 1e-7 | 1e-7 | 1e-5 |
| `qnewtopt.c`, líneas de código distintas de drvarma | — | 84 | 51 |
| `drvmlest.c` | — | 10 | 154 |
| `nlatools.c` | — | 16 | 0 |

Las cifras son de noviembre de 2026; se miden con `diff` antes de afirmar que
un arreglo «vale para los tres».

## 6. La homologación con los portes

| motor | porte | oráculo y registro |
|---|---|---|
| drvarma | drvarma-python | el C de este repositorio. La batería de la escalera fija los números del porte; el porte fija los del C (`docs/ARCHITECTURE.md` §8) |
| drvec | drvec-python | copia del C con sha por fichero y el commit anotado (`csrc/VENDORED.json`): el modelo a seguir |
| drtran | drtran-python | sus pruebas apuntan todavía al repositorio archivado: pendiente |
| fue | fue-python | contra DRVUS y fue 1.13.1: pendiente de apuntar aquí |

Cuando el C de un motor cambia, el porte se rehomologa antes de publicar
ninguno de los dos, y queda anotado contra qué commit del C.

## 7. Los partes

Todo defecto va con su repro: una carpeta `bugs/BUG-NNNN-repro/` con los datos
y un `repro.py` que sale con 1 mientras el defecto esté abierto, con 0 cuando
está arreglado y con 2 si el caso ya no vale.

**La numeración no es única, y hay que decir de quién es el número.** Cada
motor, cada librería y cada porte llevan la suya: «BUG-0001» nombra nueve
defectos distintos, y drvarma tiene un BUG-0001…0007 en el C y otros en el
porte. Al citar un parte se escribe siempre el dueño: «drvarma BUG-0006»,
«drvarma-python BUG-0027», «lib/prewhiten BUG-0002».

## 8. Lo que queda por ordenar

- Unificar la numeración de los partes, o al menos un índice común.
- Que los portes de drtran y fue apunten al C de este repositorio.
- Un paso de rehomologación en el protocolo de publicación
  (art-python `docs/PUBLISHING.md`).
- Una prueba propia para `lib/optim` y `lib/diagmv`.
