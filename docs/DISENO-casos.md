# DISEÑO — los casos en el proyecto: la madre multivariante

La madre lleva bien los proyectos **univariantes**: series, muestras, modelos
con su linaje, el editor, el vistazo, y lanza fue, fug y fuf sabiendo a qué
vienen. Para **drtran** sólo tiene un botón que lanza `drtran_gui --proyecto
P` y nada más (`gui/atsw/src/main.c:490`): ni qué series, ni qué red, ni qué
corrida. Este documento diseña lo que falta. No hay código todavía.

**Decisiones del analista, 2026-10-04:** la entidad se llama **caso**; drtran_gui
lanzado a mano **da de alta el caso solo**; **no se borra** un modelo
univariante que es entrada de un caso.

---

## 0. El hueco, medido

**El manifiesto es por serie.** `lib/proyecto` conoce `series`, `muestras` y
`modelos`, y cada modelo es de UNA serie (`PrModelo.serie`). No hay entidad
para lo que trabaja con n series a la vez.

**Y lo que drtran_gui hace hoy con el proyecto sigue el diseño inicial, que no
llegaba a esto.** `DISENO-madre.md` §3.2 decidió que las corridas cuelgan **de
una serie** —`pr_corrida_nueva(p, serie, …)`, en `<raiz>/<serie>/work/`, la
disposición que ya tenía drvarma— y drtran_gui lo hace así: cada estimación
se registra con `pr_deriva` sobre la primera serie (`proyecto_gui.c:118`).
§3.2 resolvía que las corridas **no se pisaran** (la ranura única de TASTE), y
lo resolvió. Lo que no resolvía es **qué se cruzó**:

- una corrida de cuatro series aparece como «un modelo más de EP», revuelta
  con los de fue, y sin `.inp` —drtran escribe `.out`, `.dag`, `.cns`—;
- las otras tres series no saben que están en ella;
- y no queda escrito con qué `.pre` entró cada serie, que es lo que
  `DISENO-escalera.md` §5.3 exige para poder rehacerla.

Este diseño deja §3.2 como antecedente: las corridas siguen teniendo nombre
propio y no se pisan, pero cuelgan del **caso**.

---

## 1. El vocabulario, el de la escalera

Es el que ya usan el estudio y la mitad en Python, y no se cambia:

| término | qué es | dónde se fijó |
|---|---|---|
| **caso** | un conjunto FIJO y ORDENADO de series, cada una con el `.pre` con que entra | el `name` de mtram (`drtran-python`, `mcp_server.py:271`) |
| **red** | los enlaces (`.dag`): qué serie alimenta a cuál, con (b, r, s). **Cambia de una corrida a otra** | `DISENO-mtram.md` §2.1-2.2, `_LINKS` de mtram |
| **restricciones** | el `.cns` | `DISENO-mtram.md` §2.3 |
| **corrida** | una estimación: un caso, con una red y unas restricciones, y su `.out` y sus residuos | `DISENO-madre.md` §3.2 |

---

## 2. El caso es una entidad del manifiesto

### 2.1 Su identidad es lo que se cruzó, por contenido

Es la sesión `.trn` de `DISENO-escalera.md` §5.3, escrita en el manifiesto en
vez de en un fichero aparte:

- **la lista ordenada de entradas**, cada una con su serie, el modelo del que
  sale (serie, muestra, id) y **el sha256 de su `.pre`** en el momento del
  alta. El hash es lo que art guarda en su guion (`base_pre_sha`) y mtram en
  su certificado (BUG-53): la identidad va por contenido, no por nombre;
- **el orden es parte de la identidad.** No es «la primera es la de salida»
  —eso sólo vale para una estrella; en el m6 EU recibe y alimenta a la vez—:
  es el orden con que el `.cns` del motor C indexa las posiciones (BUG-52,
  mientras no se cierre) y el orden de Cholesky de drvarma. Se guarda tal
  cual y no se reordena nunca;
- **cambiar la entrada de una serie es otro caso**, derivado del anterior
  (`padre`), igual que truncar un `.inp` es derivar y no editar
  (`DISENO-muestras.md`).

