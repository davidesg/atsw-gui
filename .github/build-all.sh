#!/bin/bash
# Compila cada directorio de $DIRS y sigue aunque falle uno; al final dice
# cuales fallaron y sale con error si hubo alguno.
fallan=()
for d in $DIRS; do
    echo "::group::$d"
    if make -C "$d"; then
        echo "::endgroup::"
    else
        echo "::endgroup::"
        echo "::error::$d no compila"
        fallan+=("$d")
    fi
done
echo
if [ ${#fallan[@]} -eq 0 ]; then
    echo "todo compila"
else
    echo "no compilan: ${fallan[*]}"
    exit 1
fi
