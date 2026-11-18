# El vídeo del asistente: la parte de la terminal

La parte de la madre (`escenas-asistente/`) enseña lo que la madre ve cuando
el asistente trabaja. Esta parte enseña el otro lado: el analista habla con el
asistente. **No lleva guion de escenas**: las respuestas de un LLM cambian de
una vez a otra, así que se graba de verdad y se queda la toma que salga bien.

## El proyecto de partida

```sh
bash docs/manual/video/terminal/proyecto.sh
```

Deja en `/tmp/curso-terminal/asistente` el proyecto en que acaba la parte de
la madre:

- serie `subyacente` (IPC subyacente del INE), muestra `2002-2019`, declarada
  por el analista;
- `m01`, del asistente: el modelo de las líneas aéreas. Residuos blancos, pero
  la diagnosis dice NO (curtosis 4.1);
- `m02`, del asistente, hijo de m01: AR(3)×SMA(1) con el escalón del IVA
  (9/2012) y el de 10/2007. Residuos blancos, curtosis 0.8, la normalidad
  falla por poco (JB p = 0.012). El φ₂ del AR tiene t = 1.23. **Elegido por
  el asistente**, con su razón;
- preparado para Claude Code (`.mcp.json`, `CLAUDE.md`, `AGENTS.md`).

El script imprime cómo abrir la madre con ese asistente. La ruta es neutra a
propósito: la terminal la enseña.

## La pantalla

La madre a la izquierda, con la ventana del asistente abierta
(Ctrl+Mayús+A); la terminal a la derecha, con letra grande. Antes de grabar,
comprobar que no sale nada personal en la terminal: ni la carpeta del usuario,
ni la cuenta del cliente (el saludo de Claude Code puede enseñarla), ni el
historial.

## Las preguntas

Tres tomas. Las dos primeras preguntan; la tercera hace. Se escriben tal cual,
sin prisa: el que mira tiene que poder leerlas.

### 1. Qué hay en el proyecto

> ¿Qué modelos hay de la serie subyacente, quién los hizo y cuál está elegido?

Lo que se espera: llama a `guion_map` y cuenta m01 y m02, ambos del asistente,
y m02 elegido por el asistente. La muestra 2002-2019 la declaró el analista.

### 2. Por qué ese modelo

> ¿Por qué m02 y no m01? ¿Hay algo en m02 que deba revisar yo?

Lo que se espera: compara las dos diagnosis (`diagnose` de cada una o
`full_report`). m01 tiene curtosis 4.1; las dos intervenciones la bajan a 0.8.
Debe decir lo que queda pendiente: la normalidad falla por poco (p = 0.012) y
el φ₂ del AR(3) no es significativo (t = 1.23).

### 3. Que lo pruebe

> Prueba un hijo de m02 sin el escalón de octubre de 2007 y dime si hace falta.

Lo que se espera: `new_version` (padre m02, con su razón), `write_inp` con solo
`step 9 2012` y `estimate_and_diagnose`. **En la madre aparece m03 «del
asistente»** en la barra de estado mientras la terminal contesta: es el plano
que une las dos partes del vídeo. La respuesta compara las log-verosimilitudes
(m02: 1060.34) y dice si el escalón de 2007 se queda. No debe elegir m03 sin
que se lo pidan: elegir es del analista.

### Cierre (opcional)

> Deja anotado en m03 lo que has visto.

`guion_annotate`: la nota queda en el proyecto, con su autor.

## Qué hacer si una toma sale mal

Rehacer el proyecto (`proyecto.sh` borra y empieza de cero) y repetir. Una
toma en que el asistente se equivoca y se corrige solo también vale: es lo que
pasa de verdad.
