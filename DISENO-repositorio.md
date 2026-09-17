# `atsw-gui`: el repositorio de la mitad en C

Decisión de arquitectura, no una fase del estudio — pero sale de lo que las
seis fases midieron.

**Decidido (2026-09-17):** el nombre es **`atsw-gui`**, y agrupa **los GUIs y
sus motores** en un solo repositorio. La mitad en Python (art, fue-python,
drtran-python, drvarma-python y el paraguas `atsw` de PyPI) **no se toca**.

---

## 1. El criterio, y de dónde sale

Sale de una cosa que el estudio midió seis veces:

> **Cada divergencia que apareció en las seis fases tiene la misma causa: el
> mismo fichero viviendo en dos árboles.**

| fichero | copias vivas | estado |
|---|---|---|
| `fugdraw.c` (+`.h`, +`fd_metrics.h`) | **4** | byte a byte **idénticas** |
| `fugplot.c` | 2 | idénticas |
| `inpcheck.c` | 2 pares | 2 y 4 líneas de diferencia |
| `preview.c` | 2 | **456 líneas** de diferencia, y en dos toolkits |
| `nlatools.c` / `nlutils.c` | **7** | todas distintas, de 544 a 1106 líneas |
| lectores del `.inp` | 4-5 | de 600 líneas comparables, sólo 218 coinciden |

Y la prueba de que no es un riesgo teórico: **`inpcheck_fue.c` divergió dentro
de una sola sesión de trabajo.** Se arregló el motor —una cota que evitaba que
once regresores externos destruyeran el montón— y no la copia, así que el GUI
se quedó cargando el fichero que reventaba. Se cazó de rebote, porque
`acuerdo.sh` lo expuso por casualidad.

`fugdraw.c` está hoy idéntico en cuatro sitios. Eso no es una virtud del
diseño: es la misma suerte que tuvo `inpcheck` hasta que se le acabó.

**De ahí el criterio: la estructura tiene que hacer imposible la copia, no
detectable.** `copias.sh` es un buen guardia, pero es un guardia contra un
peligro que crea la disposición de los ficheros.

### Por qué monorepo y no submódulos

Una razón, y es suficiente: **el corpus de conformidad cruza los programas.**

La batería compara el escritor del GUI contra el lector del motor contra el de
Python. Esa prueba **no tiene sitio en un repo por programa** — y de hecho hoy
vive donde no había repo ninguno. Una prueba que abarca N programas pertenece
al repositorio que contiene los N programas.

La razón secundaria: los submódulos son *exactamente* el mecanismo por el que
`fugdraw` acabó copiado cuatro veces. La fricción de actualizar un submódulo es
lo que hace que la gente copie.

---

## 2. Qué entra y qué no

### Entra

| | de dónde | líneas C |
|---|---|---|
| motor **fue** | `atws/fue/fue-1.14` (repo, rama `fue-1.14`) | 11 708 |
| motor **fuf** | `atws/fuf/fuf-1.09` (repo, rama `fuf-1.09`) | 9 670 |
| motor **fug** | `atws/fug/fug-1.14-proto` (**sin git**) | 9 694 |
| motor **drtran** | `drtran` (repo, `main`) | 13 255 |
| motor **drvarma** | `drvarma_source/drvarma_v.04.1` (repo, `master`) | 28 392 |
| GUI de **fue** | `gtk_fue.09` (repo, `master`) | 16 608 |
| GUI de **fug** | `fug-1.14-proto/gui` (**sin git**) | 3 765 |
| GUI de **drvarma** | dentro de `drvarma_v.04.1/gui` | (incluido arriba) |
| **conformidad** | `atws/conformidad` (repo nuevo, `527f882`) | — |
| **docs** | los siete documentos del estudio | — |

≈ **92 000 líneas de C**. Un monorepo perfectamente manejable.

### No entra

- **La mitad en Python.** Está en PyPI, art-mcp la usa a diario, y la fase 1
  midió que el puerto es la implementación **menos fiel** de las seis. Moverla
  es otra decisión, y no ésta.
- **Las versiones archivadas.** `fue-1.01`…`1.13`, `fug-1.01`…`1.13`,
  `drvus-source/*`, `gtk_fue.01`…`.08`. Hay ~100 copias de `nlatools.c` en el
  árbol y la inmensa mayoría son archivo, no duplicación viva.
- **`gtk_fmg.11`.** Superado por `fug-1.14-proto/gui` (decidido 2026-09-17).
  Pasa a archivo.
- **GSL**, los manuales, los zips de Windows, y **el fichero de códigos de
  recuperación de PyPI** que hay suelto en `atws/`.

---

## 3. La forma

```
atsw-gui/
  lib/              fugdraw/  preview/  engine/  outfile/  utils/  inpcheck/
  engines/          fue/  fuf/  fug/  drtran/  drvarma/
  gui/              fue/  fug/  drtran/  drvarma/     (+ madre, cuando exista)
  conformidad/      bateria.sh  acuerdo.sh  copias.sh  corpus/
  docs/             los documentos del estudio
```

### `lib/` sale gratis

De las **9 840 líneas de `gtk_fue.09`, 4 625 — el 47 % — no son de fue**:
`fugdraw`, `preview`, `engine`, `nlutils`, `outfile`, `utils` e `inpcheck` no
mencionan `FueContext`, ni `fue_globals`, ni `Tm.`, ni `Ts.` **ni una sola
vez**.

No es un refactor: es una **mudanza**. Y estaba prevista por escrito —
`include/previewhost.h` lo dice en su cabecera:

> *«When the two are factored into a library this is the header that stays
> different.»*

La superficie de acoplamiento de `preview.c` son **tres cosas**: un `typedef`,
`preview_open_external()` y `preview_show_status()`. Para cada programa nuevo
se escribe un `previewhost.h` de treinta líneas y **`preview.c` entra intacto**,
con zoom y lupa incluidos.

