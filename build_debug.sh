#!/bin/bash
# Script para compilar com detecção de memória e race conditions

set -e

BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "=== COMPILAÇÃO COM DETECÇÃO DE VAZAMENTO DE MEMÓRIA ==="
echo "Usando: AddressSanitizer (ASAN)"
echo ""

cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_DEBUG=ON \
    -DENABLE_ASAN=ON

cmake --build . --config Debug

echo ""
echo "✅ Build completo! Executável: ./aethercli"
echo ""
echo "Para executar com detecção de vazamentos:"
echo "  ./build/aethercli"
echo ""
echo "Variáveis de ambiente úteis:"
echo "  ASAN_OPTIONS=verbosity=1:halt_on_error=1"
echo "  LSAN_OPTIONS=verbosity=1"
