#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if ! command -v emcc >/dev/null 2>&1; then
  printf '%s\n' 'Для сборки требуется Emscripten (команда emcc).' >&2
  exit 1
fi

dist="$repo/web/dist"
mkdir -p "$dist"
emcc -std=c99 -O2 -Wall -Wextra -Werror \
  -I"$repo/include" -I"$repo/src" \
  "$repo/src/machine.c" "$repo/src/word.c" "$repo/src/instructions.c" \
  "$repo/src/timing.c" "$repo/src/media.c" "$repo/src/blocks.c" \
  "$repo/src/strela_compat.c" "$repo/src/system.c" "$repo/src/snapshot.c" \
  "$repo/src/state_json.c" "$repo/web/strela_wasm.c" \
  -o "$dist/strela_wasm.mjs" --no-entry -lm \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node \
  -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 \
  '-sEXPORTED_FUNCTIONS=_sb_new,_sb_destroy,_sb_write_word,_sb_set_pc,_sb_mount_cards,_sb_mount_tape,_sb_pause,_sb_reset,_sb_check,_sb_step,_sb_run_us,_sb_save_snapshot,_sb_load_snapshot,_sb_state_json,_sb_punched_json' \
  '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS'
cp "$repo/web/public/index.html" "$repo/web/public/style.css" \
   "$repo/web/public/app.js" "$repo/web/public/wasm-api.mjs" "$repo/web/public/upp.mjs" \
   "$repo/web/public/catalog.json" "$dist/"
mkdir -p "$dist/programs"
cp "$repo"/web/programs/*.strela "$dist/programs/"
python3 "$repo/web/stamp-dist.py"
cp "$repo/web/kitov-linear.strela" "$repo/web/vostok-orbit.strela" "$dist/"
printf '%s\n' "Файлы для размещения на сайте: $dist"