---

## 4. La mudanza, sin perder historia

**Hay 182 commits repartidos en cinco repos.** `git init` los tiraría.
`git subtree add` los trae.

### El aviso que hay que leer antes

    fue-1.14       rama fue-1.14     29 commits   SIN UPSTREAM
    fuf-1.09       rama fuf-1.09     25 commits   SIN UPSTREAM
    gtk_fue.09     rama master       25 commits   23 SIN SUBIR
    drvarma_v.04.1 rama master       13 commits    3 SIN SUBIR
    drtran         rama main         90 commits    al dia

**Cuatro de los cinco tienen trabajo que no está en GitHub**, y dos están en
ramas sin upstream. La mudanza tiene que tomar la historia **de los
directorios locales**, no de los remotos, o se pierde.

### El orden

```sh
# 0. El repo, con lo que ya está versionado como raíz
git init atsw-gui && cd atsw-gui
git subtree add --prefix=docs          ~/Dropbox/SRC/atws          master

# 1. Los cinco, cada uno desde su rama LOCAL
git subtree add --prefix=engines/fue     ~/Dropbox/SRC/atws/fue/fue-1.14   fue-1.14
git subtree add --prefix=engines/fuf     ~/Dropbox/SRC/atws/fuf/fuf-1.09   fuf-1.09
git subtree add --prefix=engines/drtran  ~/Dropbox/SRC/drtran              main
git subtree add --prefix=engines/drvarma ~/Dropbox/SRC/drvarma_source/drvarma_v.04.1 master
git subtree add --prefix=gui/fue         ~/Dropbox/SRC/gtk_fue.09          master

# 2. fug no tiene git: entra como commit normal, y se dice que no tenía
cp -a ~/Dropbox/SRC/atws/fug/fug-1.14-proto engines/fug
git add engines/fug && git commit -m "fug 1.14-proto, que no estaba bajo git"
```

`conformidad/` y `docs/` vienen juntos en el primer subtree, porque el repo de
`atws` (`527f882`) ya contiene los dos.

### Después de la mudanza, y sólo después: `lib/`

```sh
git mv gui/fue/src/fugdraw.c   lib/fugdraw/
git rm engines/{fue,fuf,fug}/src/fugdraw.c        # eran idénticas: md5 igual
```

**La prueba de que la mudanza no cambió nada es que los binarios siguen
produciendo la misma salida** — y eso ya se puede comprobar, porque el banco de
conformidad está en el mismo repositorio. Ése es el dividendo del monorepo,
cobrado el primer día.

---

## 5. Tres cosas que NO hacer

**No reconciliar `nlatools` en la mudanza.** Siete copias vivas separadas entre
13 y 80 líneas — drtran y drvarma difieren en **13 líneas de 1106**, son primos
cercanos. Pero es el núcleo numérico, y tocarlo mueve números. **La mudanza
tiene que preservar el comportamiento; reconciliar numéricas no lo preserva.**
Que entre después, con la batería delante y midiendo.

**No mover el lado Python.** Dicho arriba, y vale la pena repetirlo porque la
tentación de «ya que estamos» es real.

**No empezar por la interfaz madre.** Es la recomendación P3 del plan y sigue
en pie: hacer la madre primero es diseñar un gestor para un solo cliente, que
es como se diseñan los gestores que no sirven.

---

## 6. El orden del trabajo

1. **La mudanza y `lib/`.** Elimina de golpe 2 262 líneas de duplicado en
   `fugdraw`, 783 en `inpcheck` y 1 154 del `preview.c` obsoleto de fug. Es
   obra segura —los ficheros son idénticos— y es prerrequisito de todo.
2. **fug a GTK3.** ~74 sitios de API vieja, 50 en un solo fichero, sin dibujo
   ni señales antiguas: medio día. Deja **los tres GUIs en GTK3**, que es lo
   que hace posible una madre única.
3. **El GUI de drtran**, como segundo cliente de `lib/`. Unas 8 700 líneas, de
   las que **3 000 ya existen**. Y algo que conviene subrayar: **2 500 líneas
   del GUI de fue desaparecen** — los editores del modelo univariante no hacen
   falta, porque drtran **parte de `.pre` ya estimados**. No edita el modelo
   univariante: lo lee. Eso es el contrato haciendo su trabajo.
4. **La interfaz madre**, al final, ya con dos programas pidiéndole cosas.

---

## 7. El nombre

**`atsw-gui`.**

`atsw` ya existe y ya significa la familia: *A Time Series Workshop*, 1.4.0 en
PyPI, descrito como *«the Box-Jenkins-Treadway suite… with three MCP
assistants»*. Llamar `atsw` al repositorio de C habría sido defendible —la
familia tiene dos mitades— pero deja ambiguo qué trae `pip install atsw`, que
seguiría siendo sólo la mitad Python.

`atsw-gui` quita la ambigüedad a coste cero y dice lo que es.

---

## 8. Lo que este documento no decide

- **Si `atsw-gui` se sube a GitHub, y cuándo.** El repositorio de `atws`
  (`527f882`) se creó **sin remoto**, a propósito. Subir es una decisión
  aparte, y conviene tomarla mirando qué hay dentro: el corpus de conformidad
  lleva 121 ficheros de series reales.
- **Qué licencia.** Los motores llevan GPL v2 o posterior; TASTE, en cambio,
  tiene dueño declarado y `PROCEDENCIA.md` pide leerlo antes de compartir nada.
  TASTE no entra en `atsw-gui`, pero el oráculo lo invoca.
- **Qué hacer con las ~100 versiones archivadas.** No entran, pero tampoco se
  borran por este documento.
