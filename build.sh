#!/bin/sh
set -e
cd "$(dirname "$0")"

echo "1/3 git pull"
git pull || echo "git pull не идет чет, собираюсь локально"

#на маке gcc = clang, ищет gcc-VV из brew
if [ "$(uname)" = "Darwin" ] && [ -z "$CC" ]; then
    V=$(ls /opt/homebrew/bin 2>/dev/null | grep -E '^gcc-[0-9]+$' | sed 's/gcc-//' | sort -n | tail -1)
    if [ -n "$V" ]; then
        export CC="gcc-$V" CXX="g++-$V"
    fi
fi
echo "Компилятор: ${CC:-по умолчанию}"

echo "2/3 cmake configure"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

echo "3/3 build"
cmake --build build

echo "ура конец, всё в build/bin"
