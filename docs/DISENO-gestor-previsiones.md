# El gestor de previsiones

Plan para revisar (17-oct-2026). Sale de una petición del analista:

> «Creo que no tenemos nada para gestionar modelos de cara a ejercicios de
> previsión consecutivos. Tenemos que planear un gestor. En fue es simple si
> se hace a mano, pero necesitamos un gestor.»

Lo que vino antes:
- `ESTUDIO-taste-prevision.md`: TASTE no acumulaba nada.
- FORSIL ya está en drtran y drvarma: parámetros fijos, observaciones
  nuevas y errores por recursión.
- `TODO.md`, «Actualizar el origen de la previsión con el mismo modelo».

---

## 1. El problema

Hoy una previsión es un **acto suelto**. Se estima, se prevé, se mira el
informe, y ahí acaba. Lo que un ejercicio de previsión de verdad necesita
—el del boletín mensual, el de la clase que sigue el IPC cada mes— es una
**secuencia**:

1. llegan los datos de un mes más;
2. con el modelo vigente se prevé desde el nuevo origen, **sin tocar sus
   parámetros**;
3. la previsión se guarda tal como se hizo y **no se vuelve a tocar**;
4. cuando llega el dato que se previó, se mide el error;
5. de vez en cuando —o cuando la diagnosis lo pide— se **reestima** y el
   ejercicio sigue con el modelo nuevo, sin perder lo anterior.

En fue esto se hace a mano: copiar el `.pre`, añadirle el dato, fijar los
parámetros, correr fuf, guardar la salida con otro nombre y apuntar el error
en una hoja. Funciona con un modelo y un mes. Con varios modelos, varias
series y un año de ejercicio **se pierde**: qué modelo estaba vigente en
marzo, con qué dato de febrero, si el dato se revisó después.

## 2. Lo que ya existe, y lo que falta

