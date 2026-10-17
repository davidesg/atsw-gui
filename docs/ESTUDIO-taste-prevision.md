# TASTE y las previsiones: qué guardaba, cómo prevía y qué acumulaba

Estudio del 17-oct-2026, a petición del analista: *«cómo operaba el oráculo,
guardando modelos, y cómo recalculaba las previsiones a partir de nuevas
observaciones y nuevo origen de previsión; cómo se acumulaba esto»*. Es el
paso previo a diseñar la previsión con origen móvil (`TODO.md`,
`DISENO-prevision.md`): un fuf multivariante y un GUI que gestione las
previsiones a medida que llegan los datos.

Completa `ESTUDIO-taste.md` (TASTE como diseño). Las referencias son a las
fuentes originales en Borland Pascal (`atws/Taste/*.PAS`, contando líneas sin
CR). TASTE es privado: aquí se describe el diseño y se citan nombres y líneas,
no se copia código. **[V]** es verificado, leído o ejecutado con el port
(`port/tools/tbatch`); **[I]** es inferido.

---

## Lo esencial

- **TASTE no acumulaba previsiones.** Cada previsión era un informe de texto.
  Sólo se conservaba si el analista lo añadía a `TASTE.OUT` (tecla `A` del
  visor), y ese fichero se trunca en cada arranque. No había previsiones por
  origen, ni tabla previsión-realización, ni ECM, ni actualización con los
  pesos ψ: todo se recalculaba desde cero. **[V]**
- **Sí prevía con el modelo guardado sin reestimar**, y movía el origen
  **hacia atrás** con un desplazamiento `B` (observaciones desde el final).
  **[V]**
- **Ese origen hacia atrás no es una previsión ex-post honesta**: la
  retroprevisión de arranque y σ² usan la serie entera, también lo posterior
  al origen. Comprobado ejecutando (§3). **[V]**
- **La pieza que prevía con datos nuevos era otro programa, PREDICE.EXE**
  (J.L. Gallego y V.M. Gonzalo, 1992-1995), construido sobre las unidades de
  TASTE. No hay fuente ni se puede ejecutar; lo que se sabe sale de sus
  cadenas y símbolos (§4). **[I]**

---

## 1. El modelo guardado (`.TSM`)

- Un `.TSM` es un fichero de **un solo registro** `TFMODEL` (3947 bytes,
  `TASTECTV.PAS:185-265`). Lleva:
  - los órdenes, λ, d, D, s, la constante y los parámetros;
  - por cada input: b, r, s, sus parámetros, la λ de la transferencia y una
    **copia del modelo univariante del input**.
  **[V]**
- **No lleva** σ², la serie, los residuos, las fechas, la muestra de
  estimación, el origen ni los valores de arranque. El campo `Name` existe
  pero nunca se rellena. Esto corrige un detalle de `ESTUDIO-taste.md`: sí hay
  campo, sólo que vacío. **[V]**
- Escritura en `Graba_Mod` (`ARCHS2.PAS:248-287`): `Rewrite`, después
  `Seek(FileSize)` y `Write`.
  - El `Seek` al final delata que se quiso **ir añadiendo versiones** del
    modelo.
  - El `Rewrite` previo lo trunca, así que siempre queda una sola.
  - La lectura es `Carga_Mod` (`ARCHS1.PAS:323-344`).
  **[V]**
- Consecuencia: prever sin reestimar obliga a **recalcular todo desde la
  serie**: retroprevisión, residuos y σ². **[V]**

## 2. Cómo se calculaba una previsión

**Univariante**: `ForUS` y `ForeCastUS` (`USFO.PAS:57-315` y `407-606`). **[V]**
1. Relee la serie y usa los parámetros del modelo en memoria, sin
   reestimar.
2. Retroprevé y calcula los residuos de toda la serie. El número de
   retroprevisiones sale de una regla tosca: 200 si hay AR estacional.
3. **Recalcula σ² = Σa²/n** desde la primera retroprevisión (492-494).
4. Prevé por recursión con φ\*(B) y θ\*(B) expandidos desde el origen `N−B`.
5. Calcula la varianza con los pesos ψ, `σ²Σψ²`, y bandas normales al
   `ProbLim` %.
6. Con Box-Cox o log, destransforma la previsión y las bandas **sin corregir
   el sesgo**: la previsión resulta ser la mediana. La D.T. se queda en la
   escala transformada.

