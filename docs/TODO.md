# Lo que viene

Los pendientes vivían repartidos por las tablas «Orden» de cada diseño, que
está bien para leer un diseño y mal para saber qué toca. Esto los junta.

---

## EL SIGUIENTE PASO: los contrastes formales y el MEG

**Decisión del analista, 2026-09-26.** Traer al GUI `formal_tests` y el MEG:

- **`formal_tests`** — MEG, DCD_f y Shin-Fuller. 1 871 líneas en art.
- **`meg_frequency`** — la frecuencia que el MEG señala.
- **`meg_reformulate`** — reformular el modelo según lo que el MEG dice.

### Por qué esto cierra una pregunta abierta, y no es un encargo más

`AUDITORIA-art.md` §4 los dejó en el grupo C —«lo que se puede pero hay que
decidir antes»— y la razón no era el coste:

> La identificación de esta escuela es **visual** —la ACF, la PACF, la media
> contra la desviación típica, que es lo que fug enseña—. Traerlos al GUI no
> es sólo trabajo: **cambia lo que el GUI enseña**, y el GUI es, por la
> decisión P4, el que se usa para educación y para el analista que quiere
> control total del proceso.

La recomendación de entonces fue: `ar_factorization` sí, **el resto sólo si
decides que el GUI también los enseña**. El analista lo ha decidido: sí.
Queda dicho aquí para que dentro de seis meses no parezca que se coló.

### Lo que hará falta

| | qué | notas |
|---|---|---|
| | leer los `.out` que ya traigan MEG | el motor puede estar imprimiendo parte: mirar antes de portar |
| | GSL para las distribuciones | ya es dependencia de `drtran_gui`; de ahí salen los autovalores de `nlatools` |
| | `ar_factorization` primero | es leer el operador AR y sacarle las raíces — dice si hay un AR estacional escondido, y eso **sí** es de la escuela. Es el peldaño barato y el que menos compromete |
| | decidir **dónde** se enseña | la diagnosis juzga lo que hay; un contraste de raíz unitaria es identificación, no diagnosis. La distinción que el analista hizo con la ganancia vale aquí igual |

**No modificar los motores** salvo defecto, y con el visto bueno — como el de
la división entera del Jarque-Bera.

---

## Lo demás, por documento

### `DISENO-anomalos.md`

- **§7.4** — el mismo interruptor de calibrar en el **vistazo**, sobre la
  serie *sin* modelo. Es donde la calibración de verdad decide, porque es
  donde se eligen los órdenes.

### `DISENO-intervencion.md`

- **§5.5** — la comprobación de **Treadway sobre el `.out` del hijo**: leer si
  la forma elegida resolvió el suceso o lo dejó a medias. Cierra el círculo
  —hoy la superposición lo estima *antes*, sin motor— y las piezas están:
  `lib/outfile` más `lib/anomalos`.
- **§5.4** — el peldaño 2 estimado de verdad, para poder comparar con el 1 por
  algo que no sea el AIC.

### `DISENO-muestras.md`

- **paso 6** — renombrar la clave de una serie. El menos urgente.

### `DISENO-prevision.md`

- El **acta** de previsión y la evaluación ex-post sobre una submuestra. Se
  revirtió con aquel intento fallido y sólo está en la conversación.

---

## Higiene, sin prisa

- **`/usr/local/bin` tiene instalaciones viejas** (`fug` 1.14, `fue_gui` de
  mayo). No estorban —los GUIs se buscan primero en el árbol de
  construcción, y por eso `atsw_programa` mira ahí antes que en el PATH—
  pero están, y necesitan sudo para irse.
- **`IPC_ES_m04.inp`** quedó huérfano en el proyecto de pruebas: fichero en
  disco que el manifiesto no conoce, de cuando «Derivar» escribía a medias
  desde fue_gui. La próxima derivación lo sobrescribe.
