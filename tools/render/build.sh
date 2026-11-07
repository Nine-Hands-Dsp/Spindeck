#!/bin/bash
# Compilation Linux sans juceaide (en-tetes X11 incomplets) : modules JUCE + Spindeck + outil de capture.
# usage : tools/render/build.sh <dossier JUCE 8.0.15> <dossier de sortie>
set -e
JUCE=$1; OUT=$2; SRC=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$OUT/obj" "$OUT/gen"
python3 - "$SRC/nine_hands_logo_blanc.png" "$OUT/gen" <<'PY'
import sys,os
data=open(sys.argv[1],'rb').read(); out=sys.argv[2]
open(os.path.join(out,'BinaryData.h'),'w').write('#pragma once\nnamespace BinaryData {\n extern const char* nine_hands_logo_blanc_png;\n const int nine_hands_logo_blanc_pngSize = %d;\n}\n'%len(data))
arr=','.join(str(b) for b in data)
open(os.path.join(out,'BinaryData.cpp'),'w').write('#include "BinaryData.h"\nstatic const unsigned char d[]={%s};\nconst char* BinaryData::nine_hands_logo_blanc_png=(const char*)d;\n'%arr)
PY
MODS="juce_core juce_events juce_data_structures juce_graphics juce_gui_basics juce_gui_extra juce_audio_basics juce_audio_formats juce_audio_processors juce_audio_processors_headless juce_dsp"
DEFS="-DNDEBUG=1 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJucePlugin_Name=\"Spindeck\" -DJUCE_STANDALONE_APPLICATION=0 \
 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0 -DJUCE_USE_MP3AUDIOFORMAT=1 -DJUCE_MODAL_LOOPS_PERMITTED=1 \
 -DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XSHM=0 -DJUCE_USE_XRENDER=0 \
 -DJUCE_PLUGINHOST_VST3=0 -DJUCE_PLUGINHOST_LADSPA=0 -DJUCE_PLUGINHOST_LV2=0 -DJUCE_PLUGINHOST_ARA=0 -DJUCE_DISPLAY_SPLASH_SCREEN=0"
for m in $MODS; do DEFS="$DEFS -DJUCE_MODULE_AVAILABLE_$m=1"; done
CXX="g++ -std=c++17 -O2 -fPIC -I$JUCE/modules -I$SRC -I$OUT/gen $(pkg-config --cflags freetype2 fontconfig) $DEFS"
pids=()
compile() { [ -f "$2" ] && [ "$2" -nt "$1" ] && return 0; $CXX -c "$1" -o "$2"; }
for f in $(for m in $MODS; do ls "$JUCE"/modules/$m/$m.cpp "$JUCE"/modules/$m/${m}_[0-9].cpp "$JUCE"/modules/$m/juce_graphics_Harfbuzz.cpp "$JUCE"/modules/$m/juce_core_CompilationTime.cpp 2>/dev/null; done); do
  compile "$f" "$OUT/obj/$(basename "$f" .cpp).o" &
  pids+=($!)
  if [ ${#pids[@]} -ge 2 ]; then wait ${pids[0]}; pids=("${pids[@]:1}"); fi
done
wait
gcc -O2 -fPIC -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -I$JUCE/modules -c "$JUCE/modules/juce_graphics/juce_graphics_Sheenbidi.c" -o "$OUT/obj/sheenbidi.o"
for f in Track PluginProcessor PluginEditor; do $CXX -Wall -Wextra -Wno-unused-parameter -c "$SRC/$f.cpp" -o "$OUT/obj/$f.o"; done
$CXX -c "$SRC/tools/render/main.cpp" -o "$OUT/obj/main.o"
$CXX -c "$OUT/gen/BinaryData.cpp" -o "$OUT/obj/BinaryData.o"
g++ -o "$OUT/SpinRender" "$OUT"/obj/*.o $(pkg-config --libs freetype2 fontconfig) -lX11 -ldl -lpthread -lrt
echo built "$OUT/SpinRender"
