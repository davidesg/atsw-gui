# `xlsx` — leer una hoja de cálculo. Lo justo, y declarado.

**En toda la suite no había ninguno.** El inventario lo midió: el único sitio
donde se lee un `.xlsx` es en los guiones Python de `cases/`, con pandas. Del
lado C, nada — y los datos del analista vienen en `.xlsx`.

## La trampa son las fechas, y no se puede esquivar

**En Excel una fecha es un número.** `35095` es el 31 de enero de 1996. Lo
único que la distingue de un número es **su estilo**. De un fichero real:

```xml
<c r="A2" s="1"><v>35095</v></c>     s=1 → numFmtId=17 → "mmm-yy"   FECHA
<c r="B2" s="2"><v>55.12</v></c>     s=2 → numFmtId=2  → "0.00"     número
```

Un lector que ignore `styles.xml` importa las dos como números y **se lleva la
columna de fechas dentro de los datos sin enterarse**. Por eso los estilos no
son opcionales aquí: son la diferencia entre leer bien y leer mal.

Se reconocen los formatos de fecha de fábrica (14–22, 27–36, 45–47, 50–58) y
los propios del fichero, mirando si su `formatCode` lleva `y/m/d/h/s` **fuera
de comillas y de corchetes** — `[$-C0A]mmm\-yy;@` es una fecha; `"año"0.00`
no.

## La conversión del serial, comprobada contra Python

El origen es **1899-12-30**, no 1900-01-01: Excel cree que 1900 fue bisiesto
—el 29 de febrero de 1900 no existió— y ese origen compensa el día de más.
Comprobado en los bordes:

| serial | C | Python |
|---|---|---|
| 1 | 1899-12-31 | 1899-12-31 |
| 60 | 1900-02-28 | 1900-02-28 |
| 61 | 1900-03-01 | 1900-03-01 |
| 35095 | 1996-01-31 | 1996-01-31 |
| 2958465 | 9999-12-31 | 9999-12-31 |

## El alcance está declarado

Lo que no cabe **se dice** en vez de adivinarse:

- **La primera hoja.** Un libro con varias se lee la primera y se dice cuántas
  había.
- Números, texto compartido, texto en línea y fechas.
- **Las celdas vacías se respetan**: la referencia `r="B7"` dice qué columna es
  cada una, así que un hueco no corre las de al lado.
- **No se evalúan fórmulas**: se lee el valor cacheado, que es lo que Excel
  dejó escrito.
- Los límites son **guardas, no reservas**: la hoja se reserva por su tamaño
  real.

## Lo que hay debajo

Un `.xlsx` es un ZIP con XML. Se lee el directorio central, se localiza la
parte y se descomprime con **zlib en crudo** (`inflateInit2(-15)`, deflate sin
cabecera, que es lo que el formato ZIP guarda). Sin dependencias nuevas: zlib
ya estaba.

El XML se recorre con un escáner de las formas concretas que hacen falta, no
con un parser general. Es la misma regla que en `lib/proyecto`: **lo que no
entiende lo dice**, no lo adivina.

## Probado con los ficheros de verdad

Los seis `.xlsx` del disco del analista, incluido uno de **289 columnas** (una
descarga del INE, con las series en filas y notas al pie) que se lee y se
declara tal cual — interpretarlo es de `lib/datos`, no de aquí.

Y los dos casos conviven en sus datos: en `Datosgeneral.xlsx` la fecha es un
**serial con estilo**, y en `IPC.xlsx` es **texto**. Los dos salen bien.
