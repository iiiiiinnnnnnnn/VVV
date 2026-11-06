@echo off
setlocal enabledelayedexpansion

set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"

for /d %%D in (*) do (
    set "NAME=%%~nxD"
    set "SKIP="

    if "!NAME:~0,1!"=="." set "SKIP=1"

    if /i "!NAME!"=="Bin" set "SKIP=1"
    if /i "!NAME!"=="Obj" set "SKIP=1"
    if /i "!NAME!"=="dist" set "SKIP=1"
    if /i "!NAME!"=="External" set "SKIP=1"
    if /i "!NAME!"=="TOols" set "SKIP=1"

    if /i "!NAME!"=="Resources" set "SKIP=1"

    if not defined SKIP (
        echo Compressing: %%D
        "%SEVENZIP%" a -t7z "%%D.7z" "%%D\"
    )
)

echo.
echo Complete.
pause