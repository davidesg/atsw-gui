# Fallos conocidos — drtran_gui

## BUG-0001 — al cerrar drtran_gui, el motor sigue corriendo

**Encontrado:** 2026-10-24, con dos `drtran` huérfanos (padre: el `systemd`
del usuario) de las capturas del manual: las escenas acabaron, el GUI se
cerró, y cada `drtran` siguió ~5 h al 88 % de CPU (el motor colgado era
drtran BUG-0058).
**Dónde:** `lib/engine/engine.c` (`lanza`, `engine_stop`) y
`gui/drtran/src/main.c`, `gui/drtran/src/estima.c:726`.

### Síntoma

Estimar y cerrar la ventana mientras drtran corre: la ventana se va, el
programa sale con 0, y `drtran` sigue vivo, reparentado a init. Lo mismo si
drtran_gui se cierra porque se fue la madre (`--madre`, `sitio_vigila_madre`),
si lo mata una señal (el `timeout` de `capturas.sh`) o si se cae broadwayd.

### Por qué

- `engine_start` (`lib/engine/engine.c:399`, vía `lanza`) lanza el hijo con
  `g_spawn_async_with_pipes` y guarda su PID en el `Run`, pero **nadie lleva
  la cuenta de los trabajos vivos** y no hay nada que los pare al salir: ni
  un `atexit`, ni un registro, ni `PR_SET_PDEATHSIG` en el hijo.
- `engine_stop` (`engine.c:376`) existe, pero en drtran_gui solo lo llama el
  botón Parar (`estima.c:819`). Ni el cierre de la ventana, ni el `shutdown`
  de la `GtkApplication` (`main.c`), ni `se_fue_la_madre` miran si
  `m->est.corriendo`.
- Como los trabajos son de lib/engine, fue_gui y la madre tienen el mismo
  hueco con fue y art; con ellos se nota menos porque sus corridas duran
  décimas.

### Arreglo propuesto (no hecho)

En lib/engine, para que valga para todos los GUIs:

- una lista de los `Run` vivos (se añade en `lanza`, se quita en
  `on_child`) y `engine_stop_all()`, que manda SIGTERM a cada uno
  (`TerminateProcess` en Windows) y espera un momento a que salgan;
- llamarla en el `shutdown` de la aplicación de cada GUI (y en
  `se_fue_la_madre` antes del `g_application_quit`), y registrarla con
  `atexit` para los `exit()` directos;
- en Linux, además, `prctl(PR_SET_PDEATHSIG, SIGTERM)` en un
  `GSpawnChildSetupFunc`: cubre la muerte por señal del GUI, que ningún
  manejador ve. En Windows, un *job object* con
  `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` hace lo mismo.

Y en drtran_gui, si se cierra la ventana con una estimación en marcha,
preguntar (o al menos decir en la barra que se para).

### Reproducción

`bugs/BUG-0001-repro/repro.py`: pone drtran_gui con un `drtran` falso al lado
(apunta su PID y duerme 10 minutos), abre el caso C1 del proyecto de la
carpeta en un broadwayd propio (pantalla `ATSW_BROADWAY`, 89 por defecto),
pulsa Estimar y cierra la ventana con `escenas.c`. Sale con 1 mientras el
falso drtran siga vivo después de que drtran_gui salga. No depende de
BUG-0058. Necesita el árbol construido, broadwayd y gtk+-3.0.
