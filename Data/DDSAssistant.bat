@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

:MENU
echo.
echo ==============================
echo PNG to DDS Converter
echo ==============================
echo [1] Albedo/BaseColor  BC7_UNORM
echo [2] Normal Map        BC5_UNORM
echo [3] Exit
echo.

choice /c 123 /n /m "Select type: "

if errorlevel 3 goto END
if errorlevel 2 goto NORMAL
if errorlevel 1 goto ALBEDO

:ALBEDO
set "FORMAT=BC7_UNORM"
set "KIND=Albedo/BaseColor"
goto INPUT

:NORMAL
set "FORMAT=BC5_UNORM"
set "KIND=Normal Map"
goto INPUT

:INPUT
echo.
echo %KIND% Ç %FORMAT% Ç≈ïœä∑ÇµÇ‹Ç∑ÅB
echo Example: Terrain\layer_grass.png
echo.

set "SRC="
set /p "SRC=PNG path: "

if "%SRC%"=="" goto MENU

if not exist "%SRC%" (
    echo File not found: "%SRC%"
    goto MENU
)

for %%F in ("%SRC%") do (
    set "OUTDIR=%%~dpF"
)

if "!OUTDIR:~-1!"=="\" set "OUTDIR=!OUTDIR:~0,-1!"

echo.
echo Command:
echo "%~dp0texconv.exe" -f %FORMAT% -m 0 -y -o "!OUTDIR!" "%SRC%"
echo.

"%~dp0texconv.exe" -f %FORMAT% -m 0 -y -o "!OUTDIR!" "%SRC%"

if errorlevel 1 (
    echo Failed.
) else (
    echo Done.
)

goto MENU

:END
endlocal