# TASTE como diseño

Entrega de la **fase 4** del estudio de atsw. TASTE (1987–2001) no entra aquí
como oráculo —eso está resuelto— sino como **diseño**: lo escribieron los que
inventaron esta manera de analizar series, y resolvieron el problema que la
suite de hoy no tiene resuelto.

La fase no tenía que averiguar las cinco ideas del reconocimiento lateral. Tenía
que **convertirlas en decisiones**. Eso es la §3.

Método: los 26.672 renglones de Pascal, y —lo que más ha valido— **los
artefactos**. Los ficheros de 1991-1993 siguen ahí, y dicen cosas que el código
no dice. Donde hay una cifra, sale de abrir el fichero.

---

## 1. El modelo de datos

Todo vive en un fichero, `TASTECTV.PAS` (Constantes, Tipos, Variables). Y la
constante que manda es `MNI = 12`, el número máximo de *inputs* de una función
de transferencia. El catálogo de series y el de modelos tienen su tamaño
**derivado de ahí**:

> TASTE puede tener a la vez exactamente las series que caben en el modelo de
> transferencia más grande que sabe estimar. Ni una más.

Trece series de usuario, trece modelos, 520 observaciones cada una.

### La serie

Un registro de 6907 bytes con los datos dentro (incluido el tramo `[-200..0]`
de retroprevisiones), el nombre de catálogo de 8 caracteres, tres líneas de
título, el tipo (`C` continua, `I` impulso, `E` escalón, `R` rampa, `S` Semana
Santa), fechas y estadísticos **por duplicado** —índice 1 original, índice 2
transformada— y el estado de transformación: `trdf`, `dr`, `de`, `bc`, `lost`.

Lo importante de ese estado, que el reconocimiento lateral no había fijado: **es
transitorio**. Se pone en la identificación univariante, se usa para el barrido,
y **se deshace al salir** releyendo la serie del disco espejo. No se puede decir
«CONSUMO en logs con (1,1)» y guardarlo. Si lo quieres, hay que materializarlo
como serie nueva con el menú Generar — y entonces el registro resultante pone
`trdf := FALSE` y **la única huella de su procedencia es el texto del título**:
`"CONSUMO" TRANSFORMADA Y DIFERENCIADA`. La procedencia es una cadena.

### El modelo

`USMODEL` dentro de `TFINPUT` dentro de `TFMODEL`, 3947 bytes. Órdenes, λ, d, D,
y un vector `parms[1..17]` con las posiciones libres en `point[]`.

**Y aquí está la observación que más vale de toda la fase.** Ese vector `parms`
hace de semilla **y** de estimación, a la vez, y no hay un solo bit que diga
cuál. La estimación lo pisa en sitio, en **cada iteración** del
Levenberg-Marquardt, `TFM` pasado por `VAR` desde el catálogo. Al acabar, el
modelo del catálogo *es* el óptimo y no queda copia de las semillas. Y al
reespecificar, el diálogo se repuebla con los `parms` actuales: el óptimo se
convierte automáticamente en la semilla siguiente.

Es la idea `.inp`/`.pre` de la suite actual, **sin el marcador**.

Y eso no es una lectura de código: está medido en los ficheros. El modelo
`USCONS`, misma especificación exacta (d=2, D=1, s=4, q=1, P=2, Q=1), guardado
en dos sitios:

    USCONS.TSM  (fichero suelto)     0.89770  -0.16262  -0.45259  0.89369
    USCONS  dentro de HOWREY.AT0     0.89710  -0.16273  -0.45249  0.89377

Dos corridas distintas del mismo modelo, y **nada en ninguno de los dos ficheros
dice cuál es cuál, sobre qué muestra, ni cuándo**. El `.TSM` ni siquiera lleva
el nombre dentro: va en el catálogo, aparte, así que nombre y contenido están
desacoplados en las dos direcciones.

Treinta y cinco años después, eso es exactamente el argumento del contrato
`.inp`/`.pre`. **La suite de hoy tiene ese contrato porque TASTE no lo tenía, y
la evidencia sigue en los ficheros.**

### El área de trabajo `.AT0`

