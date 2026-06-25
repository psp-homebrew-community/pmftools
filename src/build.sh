#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-wasm"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

emcmake cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release
emmake make -j"$(nproc)"

# Copy outputs next to the web files
cp pmftools_wasm.js "$SCRIPT_DIR/pmftools_wasm.js"
cp pmftools_wasm.wasm "$SCRIPT_DIR/pmftools_wasm.wasm"

echo "Build done. Outputs copied to sourcecode/."
