# Estudio: el factor de reescalado en la suite

15-nov-2026. Qué es el factor de reescalado de los `.inp`/`.pre`, qué necesita
de él el optimizador, qué pasa cuando no es el de costumbre, y qué queda por
hacer en los motores C, la madre y los GUI. La parte de Python (art, fue,
drtran, drvarma, drvec, sima) lleva sus partes en cada repositorio.

## Resumen

- El factor es un **remedio numérico**, no una unidad. Su origen documentado es
  un consejo de precisión (Treadway, *DRVUS: manual de usuario*, 2001); con
  logaritmos y 100 resulta, además, que todo se lee en tanto por ciento, y de
  ahí que media suite lo trate como si fuera una unidad.
- **100 con logaritmos funciona y se queda.** No hace falta elegir el factor
  por los datos.
- El optimizador pide **dos** cosas a la serie estacionaria, y el factor solo
  arregla una.
- Lo que cruza series (ganancias, β, α, coeficientes cruzados) lleva el
  cociente de los factores. Los GUI ya lo deshacen al presentar; los `.out` de
  los motores, las restricciones y los asistentes de Python, no.
- La suite no es coherente consigo misma en el valor: la madre escribe 1, art
  escribe 100, drvec elige por los datos.

## 1. Qué necesita el optimizador

`qnewtopt` (cinco copias, una por motor) lleva el tamaño típico de los
parámetros fijo en 1: el paso de diferencias es η^(1/3)·max(|x|, 1), el
contraste del gradiente |g|·(|x|+1)/(|f|+1) y el del paso |Δx|/(|x|+1). El
objetivo es un cociente que arranca en 1 y σ² va concentrada, así que **AR, MA
y δ no dependen de la escala**. Dependen μ y los ω deterministas (∝ factor),
los coeficientes entre series (∝ cociente de factores), `log(var_i/var_1)` (se
desplaza) y las covarianzas `q[i,j]`.

Medido sobre los portes con sus motores C y con los de Python puro, que dan lo
mismo:

- **Dispersión de la serie transformada y diferenciada.** Banda segura: de 0.1
  a 1000. Fuera empiezan los problemas una década más allá: por abajo fue pasa
  a la raíz MA no invertible y los errores típicos se mueven, drvarma deja de
  certificar por gradiente, el hessiano por diferencias de drvec no sale; por
  arriba fue falla a 10⁴. Sin media ni deterministas, drtran y drvarma dan el
  mismo óptimo de 10⁻² a 10⁶.
- **Razón nivel/dispersión con d = 0.** A 10⁶ es marginal y a 10⁸ falla aunque
  la dispersión sea 1. **Ningún factor la arregla**: hay que diferenciar o
  cambiar las unidades de la serie. Es lo que agrava λ < 0, porque
  (y^λ − 1)/λ vale ≈ −1/λ más una variación diminuta. Con logaritmos la razón
  no pasa de unos miles ni con niveles de 10¹².

El estudio de `engines/drtran/docs/OPTIMIZER_STOPPING_STUDY.md` ya lo había
dicho para un caso: el optimizador llega al óptimo y pierde la capacidad de
certificarlo; se probaron tres variantes de tamaño típico y se rechazaron.
Este estudio no propone tocar el optimizador.

## 2. Lo que hay en los ficheros de la suite

157 modelos univariantes distintos en los repositorios. Con logaritmos y
factor 100, la dispersión va de 0.29 a 39 (71 modelos, mediana 1.08): todos
dentro. En niveles con 100, los modelados van de 0.7 a 82 (tipos de interés,
series con el logaritmo ya tomado, índices en primera diferencia). Hay 21
modelos en logaritmos con factor 1 y varios en niveles con 1 y con 10: **los
factores distintos ya conviven**.

## 3. Factores distintos en un mismo sistema

Los motores ajustan igual: la verosimilitud en unidades de y coincide al
tercer decimal. Lo que se lee, no. Dos series en logaritmos con
log Y − log X estacionario, Y con factor 100 y X con factor 1:

- drvec da β = −0.009955 (es −0.9955), α = 30.7 con t = 306, y los tres
  arranques acaban sin certificar;
- contrastar β = −1 (`-fixb2 -1`) da LR = 37.5: se rechaza una hipótesis
  cierta, porque el valor se impone en unidades del motor;
- la ganancia de una transferencia sale 44.96 en vez de 0.4496.

`engines/drvec/docs/HOMOLOGATION.md` ya lo midió en un caso real: mismos
puntos, p = 0.020 frente a 0.052 y el doble de iteraciones. drvec avisa
(`drvec.c`, «the .pre files do not share one refactor»); drtran y la escalera
de drvarma no comprueban nada.

