# Fixtures de F2 — el puente con la suite

`mmres.<i>.inp` y `mmres.<i>.pre`, para `mink_muskrat` con p=2, q=1, r=1, `-case 2`.

**Procedencia, que es lo que hace legítimo el `.pre`:**

1. `drvec ... -case 2 -writeres <pre>` escribió los dos `.inp` — un residuo de la
   regresión condicional por ecuación, pidiendo un MA(1) con media libre.
2. `python -m fue <pre>.<i> eml` los estimó y escribió los `.pre`.

O sea que **el `.pre` lo escribió quien estimó**, que es la regla del formato: un
`.pre` afirma que sus valores son un óptimo, y esa afirmación sólo la puede hacer
el programa que optimizó. `drvec` escribe `.inp` y nunca `.pre`.

Se versionan para que la batería no dependa de tener `fue` instalado. Si hay que
regenerarlos, hay que repetir los dos pasos de arriba — y entonces el valor de
oro de `run_tests.sh` (bloque 5) hay que volver a medirlo, porque un valor de oro
sólo vale para la entrada exacta con la que se midió.
