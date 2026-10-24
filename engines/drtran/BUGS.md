# Fallos conocidos — drtran (el motor en C)

La numeración sigue la de drtran, que hasta ahora se llevaba en
`drtran-python/docs/BUGS.md` (el último allí es BUG-57). Los fallos del motor
en C que se encuentran en este repositorio se apuntan aquí, con su repro en
`bugs/BUG-NNNN-repro/`.

## BUG-0058 — con la media del IPC libre, drtran puede no acabar nunca

**Encontrado:** 2026-10-24, con dos `drtran` huérfanos de las capturas del
manual (`docs/manual/capturas/capturas.sh`, escena `05-drtran.txt`): ~5 h cada
uno al 88 % de CPU. Lo normal en ese caso es **menos de un segundo**.
**Dónde:** `src/nlatools.c:200-204` (`choldcp`), alcanzado desde
`src/elfvarma.c:77` con una Q infinita que construye `src/tran_shootx.c:473`.

### Síntoma

La corrida no acaba ni escribe nada: un núcleo al 100 % para siempre. Con
`gdb`, la pila es siempre la misma, y el contador de programa no se mueve:

    choldcp (nlatools.c:200)  <- elf (elfvarma.c:77)  <- varma_lik (lib/lik/lik.c:107)
    <- objcfunc (drvmlest.c:325)  <- lnsrch (lib/optim/lnsrch.c:69)  <- raxopt  <- est

con `*d1 = inf`, `maxoffl = inf`.

### Por qué

1. El primer paso de cuasi-Newton es enorme (`maxstep` ≈ 7000; `dk` de −3752
   en un AR y de +3021 en el último parámetro) y `lnsrch` prueba primero el
   paso entero.
2. El último parámetro es `log(var_2/var_1)` de la Q (`tran_shootx.c`, paso
   5): pasa de ≈ 7 a ≈ 3000 y `var[2] = exp(x)` da `inf`. El cast no rechaza
   una Q no finita.
3. `elf` factoriza la Q con `choldcp`. La comprobación [1] no ve el `inf`, la
   diagonal sale `inf` y el reescalado del determinante
   `while (fabs(*d1) >= 1.0) { *d1 *= 0.0625; *d2 += 4.0; }` no termina:
   `inf * 0.0625 = inf`.

El tope de iteraciones (500) no sirve: el bucle está **dentro de una sola
evaluación** del objetivo. La guarda de `objcfunc` contra un objetivo no
finito (`drvmlest.c:332`) y la de `lnsrch` llegan tarde: nunca hay objetivo.

### Cuándo pasa

El caso IPC ← WTI del manual (v0.2.0), con los `.pre` de las escenas: el IPC
lleva μ libre (0.001545), el WTI μ fija en 0.

- **Estimación conjunta con una sola ω** (b = 0, r = 0, s = 0): cuelga. Es la
  especificación que sale al quitar la ω del retardo 1 a la que propone la
  CCF (b = 0, r = 0, s = 1), o al quitar el denominador a la del manual.
- **El baseline** (`-0`, «Calcular baseline») cuelga si el WTI lleva también
  μ libre (la regla intermedia del 24-oct, ya retirada: el primer huérfano) o
  con las medias fijadas en el `.pre` (`-M`, la casilla de Estimación).
- Con la μ del IPC fijada en 0 (flag 0) no cuelga ninguna de 162
  combinaciones de (b, r, s), `.pre` del WTI y `-M`. Lo que dispara el paso
  enorme es arrancar con la μ del IPC distinta de cero; la especificación del
  manual (b = 0, r = 1, s = 0) acaba en 0.2-0.9 s.

La `-f` (previsión) y la evaluación recursiva (`-estwin`) estiman en la misma
pasada, así que cuelgan con la misma especificación.

### Arreglo propuesto (no hecho)

Dos guardas, las dos baratas, y conviene poner las dos:

- `choldcp`: si algún elemento de la diagonal no es finito, `*ifault = 1` y
  volver. Así `elf` devuelve `ifault = 1` y `objcfunc` rechaza el punto (1.0),
  que es lo que ya hace con una Q que no es definida positiva. Y en el
  reescalado, salir si `*d1` no es finito o es 0 (el mismo cuelgue con
  `*d1 = 0` por subdesbordamiento).
- `tran_shootx.c`, paso 5: si `var[i]` no es finito (o `x` pasa de un tope,
  p. ej. |log var| > 700), `*ifaultx = 1`: el punto es inadmisible.

Y, aparte del cuelgue, el paso enorme dice que la escala inicial del hessiano
de `raxopt` no casa con estos parámetros; un `maxstep` más pequeño haría que
la búsqueda lineal no saliera tan lejos. Eso cambiaría trayectorias y
goldens; las dos guardas no cambian ninguna corrida que hoy acabe.

El mismo `choldcp`, con el mismo bucle, está en `engines/drvarma`,
`engines/drvec`, `engines/fue` y `engines/fuf` (`src/nlatools.c`): conviene
poner la guarda en todos.

### Reproducción

`bugs/BUG-0058-repro/repro.py`: corre las dos especificaciones que cuelgan
y sus controles (el IPC sin μ) con un límite de 60 s (`LIMITE`); sale con 1
mientras alguna no acabe.