## 4. El formato

- El factor se escribe con `%.2f` en `fue.c`, `inp_write_bare` y `gui/fue`:
  0.001 sale «0.00» y se relee como 1 (`engines/fue/BUGS.md`, BUG-0003).
  drvec (`%.10g`) y `drvarma -split` (`%g`) no lo pierden.
- μ y los ω van al `.pre` con `%.6f` y σ² (fuf) con `%.10f`: a escala pequeña
  pierden cifras (fue BUG-0001; `docs/CONTRATO.md` lo da como la razón
  mecánica del 100).

## 5. Una verosimilitud que no depende del factor

ℓ_y = ℓ + n·ln(factor) + (λ − 1)·Σ ln y_t

Es exactamente invariante al factor (comprobado con factores de 0.1 a 10⁴) y
comparable entre λ. drvec ya corrige así en el contraste de rango (su BUG-43).
Sirve para comparar dos modelos de la misma serie que no comparten factor o λ,
donde hoy solo cabe avisar.

## 6. Lo que se decidió

1. **El valor.** 100 sigue siendo la convención. Quien escribe un `.inp` solo
   se aparta si con 100 la serie estacionaria cae fuera de [0.1, 1000];
   entonces elige otra potencia de 10 que el formato aguante, y lo dice. Si no
   la hay, o falla la razón nivel/dispersión, se le dice al analista. El factor
   se hereda del fichero base.
2. **La presentación.** Todo lo que lee una persona va en las unidades de cada
   serie, por una sola regla — la que ya usan `gui/drtran` y `gui/sim`
   (`pagina.c`): μ y los deterministas divididos por su factor; un coeficiente
   entre series multiplicado por el cociente de factores; σ por 100/factor y en
   % solo con logaritmos; `log(var_i/var_j)` desplazado.
3. **Las comprobaciones** al cargar un sistema: dispersión, razón y factores
   distintos.
4. **Los motores, sin tocar.**

## 7. Pendiente en este repositorio

Con parte y repro (los dos arreglados el 15-nov-2026):

- **drvarma BUG-0005** — la tabla de previsión de la escalera (y `-forecast`)
  multiplica por 100/factor sea cual sea λ. **Prioridad alta**: los motores
  van homologados siempre, y el arreglo del porte (drvarma-python BUG-0025)
  espera a este.
- **fue BUG-0003** — el `.pre` escribe el factor con dos decimales.

Sin parte todavía (propuesta; nada de esto está hecho):

- **La madre escribe factor 1 en toda serie nueva** (`gui/atsw/src/genera.c`),
  y lo conserva cuando el analista pasa a logaritmos (`id_pon_transformacion`
  reescribe solo la línea de λ, d, D). Con logaritmos y factor 1 la dispersión
  queda en ~0.01, el borde de la banda: el hessiano por diferencias de drvec no
  sale y los motores certifican por paso. Propuesta: que la transformación
  ponga el factor con la regla del punto 6.1 (100 con logaritmos), en
  `lib/inpdet`, para la madre y para `atsw-cli` a la vez.
- **`gui/fue`** pasa el factor por un control de rango 1 a 1000; por defecto,
  1. Sin comprobar en ejecución: un fichero con factor 0.01 o 10⁶ se
  recortaría al abrirlo y guardarlo. (Su escritor ya no pierde el factor:
  fue BUG-0003.)
- **Restos de «×100 siempre»**: la figura gnuplot de la previsión de drtran
  (`fuf_graphic.c`), fuf con λ < 0 (tabla, LaTeX y gráfico), y la historia del
  gráfico de previsión de `gui/sim` (`prevision.c`).
- **`atsw-cli`** imprime la σ del `.out` con la nota «en logaritmos: ×100 es un
  %», que solo es cierta con factor 1.
- **Factores distintos**: drtran y la escalera de drvarma ya avisan, como
  drvec (`fuepre_warn_refactors`, por stderr: el `.out` no cambia). Queda que
  los `.cns` de drtran (`omega = número`, `omega_a = omega_b`) y `-fixb2`,
  `-fixb2row`, `-alpha` de drvec van en unidades del motor.
- **`PRE_MOVE = 1e-3`** en la puerta de la escalera es absoluto y cuenta μ: el
  veredicto «no es un óptimo» depende del factor.
- **Los `.out` de los motores conjuntos** escriben los coeficientes entre
  series con seis decimales en unidades del motor; con un cociente de factores
  de 10⁻⁴ o menos no quedan cifras que el GUI pueda convertir.
- `docs/TODO.md` no lleva aún la referencia a este documento: se añade cuando
  no haya otro cambio abierto sobre ese fichero.
