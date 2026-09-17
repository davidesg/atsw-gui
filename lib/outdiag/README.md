# `outdiag` — la diagnosis, leída del `.out`

## Aquí no se calcula nada

El motor ya hace toda la diagnosis, y la hace bien. Por cada serie de residuos:
media, desviación, asimetría, curtosis, el histograma con el **porcentaje
observado contra el esperado**, la ACF con el **Ljung-Box escalonado en la
columna derecha** y la PACF. Luego las correlaciones cruzadas entre residuos,
el portmanteau multivariante de **Hosking** y el **Jarque-Bera** multivariante.
Y por último, por cada enlace, la CCF entre el ruido estimado y la entrada
preblanqueada, **con su veredicto**.

Es, casi línea por línea, lo que el estudio de TASTE señaló como lo mejor de su
diseño —*la lectura pegada al gráfico*— ya implementado en el motor.

Lo que faltaba es que **2000 líneas de `.out` se conviertan en algo con lo que
se pueda decidir**. Eso es lo único que hace este módulo: leer.

## Se lee un informe, no se rehace un cálculo

Es distinto de los otros lectores de `lib/`. Con el `.inp`, el `.dag` y el
`.cns` se comparte el código del motor. Aquí no se puede: **el motor no emite
nada legible por máquina**, así que hay que leer texto formateado, que es frágil
por naturaleza.

La defensa es que la prueba se corre **contra un `.out` de verdad, recién
escrito por el motor en el mismo banco** (`gui/drtran/tests/run_tests.sh` estima
el m6 y le pasa su propio `.out`). Si el formato cambia, se sabe el mismo día.

## Lo accionable: dos diagnósticos opuestos

    significativo en k ≥ 0  →  falta estructura en LA TRANSFERENCIA
                               se arregla cambiando (b, r, s)
    significativo en k < 0  →  RETROALIMENTACIÓN: la entrada no es exógena
                               se arregla quitando el enlace, o subiendo al VARMA

Se arreglan de forma **opuesta**, así que confundirlos cuesta una tarde de
reespecificar lo que no tenía arreglo en este escalón. La pantalla los pone
delante y separados.

## Lo que salió en el m6, y por qué es buen caso de prueba

| enlace | transferencia | | exogeneidad | |
|---|---|---|---|---|
| EI | Q(15) = 5.26 | p = 0.9896 ✔ | Q(16) = 19.90 | p = 0.2246 ✔ |
| EC | Q(14) = 26.08 | **p = 0.0253 ✘** | Q(16) = 24.19 | p = 0.0854 ✔ |
| EU | Q(13) = 26.91 | **p = 0.0128 ✘** | Q(16) = 17.88 | p = 0.3310 ✔ |
| EC | Q(15) = 24.29 | p = 0.0603 ✔ | Q(16) = 21.95 | p = 0.1448 ✔ |

**Dos de los cuatro enlaces no son adecuados.** Si el lector dijera que los
cuatro están bien, no estaría leyendo el veredicto sino inventándolo — por eso
la prueba exige exactamente 2.

Y los dos contrastes globales van al revés: Hosking no rechaza (p = 0.6854,
ruido blanco) y Jarque-Bera sí (p = 0.0128, no normales). No es contradictorio:
incorrelación y normalidad son cosas distintas.

## El fallo que la prueba cerró

```c
sscanf( l, " %d observations:", &n1 ) == 1
```

devuelve 1 **en cuanto lee el número**, aunque el literal que sigue no case. Así
que cualquier línea que empezara por un número —el gráfico de la serie, el
histograma, la CCF— se colaba por ahí y pisaba `nobs`; y el histograma, que
empieza por `20 values outside…`, nunca llegaba a leerse. El literal se comprueba
aparte, y hay dos asertos que lo cierran.
