# El ciclo de iteración, y cómo el programa lo induce

Esquema previo. Nada de esto está construido todavía.

---

## 0. Lo que hoy no induce nada

- Los botones de **Anómalos…** y **Diagnosis…** están siempre activos, haya
  `.out` o no, y esté al día o no.
- **Derivar** deja al padre abierto y editable. Si el analista sigue tocando
  su `.inp`, la procedencia del hijo pasa a ser mentira: dice que salió de un
  fichero que ya no es ése.
- Anómalos y diagnosis **sólo se abren desde la madre**. Estando en fue_gui
  con el modelo recién estimado delante, hay que volver a la madre y buscarlo
  en la lista.
- El destino de la derivación está **clavado** al editor.

---

## 1. El ciclo, y la regla que lo hace inducible

```
      especificar ──→ estimar ──→ diagnosticar ──→ ¿cuadra?
           ↑                                          │ no
           └── derivar ←── intervenir ←── anómalos ←───┘
```

**Un botón se enciende cuando existe su insumo, y no antes.** No es una
restricción: es que un botón encendido que no puede hacer nada útil enseña el
camino equivocado.

| gesto | su insumo | dónde vive |
|---|---|---|
| Estimar | un `.inp` que pase `inp_check_fue` | fue_gui, editor |
| **Diagnosis…** | un `.out` **al día** | fue_gui, editor, madre |
| **Anómalos…** | un `.out` al día **con residuos** | fue_gui, editor, madre |
| Sugerir intervención | un episodio marcado | anómalos |
| Derivar | una forma elegida | sugerir, editor |

**«Al día» no es «existe».** Un `.out` más antiguo que su `.inp` es la
diagnosis de *otra* especificación. Hoy nadie lo mira. Es la comprobación que
de verdad induce la buena práctica, y la madre ya tiene la huella (tamaño +
fecha) con la que hacerla.

---

## 2. Las cinco piezas

### P1 · Las ventanas de análisis dejan de ser de la madre

`anomalos_gui.c`, `diagnosis_gui.c` y `sugerir_gui.c` reciben un `Atsw *`, o
sea que **son de la madre por accidente**: lo que necesitan de ella es el
manifiesto, una barra de estado y «abre este modelo». Lo demás no lo tocan.

Los dos programas son `G_APPLICATION_NON_UNIQUE`, así que *pedírselo a la
madre que ya corre* no existe: arrancaría una segunda. Y duplicar las
ventanas en fue_gui es garantizar que en tres meses sean dos ventanas
distintas.

Así que salen a una biblioteca con un contexto pequeño:

```c
typedef struct {
    Proyecto  *p;
    GtkWindow *padre;                    /* transient_for            */
    void     (*di)( void *dueno, const char *s );          /* su barra */
    void     (*abre)( void *dueno, const char *serie,
                      const char *muestra, const char *id );
    void      *dueno;
} AnCtx;
```

La madre pasa su barra y `atsw_editor`; fue_gui pasa la suya y «carga este
`.inp` en mí». Las ventanas dejan de saber qué es una madre.

*Coste: refactor mecánico, sin cambio de comportamiento. fue_gui gana
`lib/anomalos`, `lib/dictamen`, `lib/intervencion`, `lib/inpdet`, `lib/tabla`,
`lib/fugplot` y su `plotstats`.*

### P2 · fue_gui tiene que saber QUÉ modelo tiene abierto

Hoy conoce **una ruta**, y del proyecto sólo la raíz. Y el nombre del fichero
es cortesía: la clave no se parsea de él.

```c
/* La clave del modelo cuyo fichero es esa ruta. Es una BUSQUEDA en el
   manifiesto, no un parseo del nombre. 0 si lo encontró.              */
int pr_de_ruta( const Proyecto *p, const char *ruta,
                char *serie, char *muestra, char *id );
```

Si el fichero no está en el manifiesto **no hay clave, y los botones se
quedan apagados**. Eso es correcto y es parte de lo que se induce: fuera del
proyecto no hay linaje que cuidar.

*Coste: una función y su prueba.*

### P3 · Los botones, con su insumo

Una sola regla, la misma en los tres sitios, y **un botón apagado siempre
dice por qué** en su globo:

- sin `.out` → «todavía no está estimado»;
- `.out` más viejo que el `.inp` → «el informe es de antes que la
  especificación: reestima»;
- `.out` sin residuos → «este informe no trae los residuos».

### P4 · Derivar cierra el padre, y un padre con hijos no se edita

Al derivar: se guarda lo que haya, se escribe **la razón del hijo**, se cierra
la ventana del padre y se abre el hijo en la herramienta elegida.

Y la regla que hay detrás, que hoy falta: **editar el `.inp` de un nodo que
tiene hijos convierte su procedencia en mentira.** El manifiesto ya se niega a
*borrar* un nodo con hijos (`PR_EHIJOS`) y dice cuál cuelga; el editor no se
niega a *editarlo*. El diálogo de «derivar en vez de pisar» que el editor ya
tiene pasa a ser **obligatorio** —sin la opción de pisar— cuando el nodo tiene
descendencia.

*Necesita `pr_hijos()`, hermana de la comprobación que `pr_borra` ya hace.*

### P5 · El linaje, evaluado

