# sim_gui — los sistemas simultáneos (SIM)

El GUI de VARMA (drvarma) y, más adelante, de VEC (drvec). El diseño está en
`docs/DISENO-sim.md`: un solo GUI para los dos motores, como sima, porque
VARMA o VEC se decide dentro del análisis y no al entrar.

    sim_gui [--proyecto FICHERO [--caso C [--modelo c]]] [FICHERO.pre ...]

Parte de ficheros de fue ya hechos (el modelo de cada serie se hace en US) y
no transforma datos. Siete páginas, como drtran_gui, y R · Rango, que solo
aparece con series I(1):

| página | estado |
|---|---|
| 1 · Series | hecha (S2): los ficheros en orden (el de Cholesky), la ventana común |
| 2 · Puerta | hecha (S2): drvarma con p = q = 0 y Σ diagonal; pasa, falla, o el fichero no es un óptimo; la pregunta del rango |
| R · Rango | hecha (S10b): con dos o más series I(1) (d = 1, D = 0), la prueba del rango de cointegración con drvec (`-lrtest`, con bootstrap si hay réplicas, con su avance y «Detener»); las tablas por rango y por comparación, y el rango que propone la regla de sima. El caso mixto (alguna I(0)) lo dice y lo deja a sima |
| 3 · Identificación | hecha (S3): la rejilla m × m de la CCF de los residuos (las del `.out` de la puerta), la CCF entera al pulsar, los enlaces propuestos, «Al modelo» |
| 4 · Modelo | hecha (S3): p, q, Σ, un enlace por par ordenado, opciones avanzadas; la orden de drvarma, antes de lanzar |
| 5 · Estimación | hecha (S4): lanza drvarma con el modelo de la 4; cómo acabó (convergió, pared MA, sin converger, falló); con un sistema, cada estimación es un modelo del caso con su padre |
| 6 · Diagnosis | hecha (S4; Estructura en S7: Simplificar con `-zero`, «¿Es una red?» y «Pasar a MT»): la LR contra los univariantes, el optimizador, las raíces, Hosking, Jarque-Bera, los residuos de cada serie, los parámetros, Σ y el .out |
| Ver modelo ▾ | hecho (S4b): la página de cada serie, la de fue (`lib/equation`), con los términos cruzados en la línea del ruido |
| 7 · Previsión | hecha (S5): la previsión de cada serie en su nivel y su gráfico; la evaluación fuera de muestra contra los univariantes (la misma ventana); la respuesta al impulso y la descomposición, del .out de la estimación |

Las páginas que faltan dicen qué harán y qué piden.

`make check` (o `sh tests/run_tests.sh`) prueba `lib/sistema` y la ventana
conducida desde el código, con el drvarma y el drvec del árbol y un drvarma
falso cuya puerta falla. Sin servidor gráfico la ventana se salta con una
nota.

Va en los paquetes desde S9.
