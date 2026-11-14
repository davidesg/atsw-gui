#!/bin/bash
# TEMPORAL (se quita antes de fusionar #140): 30 atsw-cli a la vez, 20 vueltas.
CLI=$PWD/gui/atsw-cli/atsw-cli; [ -x "$CLI.exe" ] && CLI=$CLI.exe
C=$(mktemp -d); tot_c=0; tot_a=0
for v in $(seq 1 20); do
  {
    printf 'schema_version: 1\nid: conc\ntitulo: concurrencia\ncreado: "2026-11-14"\n'
    printf 'analista: ""\nherramienta: fue\nraiz: .\n\nseries:\n  IPC:\n\nmodelos:\n'
    printf '  IPC/m00:\n    version: 0\n    padre: ""\n    razon: ""\n    rol: datos\n'
    for i in $(seq 1 30); do
      printf '  IPC/m%02d:\n    version: %d\n    padre: m00\n    razon: ""\n' "$i" "$i"
    done
  } > "$C/p.yaml"
  t0=$SECONDS
  for i in $(seq 1 30); do
    ( "$CLI" proyecto razon "$C/p.yaml" IPC "$(printf m%02d "$i")" "razon $i" \
          > /dev/null 2> "$C/err.$i"; echo $? > "$C/rc.$i" ) &
  done
  wait
  ms=$(( SECONDS - t0 ))
  callados=""; avisan=""
  for i in $(seq 1 30); do
    if [ "$(cat "$C/rc.$i")" = 0 ]; then
      grep -q "razon: \"*razon $i\"*\$" "$C/p.yaml" || callados="$callados m$i"
    else
      avisan="$avisan m$i[rc=$(cat "$C/rc.$i"): $(tr '\n' ' ' < "$C/err.$i")]"
    fi
  done
  n=$(grep -c 'razon: "*razon [0-9]' "$C/p.yaml")
  echo "vuelta $v: $n de 30 en $ms s; callados:${callados:- ninguno}; avisan:${avisan:- ninguno}"
  [ -n "$callados" ] && tot_c=$((tot_c + 1)); [ -n "$avisan" ] && tot_a=$((tot_a + 1))
done
ls -la "$C"
echo "vueltas con perdida callada: $tot_c; con aviso: $tot_a"
[ $tot_c -eq 0 ]