| pieza | dónde | qué hace |
|---|---|---|
| prever con un modelo univariante | fuf, con el `.pre` de fue | prevé con los parámetros del fichero; su entrada lleva el horizonte y **la σ² de la estimación**. Comprobado: con todas las banderas a 0 sale con 0 y da las mismas previsiones |
| prever con parámetros fijos y datos nuevos, multivariante | drtran `-estwin E -O -1`; drvarma `-estwin N` | FORSIL: la ventana de estimación queda fija y el origen es el final actual de los datos |
| evaluación con el origen rodando | drtran `-C`; drvarma `.recursive` | MAE, RMSE y MAPE por horizonte, **dentro de una corrida** |
| modelos con linaje, muestras, casos, hashes de `.pre` | `lib/proyecto` | quién es hijo de quién, en qué ventana, y si un caso quedó desfasado |
| la previsión del mismo modelo sin dar de alta otro | drtran_gui (#49) | `<modelo>_prev.*`, pero **sólo la última**: se pisa |

**Lo que falta** es lo que une estas piezas en el tiempo:
- **La llegada de datos nuevos.** No hay forma de decir «esta serie tiene un
  mes más» o «esta serie se ha revisado»; hoy se rehace el `.inp` a mano.
- **El registro de cada previsión hecha**, inmutable y fechado.
- **La comparación previsión-realización**, que se rellena sola cuando llega
  el dato.
- **La vigencia**: qué modelo se estaba usando en cada origen, y por qué se
  cambió.

## 3. Los conceptos

### 3.1 La añada de datos

Cada vez que entran datos nuevos de una serie, eso es una **añada**: la serie
tal como se conocía en una fecha. Lleva la fecha de bajada, la fuente, el
hash de los números y la última observación.

**Si se acumulan o no lo decide el analista** (decisión del analista,
17-oct-2026), porque depende del uso:
- **Modo SPS**: el proyecto es un sistema de previsión en marcha. Se guarda
  el registro de las previsiones calculadas y de sus añadas, y se puede ir
  **adelante y atrás** por los orígenes.
- **Modo evaluación**: el analista prueba modelos de forma secuencial y no
  quiere el histórico. Se queda la última añada y se sobrescribe.

Es una propiedad del ejercicio (o del proyecto, por defecto), no una
constante del programa.

- **Por qué no basta con sobrescribir:** los datos **se revisan**. El IPC
  apenas; el PIB o el empleo, mucho. Una previsión hecha en marzo hay que
  juzgarla con lo que se sabía en marzo, y su error se mide contra el dato
  que llegó, el primero o el revisado, según se pida. Sin añadas esa
  pregunta no se puede contestar.
- **Encaja con lo que hay:** el `.inp` y el `.pre` llevan los datos dentro.
  Una añada es, en la práctica, «el `.pre` del modelo con los datos de esa
  añada», y su hash ya dice si cambió algo de lo anterior —revisión— o sólo
  se añadió por el final —extensión—.

### 3.2 El ejercicio de previsión

El **ejercicio** es lo que se sigue en el tiempo. Lleva:
- **qué se prevé**: un modelo univariante (serie, muestra y modelo) o un
  modelo de un caso (caso y corrida), con su motor: fuf, drtran o drvarma;
- **el horizonte** y la probabilidad de las bandas;
- **la política de reestimación** (§3.4);
- **el modelo vigente**, que cambia con las reestimaciones y deja rastro.

Un ejercicio no es un modelo: el mismo modelo puede estar en dos ejercicios
(horizontes distintos), y un ejercicio pasa por varios modelos.

### 3.3 El acta

Cada previsión hecha deja un **acta**: un registro inmutable con
- el ejercicio, el modelo vigente y el hash de su `.pre`;
- la **añada** usada y el **origen como fecha**;
- la ventana de estimación de los parámetros y la σ² **de la estimación**,
  no recalculada;
- la previsión y las bandas **en nivel y en la escala transformada**, con la
  D.T., y si la previsión destransformada es la mediana o la media;
- los ficheros que la produjeron: la orden, el `.out` y el CSV.

Un acta **no se rehace**. Si la añada se revisa, la previsión de marzo sigue
siendo la que se hizo en marzo. Se puede hacer otra «como si» con la añada
nueva, pero es otra acta y lo dice.

### 3.4 Actualizar y reestimar: dos gestos

**El modelo se congela** (decisión del analista, 17-oct-2026): no se
reestima con cada dato. Tampoco «reestimando la misma ventana», que da los
mismos números pero no es lo mismo: los parámetros del ejercicio son los del
modelo vigente, leídos, no recalculados.

- **Actualizar**: el mismo modelo con más datos, los parámetros fijos y un
  origen nuevo. Es FORSIL. No crea modelo; crea un acta.
- **Reestimar**: la ventana de estimación se alarga, o se rehace la
  identificación, y nace un **modelo hijo** del vigente, con su razón, como
  cualquier otro de la cadena. El ejercicio pasa a usarlo **desde ese
  origen**: las actas anteriores siguen siendo del padre.

La política dice cuándo se propone reestimar:
- nunca (sólo a mano);
- cada k orígenes (cada año, por ejemplo);
- cuando la diagnosis de las innovaciones nuevas —los errores a un paso— se
  sale de la banda.

**Propone; no decide**: la vigencia la cambia el analista, como la elección
de un modelo.

### 3.5 La tabla previsión-realización

**Los errores los calculan los motores, no el gestor** (decisión del
analista, 17-oct-2026):
- fuf, en la columna ERR de su informe: los errores a un paso de las
  observaciones;
- drtran, con `-C`: los errores por origen y por horizonte, en CSV;
- drvarma, en su `.recursive`.

El gestor **los lee y los junta**; no los vuelve a calcular. Así hay una sola
cuenta y es la del motor, como la diagnosis se lee del `.out`.

Cuando una añada trae un dato que alguna acta previó, su fila se completa con
lo que dice el motor. De ahí salen, **por horizonte y por modelo vigente**:
- el ECM, el EAM y el sesgo;
- la cobertura de las bandas: ¿el 95 % cubre el 95 %?;
- los errores a un paso (las innovaciones nuevas) como serie, con su ACF.
  Es la diagnosis del modelo **fuera de la muestra**, y es lo que alimenta
  la política del §3.4.

Dos ejercicios sobre la misma serie y los mismos orígenes se pueden comparar
fila a fila. Más adelante, con un contraste (Diebold-Mariano), no sólo a ojo.

## 4. Dónde se guarda

En el proyecto, con la regla de siempre: **el manifiesto dice qué es; los
ficheros, cómo se hizo**.

```yaml
anadas:
  IPC_ES:
    - id: a07
      bajada: 2026-10-15
      hasta: 09/2026
      sha: 3f9c…
      revisa: false          # sólo extiende la a06
previsiones:
  E1:
    titulo: IPC mensual, boletín
    que: {serie: IPC_ES, muestra: "", modelo: m04}   # o {caso: C1, corrida: c02}
    motor: fuf
    horizonte: 12
    prob: 95
    politica: {reestimar: cada, k: 12}
    vigencias:
      - {modelo: m04, desde: 01/2026, razon: "el elegido al empezar"}
    actas:
      - {id: 2026-09, anada: a07, origen: 09/2026, modelo: m04, sha: …}
```

Los ficheros de cada acta van en `_previsiones/E1/2026-09/`: el `.inp` que
se corrió, el `.out` y un CSV con la previsión. Un acta es una carpeta que
**no se vuelve a escribir**. La tabla previsión-realización **no se guarda**:
se calcula a partir de las actas y las añadas, como la diagnosis a partir
del `.out`.

Todo esto va en `lib/`, porque lo usarán la madre y los tres motores:
- `lib/anadas`;
- `lib/previsiones`: las actas, la tabla y los estadísticos;
- el lector del CSV de previsión, si cada motor escribe el suyo.

## 5. El GUI

**Una pestaña «Previsiones» en la madre**, al lado de Series y Casos, y no un
programa aparte. Las añadas son de las series y los modelos son de la madre.
- **«Datos nuevos…»**: importa una o varias series —desde el mismo CSV o
  XLSX de la bajada, con `lib/datos`— y crea su añada. Dice cuáles extienden
  y cuáles revisan.
- **La lista de ejercicios**, con su estado: «al día», «hay datos nuevos sin
  prever», «la política propone reestimar».
- **«Actualizar»**: un acta por ejercicio pendiente, de una vez.
- **Un ejercicio abierto** enseña:
  - la línea de tiempo de sus actas y sus vigencias;
  - el **gráfico de previsiones sucesivas**: cada acta como un abanico
    desde su origen, sobre lo que luego pasó;
  - la tabla de errores por horizonte;
  - los errores a un paso con su ACF.
- **«Reestimar…»** abre el modelo en fue_gui o en drtran_gui, el mismo
  camino que «Ver modelo» y «Abrir en drtran». Al volver, propone el hijo
  como vigente desde el origen siguiente.

## 6. Los motores: ¿hace falta un fuf multivariante?

- **Univariante**: **no hace falta tocar fuf**. Basta con su entrada: el
  `.pre` del modelo, la añada como datos, las banderas a 0 y la σ² de la
  estimación. Comprobado con `CPI_USA_model_AR`: las mismas previsiones con
  todo fijo que con los parámetros libres.
- **Multivariante**: drtran y drvarma tienen el cálculo de FORSIL (`-estwin`
  con `-O -1`), pero **vuelven a estimar la ventana** en cada corrida para
  obtener los parámetros. Con el modelo congelado (§3.4) eso no vale: hace
  falta **prever con los parámetros del modelo vigente, leídos, sin estimar
  nada**. Es el «fuf multivariante». Hay dos formas:
  - **una opción** de drtran y drvarma que lea los parámetros del modelo
    (del `.out`, o de un `.cns` con todo fijo) y sólo prevea;
  - **un programa aparte**, como fuf lo es de fue.

  Las dos tocan los motores y piden el visto bueno. **Por comprobar
  primero**: si un `.cns` con todos los parámetros fijos ya hace que drtran
  sólo prevea, como pasa con fuf y las banderas a 0. Si es así, el gestor
  sólo tiene que escribir ese `.cns`.

## 7. Orden propuesto

| paso | qué | se ve |
|---|---|---|
| 1 | `lib/anadas` y «Datos nuevos…» en la madre: importar, detectar extensión o revisión, y avisar de los modelos afectados | sí |
| 2 | `lib/previsiones` con ejercicios y actas, sólo univariantes (fuf). «Actualizar» deja un acta | sí |
| 3 | La tabla previsión-realización, los estadísticos y el gráfico de previsiones sucesivas | sí |
| 4 | Vigencias y reestimación: la política propone, el analista decide, el hijo hereda el ejercicio | sí |
| 5 | El «fuf multivariante»: prever con los parámetros congelados de una corrida (§6) | — |
| 6 | Ejercicios de casos, con eso | sí |

Cada paso, su PR y la CI en verde en las tres plataformas.

## 8. Lo que el analista tiene que decidir

1. ~~Las añadas~~ — **decidido**: lo elige el analista, en modo SPS
   (registro, adelante y atrás) o en modo evaluación (sin histórico) (§3.1).
2. ~~Contra qué dato se mide el error~~ — **decidido**: los errores los
   calculan los motores; el gestor los lee (§3.5).
3. **La política de reestimación.** El modelo se congela y no se reestima
   con cada dato (§3.4, decidido). Queda por decidir si el gestor **propone**
   reestimar (cada k orígenes, o cuando los errores a un paso se salen de la
   banda) o si es siempre a mano, y si «reestimar» es sólo alargar la ventana
   o abrir la identificación.
4. **El primer alcance**: ¿sólo univariantes (pasos 1-4) y los casos
   después, o los dos desde el principio?
5. **El sitio**: ¿una pestaña de la madre, como se propone, o un programa
   aparte?
6. ~~El «fuf multivariante»~~ — **decidido en lo esencial**: hace falta,
   porque el modelo se congela (§6). Queda por decidir **la forma**: una
   opción de drtran y drvarma, o un programa aparte. Antes hay que comprobar
   si basta con un `.cns` con todo fijo.
