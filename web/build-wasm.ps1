$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repo 'build/emsdk'
$compiler=Get-Command emcc -ErrorAction SilentlyContinue
if($compiler){
    $emcc=$compiler.Source
} elseif(Test-Path (Join-Path $sdk 'upstream/emscripten/emcc.exe')){
    $emcc=Join-Path $sdk 'upstream/emscripten/emcc.exe'
    $env:EMSDK=$sdk
    $env:EM_CONFIG=Join-Path $sdk '.emscripten'
    $python=Get-ChildItem (Join-Path $sdk 'python') -Directory | ForEach-Object { Join-Path $_.FullName 'python.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
    $node=Get-ChildItem (Join-Path $sdk 'node') -Directory | ForEach-Object { Join-Path $_.FullName 'node.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
    if(-not $python -or -not $node){ throw 'The workspace-local emsdk installation is incomplete.' }
    $env:EMSDK_PYTHON=$python
    $env:EMSDK_NODE=$node
} else {
    throw 'Emscripten is required. Install the official emsdk, then run this script again.'
}
$dist=Join-Path $repo 'web/dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$source=@(
    'src/machine.c','src/word.c','src/instructions.c','src/timing.c',
    'src/media.c','src/blocks.c','src/strela_compat.c','src/system.c',
    'src/snapshot.c','src/state_json.c','web/strela_wasm.c'
) | ForEach-Object { Join-Path $repo $_ }
$exports='_sb_new,_sb_destroy,_sb_write_word,_sb_set_pc,_sb_mount_cards,_sb_mount_tape,_sb_pause,_sb_reset,_sb_check,_sb_step,_sb_run_us,_sb_save_snapshot,_sb_load_snapshot,_sb_state_json,_sb_punched_json'
$argsList=@(
    '-std=c99','-O2','-Wall','-Wextra','-Werror',
    ('-I'+(Join-Path $repo 'include')),('-I'+(Join-Path $repo 'src'))
) + $source + @(
    '-o',(Join-Path $dist 'strela_wasm.mjs'),'--no-entry','-lm',
    '-sMODULARIZE=1','-sEXPORT_ES6=1','-sENVIRONMENT=web,node',
    '-sALLOW_MEMORY_GROWTH=1','-sFORCE_FILESYSTEM=1',
    ('-sEXPORTED_FUNCTIONS='+$exports),
    '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS'
)
& $emcc @argsList
if($LASTEXITCODE -ne 0){ throw "emcc failed with exit code $LASTEXITCODE" }
foreach($asset in @('index.html','style.css','app.js','wasm-api.mjs','upp.mjs','catalog.json')){
    Copy-Item -LiteralPath (Join-Path $repo ('web/public/'+$asset)) -Destination (Join-Path $dist $asset) -Force
}
Copy-Item -LiteralPath (Join-Path $repo 'web/kitov-linear.strela') -Destination (Join-Path $dist 'kitov-linear.strela') -Force
Copy-Item -LiteralPath (Join-Path $repo 'web/vostok-orbit.strela') -Destination (Join-Path $dist 'vostok-orbit.strela') -Force
New-Item -ItemType Directory -Force (Join-Path $dist 'programs') | Out-Null
Copy-Item -Path (Join-Path $repo 'web/programs/*.strela') -Destination (Join-Path $dist 'programs') -Force
python (Join-Path $repo 'web/stamp-dist.py')
if($LASTEXITCODE -ne 0){ throw "stamp-dist failed with exit code $LASTEXITCODE" }
Write-Output "Static Strela playground built in $dist"

