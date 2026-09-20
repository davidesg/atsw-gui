# `proyecto` — qué series, qué modelos, y de dónde viene cada uno

La entidad que el estudio declaró **ausente**: no existe ni en el lado C ni en
el Python. El agrupador de facto es el **directorio**.

## El nombre del fichero es cortesía

Decisión del analista, 2026-09-20.

```
IPC_ES_m03.inp      se llama así para que el directorio se entienda a ojo,
                    Y NINGÚN PROGRAMA LO LEE.
```

La identidad está en el manifiesto: **el id es una clave; la versión y el padre
son campos.** Es la conclusión de `DISENO-proyecto.md` §2, donde se demostró
que con todos los ficheros delante la convención `_mNN` se deduce **mal** — en
el mismo árbol `_m01` significa dos cosas distintas para la misma serie.

> **Si hay que parsear el nombre para saber algo, ese algo no está registrado.**

## El linaje es obligatorio; la razón, no

> «Debería registrar el porqué de cada iteración, pero **el linaje es lo mínimo
> que se debería mantener**.»

Dos exigencias de rango distinto, y el código las trata distinto:

| | rango | cómo |
|---|---|---|
| **linaje** | nunca se pierde | `pr_deriva()` lo escribe **al copiar, sin preguntar**: el programa ya sabe de qué `.pre` sale este `.inp` |
| **razón** | se quiere, no bloquea | `pr_razon()` se puede llamar **después** — mirando el `.out`, que es cuando de verdad se sabe — o no llamarse |

**Y «sin razón» se ve como sin razón.** `pr_sin_razon()` los lista con nombre y
apellido para que la ventana los **enseñe**, no los esconda. Nunca se infiere
una razón ni se pone relleno.

En `art`, `guion_node` **rechaza** la llamada sin razón, y funciona: 881 de 924
nodos la llevan. Pero eso es un LLM escribiendo. Un modal *«¿por qué?»* en cada
estimación de un GUI se contesta `asdf` a la tercera, y **una razón falsa es
peor que ninguna** porque no se distingue de una de verdad. Es la regla de la
huella vacía del guion: *no consta* nunca significa *cuadra*.

## La cadena que esto registra

```
.inp(-1) ──estimar──→ .pre(-1) ──copiar──→ .inp(0) ──estimar──→ .pre(0) …
```

`.pre` e `.inp` **son el mismo formato** —verificado: se copia un `.pre` a
`Z.inp`, se corre `fue` y sale `Z.pre`— pero el motor **exige la extensión
`.inp`**, así que el paso es una **copia física real**. `pr_camino()` la
recorre hasta la raíz.

## El manifiesto: un subconjunto de YAML, declarado

Escribe y lee `clave: valor` con sangría de 0, 2 o 4 espacios. **Lo que no
entiende lo dice** en vez de adivinarlo — un parser de YAML que adivina es peor
que no tenerlo. Una clave desconocida, un padre que no está, un ciclo en el
linaje o un identificador que no cabe **se rechazan con su línea**.

> Un identificador **no se recorta: se rechaza.** Recortar un nombre de 60
> caracteres a 47 puede hacerlo casar con **otro** —o dejar de casar con su
> padre— y las dos cosas rompen el linaje en silencio. Para el texto libre
> (título, razón) recortar es inocuo; para una clave, no.

Y lo que importa para que las dos encarnaciones del taller convivan:
**`yaml.safe_load` de Python lo lee entero**, con sus acentos. Hay una prueba
que lo comprueba.

```yaml
schema_version: 1
id: SF_MEG
titulo: Inflación del área euro
raiz: .

series:
  IPC_ES:
    elegido: m02
    razon: el SAR no se gana su sitio

modelos:
  IPC_ES/m00:
    version: 0
    padre: ""
    razon: ""
  IPC_ES/m01:
    version: 1
    padre: m00
    razon: el residuo de 2020-03 pedía intervención
```

## No es el `guion.json`, y no tiene que serlo

Son las **dos encarnaciones del taller** (`DISENO-madre.md` §10): ATSW Python
gestiona proyectos con `guion.json`, registro y `policy`; ATSW GUI, con esto.
**No convergen, y forzarlo haría peor a las dos.**

`lib/proyecto` **lee** el `guion.json` para enseñar un linaje si lo hay; no lo
escribe. Escribirlo sería que una encarnación gestionase los proyectos de la
otra.

Sin dependencias, como `lib/rutas`, `lib/tabla` y `lib/datos`.
