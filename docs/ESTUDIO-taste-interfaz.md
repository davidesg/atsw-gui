# TASTE como interfaz: qué le sirve todavía a la madre

Estudio del 17-oct-2026, a petición del analista: *«estudia la interfaz de
TASTE y cómo nos podría servir para mejorar la madre y las GUIs… creo que la
madre es muy superior actualmente, pero quiero el estudio y tu opinión
crítica»*. fue_gui y drtran_gui están bien y no se tocan; el estudio mira la
madre (`gui/atsw`).

Completa `ESTUDIO-taste.md` (fase 4), que estudió TASTE **leyendo el código**
y sacó seis decisiones (§3 de ese documento). Este estudio lo hace **usándolo**:
pantalla por pantalla, con la serie CONSUMO de Howrey y el área de trabajo que
grabaron los autores en 1992. TASTE es privado: aquí se describen sus
pantallas y se citan sus rótulos, no se copia código.

---

## 0. Cómo se ha mirado

- **El programa es el original**, pero en el port de 64 bits de
  `atws/Taste/port`, que conserva sin tocar la lógica de los menús y los
  diálogos. Ese port pinta en un buffer que sustituye a la memoria de vídeo
  `$B800`. Una copia en el borrador vuelca ese buffer a un fichero en cada
  repintado. Lo que sale es **la pantalla de 80×25 exacta**, la misma que
  pintaba el DOS, con sus colores. No hizo falta DOSBox.
- **Se condujo por un pty con pulsaciones**: los 8 menús, cada opción con su
  línea de ayuda, carga del área de trabajo, gráfico, identificación,
  especificación, estimación con diagnosis, el visor y las teclas F1-F10.
- **De paso, un fallo del port**, sin arreglar. En un terminal UTF-8 las letras
  con tilde descuadran la pantalla: sale «C lculo» por «Cálculo». La causa es
  que `GotoXY` de la unidad CRT cuenta bytes en vez de caracteres. Afecta sólo
  al pintado, no a `TASTE.OUT`. Es del repositorio `atws`, no de éste.

---

## 1. El veredicto

**La madre es superior, y no por poco.** Coincido con el analista. Las razones
importan, porque dicen qué no hay que copiar:

- **Recuerda.** La madre guarda el proyecto, el linaje, las razones y los
  casos, y se reanuda donde se dejó. TASTE empieza cada sesión en blanco.
  - Su `TASTE.OUT` se trunca al arrancar.
  - Lo que no se añade a mano con `[A]` se pierde al cerrar el visor.
- **Se ve todo a la vez.** La madre enseña las series, los modelos de cada
  muestra, los casos y los veredictos al mismo tiempo. TASTE es una pila de
  ventanas modales sobre una pantalla vacía. Para saber qué modelos hay se
  pulsa F3 *dentro de un campo*.
- **No pisa.** El aviso de TASTE «LA SERIE "CONSUMO" YA EXISTE Y SERA
  SOBREESCRITA» informa, pero no detiene nada: tras la tecla, se sobreescribe.
  La madre deriva en vez de pisar, y para borrar pregunta antes a la
  biblioteca.
- **Explica más.** Cada botón de la madre lleva un globo largo. En TASTE, F1
  contesta **«NO HAY AYUDA DISPONIBLE EN ESTA VERSION»**, en todas las
  pantallas probadas. La línea 25 sólo repite el nombre de la opción:
  - «Estimación» → «ESTIMACION»;
  - «Dibujar» → «REPRESENTACION GRAFICA».
  La «frase por opción» que la fase 4 elogió, leída en pantalla, es un
  rótulo, no una ayuda.
- **Rellena con prudencia.** Los campos de TASTE llegan rellenos con lo último
  usado, y un ENTER sin mirar lo acepta:
  - «NOMBRE DE LA SERIE: RESIDUOS» al ir a dibujar;
  - «NOMBRE DEL MODELO: TF2» al ir a estimar;
  - λ = 0, d = 2, D = 1 en la identificación, sin teclearlos.
  Es rápido para quien lo domina y una trampa para quien aprende.

**Lo que TASTE tiene y la madre no** cabe en tres cosas:
1. un registro de lo que se ha hecho;
2. una manera de identificar que enseña **toda la rejilla** de
   transformaciones de una vez;
