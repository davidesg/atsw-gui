# Fallos conocidos — fuf

## BUG-0001 — el informe de previsión escala mal las variaciones y ERR

**Encontrado:** 2026-10-17, al planear el gestor de previsiones
(`docs/DISENO-gestor-previsiones.md`), que va a leer la columna ERR.

### Síntoma

1. **Con λ = 1 (niveles)** la variación sale multiplicada por 100, como si la
   serie fuera en logaritmos. En `CPI_USA_model_AR` el índice pasa de 256.974
   a 257.971 (+0.997) y la columna dice **99.7**; la anual, 625.9.
2. **La columna ERR** de las observaciones no está en la escala de la σ de la
   entrada. En `IPC_ES_m01_ar1` (logaritmos), σ = √0.0627 = 0.25 y ERR llega
   a **100.4**; en `CPI_USA_model_AR`, a miles.

Con logaritmos las variaciones sí salen bien (−0.98 % en 1/2020).

Es la misma familia que el σ_w del gráfico de residuos (PR #46): una cifra que
supone logaritmos y no mira λ ni el refactor.

### Por qué importa

El gestor de previsiones lee los errores de los motores en vez de calcularlos
(decisión del analista). Con ERR mal escalado leería números falsos.

### Reproducción

`bugs/BUG-0001-repro/repro.py`: sale con 1 mientras la variación en niveles no
sea la diferencia de niveles o |ERR| supere diez veces la σ.
