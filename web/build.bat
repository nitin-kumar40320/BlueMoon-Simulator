@echo off
REM Ensure Emscripten is installed and activated.
REM If not, you can install it from https://emscripten.org/docs/getting_started/downloads.html

REM Compile the C++ code to WebAssembly using embind.
emcc simulator.cpp ^
  -o simulator.js ^
  -s WASM=1 ^
  -s ALLOW_MEMORY_GROWTH=1 ^
  -s ASSERTIONS=1 ^
  -s "EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']" ^
  -s MODULARIZE=1 ^
  -s EXPORT_NAME="createModule" ^
  -s ENVIRONMENT=web ^
  -s DISABLE_EXCEPTION_CATCHING=0 ^
  --no-entry ^
  --bind ^
  -O2

echo Build complete. Output files: simulator.js and simulator.wasm
pause