3. el teclado.

El resto son detalles. Las tres van en §3.

---

## 2. Cómo es TASTE, en cuatro pantallas

**La barra de menús**, 8 menús y 45 opciones:

     Archivos   Datos   Generar   US   UT    Cálculo    Opciones   UtiLidades

- **US y UT** repiten los mismos cuatro pasos: Identificación · Modelo ·
  Estimación · Previsión.
- **Datos**: Tabular, Estadísticos, Dibujar, Histograma, Media-DT, ACF, PACF y
  CCF, sobre cualquier serie en memoria, también sobre `RESIDUOS`.
- **Generar** hace álgebra de series:
  - operaciones con funciones, del tipo `DESTINO = expresión`;
  - combinación lineal;
  - submuestra;
  - variables de intervención.
- **Cálculo** reúne los resultados que salen del modelo:
  - ψ(B) y π(B);
  - impulso y escalón;
  - ACF/PACF teóricas;
  - retrovisión;
  - el ruido de una UT.
- **Utilidades**: expresiones, distribuciones de probabilidad, calendario,
  fechas del Domingo de Resurrección y «Acerca de».

**Un diálogo** (la estimación). La serie queda a la izquierda, el modelo a la
derecha y los campos en medio:

       ┌──────────┬───────────────────────────────────────────────┬───────────┐
       │ CONSUMO  │  NUMERO DE RETROVISIONES              : 20    │ USCONS    │
       ├──────────┘  MAXIMO NUMERO DE ITERACIONES         : 25    └───────────┤
       │             ...                                                      │
       │             DIAGNOSIS DE RESIDUOS (Si-No)        : S                 │

La línea 25 cambia según dónde se esté:
- **En un menú**: `[F1] AYUDA │ <rótulo de la opción> │ REVISAR [F10]`.
- **Dentro de un campo**: `[F2] SERIES │ [F3] MODELOS │ [F4] FUNCIONES │
  [F5] PROBS │ [F6] CALCULADORA`. Son listas y calculadoras que se abren sin
  salir del campo. F2 da cada serie con su número de observaciones, y F3 cada
  modelo con su tipo (USM/TFM).
- **Al acabar el diálogo**: `[ENTER] PROCESAR │ [C] CORREGIR │ [ESC] SALIR`.

**Todo resultado va al mismo visor**:

    │ [ESC] SALIR │ [A] AÑADIR │ [P] IMPRIMIR │  LINEAS 1-21 DE 304 │

- `A` añade el informe a `TASTE.OUT`, y `F10` vuelve a abrir el último.
- **Cada informe empieza diciendo sobre qué se calculó**: la serie y su
  título, las observaciones y fechas, el período, d, D y λ. Un gráfico
  arrancado de su contexto sigue diciendo de qué es.

**La identificación es una batería, no un gráfico.** Pedida con λ = 0, d ≤ 2 y
D ≤ 1, da un solo informe de **1536 líneas**:
- primero la serie original y su logaritmo;
- después las seis combinaciones de (d, D);
- cada una con su gráfico estandarizado, histograma, media-DT, ACF y PACF.

En la ACF, los retardos estacionales se pintan con `=` en vez de `*`, y la Q
de Ljung-Box se imprime en los múltiplos del período. En el gráfico de
residuos, los que se salen de las bandas se marcan con `>│` y `│<`,
y el final de cada año con `»` y `«`.

---

## 3. Lo que vale la pena: cada idea, con mi opinión

Para cada idea: qué hace TASTE, qué hace hoy la madre, mi juicio y, si
procede, una propuesta con su coste. Van ordenadas por valor.

### 3.1 Un cuaderno del proyecto — **adoptar; es lo que más vale**

- **TASTE**: cada resultado es texto en un visor, y `[A]` lo añade a la
  salida. Es pobre (se trunca en cada sesión y hay que acordarse de pulsar A),
  pero tiene una virtud que la fase 4 ya señaló: todo es paginable, imprimible
  y buscable en un solo sitio.
- **La madre**: cada análisis abre su ventana, y al cerrarla no queda nada.
  Sólo exportan Diagnosis y Ganancia (`lib/tabla`). No hay un sitio donde el
  analista vea, al día siguiente, qué miró y qué salió.
