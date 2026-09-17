#!/bin/sh
# End-to-end test of the GUI gtk_fmg (make check-gui): tests/gui/driver.c
# clicks the buttons of the real GUI, which runs ./fug and shows the graphs
# in its graph window; the graphs are then saved as PDF, EPS, PNG and SVG.
# gv, gedit and xdg-open are replaced by fakes that only log what they open.
# Needs a display (or xvfb-run) and the GTK 2 development files.

TOP=$(cd "$(dirname "$0")/../.." && pwd)
WORK="$TOP/tests/gui/work"
CC=${CC:-cc}

if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ] && [ -z "$GUI_TEST_XVFB" ]; then
    if command -v xvfb-run > /dev/null; then
        GUI_TEST_XVFB=1 exec xvfb-run -a sh "$0" "$@"
    fi
    echo "check-gui needs a display (or xvfb-run)"
    exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK/bin" "$WORK/data" || exit 1
for viewer in gv gedit xdg-open; do
    printf '#!/bin/sh\necho "%s $*" >> "%s/viewers.log"\n' "$viewer" "$WORK" > "$WORK/bin/$viewer"
    chmod +x "$WORK/bin/$viewer"
done
cp "$TOP/examples/IPCM.txt" "$WORK/data/D.txt"
cp "$TOP/tests/data/FULL.inp" "$WORK/data/"
# the driver plays the part of gtk_fmg: fug is next to it
ln -s "$TOP/fug" "$WORK/fug"

GTK="$(pkg-config --cflags gtk+-2.0)" || exit 1
FLAGS="-O0 -g -Wall -Wno-deprecated-declarations -I$TOP/gui/include -I$TOP/src"
$CC $FLAGS $GTK -Dmain=gui_main -c "$TOP/gui/src/main.c" -o "$WORK/gui_main.o" || exit 1
$CC $FLAGS $GTK -o "$WORK/driver" "$TOP/tests/gui/driver.c" "$WORK/gui_main.o" \
    "$TOP/gui/src/callbacks.c" "$TOP/gui/src/data_load.c" "$TOP/gui/src/nlutils.c" \
    "$TOP/gui/src/fug_run.c" "$TOP/gui/src/preview.c" "$TOP/src/fugdraw.c" "$TOP/src/inpfile.c" \
    $(pkg-config --libs gtk+-2.0) -lm || exit 1

unset FUG
GUI_TEST_DIR="$WORK" GUI_TEST_LOG="$WORK/viewers.log" PATH="$WORK/bin:$PATH" GDK_BACKEND=x11 \
    "$WORK/driver" "$WORK/data"
