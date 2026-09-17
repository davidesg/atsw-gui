#!/bin/sh
# copias.sh -- que no vuelva a haber copias.
#
#     sh copias.sh
#
# Antes este guion comparaba la copia de inpcheck.c del GUI con la del motor y
# avisaba si habian divergido. Hacia falta, y sirvio: divergieron -- se arreglo
# una cota en el motor y no la copia, y el GUI se quedo cargando un fichero que
# le destruia el monton.
#
# Desde que existe lib/, ya no hay copia que comparar. El GUI compila EL
# FICHERO DEL MOTOR, renombrando la funcion al vuelo:
#
#     $(CC) -Dinp_check=inp_check_fue -c ../../engines/fue/src/inpcheck.c
#
# de modo que es literalmente el mismo codigo, por construccion y no por
# vigilancia. La unica razon del fork era la colision de nombres --inp_check en
# fue y en fuf-- y un -D la resuelve sin duplicar nada.
#
# Asi que lo que se comprueba ahora es lo contrario: que nadie haya vuelto a
# copiar. Es mas barato de mantener y no puede dar un falso verde.
#
# Los limites PROPIOS del GUI --sus vectores estaticos son mas pequenos que los
# del motor-- siguen sin ir aqui: viven en inp_fits_gui() (lib/utils/utils.c),
# que es su puerta y no la compartida.

TOP=$(cd "$(dirname "$0")" && pwd)
RAIZ=$(cd "$TOP/.." && pwd)

malas=0

# ---------------------------------------------------------------------------
# 1. inpcheck.c solo puede estar en los motores
# ---------------------------------------------------------------------------
echo "inpcheck:"
for f in $(find "$RAIZ" -name "inpcheck*.c" -not -path "*/obj/*" 2>/dev/null); do
    rel=${f#$RAIZ/}
    case "$rel" in
        engines/*/src/inpcheck.c)
            echo "  ok        $rel" ;;
        *)
            echo "  COPIA     $rel"
            echo "            deberia compilarse del motor con -Dinp_check=..."
            malas=$((malas + 1)) ;;
    esac
done

# ---------------------------------------------------------------------------
# 2. lo que esta en lib/ no puede estar tambien dentro de un programa
# ---------------------------------------------------------------------------
echo
echo "lo de lib/:"
for f in "$RAIZ"/lib/*/*.c; do
    [ -f "$f" ] || continue
    base=$(basename "$f")
    otras=$(find "$RAIZ/engines" "$RAIZ/gui" -name "$base" -not -path "*/obj/*" 2>/dev/null)

    # Excepciones CONOCIDAS, con su razon y su fecha de caducidad. Una
    # excepcion escrita es mejor que un guardian en rojo permanente, que es
    # como se aprende a ignorarlo.
    case "$base:$otras" in
      preview.c:*gui/fug/src/preview.c*)
        echo "  pendiente $base  tambien en gui/fug/src (GTK2)"
        echo "            la de lib/ es esa misma portada a GTK3 y ampliada con"
        echo "            zoom y lupa. Se va cuando fug se porte -- paso 2 del plan."
        continue ;;
    esac

    if [ -z "$otras" ]; then
        echo "  ok        $base"
    else
        echo "  COPIA     $base  tambien en:"
        echo "$otras" | sed "s#$RAIZ/#              #"
        malas=$((malas + 1))
    fi
done

# ---------------------------------------------------------------------------
# 3. nlatools / nlutils: aqui TODAVIA hay copias, y es a proposito
# ---------------------------------------------------------------------------
echo
echo "nlatools / nlutils  (copias conocidas -- ver lib/README.md):"
find "$RAIZ/engines" "$RAIZ/gui" \( -name "nlatools.c" -o -name "nlutils.c" \) \
     -not -path "*/obj/*" 2>/dev/null | while read -r f; do
    printf '  %-10s %5s lineas  %s\n' "$(md5sum "$f" | cut -c1-8)" \
           "$(wc -l < "$f")" "${f#$RAIZ/}"
done
echo "  son el nucleo numerico: tocarlas mueve numeros, y no entran en la"
echo "  biblioteca hasta que la bateria pueda medir el cambio."

echo
if [ "$malas" = 0 ]; then echo "no hay copias indebidas"; else echo "$malas copia(s) que no deberian estar"; fi
[ "$malas" = 0 ]
