@echo off
REM QEM Simplifier 构建脚本 (Windows)
REM 需要: CMake 3.15+, Visual Studio 2019/2022, Python 3.8+

echo === QEM Mesh Simplifier Build ===

if not exist build mkdir build
cd build

echo.
echo [1/2] Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo Trying Visual Studio 16 2019...
    cmake .. -G "Visual Studio 16 2019" -A x64
)
if errorlevel 1 (
    echo ERROR: CMake configure failed. Install CMake and Visual Studio.
    pause
    exit /b 1
)

echo.
echo [2/2] Building Release...
cmake --build . --config Release --target qem_simplifier
if errorlevel 1 (
    echo ERROR: Build failed.
    pause
    exit /b 1
)

echo.
echo === Build complete ===
echo Python module: build\Release\qem_simplifier.pyd
echo CLI tool:      build\Release\qem_simplifier.exe
echo.
pause
