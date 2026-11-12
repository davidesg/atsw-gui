# La demanda de dinero en Dinamarca: el ejemplo del VEC

Es el segundo proyecto de ejemplo de **Ayuda → Ejemplos** en ATSW. Como el
de las ratas almizcleras, la madre abre una copia en la carpeta «Ejemplos de
ATSW», dentro de tus documentos. Esa copia es tuya: si la vuelves a pedir, se
abre la misma, con lo que hayas hecho. Para empezar de cero, borra la carpeta.

## Los datos

Son los datos daneses de Johansen y Juselius (1990), trimestrales, de 1974.1
a 1987.3: 55 observaciones.

| serie | qué es |
|---|---|
| LRM | el dinero real (M2), en logaritmos |
| LRY | la renta real, en logaritmos |
| IBO | el tipo de interés de los bonos |
| IDE | el tipo de interés de los depósitos |

Vienen de `engines/drvec/datasets/urca_denmark.csv` (el `denmark` del paquete
urca de R), como en el ejemplo `johansen_juselius` de sima.

## Lo que ya está hecho

- **LRM/m01**: ARI(1,1) con estacionalidad trimestral determinista (cos, sin
  y alter).
- **LRY/m01**: un paseo aleatorio, ARIMA(0,1,0).
- **IBO/m01**: ARI(1,1).
- **IDE/m01**: IMA(1,1).

Los cuatro están en niveles (λ = 1: ya son logaritmos o tipos) con una
diferencia. Son los modelos de la ruta U de sima, hechos con el motor de art
**sin revisarlos en art**: sirven para el ejemplo, no son un análisis
univariante terminado. Están estimados con fue y elegidos.

## Lo que queda por hacer

El caso **C1** es el sistema de las cuatro series, en ese orden. Está por
estimar. Márcalo y ábrelo en sim_gui (menú SIM, o doble clic). Como las
cuatro son I(1), la pregunta es si cointegran. Desde ahí:

1. la puerta;
2. **R · Rango**: el rango de cointegración, con el bootstrap. Con 55
   observaciones, la tabla asintótica es poco de fiar;
3. el modelo: la clase **VEC**, el rango r y la serie sobre la que se
   normaliza la relación (LRM, la demanda de dinero);
4. la estimación con drvec, y la diagnosis: la relación de equilibrio, α y
   la débil exogeneidad;
5. la previsión, y su evaluación frente a los modelos univariantes.

El capítulo «El VEC: la demanda de dinero en Dinamarca» del manual lo recorre
paso a paso.

## Cómo se hizo

Con las mismas funciones que usa la madre:

- los `datos.csv` y los `m00` (los datos), como los escribe Datos → Importar;
- los `m01`, con `atsw-cli proyecto nuevo`; luego el modelo de sima y la
  estimación con fue;
- la elección y el caso, con `atsw-cli proyecto elige` y
  `atsw-cli proyecto caso-nuevo --motor drvarma`.

Los `.pre` de sima llevan en la cabecera una línea más, «Built by sima's raw
entry…». fue no lee un `.pre` con esa línea, así que aquí van sin ella (es el
BUG-0007 de sima).

La batería de atsw-cli comprueba cuatro cosas:

- que el proyecto se lee;
- que el caso lleva el sha256 de cada `.pre`;
- que fue reproduce los `.pre`;
- que los `m00` tienen los datos de su `datos.csv`.
