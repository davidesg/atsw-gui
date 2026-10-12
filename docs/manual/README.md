# El manual del alumno

- **[MANUAL.md](MANUAL.md)**: el manual. Un caso de principio a fin, el IPC de
  España frente al precio del petróleo WTI: proyecto, muestra, modelos
  univariantes con art y fue, anómalos e intervención, el caso y la función de
  transferencia con drtran, y la previsión. La filosofía es la de ART para los
  univariantes y la de mtram (drtran-python) para la transferencia.
- **[datos/ipc_wti.csv](datos/ipc_wti.csv)**: los datos del caso.
- **img/**: las capturas. No están hechas a mano: las saca el programa.

## Rehacerlo

Las capturas, el PDF y el vídeo salen del programa de verdad, así que cuando
cambia una ventana se vuelven a generar:

```sh
make                                      # el arbol construido
bash docs/manual/capturas/capturas.sh     # las capturas, en img/ (unos 10 min)
python3 docs/manual/construir.py          # MANUAL.html y MANUAL.pdf
bash docs/manual/video/video.sh           # el video, video/ATSW-ipc-wti.mp4
```

Hace falta `broadwayd` (Debian/Ubuntu: `libgtk-3-bin`): las ventanas se
dibujan fuera de la pantalla y no se ve nada mientras tanto. Para el PDF,
el módulo `markdown` de Python y Chromium o Chrome; para el vídeo, `ffmpeg`
con libass y libx264.

Con `IDIOMA=es_ES.UTF-8` las escenas corren en español (con coma decimal),
que es como arranca el programa en un ordenador en español: así se probó que
los números se leen y se escriben con punto también ahí. Si ese idioma no
está instalado, `localedef -i es_ES -f UTF-8 DIR/es_ES.UTF-8` y `LOCPATH=DIR`.

**La voz del vídeo** la pone [Piper](https://github.com/rhasspy/piper), una
voz sintética libre que funciona sin conexión. Sin ella el vídeo sale mudo,
con subtítulos:

```sh
python3 -m venv ~/piper && ~/piper/bin/pip install piper-tts
curl -LO https://huggingface.co/rhasspy/piper-voices/resolve/main/es/es_ES/davefx/medium/es_ES-davefx-medium.onnx
curl -LO https://huggingface.co/rhasspy/piper-voices/resolve/main/es/es_ES/davefx/medium/es_ES-davefx-medium.onnx.json
VOZ_MODELO=$PWD/es_ES-davefx-medium.onnx PIPER=~/piper/bin/piper bash docs/manual/video/video.sh
```

Cada rótulo se dice tal cual, leído para el oído (`video/voz.py`: «AR(1)» es
«a erre uno», «d = 1» es «de igual a uno»). Si un rótulo tiene que decir otra
cosa, el guion lo pone detrás: `rotulo TEXTO || LO QUE SE DICE`. Al grabar,
el rótulo siguiente espera a que acabe la frase: voz y subtítulo no se pisan.

`capturas.sh` corre los guiones de `capturas/escenas/` y `video.sh` los de
`video/escenas/`. Cada guion lanza un GUI con el módulo `capturas/escenas.c`
cargado, que pulsa botones por su rótulo, elige filas, abre menús y
fotografía o graba. Las órdenes están explicadas al principio de
`escenas.c`. Si un rótulo cambia, el guion se para diciendo cuál.

Las escenas **hacen** el caso desde cero, como lo haría un alumno: la 01 crea
el proyecto con los datos, la 02 identifica y estima el IPC, y así. Lo que se
ve en las fotos es lo que el programa calcula hoy. Los números que cita el
texto (LR = 81.59, ω₀ = 0.019, la previsión de enero de 2020…) hay que
revisarlos si cambia un motor.

El PDF también lo construye la CI (`.github/workflows/paquetes.yml`) y va
dentro de los paquetes de Windows y macOS.