- **Opinión**: es el hueco real. El guion guarda **las decisiones**, y el
  cuaderno guardaría **la evidencia que se miró**. No son lo mismo, y el curso
  necesita lo segundo: «enséñame lo que viste antes de elegir m03».
- **Propuesta**:
  - Un botón «Al cuaderno» en cada ventana de análisis (Diagnosis,
    Contrastes, Ganancia, Anómalos, Identificar).
  - El botón escribe lo que la ventana enseña en `<proyecto>/cuaderno/`: el
    texto o la tabla de `lib/tabla`, con la cabecera de procedencia que ya
    tienen (serie, modelo, muestra, fecha), y el gráfico si lo hay.
  - Una entrada «Cuaderno…» en la barra de arriba lo abre en orden.
  - **Corrige lo malo de TASTE**: el cuaderno persiste y nunca se trunca.
  - Coste medio: casi todo está ya en `lib/tabla`.
  - Falta decidir el formato. Mi recomendación es Markdown con imágenes, que
    se lee sin la suite.

### 3.2 La rejilla de identificación — **adoptar como vista, no como sustituto**

- **TASTE** recorre λ × d × D de una vez y lo deja todo en un informe.
- **La madre** tiene el vistazo (`vistazo.c`): λ, d y D al pie del gráfico, y
  el dibujo se rehace al cambiarlos. Es mejor para **explorar**, porque
  responde al momento y no obliga a leer 1500 líneas.
- **Opinión**:
  - La batería de TASTE es ilegible como texto.
  - Pero su idea es buena: **la decisión sobre d y D es comparativa**, y en el
    vistazo sólo se ve una casilla cada vez. El analista compara de memoria.
- **Propuesta**:
  - Una entrada «Rejilla d × D» junto a «Serie y ACF/PACF».
  - Abre una ventana con 6 ACF pequeñas, d = 0, 1, 2 por D = 0, 1, para la λ
    elegida. Con un clic, una casilla pasa al vistazo.
  - Sale de fug, como el vistazo, sin dibujo propio: es la misma regla de
    «lo que dibuja es fug, no una imitación».
  - Coste medio: 6 llamadas al motor y una cuadrícula.

### 3.3 El teclado — **adoptar, y es barato**

- **TASTE** se maneja entero sin ratón.
- **La madre** no tiene ni un atajo: no hay aceleradores en `gui/atsw/src`.
  fue_gui y drtran_gui sí los tienen desde la barra: Ctrl+E, Ctrl+D y Ctrl+Q.
- **Opinión**: es una incoherencia más que una carencia de diseño. Quien pasa
  de drtran_gui a la madre pierde las manos.
- **Propuesta**, un juego mínimo y coherente con las hijas (coste bajo):
  - Ctrl+O abrir, Ctrl+N nuevo, Ctrl+Q salir;
  - Intro sobre un modelo lo abre en fue, como el doble clic;
  - Supr borra, pasando por la misma pregunta;
  - Ctrl+R relee;
  - la tecla de menú, o Mayús+F10, saca el menú contextual de la fila marcada.

### 3.4 Corregir sin cerrar — **extender lo que ya hay**

- **TASTE** acaba todo diálogo en Procesar / Corregir / Salir.
- **La madre** usa Aceptar / Cancelar. Sólo «Nuevo caso» valida sin cerrarse:
  marca el error en rojo y conserva lo tecleado.
- **Opinión**: el «Corregir» de TASTE no es una virtud en sí. En un formulario
  de texto era la única manera de volver atrás, y en GTK los campos siguen
  editables. La lección que sí vale es otra: **un error de validación no debe
  tirar lo tecleado**.
- **Propuesta**: aplicar el patrón de «Nuevo caso» a los diálogos que validan
  (Proyecto…, Editar la serie…, la muestra «+»). Coste bajo.

### 3.5 Los residuos como serie — **sigue pendiente desde la fase 4**

- **TASTE** trata `RESIDUOS` como una serie más: se dibuja, se tabula y se le
  saca la ACF con los mismos menús que cualquier serie, y la cabecera dice de
  qué modelo y de qué serie salen.
