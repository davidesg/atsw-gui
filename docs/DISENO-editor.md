# El editor del `.inp`

Entrega de la **fase 2** del estudio de atsw. Es la que esperaba: se pospuso a
propósito hasta tener el contrato cerrado (fase 1) y saber lo que la escalera
(fase 3) y el proyecto (fase 5) le iban a pedir.

**Hecho e instalado en `gtk_fue.09`** (`0498367`), con prueba conducida desde el
código.

---

## 1. El fallo tenía nombre, y eran dos funciones

```
on_save_inp          (el "Save" de la barra)        escribe desde el MODELO
on_save_inp_clicked  (el "Save .inp" de la consola) escribe desde el BUFFER
```

Dos botones que se llaman Save, escribiendo desde dos sitios distintos. Y
`on_run_fue` empezaba llamando **al del modelo**:

```c
void on_run_fue(GtkWidget *widget, FueContext *ctx) {
    on_save_inp(NULL, ctx);   /* guarda el .inp actual */
```

De modo que la secuencia *Edit .inp → tocar → Save .inp → Run* reescribía el
fichero desde las pestañas y **la edición desaparecía sin decir nada**. El
motor leía el modelo, no lo editado.

Es el fallo de **dos dueños y gana el que escribe el último**, y estaba
demostrado antes de tocar nada: la prueba `tests/test_editor.c` hace la
secuencia del usuario y, con el código de entonces, fallaba exactamente en dos
sitios —el fichero tras Run, y el modelo en memoria— y en ninguno más.

---

## 2. Las cuatro decisiones

### 2.1 Un solo dueño: **guardar recarga**

El plan decía que había dos opciones y no una tercera: o el editor recarga a
las pestañas al guardar, o las pestañas se bloquean mientras se edita el texto.

**Recargar**, por tres razones:

1. Es lo que se pidió: *«un editor que pueda modificar `.inp` y `.pre` y que
   esto se recargue al gui»*.
2. Bloquear es un **modo**, y la fase 4 midió que los modos son peores que las
   dependencias: TASTE no tiene ni campo de «habilitado» en su registro de
   menú, y el protocolo se impone por lo que existe, no por lo que está gris.
3. Recargando, el modelo vuelve a ser dueño **en el instante siguiente al
   guardado**. Hay un solo dueño en cada momento: el texto mientras se edita, el
   modelo en cuanto se guarda.

El único rato en que el fichero y las pestañas pueden discrepar está marcado en
el contexto (`editing_path`), y termina al guardar.

### 2.2 Validar al guardar, **por la puerta del motor**

El texto va primero a un temporal (`<modelo>.inp.editing`) y se pasa por
`inp_check_fue` — la misma copia de `inpcheck.c` que usa el motor. Si no vale:

- **el fichero que había no se toca** — un guardado fallido no puede costar
  trabajo;
- se dice el motivo **con su línea**: `Not saved -- line 2: the file ends too soon`;
- y **la consola sigue editable**, para corregir sin volver a empezar. Es el
  `[C] CORREGIR` de TASTE, que la fase 4 señaló como resultado de primera clase.

Guardar algo que el motor no podría leer es dejar el sistema mintiendo. Y la
lección de la fase 1 es que **una propiedad que sólo se sostiene si todos se
acuerdan es una costumbre; lo que la convierte en propiedad es que la operación
prohibida falle ruidosamente**.

### 2.3 El `.pre` se puede editar, y al guardar sale un `.inp`

Botón propio, `Edit .pre`. Y la regla del contrato, hecha comportamiento:

> Un `.pre` es un **óptimo**: su promesa es que corriendo `fue` sobre él los
> números no se mueven. Tocarlo deshace esa promesa. Así que se abre para
> mirarlo y para partir de él, y **lo que se guarda es el `.inp`**.

**El `.pre` no se sobrescribe nunca desde el editor.** Y se dice, antes y
después:

```
Editing the .pre. Saving writes the .inp: a .pre that is touched is a
specification again.

Saved as .inp and reloaded: it came from the .pre, so it is a specification
again, and the .pre and .out beside it no longer describe it.
```

Esto es lo que hace que este GUI **enseñe**: el contrato deja de ser una frase
de un documento y pasa a ser lo que ves al pulsar un botón.

