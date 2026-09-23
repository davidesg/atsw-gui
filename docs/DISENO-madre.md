# DISEÑO — la interfaz madre

Fase 6. La entrada es `INVENTARIO-madre.md`: ocho requisitos, cada uno atado a
un hecho medido. **El manifiesto ya está diseñado** (`DISENO-proyecto.md` §3),
así que aquí no se rediseña: aquí se diseña **el programa**.

---

## 1. La pregunta, y mi respuesta

> ¿Qué programa es atsw, qué comparte con qué, y cómo se llega desde él a una
> función de transferencia?

**La madre no absorbe los tres GUIs: les da un contexto que hoy no tienen.**

Ésa es toda la propuesta, y el inventario la sostiene: los tres programas
funcionan y ninguno guarda nada entre ejecuciones — **no hay fichero de sesión,
ni preferencias, ni recientes, en ninguno de los tres**. Son context-free por
construcción. Lo que les falta no es una ventana que los contenga: es **saber en
qué proyecto están**.

Y hay un argumento de coste que no se puede ignorar: absorber significa tirar
tres programas que pasan sus bancos. La suite ya sabe lo que cuesta reescribir
lo que funciona.

### El camino que NO recomiendo, y por qué lo menciono

`gui/drtran` está escrito como `X_pagina_new(Mtram *m) → GtkWidget*`: **ya es
una biblioteca de páginas** con un ejecutable de cuatro líneas encima.
Absorberlo costaría poco. `gui/fue` y `gui/fug` no están así.

Lo digo para que conste que la puerta queda abierta, no para cruzarla ahora:
**absorber uno de tres es lo peor de las dos opciones** — se paga la reescritura
y no se gana la ventana única.

---

## 2. La arquitectura, en cuatro piezas

```
   gui/atsw/            la madre: el proyecto, la rejilla, la comparación
        │
        ├── lanza ──→  fue_gui  ·  gtk_fmg  ·  drtran_gui      --proyecto P
        │
   lib/proyecto/        el proyecto.yaml: nombres → rutas, y nombra las corridas
   lib/datos/           UNA puerta de datos
   lib/tabla/           la exportación: lo que TASTE dejó sin escribir
```

Tres bibliotecas y un programa. Las tres bibliotecas **las enlazan también los
tres GUIs actuales**, y eso es lo que hace que la madre no tenga que
absorberlos: comparten el modelo de datos, no la ventana.

---

## 3. `lib/proyecto` — la pieza central

Lee y escribe el `proyecto.yaml` de `DISENO-proyecto.md` §3. Tres
responsabilidades, y ninguna es de interfaz.

### 3.1 Resolver nombres contra la raíz

El inventario dejó claro que **el lado C ya opera con nombres cortos** y no
tiene el problema de las rutas. Ésa es la decisión, no una concesión: el
proyecto declara `raiz:` y todo lo demás son nombres.

```c
/* El .pre del modelo elegido de una serie. Nombre corto -> ruta. */
int pr_ruta( const Proyecto *p, const char *serie, const char *modelo,
             const char *ext, char *out, size_t n );
```

Mover un proyecto pasa a ser mover una carpeta. **No hay que arreglar rutas
rotas, hay que no escribirlas.**

### 3.2 Nombrar las corridas

Es el requisito 4, y es el que más duele hoy: `drtran_gui` escribe siempre
`modelo.out`, `red.dag`, `modelo.cns`, así que **no caben dos modelos**. Es la
ranura única `RESIDUOS` de TASTE, reaparecida.

```c
/* Reserva un nombre de corrida y devuelve su directorio. */
int pr_corrida_nueva( Proyecto *p, const char *serie, const char *etiqueta,
                      char *dir, size_t n );
```

**El número de versión es un campo, no un trozo del nombre** — la conclusión de
`DISENO-proyecto.md` §2, donde se demostró que `_m01` significa dos cosas
distintas en el mismo árbol y que la convención se puede deducir *mal* con
todos los ficheros delante.

La disposición que se adopta es la que drvarma ya tiene, que es la única de las
tres con forma de proyecto — y que **nadie había documentado**:

