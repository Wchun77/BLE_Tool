@echo off
echo ========================================
echo BLE Tool - Build Script
echo ========================================
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Generate project files (Visual Studio 2022)
echo [1/3] Generating project files...
cmake -G "Visual Studio 17 2022" -A x64 ..
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake generation failed!
    pause
    exit /b 1
)

REM Build
echo.
echo [2/3] Building project...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

REM Success
echo.
echo [3/3] Build complete!
echo.
echo Output files:
echo - EXE: build\bin\Release\ble_tool.exe
echo - DLL: build\lib\Release\ble_api.dll
echo.
pause