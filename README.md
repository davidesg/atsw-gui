# atsw-gui

Los **GUIs** de la suite ATSW y **sus motores**, en un solo repositorio.

ATSW es *A Time Series Workshop*: la suite Box-Jenkins-Treadway. Tiene dos
mitades. **Ésta es la mitad en C** — los motores y las interfaces gráficas. La
otra mitad, en Python, con los tres asistentes MCP, vive aparte y se instala con
`pip install atsw`.

```
lib/          lo que comparten todos           (pendiente de extraer)
engines/      fue  fuf  fug  drtran  drvarma
gui/          fue  fug  drvarma                (+ drtran, por escribir)
conformidad/  el banco de pruebas del formato
docs/         el estudio del que salió esta forma
```

## Por qué un solo repositorio

Porque cada divergencia que encontró el estudio tiene la misma causa: **el
mismo fichero viviendo en dos árboles.**

| fichero | copias vivas | estado |
|---|---|---|
| `fugdraw.c` | 4 | byte a byte idénticas |
| `inpcheck.c` | 2 pares | 2 y 4 líneas |
| `preview.c` | 2 | **456 líneas** de diferencia, y en dos toolkits |
| `nlatools.c` | 7 | todas distintas, de 544 a 1106 líneas |

Y no es un riesgo teórico: **`inpcheck_fue.c` divergió dentro de una sola sesión
de trabajo**. Se arregló una cota en el motor —la que evitaba que once
regresores externos destruyeran el montón— y no la copia, así que el GUI se
quedó cargando el fichero que reventaba. Se cazó de rebote.

`fugdraw.c` está hoy idéntico en cuatro sitios. Eso no es virtud del diseño: es
la misma suerte que tuvo `inpcheck` hasta que se le acabó.

**La estructura tiene que hacer imposible la copia, no detectable.**

Y la razón de que sea monorepo y no submódulos es una sola: **el corpus de
conformidad cruza los programas.** Compara el escritor del GUI contra el lector
del motor contra el de Python. Esa prueba no tiene sitio en un repositorio por
programa.

## El banco

    cd conformidad
    sh bateria.sh      los ESCRITORES: ¿conservan el modelo?
    sh acuerdo.sh      los LECTORES:   ¿entienden lo mismo?
    bash copias.sh     ¿sigue la inpcheck del GUI siendo la del motor?

Hacen falta los tres porque ninguno ve lo de los otros. `bateria.sh` no puede
ver un fallo de lectura cuando el escritor es fiel — el fichero mal leído se
reescribe igual de mal y el juez compara su propia lectura equivocada consigo
misma. `acuerdo.sh` no puede ver un escritor que pierde un campo.

Estado al cerrar la mudanza:

    bateria.sh gtkfue   102 pasan, 0 fallan, 18 apartados
    acuerdo.sh          118 de acuerdo, 1 C-no, 1 leido distinto
    copias.sh           las copias estan al dia

## El estudio

`docs/` contiene las seis fases que llevaron a esta forma. `ESTUDIO-atsw-PLAN.md`
es el índice: sus cabeceras `FASE N — CERRADA` resumen cada una en media página.

El que más importa para entender el repositorio es
**`docs/DISENO-repositorio.md`**; el que más importa para trabajar con los
ficheros es **`docs/CONTRATO.md`**.

## Lo que falta, por orden

1. **Extraer `lib/`.** De las 9 840 líneas del GUI de fue, **4 625 — el 47 % —
   no mencionan `FueContext` ni los globales de fue ni una vez**. No es un
   refactor, es una mudanza; y `include/previewhost.h` ya dice en su cabecera
   que estaba prevista.
2. **fug a GTK3.** ~74 sitios de API vieja, 50 en un solo fichero. Deja los tres
   GUIs en GTK3.
3. **El GUI de drtran**, como segundo cliente de `lib/`.
4. **La interfaz madre**, al final, ya con dos programas pidiéndole cosas.

Lo que **no** se hace en la mudanza, y conviene que siga sin hacerse hasta que
la batería pueda medirlo: **reconciliar `nlatools`**. Siete copias separadas
entre 13 y 80 líneas, primos cercanos, pero es el núcleo numérico y tocarlo
mueve números.

## Licencia

Los motores llevan GPL v2 o posterior. `docs/DISENO-repositorio.md` §8 deja
anotado lo que aún no está decidido, incluido el caso de TASTE — que no está
aquí, pero que el oráculo invoca y que tiene dueño declarado.
