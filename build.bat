@echo off
setlocal

set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
set BUILD_DIR=build

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Configuring...
%CMAKE% -B %BUILD_DIR% -S . -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto error

echo Building...
%CMAKE% --build %BUILD_DIR% --config Release --parallel
if errorlevel 1 goto error

echo.
echo === Build successful! ===
echo VST3 plugin: build\M3KNormalizator_artefacts\Release\VST3\M3K Normalizator.vst3
goto end

:error
echo.
echo === Build FAILED ===
exit /b 1

:end