Volcado binario de los registros en memoria, cada bloque precedido de su tamaño.
Verificado byte a byte:

| | bytes |
|---|---|
| `TASTE` + versión | 8 |
| `STATUS` (el registro ESTADO) | 924 |
| `DataName` — 14 nombres de 8 caracteres | 126 |
| 14 × `SERIE` | 96 726 |
| `ModlName` — 14 nombres | 126 |
| 13 × `TFMODEL` | 51 337 |
| **total** | **149 253** |

`HOWREY.AT0` y `TEST.AT0` pesan **exactamente lo mismo**: es un fichero de
tamaño fijo. 149 KB para un caso de dos series y cuatro modelos — el 97% vacío.
Y no vacío de ceros: `TEST.AT0` contiene fragmentos de texto de una utilidad de
copia de DOS. Memoria sin inicializar, volcada a disco.

**Lleva dentro estructuras del sistema operativo.** El bloque `STATUS` incluye
los *manejadores* de los ficheros abiertos —`TmpFileV` (TextRec, 256 B) y
`DatFileV` (FileRec, 128 B)— con sus buffers. Los `GRSJAM.{1}` y `JAMGRS.{2}`
que aparecen al inspeccionar el fichero son eso: los nombres de los temporales
que estaban abiertos. Al cargarlo, `Carga_AT` tiene que reasignar y recrear los
dos para reparar el destrozo.

Por eso un `.AT0` no sobrevivía a un cambio de compilador, y es el argumento más
afilado de toda la fase: **contenía el runtime, no sólo los datos**. Y
`Carga_AT` lee la versión y **no la comprueba**, y hace `BlockRead` con el
tamaño que venga en el fichero. Sin checksum.

### El catálogo

Dos arrays paralelos: los punteros a los registros y los nombres de 8
caracteres. **El índice del array es la identidad del objeto**, y todo el
programa opera por nombre a través de cuatro funciones de resolución.

Los nombres son **completamente independientes de la ruta**: el diálogo de carga
pide «NOMBRE DEL ARCHIVO» y «NOMBRE DE LA SERIE» en dos campos distintos.
`E:\TASTE\CONSUMO.BJD` se puede cargar como `X1`. La ruta queda en un campo
informativo que nunca se vuelve a leer — la serie vive entera en memoria.

El catálogo es **denso**: borrar una serie desplaza las posteriores. Los índices
no son estables. Funciona porque nada guarda índices; todo va por nombre.

### El fichero de series `.BJD`

Cabecera de **31 líneas**, datos desde la 32. Y las 31 líneas cuentan una
historia:

| línea | qué |
|---|---|
| 1 | título → se guarda |
| 2, 3 | título 2ª y 3ª línea → se guardan **y nunca se leen** |
| **4** | **se lee a una variable llamada `shit` y se tira** |
| 5-10 | frecuencia, fecha, nº de observaciones, tipo |
| **11-31** | **21 líneas reservadas, jamás usadas** |

En los dos ficheros del caso HOWREY, alguien escribió a mano en esas líneas
**las etiquetas** y dejó los valores en blanco:

    CONSUMO.BJD    MUESTRA TRIMESTRAL: | DATOS RECOGIDOS  | UNIDADES:
    RENTA.BJD      MUESTRA TRIMESTRAL: | DATOS RECOGIDOS: | UNIDADES DE MEDIDA:

Dos ficheros de la misma sesión, el mismo autor, y **los nombres de campo no
coinciden entre sí**. El analista se inventó un esquema porque el formato no lo
tenía.

Y el escritor lo remata: `Writeln(titu)` y después `FOR i:=1 to 3 DO Writeln` —
tres líneas en blanco. **Cargar un `.BJD` y volver a grabarlo destruye las
unidades y la fuente.** No es código muerto: es pérdida de datos.

---

## 2. El modelo de sesión

Una sesión de TASTE es: trece series más trece modelos en RAM, congelables en un
`.AT0`, más un fichero de texto que el analista construye a mano.

**«Dónde me quedé» son dos cadenas de ocho caracteres**: `LastSer` y `LastMod`.
Nada de posición de menú, nada de historia, nada de qué se estaba haciendo. Cada
paso precarga sus campos con la salida del anterior y los reescribe al terminar
— el orden correcto es el camino de menor resistencia.

