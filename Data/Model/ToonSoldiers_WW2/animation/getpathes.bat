@echo off
setlocal enabledelayedexpansion

for /r %%f in (*.glb) do (
    set "full=%%f"
    set "rel=!full:*Data\=Data\!"
    set "rel=!rel:\=/!"
    echo #!rel!;
)
@pause