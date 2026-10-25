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

### Qué se hace

Nada en `fue.c`, `inpcheck.c` ni `fuf.c`. El arreglo va en el escritor:
drvarma BUG-0001. Todo escritor nuevo de `.inp` o `.pre` debe mantener las
cinco líneas libres en 88 caracteres o menos.

### Reproducción

`bugs/BUG-0001-repro/repro.py`: sale con 1 mientras estimar sobre el `.pre`
cueste más de una iteración.

## BUG-0002 — una línea de la cabecera de 89 caracteres o más cuenta como dos (no se arregla: es el formato)

**Encontrado:** 2026-10-25, en la batería de la escalera de drvarma
(`tests/escalera/pruebas.sh`, 10c): «la escalera y la via .inp no llegan al
mismo VAR(1)» con un binario construido desde un árbol con cambios sin
confirmar. Lo dispara drvarma (su BUG-0001).
**Decisión (2026-10-25, el analista): no se toca fue.** Lleva muchos años
leyendo así sin que ningún fichero real lo pise; cambiar un lector que
funciona por un fallo latente arriesga más que el fallo. El límite pasa a
ser parte del formato: **cada línea libre de la cabecera tiene como mucho
88 caracteres**, y lo cumplen los escritores. Los del paquete escriben la
cabecera fija de siempre (`gui/fue/src/file_io.c`, «Input file for program
FUE»); el único que metía texto libre es `drvarma -split`, que no va en el
paquete.
**Dónde:**
- `src/fue.c:319-323` ([3.0], «the first six lines (may contain anything)»):
  cinco `fgets( dumstrg, MAXSTR, inputv )`, con `MAXSTR` 90
  (`include/fue.h:38`, `dumstrg = NEW_STR( MAXSTR )` en `fue.c:142`).
- `src/inpcheck.c:21` (`LABEL_MAX 90`), `:62-66` (`label()`) y `:204-205`
  (el bucle de las cinco líneas): el validador copia la lectura de fue a
  propósito («accepts exactly what fue reads well»), y con ella el límite.
  Es el validador que usan fue, su GUI y la escalera de drvarma
  (`inp_check_fue`, `escalera.c:929` y `:997`) para decidir si un fichero es
  un modelo univariante de fue.
- El mismo patrón en `../fuf/src/fuf.c:263-267` (`MAXSTR` 90).

### Síntoma

Un `.inp` (o `.pre`) cuya cabecera trae una línea de 89 caracteres o más,
sin contar el salto, se rechaza:

    Error in the input file h120.inp, line 6: the frequency must be 1, 4, 12 or 'number' (it is '**')

El mensaje no dice la causa: la frecuencia está bien escrita en su sitio.
Con la línea de 88 caracteres el mismo fichero se lee y se estima.

### Por qué

`fgets( s, 90, f )` lee como mucho 89 caracteres. Si la línea es más larga
no llega al salto: la «línea» 4 se lee en dos llamadas, la quinta lectura
se come el resto de la 4, y la etiqueta de la frecuencia ([3.1]) recibe la
línea en blanco que sigue al banner. El `fscanf( "%s" )` de la frecuencia
lee entonces «**», el comienzo de la etiqueta. El formato dice que esas
cinco líneas son libres y no pone límite de longitud; los lectores que
buscan el separador «requency» en lugar de contar líneas
(`lib/fuepre/fue_pre_reader.c:424-433`, con `FUEPRE_LINE` 512) leen el
mismo fichero sin problema.

### Cuándo pasa

Con cualquier escritor que ponga texto de longitud libre en la cabecera. El
caso medido es `drvarma -split` (`../drvarma/src/split.c:87`): «* Split
from <ruta> by drvarma <versión>». Con la versión de hoy, `git describe`
da la forma con etiqueta (`v0.2.0-4-g3f56422f48`, 20 caracteres, no los 10
del hash que suponía `docs/DESIGN-v5-ladder.md` §6), y la línea de la
batería mide 84; con `-dirty` mide 90 y el fichero ya no se lee. Una ruta
larga (absoluta, por ejemplo) lo dispara sin `-dirty`.

Las notas abiertas «drvarma split header longer than 80 chars» y la del
«-dirty» son este mismo fallo; el umbral medido es 89, no 80. La de
«drvarma.c:176 overflow» es otro (drvarma BUG-0002).

### Qué se hace

Nada en `fue.c`, `inpcheck.c` ni `fuf.c`. El arreglo va en el escritor:
drvarma BUG-0001. Todo escritor nuevo de `.inp` o `.pre` debe mantener las
cinco líneas libres en 88 caracteres o menos.

### Reproducción

`bugs/BUG-0002-repro/repro.py`: deja medido el límite. Escribe el mismo
`.inp` con la línea 3 de 88 caracteres y de 120; sale con 0 mientras fue se
porte como dice el formato (88 se lee, 120 no), con 1 si cambia, y con 2 si
falla el de 88. No depende del estado de git.