### 2.2 La ventana común se comprueba al dar el alta

Es la regla de BUG-2 (`REGLAS-NO-ESCRITAS.md`): las series tienen que tener
**la misma frecuencia y la misma fecha final**. drtran la exige con salida 4
(`drtran.c:3772-3786`, que pide además el mismo número de observaciones) y
mtram y sima la exigen también. El alta **se niega** si no se cumple, con
el motivo de `fuepre_check_alignment` (`lib/fuepre`): un caso que el motor
va a rechazar no tiene que existir.

**Las muestras.** Las entradas tienen que haber nacido **todas en la misma
muestra**. La completa es donde más fácil es que las series acaben en fechas
distintas; una submuestra con `hasta:` del proyecto garantiza un final común.
Así que se admite cualquier muestra, con tal de que sea la misma para todas
y la ventana cuadre.

### 2.3 En el manifiesto

`schema_version: 2`. Un manifiesto de la versión 1 se lee igual: sin casos.

```yaml
casos:
  C1:
    motor: drtran
    titulo: "inflacion y petroleo"
    razon: "el WTI adelanta al IPC: la CCF preblanqueada lo dice en k=1"
    creado: "2026-10-05"
    entradas: ES_CPI/m10@3f2a9c… WTI/m03@81bd04…   # EN ORDEN: serie/modelo@sha256

corridas:
  C1/c00:
    version: 0
    padre: ""
    razon: ""
  C1/c01:
    version: 1
    padre: c00
    razon: "omega_1 no es significativo: fuera"
    elegido: si
    razon_elegido: "el mas simple con los residuos limpios"
```

**El formato se ajusta al lector que ya hay**, que admite «clave: valor» con
sangría de 0, 2 o 4 espacios y no sabe de listas: las entradas van **en una
línea y en orden**, y las corridas en su propia sección con la clave
`CASO/id`, como los modelos llevan `SERIE/id`. `muestra` y `padre` sólo se
escriben si no están vacíos. `yaml.safe_load` lo lee entero.

**Los ficheros de una corrida**, con nombre de cortesía:

    <raiz>/_casos/C1/work/C1_c01.out   .dag   .cns   _res.txt   _eval.csv

`_casos/` con guion bajo para que no choque con una serie que se llame así.
Los `.pre` de entrada **no se copian**: son los de las series, y el hash dice
si siguen siendo los mismos.

**`motor`** es `drtran` hoy. Un sistema de drvarma es lo mismo —n series con
su `.pre`, la misma lista— y el traspaso entre los dos, en las dos
direcciones, es «la misma lista de `.pre`» (drtran imprime la llamada a sima
cuando encuentra un ciclo, `drtran.c:2313-2324`). El campo deja hecho el
sitio; drvarma no va en la primera versión.

### 2.4 El desfase: dos cosas distintas

| qué | qué significa | cómo se ve |
|---|---|---|
| **el `.pre` de una entrada cambió o no está** (su sha256 ya no es el del alta) | lo que se estimó en el caso **ya no corresponde** a sus datos | `⚠ desfasado` en el caso y en sus corridas |
| **la serie tiene hoy otro modelo elegido** | informativo: **a menudo es deliberado** —art aconseja estacionalidad determinista para lo multivariante y estocástica para prever (`art/mcp_server.py:557`)— | una nota, sin alarma |

Nada se actualiza solo. La salida, en los dos casos, es **«Derivar caso…»**.

### 2.5 Lo que hace falta en `lib/proyecto`

Sin dependencias, con tamaños fijos, y el sha256 calculado por quien llama (la
biblioteca guarda la cadena; no lee ficheros).