Una vista por (serie, muestra) que enseñe la cadena y, por nodo, **lo que
tiene y lo que debe**:

```
IPC_ES · muestra completa
  m00  datos
   └ m01  estimado · MIRAR (autocorrelación) · sin razón ⚠
       └ m02  estimado · CUADRA · razón ✓                    ← elegido
       └ m03  SIN ESTIMAR ⚠  «episodio en 3/2022: Treadway…»
```

Con la separación de siempre: **linaje y razón del manifiesto; estimado y
dictamen del `.out`**. Y nombrando lo que falta, que es lo que induce:

- nodos sin razón (`pr_sin_razon` ya existe);
- nodos que nunca se estimaron — una rama muerta que nadie declaró muerta;
- cadenas sin elegido;
- un elegido cuyo dictamen es peor que el de un hermano.

---

## 3. Dónde vive la preferencia del destino

La madre no tiene almacén de preferencias, y los tres GUIs son *context-free
por construcción* a propósito. Tres salidas:

| | dónde | qué cuesta |
|---|---|---|
| a | un campo en el manifiesto | es del proyecto, no de la persona |
| b | un fichero de preferencias | inventar un almacén nuevo |
| c | un selector en el momento de derivar | hay que elegir cada vez |

**Propongo (c) con la última elección recordada en (a).** El selector está
donde se deriva —visible, por acto— y su valor persiste sin inventar un
sistema de preferencias.

**Mi criterio para el valor por defecto: fue_gui.** Lo que un nodo recién
derivado necesita a continuación es *estimarse*, y fue_gui estima y enseña la
diagnosis; el editor es para cuando la especificación pide algo que el
formulario no sabe expresar. Pero es tu decisión.

---

## 4. Orden — hecho

| | qué | dónde quedó |
|---|---|---|
| 1 | `pr_de_ruta` | `lib/proyecto`, con su prueba |
| 2 | las ventanas a una biblioteca | `lib/analisis`, con `AnHost` |
| 3 | los botones con su insumo | `an_estado()`, usada por la madre y por fue_gui |
| 4 | derivar cierra el padre; un padre con hijos no se edita | `AnHost.cierra` + `pr_hijos` + el editor |
| 5 | el selector de destino | en la ventana de derivar; se recuerda en el manifiesto |
| 6 | la vista de linaje | `gui/atsw/src/linaje_gui.c`, botón «Linaje…» |

Lo que cambió respecto del esquema, y por qué:

- **`AnCtx` se llama `AnHost`** y lleva dos llamadas más de las previstas:
  `cierra` --para la pieza 4-- y `abre` con la herramienta, para la 5.
- **`plothost.h` se comparte** entre los dos GUIs en vez de haber uno por
  programa. Los motores tienen el suyo porque tienen `diagnose.c`; los dos
  GUIs comparten host porque comparten `plotstats.c`, y dos copias del mismo
  fichero no son dos hosts: son un descuido esperando.
- **`default_lags` estaba duplicado** en `lib/utils` y en
  `lib/fugplot/plotsupport.c`, y la de `lib/utils` no la llamaba nadie **y
  además no acotaba**. Se quitó: al enlazar fugplot en fue_gui el choque lo
  destapó.
- **El EPS de anómalos lleva la clave en el nombre.** `lib/preview` reutiliza
  la ventana POR RUTA: con un nombre fijo, abrir dos modelos habría hecho que
  el segundo pisara el dibujo del primero en su propia ventana.
- **`fue_gui` SÍ deriva**, y la primera versión no. Se dejaron sus `guarda` y
  `abre` a NULL con el argumento «derivar lo lleva la madre» — pero el botón
  se enseñaba igual, así que escribía el `.inp` en disco, lo registraba sólo
  en la memoria de fue_gui y no abría nada: **un huérfano**, y un botón que
  parecía no hacer nada mientras hacía daño.

  Dos arreglos, y los dos hacían falta. El botón **existe sólo si el
  anfitrión sabe guardar y abrir** — derivar son tres cosas y media derivación
  no vale —; y fue_gui aprende a hacer las tres, porque el bucle que se rompía
  era el bueno: estimar aquí, mirar los residuos aquí y tener que volver a la
  madre para poner la intervención es el camino largo, que es el que no se
  recorre.

  Su manifiesto se **relee de disco** antes de derivar: la madre sigue viva al
  lado, y escribir encima la copia de hace media hora no da error — deja un
  fichero bien formado sin lo que falta. Y el desplegable enseña sólo lo que
  el anfitrión sabe abrir (`AnHost.puede`): desde fue_gui no hay editor.

---

## 5. Lo que **no** propongo

- **Nada de IPC entre programas.** Los dos son `NON_UNIQUE`: «pídeselo a la
  madre» arrancaría una segunda madre con el mismo proyecto abierto.
- **Nada de duplicar las ventanas en fue_gui.** Dos copias divergen; ya pasó
  con `getExt` y con `inpcheck`.
- **Nada de impedir.** Inducir la buena práctica es hacer que el camino bueno
  sea el fácil, no que los otros sean imposibles: un botón apagado dice por
  qué, y lo que falta se puede hacer. La única excepción es P4, porque ahí lo
  que se impide no es una molestia sino **escribir algo falso**.
