# Fallos conocidos — atsw-cli

Cada fallo lleva su `bugs/BUG-NNNN-repro/repro.py`, que sale con 1 mientras
está abierto y con 0 cuando está arreglado.

## BUG-0001 — `especifica --intervencion` repite una intervención que el .inp ya trae

**Encontrado:** 2026-11-18, al grabar la parte de la terminal del vídeo del
asistente.
**Arreglado:** 2026-11-18, en `especifica` (`src/main.c`) y en el texto de
`write_inp` de atsw-us.

### Síntoma

Se le pide al asistente «un hijo de m02 sin el escalón de octubre de 2007».
Deriva el hijo con `new_version`, que copia el `.pre` de m02 con sus dos
escalones, y llama a `write_inp` con `interventions=["step 9 2012"]`, porque
cree que la lista **sustituye** a la que hay. `especifica` la **añade**, así
que el hijo queda con `step 9 2012`, `step 10 2007` y otra vez `step 9 2012`.
fue lo acepta y lo estima: dos columnas iguales, un modelo singular (el
hessiano no es definido positivo) con la misma verosimilitud que el padre.

### Por qué

`especifica` pasaba las intervenciones a `id_anade` sin mirar las que el
`.inp` ya traía. Con los armónicos y Semana Santa sí lo miraba («los que el
.inp ya trae no se repiten»), y con las intervenciones no. La ayuda de
`write_inp` no decía que se añaden.

### Arreglo

Como en art-python, que tuvo el mismo problema (su BUG-0162):

- una intervención **igual** a una que el `.inp` ya trae, o repetida en la
  orden, se rechaza, y el `.inp` queda como estaba. El mensaje dice que las
  intervenciones se añaden y que, para un modelo sin una de ellas, se parte
  del `.pre` de antes de esa intervención;
- una de **otra forma en la misma fecha** se añade, con el aviso de art: las
  dos se reparten el efecto, y el síntoma es un ω que deja de ser
  significativo;
- la ayuda de `write_inp` (atsw-us) dice que las intervenciones se añaden.

Reformular una intervención en un solo paso, como el `rehacer=True` de
`suggest_intervention_form` en art, queda para cuando esa herramienta pase
al asistente (`docs/ESTUDIO-art-python-asistente.md`).

### Reproducción

`bugs/BUG-0001-repro/repro.py`: un proyecto con el golden RIPC.2 de fue,
cuyos datos traen `step 2 2002`; un modelo nuevo, y `especifica
--intervencion "step 2 2002"`.
