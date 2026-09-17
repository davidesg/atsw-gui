# `ccfplot` — la CCF bidireccional

El gráfico de identificación de la función de transferencia.

## El formato no es una elección

Viene de **dos prototipos aprobados por Treadway que coinciden entre sí**:

| | dónde |
|---|---|
| GraphMaker | `SRC/GraphMaker/Projects/graphmakertri2/ccfgrafico.cpp` |
| drvus | `SRC/drv4.040804/drvus/ccf2_1.eps`, con su `ccf.c` |

De ahí sale todo lo de aquí: panel ancho y bajo de 360×126 puntos, retardos de
−k a +k **simétricos**, barras, bandas a ±2/√N, una vertical discontinua en el
retardo cero, y el estadístico arriba a la derecha. Los retardos por defecto
son los de GraphMaker: **7** al año, **15** al trimestre, **8–39** al mes.

## Por qué bidireccional

Los dos lados dicen cosas distintas, y es **todo** el interés del gráfico:

    k > 0    la influencia de la ENTRADA sobre la SALIDA — la transferencia
    k < 0    la influencia de la salida sobre la entrada — si ahí hay algo
             que no sea ruido, la entrada no es exógena y el modelo de
             transferencia no se sostiene

Por eso el título pone **la entrada primero**, `entrada − salida`. Es el
convenio de GraphMaker y su código deja escrita la razón: *«en los retardos
positivos representamos la influencia de la 2ª serie sobre la 1ª»*.

## El estadístico

El portmanteau multivariante de **Hosking** (1980):

    Q = N · Σ_k  tr( C_k' C₀⁻¹ C_k C₀⁻¹ )

**drtran ya lo tiene** — `engines/drtran/src/diagnose.c::hosking_test` — y ya
calcula la CCF en los dos sentidos (`diagnose.c:419-420`). Aquí no se
reimplementa nada de eso: lo que faltaba era el dibujo.

Grados de libertad: `m²(k − p − q)`, que con m = 2 es `4(k − p − q)`.

**Se etiqueta `P` y no `Q`**, siguiendo a GraphMaker, que da la razón en su
código: *«la llamamos P para no confundirlo con el Q de Ljung-Box»*. El
prototipo de drvus la etiqueta `Q`; es la única diferencia entre los dos
aprobados.

## Ejemplo

`ejemplo_EI_EP.eps` — EI contra EP del m6 de Relloso, con los números que da
drtran sobre esos mismos `.pre`: `P(60) = 944.3`.
