# sim_gui — los sistemas simultáneos (SIM)

El GUI de VARMA (drvarma) y, más adelante, de VEC (drvec). El diseño está en
`docs/DISENO-sim.md`: un solo GUI para los dos motores, como sima, porque
VARMA o VEC se decide dentro del análisis y no al entrar.

    sim_gui [--proyecto FICHERO [--caso C [--modelo c]]] [FICHERO.pre ...]

Parte de ficheros de fue ya hechos (el modelo de cada serie se hace en US) y
no transforma datos. Siete páginas, como drtran_gui:

| página | estado |
|---|---|
| 1 · Series | hecha (S2): los ficheros en orden (el de Cholesky), la ventana común |
| 2 · Puerta | hecha (S2): drvarma con p = q = 0 y Σ diagonal; pasa, falla, o el fichero no es un óptimo; la pregunta del rango |
| 3 · Identificación | hecha (S3): la rejilla m × m de la CCF de los residuos (las del `.out` de la puerta), la CCF entera al pulsar, los enlaces propuestos, «Al modelo» |
| 4 · Modelo | hecha (S3): p, q, Σ, un enlace por par ordenado, opciones avanzadas; la orden de drvarma, antes de lanzar |
| 5 · Estimación, 6 · Diagnosis | S4 |
| 7 · Previsión | S5 |

Las páginas que faltan dicen qué harán y qué piden.

`make check` (o `sh tests/run_tests.sh`) prueba `lib/sistema` y la ventana
conducida desde el código, con el drvarma del árbol y uno falso cuya puerta
falla. Sin servidor gráfico la ventana se salta con una nota.

Todavía no va en los paquetes: entra cuando cubra al GUI viejo de drvarma
(fase S9).
