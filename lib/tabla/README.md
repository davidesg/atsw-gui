# `tabla` — la tabla publicable

**Es el módulo que TASTE declaró y nunca escribió.** En su fuente están
declarados `TABLA`, `Graba_Ser_PRN` y `Graba_Ser_TSF`, y los tres están vacíos.
El estudio lo señaló así:

> Se construyó el instrumento y se dejó sin construir el informe. **Ése es
> exactamente el hueco que la interfaz madre tiene que llenar**, y la fase 5
> debería tomarlo como su **primer** requisito y no como el último.
> — `ESTUDIO-taste.md` §5

Treinta años después, el inventario encontró lo mismo en la suite de hoy:
**ningún botón «Exportar» en ninguno de los tres GUIs**, ningún export de
tablas, y un solo CSV en todo el programa. Siete pantallas, y la única forma de
sacar un número era copiarlo de una etiqueta.

## Una tabla, tres renderizados, **los mismos números**

Los decimales los declara la **columna**, y valen para los tres formatos. Una
tabla que dice `0.42` en el PDF y `0.4237` en el CSV **son dos tablas
distintas**, y la de arriba es la que se publica. Si hace falta toda la
precisión está el `.out`, que es el registro.

| | para qué |
|---|---|
| **CSV** | una hoja de cálculo. RFC 4180, punto decimal |
| **TXT** | ancho fijo, como los cuadros del `.out`: se pega en un correo y sigue cuadrando |
| **TeX** | un `tabular` con su `caption` |

## La procedencia va dentro, y no es decoración

Una tabla que sale del programa sin decir **de qué modelo, de qué muestra y de
qué motor** viene es la forma más fácil de que un número acabe en un paper sin
poder reproducirlo. Aquí no se puede escribir una tabla sin ella: los tres
escritores la emiten.

En CSV va en líneas de `#`, que es el convenio que la suite ya usa en el
fichero de residuos de `drtran -e`. En TXT y en TeX, al pie.

```
# Modelo estimado
# Series: EP, EI, EU, EC, EA, P
# Modelo: 4 enlaces
# Muestra: 1/1977 - 4/1992, 64 obs
# Fichero: …/modelo.out
Parámetro,Estimado,d.t.,t,p,Nota
omega1[0],0.749918,0.279073,2.687,0.0072,
```

## Dos decisiones que no son obvias

**El ancho se cuenta en CARACTERES, no en bytes.** `strlen` cuenta bytes y
`Parámetro` ocupa 10 para 9 caracteres: con `%-*s` la cabecera quedaba una
columna corrida respecto a los datos. Es el mismo fallo que descuadró la matriz
de covarianzas de la página Modelo, y la regla **R5** del diseño está para eso:
*una tabla que no mantiene los espacios deja de leerse.*

**El punto decimal, siempre.** Es lo que lee cualquier herramienta — y una
tabla con coma decimal y coma de campo no se puede leer.

## Quien la llena, la llena de DATOS

En `drtran_gui` la tabla se arma de las estructuras (`OdParams`, `OdSerie`,
`OdEnlace`, lo que calcula `lib/gof`), **no raspando el `GtkTreeView`**. Raspar
el widget sería más corto y convertiría todo en texto: un número dejaría de ser
un número y el CSV no valdría para nada.

Y dice lo mismo que la pantalla. Si en pantalla un parámetro pone «no
significativo», en el CSV también — o son dos tablas distintas.

## Sin dependencias

Como `lib/rutas`: lo enlazan los motores, que no tienen glib.
