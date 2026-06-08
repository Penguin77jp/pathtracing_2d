#!/bin/sh

set -eu

if [ "$(uname)" = "Darwin" ] && [ -z "${OpenMP_ROOT:-}" ]; then
    if command -v brew >/dev/null 2>&1 && brew --prefix libomp >/dev/null 2>&1; then
        OpenMP_ROOT="$(brew --prefix libomp)"
        export OpenMP_ROOT
    elif [ -d /opt/homebrew/opt/libomp ]; then
        OpenMP_ROOT=/opt/homebrew/opt/libomp
        export OpenMP_ROOT
    elif [ -d /usr/local/opt/libomp ]; then
        OpenMP_ROOT=/usr/local/opt/libomp
        export OpenMP_ROOT
    elif [ -n "${CONDA_PREFIX:-}" ] && [ -d "$CONDA_PREFIX/include" ] && [ -d "$CONDA_PREFIX/lib" ]; then
        OpenMP_ROOT="$CONDA_PREFIX"
        export OpenMP_ROOT
    fi
fi

if [ "$(uname)" = "Darwin" ] && [ -n "${OpenMP_ROOT:-}" ]; then
    cmake -S . -B build \
        -DOpenMP_ROOT="$OpenMP_ROOT" \
        -DOpenMP_CXX_FLAGS="-Xpreprocessor -fopenmp -I$OpenMP_ROOT/include" \
        -DOpenMP_CXX_LIB_NAMES=omp \
        -DOpenMP_omp_LIBRARY="$OpenMP_ROOT/lib/libomp.dylib"
else
    cmake -S . -B build
fi
cmake --build build -j
./build/pathtracing_2d