```
<raiz>/<serie>/               la serie
<raiz>/<serie>/work/          las corridas
<raiz>/proyecto.yaml
```

### 3.3 Declarar cuál es el modelo elegido

Es el hallazgo que más directamente pide esta biblioteca. Hoy esa decisión vive
en un diccionario a pelo, repetido en tres guiones Python sueltos:

```python
ART_FUF = {"IPC_ES": "cases/IPC_ES/work/IPC_ES_m10.fuf.inp", ...}
```

En el manifiesto es un campo:

```yaml
series:
  IPC_ES: {elegido: m10, razon: "AR(1); el SAR no se gana su sitio"}
  IPC_FR: {elegido: msar}
```

> Y con eso los tres guiones de `cases/` dejan de tener razón de existir: lo
> que hacen —recorrer casos y comparar peldaños— pasa a ser una consulta.

### 3.4 Lo que NO hace

**No reimplementa `guion.py`.** El lado Python tiene 1562 líneas maduras —
linaje por SHA, nodos de decisión con evidencia y decisor, `diff_nodes` que
empareja por nombre de nodo y no por posición. `lib/proyecto` **lee** el
`guion.json` para enseñarlo; no lo escribe y no lo duplica.

Y un aviso del inventario que hay que respetar: el alcance real del guion **no
es la serie, es el directorio**. La madre tiene que tratarlo como tal.

---

## 4. `lib/datos` — una puerta, y hay que elegir cuál

Requisitos 1 y 2. Hoy hay **cuatro** lectores del mismo formato y **discrepan**:
fug aplana todas las columnas en un vector, fue se queda con la primera. El
mismo fichero da dos series distintas.

Esto no es como `fugdraw`, que tenía cuatro copias **idénticas** y se
factorizaron sin riesgo. Aquí unificar **obliga a elegir un comportamiento**, y
elegirlo es una decisión del método, no de ingeniería. Mi criterio:

| | decisión | por qué |
|---|---|---|
| columnas | **la 1 es la serie; las demás, regresores** | es lo que hace fue, y es lo único que tiene sentido: aplanar dos columnas fabrica una serie que no existe |
| cabecera | **se acepta y se usa** para nombrar | fug ya la salta; usarla es gratis y quita un tecleo |
| separadores | espacio, TAB, `;` **y coma** | hoy el filtro anuncia `*.csv` y un CSV de verdad falla |
| coma decimal | **sí, si no hay punto** | es la regla de fug y es correcta |
| frecuencia y fecha | **del fichero si están; del diálogo si no** | requisito 2 |

Ese último es el que importa. Hoy **la frecuencia y la fecha de inicio no
viajan con la serie**: salen de un combo, con defaults distintos por programa
(fug `freq=1`; fue y drvarma `freq=12`, año 2000). Una serie mal fechada al
cargarla envenena todo lo que venga después y no hay nada que lo detecte.

> Y una consecuencia que conviene decir en voz alta: **`gui/drtran` sigue sin
> leer datos crudos.** Es la única puerta cerrada a propósito y se queda
> cerrada. `lib/datos` es para fue y fug, y para la madre al dar de alta una
> serie.

---

## 5. `lib/tabla` — el informe que nunca se construyó

Requisito 8, y el estudio de TASTE ya lo puso como **primer** requisito y no
como último:

> Se construyó el instrumento y se dejó sin construir el informe. Ése es
> exactamente el hueco que la interfaz madre tiene que llenar.

TASTE declaró `TABLA`, `Graba_Ser_PRN` y `Graba_Ser_TSF` y no escribió ninguno.
Hoy, treinta años después, el inventario dice lo mismo: **ningún botón
«Exportar» en ninguno de los tres GUIs**, ningún export de tablas, y un solo
CSV en toda la suite.

Una tabla es un objeto con columnas tipadas, unidades y procedencia:

```c
Tabla *tb_new( const char *titulo, const char *fuente );
void   tb_col( Tabla *t, const char *nombre, const char *unidad, int decimales );
int    tb_csv( const Tabla *t, const char *path );
int    tb_tex( const Tabla *t, const char *path );
int    tb_txt( const Tabla *t, const char *path );   /* ancho fijo, como el .out */
```

