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

---

## `mmdiag.<i>.inp` / `.pre` — el peldaño diagonal

Los mismos datos, pero para `2 1 0 -case 1 -diagar -diagma -diagcov`: los dos
componentes de Ȳ, que con r = 0 son ∇log muskrat y ∇log mink. Misma procedencia:
los escribió `drvec -writeinp` y los estimó `python -m fue`.

Sostienen los **dos contratos de la escalera** en el bloque 5 de la batería
(`LADDER_AS_OPTIMISATION.md` §2.1 y §3): la identidad de cruce —la conjunta
evaluada en estos valores debe ser la suma de las univariantes— y el certificado
de optimalidad —ajustar no puede dar menos que evaluar, y la brecha es cero si y
sólo si estos ficheros son óptimos univariantes—.

La tolerancia de esas comprobaciones es 1e-4 y no menor por una razón del
formato: **un `.pre` guarda sus coeficientes con `%.6f`**, y ese redondeo es lo
que acota lo afilado que puede ser el certificado.
