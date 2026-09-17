# Los ficheros construidos

El grueso del corpus son ficheros reales, de trabajos de verdad. Éstos no: se
construyeron a mano, cada uno para ejercitar **una** cosa que ninguno de los
reales tocaba. Un corpus de conformidad vale por los casos que tiene, y el
hueco de cada uno de éstos había dejado pasar un fallo durante años.

Todos derivan de un fichero real cambiando una línea, y **los siete los acepta
el motor** (`fue` 1.14, con su `inpcheck`): son ficheros legítimos, no basura.

| fichero | de | qué ejercita | quién caía |
|---|---|---|---|
| `RIPC.3.delta2.inp` | `RIPC.3.inp` | un determinista con **dos δ** | el motor escribía los pares pegados (`1.8400  1-0.8631  1`) y el `.pre` no lo releía **ni él mismo** |
| `hueco_mu_fijo.inp` | `RIPC.3.inp` | **μ fijo no nulo** (bandera 0, valor −88.72) | los cuatro escritores emitían un `0` pelado y tiraban el valor. Arreglado en G |
| `hueco_lambda.inp` | `RIPC.3.inp` | **λ = 1/3**, no representable en 2 decimales | E y los dos de Python lo dejan en `0.33`. G ya usaba `inp_format` |
| `hueco_cbands.inp` | `RIPC.3.inp` | **cbands = 2.5** y refactor = 50 | *punto ciego*: el lector de Python no guarda cbands, así que el juez no lo ve (fue BUG-0018) |
| `hueco_regresor.inp` | `R.2_5.inp` | un regresor externo **con nombre propio** (`XREG_PIB`) | el del fichero original se llamaba literalmente `non-standard`, que es la etiqueta que escriben los que pierden el nombre: invisible |
| `hueco_number.inp` | `en4_ar18.inp` | serie **sin fechar** (frecuencia `number`) | *punto ciego*: el lector lo mete en una variable local que nunca sale (fue BUG-0018) |
| `hueco_armonico.inp` | `RIPC.3.inp` | **`cos 1.5`**, armónico no entero | G lo guardaba en un `int` y escribía `cos 1`. Arreglado |
| `hueco_freq6.inp` | `R.2_5.inp` | **frecuencia 6**, ni 4 ni 12 | los `ifadf` se leen, se guardan, se reescriben **y el motor los ignora** al construir el operador (`fue.c:950-965`) |
| `hueco_trend.inp` | `RIPC.3.inp` | un determinista **`trend`**, que no lleva fecha | G escribía `trend 0 0`, un fichero que su propio `inpcheck` rechaza. Arreglado |
| `hueco_freqfix.inp` | `ES_CPI_mSto.inp` | operador de frecuencia fija con **k = 3.5** | G truncaba a 3 y el motor **redondea** a 4. Los dos cambian el armónico 2π*k*/*s*. Arreglado en G |
| `hueco_11nonstd.inp` | `R.2_5.inp` | **11 regresores externos**, uno más que `NT = 10` | el motor **destruía el montón** (`Fatal glibc error: malloc.c:2599`). Arreglado: `inpcheck` lo para con su línea |
| `hueco_nombre_num.inp` | `en4_ar18.inp` | una serie anual **llamada `2020`** | *punto ciego de la batería*: los dos lectores lo aceptan y **leen cosas distintas** — el C empieza en 1766 y Python en 2020 (fue BUG-0022) |

## Los tres puntos ciegos

Hay tres ficheros que **la batería no puede juzgar**, y cada uno enseña un
límite distinto del banco.

**`hueco_cbands.inp` y `hueco_number.inp`** — el juez es `fue.load()` y el
lector descarta esos dos campos, así que pasan «iguales» haga lo que haga el
escritor (fue BUG-0018). *Una batería sólo puede medir lo que su juez
conserva.*

**`hueco_nombre_num.inp`** — éste es peor y más interesante. Los dos lectores
lo aceptan, y el escritor en C lo reescribe **byte a byte igual**; el juez
compara entonces su propia lectura equivocada consigo misma y dice «iguales».
El fallo no está en ningún escritor: está en que los dos **lectores** no
entienden lo mismo. *Cuando el escritor es fiel, un fichero mal leído se
reescribe igual de mal.*

Para ése hizo falta otra herramienta: `acuerdo.sh`, que compara lo que cada
lector **entiende** en vez de lo que cada escritor escribe. Los tres se quedan
en el corpus a propósito.

## Lo que falta

Casos que siguen sin testigo:

- un `.inp` con una **sección comentada** en medio (R1 la descarta; el C se
  desincroniza). No es un fichero legítimo —el motor lo rechaza— así que no
  encaja en este corpus tal como está; pertenece al banco de acuerdo entre
  lectores, no al de conservación entre escritores.
- un `.inp` de **más de 2000 observaciones** o de **más de 50 deterministas**,
  para el límite propio del GUI (`inp_fits_gui`). Hay una prueba de eso en
  `gtk_fue.09/tests/test_units.c`, que construye el fichero al vuelo.
- un fichero con **μ fijo en cero** frente a μ fijo no nulo, para ver si al
  arreglar los escritores distinguen «no hay media» de «la media es cero».
