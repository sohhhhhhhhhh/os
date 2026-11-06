@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo 1/3 git pull
git pull
if errorlevel 1 echo git pull не идет лол, собираю локально

echo 2/3 cmake configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

echo 3/3 build
cmake --build build
if errorlevel 1 exit /b 1

echo готово, всё в build\bin
