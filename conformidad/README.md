# La batería de conformidad del formato univariante

El `.inp`/`.pre` univariante está implementado **seis** veces. Esta batería
mide si las que ESCRIBEN se comportan igual: coge un corpus de ficheros, los
pasa por el lector y el escritor de cada una, y compara el **modelo** que sale
con el que entró.

No compara bytes. Dos implementaciones del mismo formato no van a escribir los
mismos bytes nunca —espaciado, decimales, el orden de los comentarios— y eso
no importa. Lo que importa es que quien lo lea después vea lo mismo.

    sh bateria.sh                 los cuatro actores
    sh bateria.sh gtkfue          uno solo
    sh bateria.sh motor corpus2   otro corpus

El detalle de cada fallo queda en `bateria.log`.

## Los cuatro actores

| actor | qué es | dónde |
|---|---|---|
| `gtkfue` | el lector+escritor en C del GUI | `gtk_fue.09/src/file_io.c` |
| `pyart`  | el escritor de art | `art/pipeline.py::_write_inp` |
| `pypre`  | el escritor `.pre` de Python | `fue/report.py::write_pre` |
| `motor`  | el propio `fue` en C | `fue-1.14/src/fue.c` |

Las otras dos implementaciones no escriben, así que no son actores: el lector
de Python (`fue/inp.py::load`) es **el juez** —es quien lee los dos ficheros y
dice si son el mismo modelo— y `inpcheck.c` es la puerta que los actores en C
usan para rechazar lo que no es de su dialecto.

Que el juez sea una de las implementaciones tiene un precio: **la batería sólo
puede medir lo que el juez conserva.** Hay dos campos que el lector de Python
descarta —`cbands` y la frecuencia `number`— y para esos dos el banco está
ciego. Están documentados en `corpus/HUECOS.md` y reportados (fue BUG-0018).

## Copia frente a estimación

`gtkfue` y `pyart` **copian**: el fichero que sale tiene que decir lo mismo que
el que entró, valores incluidos.

`pypre` y `motor` **estiman**, y su `.pre` lleva las estimaciones — otros
valores por definición. A ésos se les compara en modo estructura
(`comparar.py --estructura`): todo menos los valores que el fichero de partida
declaraba **libres**. Los **fijos** se siguen comparando, porque un parámetro
fijo que se mueve es un fallo de cualquiera.

## Códigos de salida de un actor

    0   escribió el fichero
    1   no pudo
    3   la estimación no salió  (no es un fallo del formato)
    4   rechazó el fichero en la puerta: es de otro dialecto (fuf, fug)

## El corpus

`corpus/` son en su mayoría ficheros reales, de trabajos de verdad. Los que
empiezan por `hueco_` no: se construyeron para ejercitar casos que ninguno de
los reales tocaba, y cada uno de esos huecos había dejado pasar un fallo
durante años. Están explicados uno a uno en `corpus/HUECOS.md`, con lo que
falta al final.

## La otra herramienta: `acuerdo.sh`

La batería mide a los **escritores**: si lo que sale dice lo mismo que lo que
entró. `acuerdo.sh` mide otra cosa, y es una promesa distinta: que los dos
**lectores** —`inpcheck.c` en C, `fue.load()` en Python— estén de acuerdo en
qué ficheros son legibles, de qué dialecto, y **qué dicen**.

    sh acuerdo.sh

Hace falta aparte porque hay un fallo que la batería no puede ver por
construcción: **cuando el escritor es fiel, un fichero mal leído se reescribe
igual de mal**, y el juez —que es uno de los dos lectores— compara entonces su
propia lectura equivocada consigo misma y dice «iguales».

Pasó de verdad. `hueco_nombre_num.inp` es una serie anual llamada `2020`:

    C:      nobs=258 freq=1 year=1766 period=1 name=2020
    Python: nobs=258 freq=1 year=2020 period=1 name=series

254 años de diferencia en el origen, y el nombre perdido. La batería lo da por
bueno; `acuerdo.sh` lo ve.

Los veredictos son `ok`, `C-no`, `py-no`, `dialecto` y `cabecera`, y el detalle
queda en `acuerdo.log`.

## Las copias tienen que seguir siendo copias

`gtk_fue.09/src/inpcheck_fue.c` es `fue-1.14/src/inpcheck.c` copiado, y sólo
cambia el nombre de la función. Eso es lo único que garantiza que el GUI
acepte **exactamente** lo que el motor acepta.

Ya se desincronizó una vez, y lo destapó `acuerdo.sh`: el motor rechazaba un
fichero que el GUI seguía cargando. Cuando se toque uno hay que tocar el otro,
y comprobarlo:

    diff <(sed 's/inp_check_fue/inp_check/' ../../gtk_fue.09/src/inpcheck_fue.c) \
         ../fue/fue-1.14/src/inpcheck.c

Lo que **no** va en esa copia son los límites propios del GUI —sus vectores
estáticos son más pequeños que los del motor—: ésos viven en
`inp_fits_gui()` (`gtk_fue.09/src/utils.c`), que es su puerta y no la
compartida.

## Qué encontró

Lo que la batería ha destapado y lo que se decidió con ello está en
`../CONTRATO.md`, que es el documento; esto es sólo la herramienta.
