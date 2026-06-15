@echo off
setlocal
rem ============================================================
rem  Pouziti:  bump 1.0.1 "popis zmeny"
rem  Udela vse: nastavi verzi, prebuildi, commit + tag v gitu.
rem ============================================================

if "%~1"=="" (
  echo Pouziti: bump VERZE "popis zmeny"
  echo Priklad: bump 1.0.1 "oprava limiteru"
  exit /b 1
)

set VER=%~1
set MSG=%~2
if "%MSG%"=="" set MSG=v%VER%

set ROOT=%~dp0
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"

echo.
echo === Nastavuji verzi %VER% ===
powershell -NoProfile -Command "(Get-Content '%ROOT%CMakeLists.txt') -replace 'project\(M3KNormalizator VERSION [0-9]+\.[0-9]+\.[0-9]+', 'project(M3KNormalizator VERSION %VER%' | Set-Content '%ROOT%CMakeLists.txt'"

echo.
echo === Build ===
%CMAKE% -B "%ROOT%build" -S "%ROOT%" >nul
%CMAKE% --build "%ROOT%build" --config Release --parallel
if errorlevel 1 (
  echo.
  echo === BUILD SELHAL - nic se necommitovalo ===
  exit /b 1
)

echo.
echo === Commit + tag v gitu ===
git -C "%ROOT%" add -A
git -C "%ROOT%" commit -m "v%VER% - %MSG%"
git -C "%ROOT%" tag v%VER%

echo.
echo === HOTOVO: verze %VER% ===
echo VST3: build\M3KNormalizator_artefacts\Release\VST3\M3K Normalizator.vst3