Y **la procedencia va dentro**: qué modelo, qué muestra, qué versión del motor.
Una tabla que sale del programa sin decir de dónde viene es la forma más fácil
de que un número acabe en un paper sin poder reproducirlo.

Clientes inmediatos, todos existentes y ninguno hipotético: la tabla de
parámetros con sus d.t. (`od_params`, ya la leo), la de previsión por horizonte,
la de evaluación fuera de muestra, las tres del Ajuste de Diagnosis, la ACF/PACF.

---

## 6. `gui/atsw` — la madre propiamente dicha

Lo único que es ventana nueva. Tres cosas, y ninguna la hace hoy nadie.

```
┌──────────────────────────────────────────────────────────────────────┐
│ Proyecto: SF_MEG          Abrir…  Nuevo…            Exportar…        │
├────────────────┬─────────────────────────────────────────────────────┤
│ SERIES         │  ES_CPI                                             │
│                │  ┌───────────────────────────────────────────────┐  │
│ ▸ ES_CPI   m10 │  │ modelo │ σ_a   │ Q     │ estado    │ instrum.│  │
│ ▸ ES_CORE  m03 │  │ m00    │ 0.53  │ ok    │ dead-end  │ art .2.1│  │
│ ▸ DE_CPI   m07 │  │ m01    │ 0.47  │ ok    │ exploring │ art .2.1│  │
│ ▸ FR_CPI   —   │  │ m10 ★  │ 0.42  │ ok    │ adopted   │ art .2.1│  │
│ ✗ FR_CORE      │  └───────────────────────────────────────────────┘  │
│                │                                                     │
│                │  Abrir en:   fue   ·   fug   ·   drtran             │
├────────────────┴─────────────────────────────────────────────────────┤
│ ● 8 series, 31 modelos, 6 con el elegido sin declarar                │
│ ● 2 modelos cuelgan de un padre que ya no existe — linaje dudoso     │
└──────────────────────────────────────────────────────────────────────┘
```

**La rejilla n series × n modelos.** Es lo que no existe en ninguna parte. El
estudio ya dijo dónde está la primitiva: `diff_nodes` existe y está bien, *«no
hay que rehacerla, hay que llamarla n×m»*.

**El elegido, declarado y a la vista** (`★`). Hoy es un diccionario en tres
guiones.

**Los dos veredictos abajo**, como en las siete páginas de `drtran_gui`. Y el
segundo tiene ya su primer contenido, medido: `linaje_dudoso` sobre los 131
guiones del disco encuentra **170 de 194** entradas cuyo `base_pre_path` apunta
a un fichero que no está.

### Cómo se abre un motor

```
drtran_gui --proyecto ~/SF_MEG/proyecto.yaml --serie ES_CPI
```

Un proceso aparte, como hasta ahora. Los tres GUIs ganan `--proyecto`, que es
un cambio pequeño **porque hoy no aceptan ninguno**. Y con él, `lib/proyecto`
les resuelve dónde escribir: se acabó el `~/.cache` de nombres fijos.

---

## 7. Lo que hay que arreglar en el camino

Tres fallos reproducidos en el inventario. No son del diseño de la madre, pero
la madre los hace visibles porque empieza a pasar rutas con directorio:

| | qué pasa | dónde |
|---|---|---|
| `fue caso/X` | `Warning: can not write Acaso/X.eps` — **el informe sale sin el gráfico de residuos y sólo avisa** | `fue.c:1567` |
| `fue caso/X -f` | aborta: `forecast_caso/X.inp` | `fue.c:205` |
| `drtran dt/*.pre` | el `.out` cae en el cwd, no junto a los datos | `drtran.c:186-194` |

El patrón es el mismo en los tres: **el prefijo se antepone a la ruta entera en
vez de al nombre**. Es exactamente el fallo que ya corregí en
`lib/fugplot/save_eps` esta semana. `fug` lo esquiva haciendo `chdir`; los otros
tres no.

> Arreglarlos es requisito 3, y es previo: mientras un motor escriba donde se
> lanzó, el proyecto no puede prometer dónde están las cosas.

---

## 8. El orden del trabajo

Cada paso deja algo utilizable, y ninguno depende del siguiente.