| función | qué |
|---|---|
| `pr_caso_add(p, entradas[], n, muestra, motor, titulo, razon, id_out)` | el alta; comprueba que cada serie existe, que cada modelo es suyo y está en esa muestra, que no es DATOS, y que no hay series repetidas |
| `pr_caso_deriva(p, caso, entradas[], n, id_out)` | otro caso a partir de uno; queda `padre` |
| `pr_caso_idx`, `pr_caso_ver` | buscarlo, leerlo |
| `pr_caso_borra(p, caso, e)` | se niega si tiene corridas, y dice cuántas |
| `pr_corrida_nueva(p, caso, padre, id_out, ruta_out)` | la iteración: `c00`, `c01`…, con el linaje sin preguntar |
| `pr_corrida_ruta(p, caso, corrida, ext, out)` | el nombre de cortesía |
| `pr_corrida_elige`, `pr_corrida_razon`, `pr_corrida_borra` | como los de los modelos: una elegida por caso; no se borra una corrida con hijas |
| `pr_caso_de_ruta(p, ruta, caso, corrida)` | de un fichero a su (caso, corrida), como `pr_de_ruta` |
| `pr_caso_de_entradas(p, entradas[], n)` | el caso que ya tiene exactamente esas entradas (orden y hashes), si existe: lo que necesita el alta automática para no duplicar |

**`pr_borra` se niega a borrar un modelo que es entrada de un caso**
(`PR_EENCASO`) y dice de cuál. Borrarlo dejaría al caso sin escalera.

Tamaños: 32 casos, 16 entradas por caso, 256 corridas en total. `Proyecto` ya
ocupa ~800 KB: va siempre en el montón (`PRUEBAS.md` §4).

---

## 3. drtran_gui

**`--caso C`**, con `--proyecto`:

    drtran_gui --proyecto P --caso C1 [--corrida c01]

- carga las entradas del caso en su orden, cada una del `.pre` de su modelo,
  y **comprueba sus hashes**: si alguno cambió, lo dice antes de estimar (el
  caso está desfasado) y ofrece derivar uno nuevo;
- con `--corrida`, o si el caso tiene elegida, carga su `.dag` y su `.cns`;
  si no, empieza sin red;
- cada estimación es una corrida **del caso**, hija de la que se cargó.

**Alta automática** —sin `--caso`, con `--proyecto`, drtran_gui lanzado a
mano—: al estimar, si **todas** las series cargadas son modelos del proyecto
(`pr_de_ruta`), en la misma muestra y con la ventana común, se busca el caso
con esas entradas (`pr_caso_de_entradas`) y, si no existe, **se da de alta**;
la corrida va a él. Si alguna serie no es del proyecto, se estima como sin
proyecto, en la caché, y se dice por qué.

**Lo que devuelve drtran al univariante.** mtram reescribe los bloques
univariantes reestimados en conjunto como `.inp` nuevos (`write_inp`,
`ES_CPI_m10.1.inp`). Si drtran_gui ofrece lo mismo, esos `.inp` entran como
**modelos derivados de cada serie**, con el modelo de entrada como padre: son
univariantes, no ficheros de la corrida.

**Sin `--proyecto`**: como siempre.

---

## 4. La madre

```
┌──────────────────────────────────────────────────────────────────────┐
│ Proyecto: SF_MEG                                                     │
├────────────────┬─────────────────────────────────────────────────────┤
│ SERIES         │  C1 — inflacion y petroleo            drtran        │
│ ▸ ES_CPI   m10 │   entrada  modelo  .pre       elegido hoy           │
│ ▸ WTI      m03 │   ES_CPI   m10     igual      m10                   │
│ ▸ ES_CORE  m03 │   WTI      m03     igual      m04  (otro: nota)     │
│                │                                                     │
│ CASOS          │   corridas                    logL      puerta diag.│
│ ▸ C1  c01 ★    │   c00                         -767.42   ok          │
│ ▸ C2  — ⚠      │    └ c01 ★  «omega_1 ... fuera»  -768.10            │
│                │                                                     │
│                │   Abrir en drtran · Elegir · Razón… · Derivar caso… │
├────────────────┴─────────────────────────────────────────────────────┤
│ ● C2 desfasado: el .pre de DE_CPI m07 cambió después del alta        │
└──────────────────────────────────────────────────────────────────────┘
```

- **Una sección CASOS** debajo de SERIES, con la corrida elegida (`★`) y la
  marca de desfase real (`⚠`).
- **«Nuevo caso…»**: las series del proyecto con algún modelo estimado (con
  `.pre`) en la muestra elegida. **Por defecto, el elegido**, pero se puede
  escoger otro —por lo de art de arriba—. Se ordenan, y la ventana común se
  comprueba antes de aceptar.