**El diálogo** (`ForUsI`, 610-767): serie, modelo, ORIGEN, HORIZONTE y
PROBABILIDAD, con valores por defecto B=0, L=25 y 95. **[V]**
- Se valida sólo `nobs > B` y `L ≤ 104`.
- El origen se teclea como desplazamiento, no como fecha. El informe sí
  imprime las fechas del origen y del horizonte.

**Deterministas** (`ForeInter`, 319-403): el impulso, el escalón y la rampa se
extrapolan. **Fallo:** la Semana Santa ignora `B` y queda mal fechada al mover
el origen. **[V]**

**Transferencia** (`ForTF`, `TFFO.PAS:31-519`). **[V]**
- Cada input se prevé con su modelo univariante.
- La salida es Σ ν_j(B) x̂_j + N̂.
- La varianza **incluye la incertidumbre de los inputs**:
  σ²_a Σψ² + Σ_i σ²_i Σ(ν_i ψ_i)².
- Restricción: los inputs tienen que tener **el mismo número de
  observaciones que la salida** (`ForTfI`, 821), así que **no se pueden dar
  valores futuros conocidos de un input**.

## 3. Cuando llegaban observaciones nuevas

**El flujo posible**, a mano. **[V]**
1. Editar el `.BJD`, añadir los valores y **corregir a mano el número de
   observaciones**: el lector lee exactamente `nobs` líneas.
2. Borrar la serie del catálogo, porque recargar con el mismo nombre se
   rechaza, y volver a cargarla.
3. Prever con `B=0` y el mismo modelo, sin reestimar.
4. Reestimar pisa los parámetros y, al regrabar el `.TSM`, el modelo
   anterior. La estimación **no tiene ventana**: para estimar en una
   submuestra hay que materializar otra serie (Generar → Submuestra).

**No hay actualización con ψ**: cada previsión recalcula todo. **[V]**

**El origen hacia atrás no equivale a truncar la serie.** **[V, ejecutado]**

Univariante, USCONS sobre el consumo de Howrey:

| | σ̂ | previsión de 1/1922 |
|---|---|---|
| origen 4 obs. atrás (92 obs.) | 0.00768 | 804.14 |
| serie truncada a 88 obs., origen al final | 0.01162 | 806.49 |

- La retroprevisión y σ² usan toda la serie: es fuga de información.
- σ̂ salta mucho con pocas observaciones menos (92 → 0.00768, 89 → 0.00873,
  88 → 0.01162) porque se recalcula en vez de tomarse de la estimación.

En transferencia, dentro de la muestra, `ForTF` usa el **ruido realizado** y
no su previsión (`TFFO.PAS:321-324`). Esa «previsión ex-post» sólo lleva el
error de los inputs. Comprobado a mano: ŷ = y·(x̂/x)^ω₀ reproduce las cifras
del informe.

No se controla que quede muestra suficiente: con el origen en la observación
1 la previsión explota hasta 10²⁰. **[V]**

## 4. Acumulación: nada en TASTE; PREDICE

**En TASTE no se acumula nada.** **[V]**
- Las previsiones son vectores locales: no van a ninguna serie, ni al `.AT0`,
  ni a ningún fichero de datos.
- La tabla del informe lleva FECHA, BANDAS, PREVISIÓN y D.T. No lleva el
  valor observado ni el error, tampoco con `B>0`.
- No hay estadísticos de error de previsión. El registro `TABLA` con su campo
  `Errores` está declarado y no se usa nunca.

**PREDICE.EXE (1995) y OLDPRE.EXE (1993)** son la versión nueva y la antigua
del mismo programa. Ninguno aparece en la documentación. Les faltan
`rtm.exe` y `dpmi16bi.ovl`, así que no se pueden ejecutar. Por sus cadenas y
sus símbolos de depuración (módulos `PREDICE`, `ACTUAL`, `USFO2`, `TFFO2`,
`COMPUTE`, `AGREGATE`, `GESTION`, `CALCULOS`): **[I]**
- Lee los `.BJD` y `.TSM` de TASTE, en modo interactivo o **por lotes**
  desde un fichero `.PRE`, con claves `AGREG`, `TASA` y `DET`. La extensión
  que hoy significa «óptimo» nació como fichero de lotes de previsión.
