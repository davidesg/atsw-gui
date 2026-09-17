# mtram — el GUI de drtran

Modelos de **función de transferencia** y **redes** de transferencias.

Parte de **`.pre` ya estimados**, no de datos crudos, y es deliberado: dejar
cargar un CSV aquí sería saltarse el escalón univariante, que es el que la
escalera certifica. El modelo de cada serie se hace con fue; aquí se cruzan.

## Lo que ya hace

**Las series y su orden.** El orden no es cosmético: la primera es la salida
(Y), y ese orden **es el índice** al que se refieren `q[i,j]`, `phi_i`,
`theta_i` y `mu[i]` en el `.cns`. El estudio de la escalera midió lo que cuesta
equivocarse —permutar dos series mueve la verosimilitud en 35,6 y cambia de
signo una covarianza, sin un solo aviso— así que aquí el orden se ve y se
cambia a la vista.

**La ventana muestral común.** El motor **no recorta por fecha**: compara
`nobs` y nada más. Dos series de la misma longitud y distinta fecha de inicio
se estiman desalineadas y en silencio — medido: catorce años de desfase cambian
la verosimilitud en 31 unidades y drtran sale con 0. Aquí se ve la intersección
de los calendarios y cuántas observaciones pierde cada serie.

**La compatibilidad de operadores.** Si algún par cruza operadores distintos,
el motor pasa del cast empotrado al cast por resta — y esas dos verosimilitudes
no son comparables entre sí. El motor lo dice por `stderr` cuando ya has
lanzado; esto lo dice antes.

## Lo que no tiene lector propio

**No hay un lector del `.pre` en este GUI.** Se enlaza el del motor,
`engines/drtran/src/fue_pre_reader.c`, tal cual. Lo que ése acepte es lo que
acepta drtran, **por construcción y no por parecido**.

El estudio del contrato contó **seis** implementaciones del formato
univariante, y cada divergencia que encontró salía de que el mismo fichero
viviera en dos sitios. Aquí no se abre la séptima.

Por la misma razón, la comparación de operadores llama a `operators_differ_tm`,
que vive en el propio lector del motor: es **la misma** comparación que hace
drtran para decidir el cast, no una parecida.

## El banco

    make check

Contrasta lo que el GUI dice con lo que dice drtran sobre los mismos ficheros
—el m6 de Relloso—: la fecha de inicio contra el `Start:` del `.out`, y que
ningún par salga incompatible, porque drtran estima ese caso con el cast
empotrado.

## Lo que falta, por capas

Cada una es ejecutable y comprobable contra el motor por sí sola:

1. ~~series, ventana común, operadores~~ **hecho**
2. el escalón diagonal (`-0`), que es lo que valida las tres anteriores: tiene
   que reproducir a fue corrido por separado sobre cada serie
3. los enlaces, con la CCF preblanqueada para identificar (b, r, s)
4. el editor del `.dag`, con detección de ciclo en vivo
5. el editor del `.cns`, con las ranuras del modelo
6. previsión, ventana fija y evaluación fuera de muestra