### `TASTE.OUT`, y el matiz que corrige al reconocimiento

Cada operación escribe su informe a un **temporal**, que se borra al empezar la
siguiente. El informe se pagina en un visor cuya barra dice:

    [ESC] SALIR      [A] AÑADIR      [P] IMPRIMIR      LINEAS n-m DE total

y `A` vuelca **el informe entero que estás viendo** al final de `TASTE.OUT`.
Todo o nada: no se selecciona un fragmento. Si no pulsas `A`, la siguiente
operación lo borra.

El matiz: `TASTE.OUT` se abre con **`Rewrite` en cada arranque**. No es
append-only entre sesiones — es append-only *dentro* de una sesión, y **el
registro de ayer se destruye** salvo que lo renombres a mano. El plan lo tenía
por append-only; no lo es.

El artefacto confirma cómo funcionaba la curación: `TASTE.OUT` tiene tres
bloques —la ficha de la serie, la estimación con sus σ², la tabla de previsión—
y **ni un gráfico ni una ACF**. El analista vio siete baterías de
identificación y se quedó con tres bloques. Eso es curación funcionando.

### Lo que no se guardaba

El recorrido y el porqué. No hay un campo de nota, comentario o justificación en
ningún registro: la única cadena editable por el usuario es el título de una
serie. La diagnosis de la estimación —σ², desviaciones típicas, matriz de
correlaciones entre parámetros— se calcula y **se escribe sólo al temporal**. Si
no pulsas `A`, desaparece.

Y la ligadura modelo↔serie↔muestra no existe como dato. Lo único que registra
con qué se estimó qué es el **título** del registro de residuos —`MODELO UT
"TF2" CON SERIE OUTPUT: "CONSUMO"`— una cadena que se sobrescribe en la
estimación siguiente.

---

## 3. Las ideas, con veredicto y decisión

Ésta es la entrega, y es la entrada de la fase 5.

### 3.1 El área de trabajo — **la suite no la tiene** → adoptar, apuntando

Un fichero que reanuda un caso entero es lo que atsw no tiene. Pero el `.AT0`
enseña exactamente dónde está la frontera, y la enseña por la vía del desastre:
tamaño fijo, 97% vacío, memoria sin inicializar, manejadores del sistema
operativo dentro, sin versión comprobada y sin checksum.

**Decisión: el manifiesto contiene la IDENTIDAD y las RELACIONES; apunta al
CONTENIDO.**

- **Contiene**: el catálogo (nombre corto → ruta + hash), las relaciones (qué
  modelo se estimó con qué serie sobre qué muestra), el certificado de cada
  escalón, y el puntero de sesión.
- **Apunta**: a los `.inp`/`.pre`/`.out` y a los ficheros de datos, que ya son
  el formato de la suite y ya están probados.

El argumento del hash no es teórico: es `USCONS.TSM` frente a `USCONS` dentro
del `.AT0`, distintos en la cuarta cifra y sin nada que los distinga. **Un
manifiesto sin hashes reproduce ese fallo exacto.**

### 3.2 Nombres o rutas — **la suite lo tiene peor** → adoptar el catálogo

TASTE operaba sobre nombres cortos desacoplados de la ruta. Hoy todo son rutas
absolutas, incluso dentro del `guion.json`: un caso que se mueve de directorio
se rompe.

**Decisión: adoptar el catálogo**, con una corrección. El de TASTE era denso y
se compactaba al borrar, así que los índices no eran estables — le funcionaba
porque nada guardaba índices. Hoy el guion sí guarda referencias, así que **el
nombre es identidad estable y no se renumera nunca**. Un nombre borrado no se
reutiliza.

### 3.3 La transformada, ¿serie o estado? — **las dos tienen razón** → estado de la VISTA

TASTE dijo estado, y lo hizo **transitorio**: se deshace al salir. atsw dice que
la receta está en el `.inp`. Lo que TASTE hizo mal no fue elegir estado: fue
dejar que se materializara en una serie nueva cuya única procedencia es una
cadena de texto.

