@echo off
REM Build a self-contained Windows x64 distribution.
REM Run this from a Visual Studio 2022 Developer Command Prompt.
REM Prerequisites: CMake and Qt 6 MSVC 2022 x64 (including Multimedia).
setlocal enabledelayedexpansion
set ROOT=%~dp0
cd /d "%ROOT%"

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
if not exist build_windows mkdir build_windows
pushd build_windows
cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT_DIR%"
cmake --build . --config Release
set QT_QPA_PLATFORM=offscreen
ctest -C Release --output-on-failure
popd

set DIST=%ROOT%AudioFall-Windows
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy "%ROOT%build_windows\Release\audiofall.exe" "%DIST%\AudioFall.exe" >nul
copy "%ROOT%assets\audiofall.png" "%DIST%\audiofall.png" >nul
pushd "%DIST%"
windeployqt --release --compiler-runtime AudioFall.exe
popd

echo.
echo Packaged: %DIST%
echo Zip this folder for distribution, or use the GitHub release workflow.
endlocal