1. **Los tres arreglos de §7.** Pequeños, con prueba en la batería. Sin esto lo
   demás no se sostiene.
2. **`lib/tabla` y un botón «Exportar» en `drtran_gui`.** Es el paso que más
   valor da por línea escrita, no necesita proyecto, y cierra el hueco que el
   estudio de TASTE lleva señalando desde la fase 4.
3. **`lib/datos`**, y fue y fug pasan a usarla. Aquí se decide de una vez qué
   significa una segunda columna.
4. **`lib/proyecto`** y `--proyecto` en los tres GUIs. `drtran_gui` deja de
   escribir en nombres fijos y empiezan a caber dos modelos.
5. **`gui/atsw`**, ya con las tres bibliotecas hechas y tres programas
   pidiéndoles cosas.

El orden no es caprichoso: **la madre va la última otra vez**, por la misma
razón de P3. Hacerla antes de que existan `lib/proyecto`, `lib/datos` y
`lib/tabla` sería diseñar la ventana de un gestor que todavía no gestiona nada.

---

## 8 bis. El gestor de MODELOS, que es otra entidad

**La ventana principal gestiona tres cosas, no una: datos, modelos y
proyectos.** Yo había diseñado `lib/proyecto` para agrupar n series × n
modelos, y eso es la tercera. La segunda —**la cadena de iteración**— es
distinta y no estaba.

### La cadena, y por qué necesita gestor

```
.inp(-1) ──estimar──→ .pre(-1) ──copiar──→ .inp(0) ──estimar──→ .pre(0) …
especificación        óptimo               especificación
valores = SEMILLA     reejecutable         del paso siguiente
```

`.pre` e `.inp` **son el mismo formato** —el contrato lo dice y lo verifiqué en
máquina: copié un `.pre` a `Z.inp`, corrí `fue Z` y salió `Z.pre`—, pero el
motor **exige la extensión `.inp`**. Así que el paso `.pre(-1) → .inp(0)` es
una **copia física real**, no una manera de hablar.

Y nadie la numera. De ahí el hallazgo de `DISENO-proyecto.md` §2: con acceso
total, todos los ficheros delante y una hora de trabajo, la convención `_mNN`
se puede deducir **mal**, y en el mismo árbol `_m01` significa dos cosas
distintas para la misma serie.

> **El número de versión tiene que ser un campo, no un trozo del nombre que
> cada lector reinterpreta.**

### La decisión: el porqué se registra; el linaje **no se puede perder**

Decisión del analista, 2026-09-20:

> «Debería registrar el porqué de cada iteración, pero **el linaje es lo mínimo
> que se debería mantener**.»

Son dos exigencias de rango distinto, y el diseño tiene que tratarlas distinto:

| | rango | cómo |
|---|---|---|
| **linaje** | obligatorio, nunca se pierde | **automático**: el programa ya sabe de qué `.pre` salió este `.inp`. No se pregunta porque no hace falta preguntarlo |
| **el porqué** | se quiere, no bloquea | se **pide**, no se exige; y se puede rellenar después |

**Por qué no se exige, aunque `art` sí lo exija.** En la encarnación Python,
`guion_node` **rechaza la llamada sin razón**, y funciona: 881 de 924 nodos la
llevan. Pero eso es un LLM escribiendo. Un diálogo modal *«¿por qué?»* en cada
estimación de un GUI se contesta `asdf` a la tercera — y **una razón falsa es
peor que ninguna**, porque no se distingue de una de verdad.

Así que:

- el linaje se escribe **siempre**, sin preguntar;
- el porqué tiene su sitio a la vista y **se puede escribir en cualquier
  momento**, también más tarde, mirando el `.out`;
- y **«sin razón» se ve como sin razón**. Nunca se infiere una, nunca se pone
  un texto de relleno. Es la misma regla que la huella vacía del guion: *no
  consta* nunca significa *cuadra*.

### Lo que eso le añade a `lib/proyecto`