- Pregunta «NÚMERO DE OBSERVACIONES EN ESTIMACIÓN» (`NobsEst`) y tiene
  `ACTUNIVARIANTE` con `ResiduosySigma2`. Es **la actualización**: el modelo
  se estimó con `NobsEst` observaciones, el fichero trae más, y con los
  mismos parámetros recalcula residuos y σ² sobre la muestra de estimación y
  prevé desde el final.
- Sus tablas llevan columnas de errores típicos y de residuos (los errores a
  un paso de las observaciones nuevas).
- Los inputs pueden venir **alargados con valores futuros**, justo lo que
  TASTE no permite.
- Exporta tablas: nivel con tasas (TCVM, TCVA…), stock y flujo, y la
  **previsión de agregados con su desviación típica**.
- `COMPUTE` tiene `ELFARMA`: residuos por verosimilitud exacta, no por
  retroprevisión.
- Tampoco hay rastro de un archivo histórico de previsiones por origen.

`US.EXE` (Caja de Madrid, 1995) es otra cosa: evalúa la verosimilitud exacta
y diagnostica. Es un antecesor de fue, no un previsor. **[V]**

## 5. Los menús

- Univariante y transferencia tienen la misma botonera I · M · E · P. P es
  «GENERACIÓN DE PREVISIONES».
- Prever en univariante:
  1. la serie y el modelo;
  2. el origen, el horizonte y la probabilidad;
  3. Procesar;
  4. el visor (`A` lo añade a `TASTE.OUT`).
- En transferencia, además, un input por línea con su modelo univariante.
- Los datos nuevos se meten con Archivos → Editar, Borrar serie y Cargar
  serie.

## 6. Balance

**Lo que conviene conservar**
- La varianza de previsión con la incertidumbre de los inputs.
- Que el modelo de transferencia lleve dentro los univariantes de sus
  inputs.
- Las fechas del origen y del horizonte en el informe.
- De PREDICE: la actualización sin reestimar (`NobsEst`), los inputs con
  valores futuros, los lotes, los agregados con su D.T. y las tablas de
  tasas.

**Lo que no hay que reproducir**
- No guardar σ², la muestra, el origen ni las previsiones.
- El origen como desplazamiento, sin validar la muestra que queda.
- La fuga de información con el origen hacia atrás.
- El ruido realizado dentro de la muestra en transferencia.
- La Semana Santa mal fechada.
- La destransformación sin decir si da la media o la mediana.
- La estimación sin ventana.
- No tener tabla previsión-realización.

## 7. Ideas para el diseño

1. **Un acta de previsión inmutable por origen.** Lleva:
   - el modelo (el hash de su `.pre`);
   - el origen como fecha y la muestra usada;
   - σ² de la estimación, no recalculada;
   - λ, el horizonte, la previsión, las bandas y la D.T. en las dos escalas.

   Las actas se acumulan, no se sobrescriben: es lo que el `Seek(FileSize)`
   de `Graba_Mod` quiso hacer y no hizo.
2. **El origen es una fecha, y ex-post es truncar de verdad.** La previsión
   desde t₀ usa sólo los datos hasta t₀, también en el arranque y en σ².
3. **Dos gestos distintos y visibles**:
   - «actualizar sin reestimar»: los mismos parámetros y más datos, el
     `NobsEst` de PREDICE; no toca el `.pre`;
   - «reestimar»: un modelo nuevo, hijo del anterior.
4. **La tabla previsión-realización y la de errores por horizonte**, que se
   llenan solas al llegar los datos:
   - el error, el error relativo y si la realización cae dentro de la banda;
   - el ECM y el EAM por horizonte y por origen.

   Ningún programa de la familia la tuvo.
5. **En transferencia, inputs con valores futuros conocidos o escenarios**,
   distinguidos en el acta de los previstos, y la varianza con la
   incertidumbre de los inputs que sí se prevén.
6. **Más adelante, de PREDICE**: agregados con su D.T., tasas derivadas y
   tablas de nivel exportables.
7. **Decir en el acta si la previsión destransformada es la mediana o la
   media.**

Sin estudiar todavía: un C de 2004 en `atws/atws/fuf/predice/`, antecesor de
fue, que por el nombre puede ser el puente entre PREDICE y fuf.
