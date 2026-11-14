# La madre, los hijos y el proyecto: auditoría de procesos

Auditoría pedida el 14-nov-2026: *«Revisemos todos los procesos entre madre y
los hijos y el proyecto. Como es parte de la arquitectura y se ha escrito por
partes puede ir acumulando errores y fallos de arquitectura.»*

Venía de un fallo de esa clase: en macOS, la madre se quedaba viva y sin
ventanas si se cerraba su presentación, y fue_gui llamaba a `gtk_main_quit`
sin `gtk_main` (#138, arreglados).

**Cómo se hizo.** Tres revisiones de solo lectura, en paralelo, cada una
sobre un frente:
1. el ciclo de vida de los procesos (lanzar, vigilar, cerrar, motores);
2. el estado compartido del proyecto (el manifiesto y el cuaderno entre
   procesos);
3. el contrato entre la madre, los hijos y los documentos de diseño.

Cada hallazgo de gravedad alta o media se comprobó después **en el código**,
leyendo el camino entero. La columna «visto» lo dice: **sí** es comprobado
así; **lectura** es de las revisiones, coherente con el código pero sin
seguirlo hasta el final. Nada se ha arreglado todavía: este documento es para
decidir el orden.

---

## 1. Lo que hay debajo: cada proceso tiene su copia del manifiesto

Casi todo lo grave sale de una misma decisión que nunca se tomó en voz alta.
**Varios procesos escriben el mismo `proyecto.yaml`**:
- la madre;
- fue_gui;
- drtran_gui;
- sim_gui;
- atsw-cli;
- las ventanas de análisis de la madre, que son de la madre pero viven aparte.

Cada uno lo lee en su momento y **lo escribe entero desde su copia en
memoria**, y ninguna de estas precauciones existe:
- un cerrojo;
- releer justo antes de escribir (solo lo hacen fue_gui al derivar y la
  vuelta de drtran);
- comprobar que nadie lo cambió desde que se leyó;
- escribirlo de forma atómica (`pr_escribir` trunca el fichero y lo escribe
  en el sitio).

Ningún documento de diseño declara una regla de concurrencia. La única
intención escrita son los comentarios «la madre sigue viva al lado».

Mientras se trabaja con una ventana cada vez, no se nota. En cuanto hay dos
procesos vivos que escriben —lo normal: la madre más un drtran_gui o un
sim_gui—, **se pierden cambios sin aviso**. Además, la madre relee al
recuperar el foco y **da por buena la pérdida**.

---

## 2. Hallazgos

### Alta

| # | qué | dónde | visto |
|---|---|---|---|
| A1 | **drtran_gui y sim_gui escriben la copia que leyeron al arrancar.** Lo que la madre hizo entretanto desaparece del manifiesto: un modelo iterado, una muestra, un caso nuevo o una elección. Sus ficheros quedan huérfanos en disco. | drtran: `proyecto_gui.c:57` (lee), `estima.c:600` → `proyecto_gui.c:83` (escribe). sim: `proyecto.c:32` (lee), `estima.c:156`, `estructura.c:96, 281` (escribe) | sí |
| A2 | **La madre tampoco relee al guardar**, y sus ventanas secundarias escriben sobre su copia: análisis (formales, identificar, sugerir, factores) y editor. `atsw_guarda` vuelve a tomar la huella del fichero que acaba de escribir, así que lo que otro proceso escribió entretanto se pierde **sin el aviso** de «ha cambiado fuera». | `ventana.c:243-250`; `lib/analisis/an_formales.c:782`, `an_identifica.c:497`, `an_sugerir.c:453`, `an_factores.c:165`; `editor.c:403` | sí |
| A3 | **Dos procesos pueden dar el mismo identificador** (mNN, cNN, CNN) y **el segundo pisa los ficheros del primero**. El id se calcula como máximo + 1 sobre la copia en memoria, y no se mira si el fichero ya existe en disco. Ejemplo: fue_gui deriva m04 y lo estima; la ventana Formales de la madre, que tenía abierta de antes, reformula y también asigna m04, y escribe su `.inp` encima. | `lib/proyecto/proyecto.c` `pr_deriva_rol` (~648), `pr_corrida_nueva` (~1045), `pr_caso_add` | sí |
| A4 | **El manifiesto se escribe sin temporal ni rename.** Un lector a mitad de escritura (la madre al recibir el foco, la relectura de fue) puede ver un prefijo que se lee bien, por ejemplo sin `casos:`, y en su siguiente escritura lo hace permanente. Un fallo durante la escritura lo deja truncado. El proyecto vive en Dropbox, que puede sincronizar ese estado a medias. | `proyecto.c:1192-1198` | sí |
| B1 | **Uso de memoria liberada en el editor de `.inp` de la madre.** Cerrarlo con fue estimando llama a `engine_stop`, que solo manda la señal: lib/engine sigue llamando a `acabo` con el editor ya liberado. Y al derivar, `atsw_editor_cierra` destruye la ventana **sin parar el motor**. Puede tumbar la madre. | `editor.c:518-523` (`acabo`), `:601-605`, `:627-633`, `:673`; `lib/engine/engine.c:555` (`engine_stop`) | sí |
| C1 | **sim_gui estima un caso desfasado y cuelga la corrida del caso viejo.** Detecta el desfase pero solo lo pone en el título. La corrida se estima con los `.pre` de hoy y se registra bajo unos hashes que dicen otra cosa. drtran_gui hace lo que dice el diseño: deriva otro caso (DISENO-casos §2.4, §3). Además, la puerta y el rango de sim se guardan por caso y no por las entradas, así que con las entradas cambiadas se recarga un `puerta.out` viejo y la puerta se da por superada. | sim: `proyecto.c:91`, `ventana.c:241`, `estima.c:150-156`; `proyecto.c:97-105, 122-135`. drtran, bien: `proyecto_gui.c:263-287` | sí |

### Media

| # | qué | dónde | visto |
|---|---|---|---|
| C2 | **Corridas en el linaje sin estimación.** sim_gui da de alta la corrida **antes** de lanzar el motor. Si la orden no cabe, si el motor no arranca o si muere, queda una corrida sin `.out`, y además es la `previa` de la siguiente. drtran_gui la guarda y la hace `previa` sea cual sea el estado: también si se detuvo o no corrió. La cadena de padres pasa por estimaciones que no existen. | sim `estima.c:204-231`; drtran `estima.c:595-602` | sí |
| C3 | **«Guardar» (Ctrl+S) en fue_gui pisa el `.inp` de un modelo ya estimado**, incluso uno que es entrada de un caso. La comprobación de derivar solo está en «Estimar». El `.out` y el `.pre` quedan describiendo otro modelo, y el hash del caso no lo ve. «Prever» (`fue -f`) reestima desde ese `.inp` y reescribe el `.out`. El editor de la madre, en cambio, avisa y ofrece derivar. | `file_io.c:1073` → `save_inp_file`; `deriva_si_estimado` (`:1181`) solo en Estimar | sí |
| B2 | **Cerrar la madre pierde sin preguntar lo no guardado en sus editores.** El editor no pertenece a la GtkApplication: al irse la ventana principal, el proceso sale sin pasar por su «¿Guardar?». Solo se pregunta si hay hijos vivos. | `editor.c:727`; `main.c:2476-2495` | lectura |
| B3 | **Se pueden abrir dos hijos sobre lo mismo** (Ctrl+E dos veces sobre C1, dos fue_gui sobre m01). Asignan la misma corrida o estiman a la vez sobre los mismos ficheros. La madre guarda el programa de cada hijo, pero no el caso ni el modelo. | `gestos.c:157-243` | lectura |
| A5 | **Borrar un modelo o un caso no mira si hay un hijo abierto sobre él.** fue_gui, que ya no encuentra el modelo, estima «fuera del proyecto» y deja `.out` y `.pre` huérfanos con un nombre que el siguiente `pr_deriva` reutilizará. drtran_gui hace reaparecer el caso borrado al escribir su copia (A1). | `main.c:750-790`, `casos_gui.c:1360-1460`; `atsw_hijos_vivos` solo al cerrar | lectura |

### Baja

- **B4.** Al irse la madre, los hijos salen con `g_application_quit` y se
  saltan sus propias confirmaciones. Es así por diseño, pero si la madre se
  cae los hijos desaparecen sin avisar.
- **B5.** `engine_run` (síncrono) espera sin plazo: la previsión de fue_gui y
  gtk_fmg (`sitio_lanza_y_espera`). Con un motor colgado, la interfaz se
  congela y no se atiende al vigía de la madre.
- **B6.** En macOS no hay PDEATHSIG ni job object. Si un GUI muere por una
  señal, sus motores siguen vivos.
- **B7, sospechas sin comprobar:**
  - en Windows, `g_spawn_async` con `STDOUT_TO_DEV_NULL` necesita
    `gspawn-win32-helper*.exe` en el paquete;
  - si `OpenProcess` falla en el hijo, este se cierra al segundo aunque la
    madre siga viva.
- **C4. Rutas y nombres reimplementados a mano, y ya separados.**
  - fue_gui no respeta `raiz:` (ya apuntado en el TODO);
  - fug la arregló copiando `raiz_real` en vez de usar `pr_raiz`;
  - `sim_artefacto` repite `pr_corrida_ruta`;
  - la madre fija a mano el prefijo «A» del gráfico de fue;
  - hay tres `sha_de` (hoy iguales).
- **C5.** `DISENO-casos.md` documenta `--corrida`; la madre pasa `--modelo`, y
  sim solo entiende `--modelo`.
- **C6.** `pr_leer` rechaza una clave desconocida, así que ningún hijo tira
  campos en silencio. Pero si un programa más nuevo escribe una clave, la
  madre se queda con su copia en memoria y su siguiente guardado la borra.
- **A6. El cuaderno.**
  - en Windows, el modo «a» del CRT no es atómico entre procesos (sospecha);
  - dos procesos que mandan el mismo gráfico en el mismo segundo pueden
    elegir el mismo nombre de imagen;
  - la huella de la madre (tamaño + mtime en segundos) no ve un cambio del
    mismo tamaño en el mismo segundo.

---

## 3. Lo que está bien

Comprobado por las revisiones, y conviene no tocarlo al arreglar lo de arriba.

- **Lanzar.**
  - Un único sitio (`lanza_v`), argv como vector y sin shell. En Windows,
    `CreateProcessW` con UTF-16 y las reglas de comillas de
    `CommandLineToArgvW`.
  - La madre guarda el PID, recoge al hijo con `g_child_watch_add` (sin
    zombis) y dice en la barra si falta el programa.
- **Los argumentos.** Lo que la madre pasa a cada hijo es exactamente lo que
  cada hijo entiende. Los cuatro hijos se niegan a arrancar con un manifiesto
  roto, y no lo pisan.
- **Vigilar a la madre.**
  - En Unix, `getppid() == madre`, sin riesgo de reutilización de PID. En
    Windows, el HANDLE abierto al arrancar.
  - Al irse la madre, los motores se paran (`engine_stop_all`).
- **lib/engine.**
  - Lista de vivos; SIGTERM y después SIGKILL; `atexit`; PDEATHSIG en Linux
    y job object en Windows.
  - `done` solo cuando están cerradas las dos tuberías y el hijo ha salido.
  - Los callbacks de sim_gui y drtran_gui son seguros: su estado vive lo que
    el proceso.
- **La identidad.** Sale del manifiesto (`pr_de_ruta`, `pr_caso_de_ruta`) y no
  del nombre de los ficheros. El desfase se calcula igual en la madre, en
  drtran y en sim. `pr_borra` impide borrar un modelo que tiene hijos o que es
  entrada de un caso.
- **Las rutas.** `raiz: "."` relativa, así que el proyecto se puede mover;
  raíces de Windows; `/` y `\`. Los `.inp` derivados y los del editor se
  escriben de forma atómica (temporal + rename).
- **Las relecturas que sí hay** (la madre, fue, la vuelta de drtran) leen en
  una copia aparte y solo la usan si salió bien.

---

## 4. El plan de arreglo, por fases

Cada fase en su PR, con su prueba, y la CI en verde en las tres plataformas.

| fase | qué | arregla |
|---|---|---|
| **1** | **El manifiesto, transaccional, en `lib/proyecto`.** Una sola forma de cambiarlo, la misma para todos los procesos: **cerrojo** (un fichero junto al manifiesto, creado con `O_EXCL`), **releer**, aplicar el cambio, **escribir de forma atómica** (temporal + rename, que reemplaza también en Windows) y soltar el cerrojo. Los ids nuevos saltan también los que ya tienen ficheros en disco. Después, cada sitio que hoy escribe su copia pasa a esa forma: madre, ventanas de análisis, editor, fue, drtran, sim y atsw-cli | A1–A4, y lo de los ids de B3 y C6 |
| **2** | **El editor de la madre.** Desenganchar el callback del motor al cerrarlo o al derivar (o mantener vivo el editor hasta que el motor avise). Preguntar por lo no guardado antes de parar la estimación, y también al cerrar la madre | B1, B2 |
| **3** | **sim_gui con un caso desfasado, como drtran_gui**: derivar otro caso o reutilizar uno con esas entradas. La puerta y el rango, guardados por el hash de las entradas | C1 |
| **4** | **Las corridas, solo con su `.out`**: darlas de alta cuando el motor ha escrito resultados (o marcarlas «fallida» sin hacerlas `previa`), en drtran y en sim | C2 |
| **5** | **«Guardar» de fue_gui sobre un modelo estimado deriva**, como «Estimar» y como el editor de la madre | C3 |
| **6** | **La madre, ante dos hijos sobre lo mismo y ante borrar lo que un hijo tiene abierto**: llevar la cuenta de qué tiene abierto cada hijo; no abrir dos veces lo mismo, sino traer al frente el que ya está; y avisar antes de borrar | B3, A5 |
| **7** | **Lo menor**: rutas por `pr_raiz` y `pr_corrida_ruta`, un solo `sha_de`, `--corrida` en el documento, plazos en las esperas síncronas | C4, C5, B5 |

**La fase 1 va primero** porque A1–A4 son la raíz y se arreglan en un solo
sitio. Las fases 3 a 5 son independientes de ella y pequeñas. Toca la
arquitectura y pide decidir:
- el cerrojo: si es de aviso o bloqueante, y qué hacer si se queda colgado
  porque un proceso murió con él cogido;
- si la madre, al releer y encontrar una pérdida, debe avisar y no solo
  releer.
