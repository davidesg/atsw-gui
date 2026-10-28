#!/bin/bash
# descarga.sh PATRON DIR: el fichero de la «release» vVERSION que case con
# PATRON, en DIR. Con una etiqueta recien puesta la release la cuelga
# paquetes.yml al acabar (una media hora): se espera hasta ESPERA minutos.
set -u
V=${VERSION#v}
fin=$(( $(date +%s) + ${ESPERA:-75} * 60 ))
mkdir -p "$2"
while :; do
    gh release download "v$V" -R "$GITHUB_REPOSITORY" -p "$1" -D "$2" --clobber && exit 0
    [ "$(date +%s)" -lt "$fin" ] || { echo "::error::no hay $1 en la release v$V"; exit 1; }
    echo "todavia no esta $1 en v$V: espero un minuto"
    sleep 60
done