- **La madre** sólo los enseña a través de ventanas concretas: el PDF de fue,
  Anómalos, «Identificar los residuos» y Diagnosis.
- **Opinión**: la decisión de la fase 4, «unos residuos por modelo», sigue
  siendo correcta y sigue sin hacerse. Mi forma barata: en el menú del modelo,
  «Residuos: serie y ACF/PACF» abre **el mismo vistazo** con los residuos.
  - Antes hay que comprobar que fue deja los residuos en un fichero legible.
  - Si no los deja, se anota y no se toca el motor.

### 3.6 Series derivadas — **preguntar antes**

- **TASTE** tiene el menú Generar: `DESTINO = expresión`, combinación lineal,
  submuestra e intervenciones.
- **La madre** no puede crear una serie que no venga de un fichero, aunque sea
  una tan corriente como deflactar o un cociente.
- **Opinión**: el defecto de TASTE es el que señaló la fase 4. La serie nueva
  pierde de dónde viene, que se queda en un título, y además pisa. Si se hace,
  la serie derivada lleva **su fórmula en el manifiesto** y se regenera cuando
  cambian sus series madre. Así ya lo decidió la fase 4 (§3.3: materializar es
  un acto explícito y con procedencia).
- **Lo pregunto, no lo propongo.** Quizá el analista prefiera que los datos
  lleguen ya hechos desde la hoja de cálculo, y que la madre no sea un
  sitio de cálculo.

### 3.7 Lo que no merece la pena

- **Las listas en el campo (F2/F3).** Los desplegables de GTK ya hacen eso.
- **La calculadora y las probabilidades (F4-F6).** Útiles en 1990; hoy el
  analista tiene R o Python al lado.
- **El calendario y las fechas de Pascua.** Los motores ya colocan la Semana
  Santa, y la tabla de fechas no aporta nada en el flujo.
- **La barra superior con «SERIES: 2 │ MODELOS: 4».** La madre enseña las
  listas mismas, que es mejor que contarlas.
- **El menú por pasos (I/M/E/P).** La fase 4 ya concluyó que lo valioso era el
  grafo de dependencias, no la forma del menú. Las hijas ya están organizadas
  así.

---

## 4. Lo que hay que evitar, aprendido de usarlo

Lo que en el código parecía neutro, en pantalla molesta:

1. **Campos que llegan rellenos y se aceptan con ENTER.** La madre propone
   valores, pero los enseña. Hay que mantenerlo así.
2. **Avisos que no detienen.** «SERA SOBREESCRITA» seguido de «pulse
   cualquier tecla» no es una pregunta. Si algo es destructivo, se pregunta
   con un «no» por defecto.
3. **El resultado que se pierde si no se pulsa A.** El cuaderno de §3.1 lo
   haría al revés: es fácil añadir y nada se pierde por omisión.
4. **La ayuda vacía.** F1 existe en todas las pantallas y no contesta nada.
   Un botón de ayuda sin contenido es peor que no tenerlo.
5. **Una sola ranura para lo que se compara**: los residuos, el último
   informe en F10. La madre ya lo hace por modelo, y debe seguir así.

---

## 5. La propuesta, en orden

| | qué | dónde | coste | ¿decide el analista? |
|---|---|---|---|---|
| 1 | Atajos de teclado en la madre | `gui/atsw/src/main.c` | bajo | no; es coherencia con las hijas |
| 2 | Corregir sin cerrar en los diálogos que validan | `serie_gui.c`, `main.c` | bajo | no |
| 3 | El cuaderno del proyecto | `lib/tabla`, `lib/analisis`, la madre | medio | **sí**: el formato y si va al manifiesto |
| 4 | La rejilla d × D | junto al vistazo, a través de fug | medio | sí: si la quiere |
| 5 | Residuos en el vistazo | menú del modelo | bajo si fue los deja en fichero | no |
| 6 | Series derivadas con fórmula | manifiesto + Datos… | medio-alto | **sí**: si la madre debe calcular |

Mi recomendación es hacer 1 y 2 juntos, que son pequeños, y después el
cuaderno, que es el que cambia el uso. La rejilla y las series derivadas,
sólo si el analista las quiere. Nada de esto toca los motores ni las dos GUIs
hijas.
