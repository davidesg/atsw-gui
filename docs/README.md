# El estudio de atsw, y su banco de conformidad

Este repositorio guarda **el estudio** y **el banco de pruebas** de la suite
ATSW. No guarda el árbol de trabajo: `atws/` son 908 MB con GSL dentro, diez
repositorios anidados, los manuales y los binarios de Windows. Por eso el
`.gitignore` va al revés de lo normal — **se ignora todo y se deja entrar sólo
lo que se nombra**, que es la única forma de que nada se cuele por descuido.

---

## Los documentos

Se escribieron en septiembre de 2026, en seis fases. El plan
(`ESTUDIO-atsw-PLAN.md`) es el índice: sus cabeceras `FASE N — CERRADA`
resumen cada una en media página y dicen si vale la pena abrir el documento
largo.

| | qué contesta |
|---|---|
| **ESTUDIO-atsw-PLAN.md** | el plan, las decisiones tomadas, y el estado de las seis fases |
| **CONTRATO.md** | el contrato `.inp`/`.pre`/`.out` **tal como es**, no como se cuenta. Seis implementaciones, sus divergencias, y la propuesta para P1 |
| **REGLAS-NO-ESCRITAS.md** | 52 invariantes que el código impone y ninguna especificación menciona |
| **DISENO-editor.md** | el editor del `.inp` (implementado en gtk_fue) |
| **DISENO-escalera.md** | el paso univariante → transferencia, y el fichero de sesión que falta |
| **ESTUDIO-taste.md** | TASTE (1987-2001) como **diseño**: qué resolvió y qué sigue siendo buena idea |
| **DISENO-proyecto.md** | el manifiesto de proyecto, que hoy no existe |
| **DISENO-repositorio.md** | `atsw-gui`: por qué monorepo, qué entra, y cómo mudarse sin perder los 182 commits |
| **DISENO-mtram.md** | las siete pantallas del GUI de drtran: qué se rescata de TASTE, y dónde TASTE no llega porque mtram es un grafo y no una familia |
| **DISENO-flujo.md** | dónde está cada página en el ciclo de Box-Jenkins: qué nace dónde, qué se modifica dónde, y cómo simplificar |
| **DISENO-diagnosis.md** | la página Diagnosis: por qué LR contra el diagonal y no R², y por qué los gráficos necesitan que el motor escriba los residuos |
| **DISENO-interfaz.md** | cómo se presenta cada pantalla y cómo se llaman los botones. Página a página; hoy, Series |

Dos reglas de lectura, porque el estudio se escribió con ellas:

- **Donde hay una cifra, sale de correr el programa.** Lo leído y lo verificado
  se distinguen a propósito.
- **Lo que el estudio se equivocó, está escrito.** `CONTRATO.md` §6 conserva
  tachada la regla que propuso primero y que los actores que estiman
  desmintieron. Un documento que sólo cuenta sus aciertos no sirve para volver
  a él.

---

## El banco: `conformidad/`

Tres preguntas distintas, tres bancos, y hacen falta los tres porque ninguno ve
lo de los otros.

    sh bateria.sh      los ESCRITORES: ¿conservan el modelo?
    sh acuerdo.sh      los LECTORES:   ¿entienden lo mismo?
    bash copias.sh     ¿sigue la inpcheck del GUI siendo la del motor?

`bateria.sh` no puede ver un fallo de lectura cuando el escritor es fiel — el
fichero mal leído se reescribe igual de mal y el juez compara su propia lectura
equivocada consigo misma. `acuerdo.sh` no puede ver un escritor que pierde un
campo. Y `copias.sh` mira lo que ninguno de los dos: que la copia de
`inpcheck.c` del GUI siga siendo la del motor. **Ya se desincronizó una vez, y
el GUI se quedó cargando un fichero que le destruía el montón.**

El detalle de cada uno, en `conformidad/README.md`.

### El corpus

121 ficheros. La mayoría son reales, de trabajos de verdad. Los que empiezan
por `hueco_` no: se construyeron para ejercitar casos que ninguno de los reales
tocaba, y **cada uno de esos huecos había dejado pasar un fallo durante años**.
Están explicados uno a uno —con lo que falta al final— en
`conformidad/corpus/HUECOS.md`.

---

## Lo que el estudio encontró, en una tabla

| dónde | qué |
|---|---|
| gtk_fue | ocho fallos del escritor del `.inp`; el corpus pasó de 63 a 102 |
| fue 1.14 | los δ salían pegados y el `.pre` no lo relee **ni el propio motor**; y más de 10 regresores externos **destruían el montón** |
| art-python | BUG-0187/0188/0189 |
| fue (Python) | BUG-0017 a BUG-0022 |
| drtran | BUG-17, BUG-18, y reabierta la mitad en C de BUG-2 (closed 2026-09-26: `lib/fuepre`, `fuepre_check_alignment`) |

Y tres formas que se repitieron en más de una fase, que están en
`DISENO-proyecto.md` §0:

1. **La suite calcula lo que importa y lo imprime.** El log de decisiones de
   `build_model`, el `LastReport` de TASTE, y la cola de reestimación de
   `update.py` — los tres se calculan bien y acaban en un texto que nadie lee.
2. **El programa tiene el dato delante al escribir, y no lo escribe.**
   `drtran -g` pone los nombres de las series en un comentario y escribe el
   número; TASTE lee la línea de las unidades a una variable llamada `shit`;
   `fue` imprime su versión y no la estampa en el `.out`.
3. **Un campo que la herramienta EXIGE se rellena; uno que OFRECE, no.**
   Medido: `rationale` es obligatoria y está en 881 de 924 nodos (95%);
   `analyst` es opcional y está en 0 de 125 (0%).
