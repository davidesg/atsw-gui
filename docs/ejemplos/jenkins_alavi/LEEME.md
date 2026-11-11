# Ratas almizcleras y visones: el ejemplo de los sistemas

Es el proyecto de ejemplo de **Ayuda → Ejemplos** en ATSW. La madre abre una
copia en la carpeta «Ejemplos de ATSW», dentro de tus documentos. Esa copia
es tuya: si la vuelves a pedir, se abre la misma, con lo que hayas hecho.
Para empezar de cero, borra la carpeta.

## Los datos

Son las pieles de rata almizclera (MUSKRAT) y de visón (MINK) que compró la
Compañía de la Bahía de Hudson: 62 años, de Jones (1914). Las fechas van de
1850 a 1911, como en Reinsel (1997). Jenkins y Alavi (1981) fechan los mismos
valores de 1848 a 1909, así que cada fecha de aquí es dos años posterior a la
suya.

## Lo que ya está hecho

- **MUSKRAT/m01**: ARIMA(6,1,1) en logaritmos.
- **MINK/m01**: AR(4) en logaritmos, con media.

Los dos se hicieron en art por la vía guiada; son los modelos univariantes
del ejemplo `jenkins_alavi` de sima. Están estimados con fue y elegidos.

## Lo que queda por hacer

El caso **C1** es el sistema de las dos series, en ese orden. Está por
estimar. Márcalo y ábrelo en sim_gui (menú SIM, o doble clic). Desde ahí:

1. la puerta;
2. la identificación de la dinámica cruzada;
3. el modelo y su estimación;
4. la diagnosis;
5. la previsión.

El capítulo «Sistemas» del manual lo recorre paso a paso.

## Cómo se hizo

Con las mismas funciones que usa la madre:

- los `datos.csv` y los `m00` (los datos), como los escribe Datos → Importar;
- los `m01`, con `atsw-cli proyecto nuevo`; luego el modelo de art y la
  estimación con fue;
- la elección y el caso, con `atsw-cli proyecto elige` y
  `atsw-cli proyecto caso-nuevo --motor drvarma`.

La batería de atsw-cli comprueba cuatro cosas:

- que el proyecto se lee;
- que el caso lleva el sha256 de cada `.pre`;
- que fue reproduce los `.pre`;
- que los `m00` tienen los datos de su `datos.csv`.
