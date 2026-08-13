@echo off
REM Build a self-contained Windows x64 distribution.
REM Run this from a Visual Studio 2022 Developer Command Prompt.
REM Prerequisites: CMake and Qt 6 MSVC 2022 x64 (including Multimedia).
setlocal enabledelayedexpansion
cd /d "%~dp0"

REM install-qt-action supplies QT_ROOT_DIR. For a local installation, set
REM QT6_DIR to a directory such as C:\Qt\6.8.2\msvc2022_64.
if "%QT_ROOT_DIR%"=="" set QT_ROOT_DIR=%QT6_DIR%
if "%QT_ROOT_DIR%"=="" (
    if exist "C:\Qt\6.11.1\msvc2022_64" set QT_ROOT_DIR=C:\Qt\6.11.1\msvc2022_64
    if exist "C:\Qt\6.10.1\msvc2022_64" set QT_ROOT_DIR=C:\Qt\6.10.1\msvc2022_64
    if exist "C:\Qt\6.8.2\msvc2022_64" set QT_ROOT_DIR=C:\Qt\6.8.2\msvc2022_64
)
if "%QT_ROOT_DIR%"=="" (
    echo ERROR: Set QT_ROOT_DIR or QT6_DIR to a Qt 6 MSVC x64 installation.
    exit /b 1
)

set PATH=%QT_ROOT_DIR%\bin;%PATH%
echo Using Qt: %QT_ROOT_DIR%
cmake -S . -B build_windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT_DIR%"
cmake --build build_windows
set QT_QPA_PLATFORM=offscreen
ctest --test-dir build_windows --output-on-failure

set DIST=AudioFall-Windows
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy "build_windows\audiofall.exe" "%DIST%\AudioFall.exe" >nul
copy "assets\audiofall.png" "%DIST%\audiofall.png" >nul
pushd "%DIST%"
windeployqt --release --compiler-runtime AudioFall.exe
popd

echo.
echo Packaged: %DIST%
echo Zip this folder for distribution, or use the GitHub release workflow.
endlocal