### 2.4 La terna: avisar, no borrar

Al guardar un `.inp` que cambia, el `.pre` y el `.out` de al lado son de otro
modelo. **No se borran** —borrar trabajo ajeno no es cosa de un editor— pero se
nombra cuáles y se dice que ya no describen lo que hay en el `.inp`.

Importa porque el contrato dice que **los errores típicos se leen del `.out` y
nunca de reejecutar un `.pre`** (la grieta BUG-0027/0061 de la fase 1). Un
`.out` rancio junto a un `.inp` nuevo es exactamente la situación en que esa
regla se rompe sin darse cuenta.

---

## 3. Qué edita: texto, con la estructura al lado

Se mantiene la decisión del plan, y la fase 4 la refuerza. El valor de este GUI
es **enseñar las tripas** — es su razón de ser frente a la suite conversacional
— y las tripas de un modelo de esta escuela son un fichero de texto con
secciones.

El editor estructurado por secciones ya existe: **son las pestañas**. Tenerlas
al lado del texto, y que se recarguen al guardar, es tener las dos vistas del
mismo objeto — que es lo que la fase 5 pide para la transformada («estado de la
vista, no de la serie»).

---

## 4. La prueba

`gtk_fue.09/tests/test_editor.c`, conducida desde el código, sin manos.
Levanta la ventana real (sin mostrarla), hace la secuencia del usuario y
comprueba siete cosas:

1. `Edit .inp` trae el fichero a la consola y lo deja editable.
2. `Save .inp` deja la edición **en el fichero**.
3. **`Run` no la borra** — el motor lee lo que se editó.
4. Tras guardar, el **modelo en memoria** está de acuerdo con el fichero.
5. Un texto inválido **no toca** el fichero que había, y se dice por qué con su
   línea, y sigue editable.
6. `Edit .pre` abre el `.pre` y avisa de que al guardar sale un `.inp`.
7. Al guardar desde un `.pre`: **el `.pre` no cambia**, lo editado va al `.inp`,
   y se dice que ahora es una especificación.

Antes del arreglo fallaban la 3 y la 4, y ninguna más. Ahora no falla ninguna.

    53 comprobaciones, 0 fallos
    corpus de conformidad: 102 pasan, 0 fallan, 18 apartados
    copias de inpcheck: al dia

---

## 5. Lo que queda, y por qué no está aquí

- **Resaltado de sintaxis por secciones.** Es cómodo y no es urgente: la
  prioridad que fijó la fase 0 es *primero que no mienta, después que enseñe, y
  sólo después que sea cómodo*. Esto es lo tercero.
- **Editar un fichero de otro directorio.** Hoy el editor trabaja sobre
  `<área de trabajo>/<modelo>.{inp,pre}`. Abrir uno cualquiera es fácil, pero
  choca con lo que la fase 5 acaba de concluir: **la raíz tiene que estar
  declarada**. Cuando exista el manifiesto del proyecto, el selector abre dentro
  de él; antes de eso, abrir por ahí es invitar a las rutas absolutas que ya
  rompieron el 42% del linaje.
- **Escribir el guion.** Al guardar un `.inp` editado a mano se está creando una
  versión del modelo, y eso merece una entrada con su razón. Pero el guion es
  del proyecto (fase 5) y su alcance hay que arreglarlo antes — es la decisión
  P2-B del plan, y va después.

---

## 6. Estado de la fase 2

**Cerrada, e implementada.** Lo que cambió respecto al plan:

- **El fallo no era conceptual, era dos funciones homónimas.** `on_save_inp` y
  `on_save_inp_clicked`, escribiendo desde el modelo y desde el buffer. Merece
  la pena decirlo porque el diseño correcto ya estaba: lo que faltaba era que
  hubiera un solo camino.
- **La validación al guardar salió gratis**, porque `inpcheck` ya estaba en el
  GUI desde la fase 1. Es el primer dividendo de haber hecho el contrato
  primero.
- **El `.pre` resultó ser la parte pedagógica.** No es una restricción que haya
  que explicar en un manual: es un botón que al pulsarlo te dice por qué lo que
  vas a guardar ya no es lo que abriste.

Queda la **fase 6**: la interfaz madre y el GUI de drtran.
