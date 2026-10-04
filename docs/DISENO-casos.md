# DISEÑO — las redes en el proyecto: la madre multivariante

La madre lleva bien los proyectos **univariantes**: series, muestras, modelos
con su linaje, el editor, el vistazo, y lanza fue, fug y fuf sabiendo a qué
vienen. Para **drtran** sólo tiene un botón que lanza `drtran_gui --proyecto
P` y nada más (`gui/atsw/src/main.c:490`): ni qué series, ni qué red, ni qué
corrida. Este documento diseña lo que falta. No hay código todavía.

---

## 0. El hueco, medido

**El manifiesto es por serie.** `lib/proyecto` conoce `series`, `muestras` y
`modelos`, y cada modelo es de UNA serie (`PrModelo.serie`). No hay ninguna
entidad para lo que trabaja con n series a la vez.

**Y lo que drtran_gui hace hoy con el proyecto es peor que no hacer nada.**
Con `--proyecto`, cada estimación se registra con `pr_deriva` **sobre la
primera serie** —la de salida— como si fuera un modelo univariante de ella
(`gui/drtran/src/proyecto_gui.c:118`):

- una red de cuatro series aparece en la madre como «un modelo más de EP»,
  revuelto con los modelos de fue de EP, y en su cadena;
- ese «modelo» no tiene `.inp` —drtran escribe `.out`, `.dag`, `.cns`,
  `_res.txt`— así que abrirlo en fue o en el editor no tiene sentido;
- las otras tres series no saben que están en una red;
- y la escalera se pierde: no queda escrito **con qué modelo univariante**
  entró cada serie, que es justo lo que `DISENO-escalera.md` exige que se
  pueda rehacer.

---

## 1. La red es una entidad del manifiesto

**Una red es un conjunto FIJO de series, cada una con el modelo univariante
con que entra.** Es la escalera escrita: se sube desde los univariantes
(`.pre` de fue) a la transferencia. De ahí sale todo lo demás:

- **la identidad de una red son sus series y sus modelos de entrada.**
  Cambiar el modelo de entrada de una serie no es «editar la red»: es otra
  red, igual que truncar un `.inp` no es editarlo sino derivar otro
  (`DISENO-muestras.md`);
- **las corridas de drtran cuelgan de la red**, no de una serie: cada
  estimación —con su `.dag` y su `.cns`— es una corrida con su linaje, su
  razón y, una de ellas, la elegida. Es la misma cadena que la de los
  modelos de una serie, un piso más arriba;
- **la muestra es la completa**, y por ahora sólo esa: drtran cruza series
  sobre una ventana común, y cruzar series recortadas de distinta forma es
  lo que una red no puede permitirse (`proyecto_gui.c:115`). El campo existe
  para cuando se decida otra cosa.

### 1.1 En el manifiesto

`schema_version: 2`. Un manifiesto de la versión 1 se lee igual: sin redes.

```yaml
redes:
  R1:
    titulo: "inflacion y petroleo"
    razon: "el WTI adelanta al IPC: la CCF preblanqueada lo dice en k=1"
    creado: "2026-10-05"
    muestra: ""
    padre: ""                      # la red de la que se derivo, si alguna
    series:
      - "ES_CPI m10"               # la PRIMERA es la de salida
      - "WTI m03"
    corridas:
      c00:
        padre: ""
        creado: "2026-10-05"
        razon: ""
      c01:
        padre: "c00"
        razon: "omega_1 no es significativo: fuera"
        elegido: 1
        razon_elegido: "el mas simple con los residuos limpios"
```

**Los ficheros de una corrida**, con nombre de cortesía como siempre:

    <raiz>/_redes/R1/work/R1_c01.out   .dag   .cns   _res.txt   _eval.csv

`_redes/` con guion bajo para que no choque con una serie que se llame
`redes`. Los `.pre` de entrada **no se copian**: son los de las series
(`<raiz>/ES_CPI/work/ES_CPI_m10.pre`), y se leen de ahí.

### 1.2 Lo que hace falta en `lib/proyecto`

Sin dependencias, como el resto de la biblioteca, y con tamaños fijos.

| función | qué |
|---|---|
| `pr_red_add(p, series[], modelos[], n, titulo, razon, id_out)` | da de alta una red; comprueba que cada serie existe y cada modelo es suyo, **en la completa**, y que no son DATOS |
| `pr_red_deriva(p, red, cambios…, id_out)` | otra red a partir de una, con algún modelo de entrada cambiado; queda `padre` |
| `pr_red_idx`, `pr_red_ver` | buscarla, leerla |
| `pr_red_borra(p, red, e)` | se niega si tiene corridas, y dice cuántas (como `pr_muestra_borra`) |
| `pr_corrida_nueva(p, red, padre, id_out, ruta_out)` | la iteración: `c00`, `c01`…, con el linaje sin preguntar |
| `pr_corrida_ruta(p, red, corrida, ext, out)` | el nombre de cortesía |
| `pr_corrida_elige`, `pr_corrida_razon`, `pr_corrida_borra` | como los de los modelos: el elegido es uno por red; no se borra una corrida con hijas |
| `pr_red_desfase(p, red, series_out)` | las series cuyo modelo **elegido de hoy** ya no es el de entrada de la red |
| `pr_red_de_ruta(p, ruta, red, corrida)` | de un fichero a su (red, corrida), como `pr_de_ruta` |

