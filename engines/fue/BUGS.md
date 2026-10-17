# Fallos conocidos — fue

## BUG-0001 — el .pre no es el óptimo exacto: va redondeado

**Encontrado:** 2026-10-17, revisando drtran_gui (el `.pre` del caso IPC ← WTI
tardaba 10 iteraciones al volver a estimarlo).
**Dónde:** `src/fue.c`, la escritura del `.pre`: los AR, MA y δ con `%.4f`,
los ω (deterministas) con `%.6f`.

### Síntoma

Estimar sobre el propio `.pre` vuelve a iterar: `IPC_ES_m01_ar1` converge en
22 iteraciones desde el `.inp` y en **10** desde su `.pre`. La verosimilitud y
los números impresos coinciden (`logelf −7.4026486474` en los dos), pero el
`.pre` no es el punto óptimo: es su redondeo. Un coeficiente determinista
pequeño (0.000535) queda con tres cifras significativas.

### Por qué importa

- Rompe la regla del convenio: «corre fue sobre un `.pre` y los números no se
  mueven» (`docs/LADDER_AS_OPTIMISATION.md` de drtran).
- drtran **hereda** el redondeo cuando mantiene fijos los parámetros del
  `.pre` (`-N`, `-X`, `-D`, `-E`, `-M`), y la puerta diagonal no cierra
  exacta.

### Arreglo propuesto (no hecho)

Escribir cada número con lo justo para leerse de vuelta exacto (`%.17g`, o el
más corto que vuelve igual, como `coef_linea` en `lib/inpdet`). Cambia los
goldens de los `.pre`.

### Reproducción

`bugs/BUG-0001-repro/repro.py`: sale con 1 mientras estimar sobre el `.pre`
cueste más de una iteración.