```c
/* De que .pre sale este .inp. Se escribe SOLO, al copiar. */
int pr_deriva( Proyecto *p, const char *serie,
               const char *padre, const char *hijo );

/* El porque. Se puede llamar despues, y se puede no llamar. */
int pr_razon( Proyecto *p, const char *serie, const char *modelo,
              const char *razon );

/* Los que no la tienen. La ventana los enseña, no los esconde. */
int pr_sin_razon( const Proyecto *p, char nombres[][64], int max );
```

Y una consulta que hoy no existe en el lado C y que es la que da valor al
registro: **`pr_camino()`** — de dónde viene este modelo, hasta la raíz. Es
`path_to_root` de `guion.py`, que ya está escrita… en la otra encarnación.

> Esto **no** es reimplementar el `guion.json`. Es el mínimo que hace falta
> para que la cadena de iteración del GUI sea reconstruible, en el formato del
> `proyecto.yaml`. Las dos encarnaciones gestionan proyectos de forma distinta
> —§10— y ésta es la de aquí.

---

## 9. Lo que queda decidido y lo que no

**Decidido aquí:** la madre lanza y agrupa, no absorbe; tres bibliotecas
compartidas antes que una ventana; la columna 1 es la serie; el proyecto nombra
las corridas; el elegido es un campo; `guion.py` se lee, no se reimplementa.

**Decidido por el analista (2026-09-20), y cierra P4 — ver §10.**

**Abierto, y no lo decido yo:**

- **El catálogo huérfano.** `registry.yaml` existe, es un buen esquema de
  procedencia y **nadie lo lee**. Conectarlo es barato; decidir si es *el*
  catálogo de la suite, no.
- **Los residuos como serie de primera clase.** Hay tres implementaciones de
  graficarlos y la última la escribí yo esta semana. Si los residuos fueran una
  serie más, sobrarían dos.

---

## 10. P4 resuelto — **ATSW es un taller, y tiene dos encarnaciones**

Decisión del analista, 2026-09-20. Cierra la pregunta P4 del plan
(`ESTUDIO-atsw-PLAN.md`), abierta desde la fase 5.

**ATSW —*A Time Series Workshop*— es el taller.** No es un programa: es lo que
las dos encarnaciones tienen en común, que son los motores y el método.

| | **ATSW GUI** | **ATSW Python** |
|---|---|---|
| qué es | interfaz clásica, motores en C, multiplataforma | interfaz de última tecnología, conducida por un LLM |
| para quién | **educación**, y analistas muy especializados que quieren **control total del proceso** | análisis asistido |
| motores | los de C | los mismos, total o parcialmente |
| proyectos | `proyecto.yaml` (este documento) | **gestión distinta** — `guion.json`, registro, policy |

### Lo que se sigue de ahí, y afecta a este diseño

**Distribuciones separadas.** `pip install atsw` **no** trae el GUI, ni ahora ni
en la primera versión. Son dos productos que comparten motores y método, no un
producto con dos caras.

> Esto retira de la mesa la tentación de hacer que `lib/proyecto` y el
> `guion.json` converjan. **No tienen que converger.** Son dos gestiones de
> proyecto para dos formas de trabajar, y forzar una sola habría hecho peor a
> las dos.

Y confirma lo que el inventario ya había encontrado por otro camino: el alcance
del `guion.json` es el **directorio**, y el del `proyecto.yaml` es el
**proyecto**. Son cosas distintas porque responden a preguntas distintas.

**`lib/proyecto` lee el `guion.json`, no lo escribe** (§3.4). Esa decisión, que
en §3 era una precaución contra duplicar 1562 líneas maduras, pasa a ser
estructural: **es de la otra encarnación**. Leerlo para enseñar un linaje está
bien; escribirlo sería que una encarnación gestionase los proyectos de la otra.

**El nombre.** La familia es `atsw`; el programa, `atsw_gui`. Es el mismo
convenio que acaba de costarnos el renombrado de `mtram → drtran_gui`, y por la
misma razón: **un nombre señala una cosa sola.**

### Lo que queda anotado para después, no para la primera versión

> «No descarto instalar una consola en la madre para que un LLM como Claude
> Code funcione de ATSW GUI, pero no en la primera versión.»