Y un cambio en lo que ya hay: **`pr_borra` se niega a borrar un modelo que
es la entrada de una red** (`PR_EENRED`), y dice de cuál. Borrarlo dejaría a
la red sin escalera.

Tamaños: 32 redes, 16 series por red, 256 corridas en total. `Proyecto` ya
ocupa ~800 KB: se reserva siempre en el montón (ver `PRUEBAS.md` §4).

---

## 2. drtran_gui

**`--red R`**, con `--proyecto`:

    drtran_gui --proyecto P --red R1 [--corrida c01]

- carga las series de la red, cada una del `.pre` de su modelo de entrada,
  **en el orden de la red** (la primera, la de salida);
- si se da `--corrida`, o si la red tiene elegida, carga su `.dag` y su
  `.cns` como punto de partida; si no, empieza en blanco;
- cada estimación es una corrida **de la red** (`pr_corrida_nueva`), hija de
  la que se cargó. Se acaba el registrarla como modelo de la serie de salida.

**Sin `--red`, con `--proyecto`** (drtran_gui lanzado a mano): al estimar, si
todas las series cargadas son modelos del proyecto (`pr_de_ruta`), **se da
de alta la red sola** —es lo que el analista acaba de decir al cargarlas— y
la corrida va a ella. Si alguna no es del proyecto, se estima como sin
proyecto, en la caché, y se dice por qué.

**Sin `--proyecto`**: como siempre.

---

## 3. La madre

```
┌──────────────────────────────────────────────────────────────────────┐
│ Proyecto: SF_MEG                                                     │
├────────────────┬─────────────────────────────────────────────────────┤
│ SERIES         │  R1 — inflacion y petroleo                          │
│ ▸ ES_CPI   m10 │   serie    entra con   elegido hoy                  │
│ ▸ WTI      m03 │   ES_CPI   m10         m10                          │
│ ▸ ES_CORE  m03 │   WTI      m03         m04   ⚠ desfasada            │
│                │                                                     │
│ REDES          │   corridas                                          │
│ ▸ R1  c01 ★ ⚠  │   c00                                               │
│ ▸ R2  —        │    └ c01 ★  «omega_1 no es significativo: fuera»    │
│                │                                                     │
│                │   Abrir en drtran · Elegir · Razón… · Derivar red…  │
├────────────────┴─────────────────────────────────────────────────────┤
│ ● 1 red desfasada: WTI tiene otro modelo elegido (m04)               │
└──────────────────────────────────────────────────────────────────────┘
```

- **Una sección REDES** debajo de SERIES, con su elegida (`★`) y la marca de
  desfase (`⚠`).
- **«Nueva red…»**: un diálogo con las series del proyecto que tienen
  modelo elegido en la completa; se marcan las que entran y se ordena la de
  salida. Las que no tienen elegido salen apagadas, con el porqué.
- **«Abrir en drtran»** lanza `drtran_gui --proyecto P --red R --corrida c`
  con la corrida seleccionada.
- **El árbol de corridas** es el linaje, como el de los modelos
  (`linaje_gui.c`), con la razón al lado y «sin razón» a la vista.
- **«Derivar red…»**: la misma red con los modelos elegidos de hoy. Es la
  salida natural del desfase; no se actualiza nada solo.
- **El veredicto de abajo** gana una línea: las redes desfasadas.
- **La rejilla de una corrida** (σ, logL, Q de los residuos) sale del `.out`
  de drtran con `lib/outdiag`, que ya lo lee para la Diagnosis de drtran_gui.

---

## 4. Lo que hay en disco hoy

Los manifiestos donde drtran_gui ya registró corridas como modelos de la
serie de salida: esos «modelos» se reconocen porque **tienen `.out` y `.dag`
y no tienen `.inp`**. La madre los señala en el veredicto («n corridas de
drtran registradas como modelos de EP») y ofrece **convertirlos** en una red
con una corrida por cada uno. No se convierte nada sin preguntar.

---

## 5. Lo que NO

- **Submuestras en las redes.** El campo está; el diálogo no las ofrece.
- **drvarma.** Un sistema VARMA es también «n series con su modelo de
  entrada», y la misma entidad podría llevarlo con un campo `motor`. No va
  en la primera versión; el diseño no lo impide.
- **Actualizar una red sola** cuando cambia un univariante. Se marca y se
  deriva, nunca se reescribe por debajo.

---

## 6. Pruebas

- `lib/proyecto`: ida y vuelta del manifiesto con redes; un manifiesto de la
  versión 1 se lee; `pr_red_desfase`; las negativas (borrar una red con
  corridas, un modelo que es entrada de una red, una corrida con hijas);
  `pr_red_de_ruta` también con rutas de Windows.
- `drtran_gui`: `--red` carga las series en su orden y registra la corrida
  en la red; sin `--red`, la red se da de alta sola.
- la madre: nueva red, abrir en drtran (con el hijo falso), elegir, razón,
  desfase al cambiar el elegido de una serie, derivar red.

---

## 7. Orden

1. `lib/proyecto`: la entidad, el formato y sus pruebas.
2. `drtran_gui`: `--red` y el alta automática; sus pruebas.
3. La madre: la sección REDES, el diálogo, abrir, elegir, razón, desfase.
4. La conversión de lo que hay en disco (§4).

Cada paso con su PR y la CI en las tres plataformas.
