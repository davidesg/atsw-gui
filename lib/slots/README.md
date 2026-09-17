# `slots` — la tabla de parámetros y el `.cns`

## Un slot es un parámetro estructural, con nombre

Y **la tabla la genera el modelo**, no el analista. Depende de la red (`.dag`),
de los órdenes de cada enlace, y de qué coeficientes trae **libres** cada `.pre`:

    omega1[0]  omega1[1]  delta1[1]     por enlace: s+1 omegas y r deltas
    phi_2[B^1]  theta_2[B^12]           el ARMA de cada serie, EXPANDIDO
    theta_5[f=1]                        un factor de frecuencia fija
    omega_d1[1,0]                       los deterministas
    mu[3]                               la media
    log(var2/var1)                      las varianzas relativas
    q[3,2]                              las covarianzas de las innovaciones

**Y aquí está el punto.** La tabla sólo lleva los coeficientes que el `.pre`
marca como libres. `0.0000  0` en un `.pre` es un coeficiente **FIJO**, o sea
**especificación**, y no es un parámetro que se estime. Es el contrato, visto
desde el otro lado.

Medido: en el m6, **las seis medias están fijas en sus `.pre`**, así que no hay
ni un `mu[i]` en la tabla. Eso no es un olvido — es la única lectura correcta
del fichero.

Las `q[i,j]` entran **siempre**, pero **fijas en cero**: la diagonal es el caso
por defecto y liberar una covarianza es una decisión del analista. El m6-1 no
libera las 15 de su sistema: libera **tres**.

## El `.cns`: cinco formas, un solo lenguaje

    NOMBRE = free            liberar   (las q[i,j] nacen fijas en cero)
    NOMBRE = 0.5             fijar
    NOMBRE = OTRO            compartir: un grado de libertad en dos sitios
    NOMBRE = [-]a * b        producto  — numerador factorizado con MA compartida
    NOMBRE = a + b - c       combinación lineal — un factor FIJO (1−B) impone
                             ν(1) = 0, o sea ω₀ = ω₁ + ω₂ + …

El m6 las usa todas. De su `m6_net_full.cns`:

```
q[5,2] = free
omega1[1] = omega1[0] * theta_2[B^1]         # −x5(1−x6B), x6 = la MA de EI
omega3[0] = omega3[1] + omega3[2] + omega3[3] # el (1−B) fijo de EI←EU
```

## No es código nuevo

`add_slot`, `add_arma_slots`, `build_slots`, `find_slot` y `read_constraints`
estaban dentro de `drtran.c` sobre sus globales. **El motor llama a estas
mismas**, así que mtram no puede ofrecer un slot que el motor no tenga ni
escribir un `.cns` que el motor rechace.

La mudanza se hizo sin tocar los 66 puntos de uso del motor: `SlotTable` es una
**estructura de arrays** —la forma que ya tenían los globales— y `drtran.c`
guarda una y define los nombres viejos como macros sobre ella.

```c
static SlotTable ST;
#define slot_name  ST.name
#define slot_kind  ST.kind
...
#define n_slot     ST.n
```

## La librería no elige idioma

Como en `lib/netfile`: `cns_read` devuelve el **hecho** (`CnsError`), no la
frase. El motor la cuenta en inglés (`cns_error_en`), mtram en español. Los dos
leen el mismo fichero y dan el mismo veredicto.

## El oráculo

El motor imprime su propio recuento:

    drtran M6_*.pre -n m6_net.dag -c m6_net_full.cns
    → Structural parameters: 67   (free: 52, fixed/shared: 15)

y `test_slots.c` exige esos tres números y esos nombres. Además:

- que `q[5,2]` exista y `q[2,5]` no — sólo por debajo de la diagonal;
- que las `q` **nazcan fijas**, no libres;
- las cinco formas del `.cns`, aplicadas y leídas de vuelta;
- los cinco rechazos, con su frase;
- y la media en **las dos direcciones**: como las seis del m6 son fijas, la
  prueba levanta una bandera a mano y exige que aparezca **un** slot más. Sin
  eso, media prueba no se recorrería nunca.

Y `gui/drtran/tests/run_tests.sh` reescribe el `.cns` con `cns_write`, se lo
pasa al motor y exige que cuente lo mismo: **lo que el GUI escribe se le
pregunta al motor**.

La batería del motor: **304 PASS, 0 FAIL** después de la mudanza.