Es un requisito **futuro**, y conviene que conste ahora porque tiene una
consecuencia de diseño barata si se tiene en cuenta desde el principio y cara
si no: **todo lo que la madre sepa hacer tiene que poder hacerse sin la
ventana.** Es decir, la lógica en `lib/proyecto`, `lib/datos` y `lib/tabla`, y
`gui/atsw` sólo como vista.

Que es exactamente la arquitectura de §2 — así que no hay que cambiar nada. Se
anota para no perderla.

#### Y cuando llegue: una consola de verdad NO, un panel de conversación SÍ

Consultado el 2026-09-23: *«¿cuáles son las posibilidades reales de insertar
una consola real en la madre, para operar con Claude y un MCP como art pero
específico para atsw_gui? Algo como WezTerm. Hay que tomar en cuenta que el
programa tiene que ser multiplataforma.»*

**Con Windows en la ecuación, empotrar un emulador de terminal está
descartado**, y no por dificultad puntual:

| opción | Linux | macOS | Windows | coste |
|---|---|---|---|---|
| VTE (`libvte-2.91`) | sí | regular | **no existe** | bajo |
| empotrar WezTerm/xterm (XEmbed) | sólo X11 | no | no | bajo |
| terminal propia (pty + ConPTY + parser ANSI) | sí | sí | sí | **muy alto** |
| panel de conversación (no es terminal) | sí | sí | sí | medio |
| el agente fuera, los ficheros dentro | sí | sí | sí | **casi cero** |

VTE depende de los ptys de Unix y no tiene puerto a Windows; XEmbed no
sobrevive ni a Wayland. Y una terminal propia no es un widget, es un proyecto:
WezTerm y Alacritty *son* eso. El parser de ANSI bien hecho —colores,
direccionamiento del cursor, pantalla alterna, redimensionado— son semanas, y
después hay que mantenerlo en tres sistemas.

**Pero la terminal no es lo que hace falta.** «Operar con Claude y un MCP» son
dos cosas y sólo una necesita terminal:

- que el LLM **actúe** sobre el proyecto → eso es un servidor MCP, y no tiene
  por qué vivir dentro de la ventana;
- un sitio donde **escribirle** → eso es lo reemplazable.

##### El panel de conversación, que es la opción que queda anotada

Un `GtkTextView` con la conversación y una entrada abajo, hablando con el MCP
del proyecto. Multiplataforma por construcción, sin emular nada, y con la
ventaja de que **puede enseñar lo que no cabe en un terminal**: una fila de la
rejilla como fila, un gráfico como gráfico, un `.out` en su pestaña.

Lo que hay que tener claro antes de hacerlo:

1. **No es un cliente de un LLM**, es un cliente del MCP. Meter aquí la
   autenticación y el bucle agéntico sería reimplementar Claude Code dentro de
   una ventana de GTK — y mantenerlo.
2. **Roza P4.** Las dos encarnaciones tienen *gestión de proyectos distinta*;
   el LLM dentro de la ventana empieza a borrar esa línea. La versión «el
   agente fuera, los ficheros dentro» la respeta: cada uno gestiona a su
   manera y se encuentran en el `proyecto.yaml`.
3. **Va después del MCP, no antes.** Sin el servidor no hay con quién hablar,
   y con el servidor puede que el panel ya no haga falta.

##### Lo que sí se hizo ya, porque vale con agente y sin él

La madre **relee el manifiesto por huella** al recuperar el foco, como ya hacía
con los `.out`. Con eso, un proceso que trabaje al lado —otra madre, un editor,
un agente con un MCP sobre `lib/proyecto`— aparece en la ventana solo, sin
empotrar nada.

Dos reglas que eso trajo, y las dos son del mismo tipo:

- **Un manifiesto roto por fuera no se traga.** Se lee a otro sitio y sólo se
  cambia si salió bien: lo que hay en memoria funciona, y un fichero a medio
  escribir no puede tirarlo por delante.
- **Guardar es escribir Y apuntar la huella.** Si sólo se escribe, el siguiente
  foco relee nuestra propia escritura creyendo que la hizo otro — y lo dice. Un
  aviso falso de «esto ha cambiado fuera de aquí» es peor que no avisar:
  enseña a no creerse los avisos.

---

**Estado: propuesto, con P4 resuelto.** A la espera de revisión del resto.
