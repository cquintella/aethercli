#!/bin/bash
# Script para compilar com detecção de race conditions

set -e

BUILD_DIR="build_tsan"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "=== COMPILAÇÃO COM DETECÇÃO DE RACE CONDITIONS ==="
echo "Usando: ThreadSanitizer (TSAN)"
echo ""
echo "⚠️  Nota: TSAN tem overhead muito alto, programa será lento"
echo ""

cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_DEBUG=ON \
    -DENABLE_TSAN=ON

cmake --build . --config Debug

echo ""
echo "✅ Build completo! Executável: ./aethercli"
echo ""
echo "Para executar com detecção de race conditions:"
echo "  ./build_tsan/aethercli"
echo ""
echo "Variáveis de ambiente úteis:"
echo "  TSAN_OPTIONS=verbosity=2:halt_on_error=1"
echo ""
echo "Exemplos de execução:"
echo "  # Modo headless (mais rápido para testes)"
echo "  ./build_tsan/aethercli -p 'help'"