**Decisión: la transformación es estado de la VISTA, no de la serie.** La
interfaz enseña la serie transformada para identificar, y **no la escribe
nunca**. Lo que se escribe es la receta —λ, d, D— en el modelo, que es donde la
suite ya la tiene. Materializar una serie derivada es un acto explícito y lleva
su procedencia como dato, no como título.

### 3.4 Los residuos como serie de primera clase — **la suite no lo tiene** → adoptar

Idea barata y excelente, y más barata de lo que parecía: **una ranura reservada
y dos líneas** en las funciones de resolución del catálogo. Con eso, las ocho
operaciones del menú de datos —tabular, gráfico, histograma, media-DT, ACF,
PACF, CCF, estadísticos— funcionan sobre los residuos **sin una sola línea de
código adicional**.

Y la prueba de que fue intencionado: la rutina de diagnosis es literalmente la
misma secuencia de llamadas que la batería de identificación, con la cadena
`'RESIDUOS'` en lugar de la variable. **Diagnosticar es identificar otra vez**,
y el programa lo dice con su estructura, no con un manual.

La asimetría también es buena: primera clase **para leer**, excluidos **para
escribir y para enumerar** — no se pueden usar como operando de los generadores.
Con una excepción deliberada: la prohibición está **comentada** justo en la CCF,
para poder cruzar residuos contra un input. Alguien la puso y alguien la quitó a
conciencia.

**Decisión: adoptar, corrigiendo el único defecto.** TASTE tenía **una** ranura,
así que estimar un segundo modelo pisaba los residuos del primero y no se podían
comparar dos. Hoy eso no cuesta nada: **unos residuos por modelo**, nombrados
por su modelo.

### 3.5 El menú como protocolo — **la suite lo tiene peor** → adoptar, pero la forma exacta

Los cuatro pasos —Identificación · Modelo · Estimación · Previsión— aparecen
idénticos bajo univariante y bajo transferencia. No es una coincidencia de
diseño: **son el mismo bloque de datos copiado**, con las mismas teclas
`I/M/E/P`, y sólo cambian la columna de pantalla y dos frases de ayuda.

Pero la idea que hay que copiar **no es** «organizar las pestañas por paso del
método». Es más fina y más barata:

> El registro de menú de TASTE **no tiene campo de "habilitado"**. Toda opción
> está siempre disponible y falla, si falla, nombrando el objeto que le falta.

El protocolo no se impone con una máquina de estados: se impone como **grafo de
dependencias entre artefactos**. No puedes identificar una transferencia sin
haber modelado antes cada input, porque el diálogo **te pide sus nombres**. No
puedes diagnosticar residuos antes de estimar, no porque haya una bandera, sino
porque la serie `RESIDUOS` sencillamente no existe todavía.

**Decisión, en una frase: no grisar el botón — hacer que el paso pida por nombre
algo que sólo el paso anterior sabe fabricar.**

Se puede estimar sin identificar y prever sin estimar. TASTE no lo impide;
simplemente no lo pone fácil, y no deja mentir sobre qué objetos existen. Para
un GUI que enseña, eso es mejor que prohibir.

Dos cosas más del mismo menú, gratis y que la suite no tiene:

- **Una frase por opción.** Cada una de las 45 opciones lleva 50 caracteres de
  explicación que se pintan en la fila 25 al pasar por encima. Documentación en
  el punto de uso.
- **`[C] CORREGIR` como resultado de primera clase.** Todo diálogo acaba en
  Procesar / Corregir / Salir. Revisar lo que acabas de teclear no es cancelar y
  volver a empezar. Hoy los diálogos del GUI son aceptar o cancelar.

### 3.6 La tabla publicable — **nadie la tiene** → hacerla, pero capturar primero

El plan decía «señal, no casualidad». La señal es más fuerte de lo que parecía, y
tiene cuatro patas independientes:

1. El registro `TABLA` existe en el `TYPE` exportado —título, **unidades**,
   **fuente**, nº de históricos, nº de previsiones, formato, columna de
   errores— y aparece en **tres líneas de todo el árbol**: su propia
   declaración. Ninguna variable, ningún fichero, ningún campo leído jamás.
