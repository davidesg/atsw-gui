# `fuepre` — el `.pre` de fue, leído una vez

El lector del `.pre` y lo que se hace con el modelo leído. Vivía en
`engines/drtran/src/fue_pre_reader.c`. Sale aquí porque drvarma 5.0 también
lee los `.pre` (la escalera: fue → drtran → drvarma), y el criterio de `lib/`
es que un fichero que hace falta en dos sitios no se copia.

## Qué hay

| fichero | qué | lo enlazan |
|---|---|---|
| `fue_pre_reader.c/.h` | `read_fue_pre`, `free_fue_pre`, `build_det_component`, `operators_differ_tm` | drtran, GUI de drtran y sus pruebas, drvarma |
| | recuento y (des)empaquetado de los coeficientes **libres**: `n_*_free_params`, `pack_*`, `unpack_*` | drtran, drvarma |
| | `invalid_fixfreq`: un factor de frecuencia fija con c₂ ≥ 0 | drtran, drvarma |
| | `fuepre_check_alignment`: BUG-2 | drtran, drvarma |
| `fuepre_motor.c` | `unstable_delta`: el δ(B) determinista, con `chekma` | drtran, drvarma |
| `tusmodel.h` | `struct Tusmodel`, lo que el lector llena | los mismos |

`unstable_delta` va aparte porque `chekma` vive en `elfvarma.c`, con la
verosimilitud. El GUI de drtran y sus pruebas enlazan el lector sin el motor,
y no tienen por qué arrastrarlo.

## El empaquetado es un contrato

fue lleva un flag por coeficiente (`Ia1/Ia2/Ia1f`, `Im1/Im2/Im1f`,
`Imega/Ielta`). Una línea `0.0000  0` en el `.pre` es un coeficiente **fijo**,
no un valor inicial. El orden en que `pack_*` recorre esos flags es el orden
en que `unpack_*` los lee, y un segundo empaquetador que los recorriera en otro
orden no daría ningún error: daría parámetros cruzados. Por eso los dos
motores usan estos.

## BUG-2: se cruzan fechas, no posiciones

`fuepre_check_alignment(Ts, m, why, size)` exige la misma frecuencia y la
misma **fecha final** en `Ts[1..m]`. El cast conjunto alinea por el final y
recorta a la serie más corta. Eso es correcto con distintos d/D sobre la misma
ventana, y mudo cuando las ventanas son trozos distintos del calendario:
drtran cruzaba series con catorce años de desfase porque sólo comparaba nobs.

Es un **rechazo**, como `cast.check_alignment` de drtran-python. Recortar a la
ventana común de calendario cambia la muestra, y esa decisión corresponde a
quien construye los `.pre` en art.

## Lo que pone el anfitrión

Su `main.h`, con:
- `real`;
- `struct Tseries`, con `numbering` y `refactor`;
- `#include "tusmodel.h"`;
- los asignadores de nlatools (`vector`, `matrix`, ...);
- `chekma`, sólo si enlaza `fuepre_motor.c`;
- `Easter`.

`DateToObs` y `ObsToDate` vienen de `lib/dates`.

El lector tiene su **propio** tamaño de línea (`FUEPRE_LINE`, 512). Usaba el
`MAXSTR` del anfitrión, que vale 200 en drtran y 80 en drvarma: con 80, una
línea larga de un `.pre` se partía en dos lecturas y todo lo que venía detrás
se desplazaba un renglón.

## Reader fixes (2026-09-26)

Three defects that the study recorded in `docs/REGLAS-NO-ESCRITAS.md`, which
drvarma would otherwise have inherited:

- **A short data block is an error** (rule 16). The reader stopped at the end
  of the file and returned success, with the rest of the series left at zero.
- **The annual-difference factors line** must carry exactly `freq/2+1`
  integers (rule 30). With fewer, the pointer advanced by an uninitialised
  offset.
- **Deterministic keywords are whole words** compared with `strcmp`, as fue
  does (rule 50). A variable called `timeshift` was generated as a linear
  trend. `time` is no longer taken as `trend`: fue never accepted it, and no
  `.pre` in the repository uses it.