- **«Abrir en drtran»** lanza `drtran_gui --proyecto P --caso C --corrida c`.
- **El árbol de corridas** es el linaje, como el de los modelos
  (`linaje_gui.c`), con la razón al lado y «sin razón» a la vista.
- **Por corrida, lo que la escalera pide ver** (`DISENO-escalera.md` §5.3):
  la logL, **la puerta diagonal** (la suma de las univariantes contra la
  conjunta, y si los ficheros eran óptimos o especificaciones), la ventana
  común y el *cast* usado. Sale del `.out` con `lib/outdiag`, que ya lo lee
  para la Diagnosis de drtran_gui.
  **Pendiente (2026-10-04):** la logL sale; la puerta diagonal se enseña como
  «—». El `.out` de drtran sólo imprime la logL conjunta, y la suma de las
  univariantes está en los `.out` de fue de cada entrada, que `lib/outfile`
  todavía no lee. Hace falta ese lector, no un segundo parser del de drtran.
- **«Derivar caso…»**: el mismo caso con las entradas cambiadas. La salida
  natural del desfase.
- **Un ciclo** (drtran sale con 7, o la red de la corrida no es un DAG):
  «**Pasar a drvarma**» crearía un caso hermano con las mismas entradas y
  `motor: drvarma`. Va en el diseño; no en la primera versión.
- **El veredicto de abajo** gana dos líneas: casos desfasados, y entradas de
  casos que hoy no son el elegido de su serie (la nota, sin alarma).

**Lo que lee de la mitad en Python, sin escribirlo.** Si junto a una serie
hay un `<serie>_guion.json` de art, la madre puede enseñar qué modelo marcó
art como adoptado. No se converge con el guion (`DISENO-madre.md` §10): se
lee.

---

## 5. Lo que hay en disco hoy

Los manifiestos donde drtran_gui ya registró corridas como modelos de la
serie de salida: esos «modelos» se reconocen porque **tienen `.out` y `.dag`
y no tienen `.inp`**. La madre los señala en el veredicto («n corridas de
drtran registradas como modelos de EP») y ofrece **convertirlos** en un caso
con una corrida por cada uno —leyendo del `.out` qué `.pre` entraron, que lo
imprime en su cabecera—. No se convierte nada sin preguntar.

---

## 6. Lo que NO

- **drvarma** en la primera versión: el campo `motor` y la acción quedan
  diseñados.
- **Actualizar un caso solo** cuando cambia un univariante: se marca y se
  deriva, nunca se reescribe por debajo.
- **El recorrido** —qué enlaces se probaron y se podaron, y por qué—:
  `DISENO-escalera.md` §5.4 lo deja para un guion de drtran que no existe.
  La razón de cada corrida es lo que hay hasta entonces.

---

## 7. Pruebas

- `lib/proyecto`: ida y vuelta del manifiesto con casos; uno de la versión 1
  se lee; las negativas (alta con una serie repetida, con un modelo de otra
  muestra, borrar un caso con corridas, un modelo que es entrada, una
  corrida con hijas); `pr_caso_de_entradas` distingue el orden y el hash;
  `pr_caso_de_ruta` también con rutas de Windows.
- `drtran_gui`: `--caso` carga las entradas en su orden y registra la
  corrida en el caso; un hash cambiado se dice antes de estimar; sin
  `--caso`, el caso se da de alta solo y la segunda vez se reutiliza.
- la madre: nuevo caso (con la ventana que no cuadra rechazada), abrir en
  drtran (con el hijo falso), elegir, razón, los dos desfases, derivar.

---

## 8. Orden

1. `lib/proyecto`: la entidad, el formato y sus pruebas.
2. `drtran_gui`: `--caso`, el alta automática y los hashes; sus pruebas.
3. La madre: la sección CASOS, el diálogo, abrir, elegir, razón, desfase,
   la puerta diagonal por corrida.
4. La conversión de lo que hay en disco (§5).

Cada paso con su PR y la CI en las tres plataformas.