2. `Graba_Ser_PRN` y `Graba_Ser_TSF`, declarados en el interfaz, con cuerpo
   literalmente `BEGIN END;` y cero llamadas. Y el andamiaje sigue ahí: una
   variable selectora de formato declarada tres veces y nunca usada, y un
   mensaje de éxito que dice `[FORMATO BJD]` — etiquetar el formato sólo tiene
   sentido si se esperaban otros.
3. El `.BJD` reserva **21 líneas de cabecera** que nunca se usaron, y la línea
   de las unidades se lee a una variable llamada `shit`.
4. Los dos campos de título extra son **de sólo escritura** en todo el programa:
   51 apariciones, todas asignaciones. Nunca se leen, nunca se muestran. Y el
   escritor los destruye al grabar.

**Decisión, y el orden importa: primero capturar, después la tabla.** El módulo
no se escribió, pero el problema de fondo es anterior — **los datos que habría
necesitado nunca se capturaron, porque no había dónde ponerlos**. Unidades,
fuente y descripción tienen que ser **campos estructurados**, no líneas libres.
La prueba de que las líneas libres no bastan está medida: dos ficheros del mismo
autor, en la misma sesión, con nombres de campo distintos.

---

## 4. Lo que la suite de hoy hace mejor

Conviene decirlo, porque la fase podría leerse como nostalgia.

- **El contrato `.inp`/`.pre`.** TASTE tenía un solo vector que era semilla y
  estimación a la vez, y la estimación lo pisaba en sitio. Medido en sus propios
  ficheros: dos copias del mismo modelo, distintas, sin nada que las distinga.
- **El guion.** TASTE no guardaba el recorrido ni el porqué, y no tenía dónde:
  no hay un campo de nota en ningún registro.
- **La diagnosis persistida.** La de TASTE se calculaba y se escribía sólo al
  temporal.
- **Comparar modelos.** Un solo juego de residuos, ningún campo de bondad de
  ajuste comparable. Para comparar dos modelos había que tener los dos bloques
  pegados en `TASTE.OUT` y leerlos con los ojos.

Y una que la suite tiene peor sin darse cuenta: **un solo canal de salida, y es
texto.** Tablas, gráficos, ACFs y diagnósticos acababan todos en el mismo
fichero de texto, y por eso todo era uniformemente paginable, imprimible,
archivable y —hoy— *greppable*. Es lo que un GUI moderno rompe por defecto y lo
que más costaría recuperar.

---

## 5. Lo que quedó sin escribir, y qué dice

El motor econométrico de TASTE está completo hasta los autovalores de las
matrices de correlación cruzada. Lo que falta es siempre **la última milla de
presentación**: la tabla publicable, la exportación a otros formatos, la ayuda
en línea (con su índice de contexto diseñado y persistido en el `.AT0`, y nunca
leído), el modo batch, y los p-valores de los t-ratios —comentados ocho veces,
una por rama de parámetro—.

Se construyó el instrumento y se dejó sin construir el informe.

**Ése es exactamente el hueco que la interfaz madre tiene que llenar**, y la
fase 5 debería tomarlo como su primer requisito y no como el último.

---

## 6. Estado de la fase 4

**Cerrada.** Las seis decisiones de §3 son la entrada de la fase 5.

Lo que cambió respecto al plan:

- **El estado de transformación era TRANSITORIO**, no una propiedad guardada de
  la serie. Eso cambia la decisión: es estado de la vista.
- **`TASTE.OUT` no es append-only entre sesiones**: se trunca al arrancar.
- **El `.AT0` llevaba dentro manejadores del sistema operativo**, que es la
  razón concreta de por qué no sobrevivía a un cambio de compilador, y el
  argumento más claro de «apuntar, no contener».
- **El menú no impone orden y hace bien**: el protocolo está en el grafo de
  dependencias entre artefactos, no en una máquina de estados. Esa es la idea
  que hay que copiar, y no la organización de las pestañas.
- **La tabla publicable no se escribió porque los datos nunca se capturaron.**
  El orden de la solución es el inverso del que parecía.

Siguiente según el orden fijado: **fase 5, el proyecto**.
