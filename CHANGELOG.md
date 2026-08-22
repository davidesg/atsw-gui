# Changelog — drvec

Los informes completos están en [`docs/DEVELOPMENT_RECORD.md`](docs/DEVELOPMENT_RECORD.md)
(qué se probó y qué se refutó) y en [`docs/HOMOLOGATION.md`](docs/HOMOLOGATION.md)
(las mediciones). Los defectos, en [`docs/BUGS.md`](docs/BUGS.md). Etiquetas de
publicación: `v*`.

## Sin publicar — 2026-08-22

### La entrada por `.pre`, que es la del conjunto (P9)

```sh
drvec s1.pre s2.pre ... sM.pre p q r [opciones]
```

Un modelo univariante por serie, como `drtran`. De cada `.pre` se toma la serie
con su `w = refactor·BoxCox(z)`, sus términos deterministas —**restados**, con
las fechas de esta muestra— y su calendario; los ficheros se alinean **por
fecha** y se usa la intersección. Antes había que dar tres pasos a mano:
exportar a un `.inp`, `-interv` y `-seed`.

Certificada por dos identidades, no por parecido: reproduce la ruta `.inp`
**byte a byte** desde `ESTIMATION SUCCESSFUL`, y con una determinista en juego
coincide **exactamente** con `-interv` sobre los mismos modelos. `-name NAME`
nombra los productos (`-m` está tomado: es el método de estimación).

### El idioma (P7)

`src/drvec.c` entero en inglés: 363 comentarios y ~40 mensajes. Hecho contra un
invariante —quitar los comentarios de las dos versiones tiene que dar ficheros
idénticos byte a byte— porque traducir 1 200 líneas dentro de 7 600 es como se
mueve una línea de código sin querer. `tools/check_language.py` lo mantiene,
sólo sobre `drvec.c`: los ficheros vendorizados son de su dueño.

### Sistema de versiones y de defectos (P6, ampliada)

- `docs/VERSIONS.md`: el razonamiento de la versión sale del código y pasa a
  donde un lector lo necesita, con la política, las condiciones para llegar a
  1.0, y la regla de que un defecto **encontrado** no retrasa una versión y uno
  **tapado** sí.
- `tools/check_version.sh` y `tools/check_bugs.py`, los dos en la batería: el
  número tiene que decir lo mismo en seis sitios, y cada defecto tiene que tener
  número, estado y **coste medido**.

Batería: 235 → 252.

## 0.9 — 2026-08-22

Primera versión numerada. **Por qué 0.9 y no 1.0**: el programa no mejora la
previsión de un ARIMA por serie sobre su banco (HOMOLOGATION.md §4t) y su
especificación por defecto cambió el 2026-08-20. Ninguna de las dos es un
defecto que arreglar —son el estado del conocimiento, medidas y escritas—, pero
tampoco son algo sobre lo que poner un 1.0. El razonamiento va escrito junto al
`#define`, en `src/drvec.c`.

### La interfaz (P1)

- Tabla de opciones única: la lista que el parser **acepta** y la que `usage()`
  **enumera** salen de la misma estructura, y la batería compara las dos. Antes
  `usage()` cubría 22 de 33 opciones.
- Validación antes de tocar nada: una opción desconocida o un argumento que no
  es un número se rechazan con el uso y código de salida 2, en vez de estimarse
  en silencio con `p = q = r = 0`.
- `--version`, y la versión en la primera línea del `.out`.

### La especificación por defecto (P4)

- El defecto pasa a ser `Θ = [T₁₁ T₁₂ ; 0 0]` (`-marow`), con `-mafree` para la
  clase libre de antes. En esta clase el Corolario 6.3 hace que la frontera de
  invertibilidad **no se pueda alcanzar**: `det Θ̃(1) = det(I_r − ΣT_k)` y la
  puerta que el motor ya aplicaba **es** la condición de admisibilidad.

### La previsión (P5)

- `-f H`: previsión en niveles con bandas, por un solo mapa de error que lleva
  el error acumulado de `∇Y₂` al bloque `Y₁` a través de `Y₁ = W − B₂′Y₂`.
- `-estwin E`: evaluación de origen móvil fuera de muestra, con los parámetros
  estimados **una vez** sobre `1..E` y congelados.
- El certificado de un paso: la recursión se comprueba contra los residuos que
  devuelve el motor, no contra sí misma.

### La salida (P6)

- **`<base>.forecast`** y **`<base>.recursive`**, el sistema de ficheros del
  conjunto (`drvarma` v.04.1): un fichero por producto, con su cabecera
  reproducible, sin que haya que nombrar la ruta. La previsión lleva ahora
  FECHA en cada fila. `-C FICHERO` pasa a ser una redirección.
- **Bloque de hipótesis por defecto**: Wald sobre la covarianza que la
  estimación ya calcula —exogeneidad débil fila a fila, exclusión de la relación
  de largo plazo fila a fila, y la dinámica corta— sin reestimar nada. `Λ = 0`
  se imprime **declarando que no es un contraste** (bajo esa nula `B` no está
  identificada; el instrumento es `-lrtest -bootstrap`).
- `LICENSE`, `CITATION.cff`, este fichero, `docs/BUGS.md`, `make install`
  con `PREFIX` e integración continua.

### Arreglos

- **`xitol` sin inicializar en la evaluación de origen móvil** (BUG-14): `elf`
  recibía como tolerancia de truncamiento lo que hubiera en la pila. Las tres
  columnas de §4t se volvieron a medir con el arreglo y **reproducen la tabla
  publicada dígito a dígito**, así que la medida se sostiene.
- **`refactor` del `.pre` en `-interv`** (BUG-15): las deterministas se restaban
  sin deshacer el reescalado ×100 de la suite, y con eso el rango medido de los
  tres IPC salía `r = 1` donde es `r = 0`.
- **BUG-13**, la χ² del motor con `df ≥ 30`: la cola salía invertida. Arreglado
  en las cuatro copias de `nlatools.c` y verificado contra GSL.
- `nlatools.c` vuelve a ser idéntico a la copia canónica de `drvarma`: cero
  líneas de divergencia, y con ello cinco fugas menos.
