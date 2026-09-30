@rem Finally.bat
@echo off
setlocal
chcp 65001 >nul

for %%I in ("%~dp0..\..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"

set RELEASE_REPO=iiiiiinnnnnnnn/shuushoku-download-site
set RELEASE_TAG=vvv-latest
set ZIP_NAME=VVV.zip
set OUT_DIR=Bin\Development
set DIST_DIR=dist

echo [1/5] Checking development build...

if not exist "%OUT_DIR%\Game.exe" (
    echo Game.exe が見つかりません。
    echo 先に Visual Studio で Development x64 ビルドしてください。
    pause
    exit /b 1
)

where gh >nul 2>nul
if errorlevel 1 (
    echo GitHub CLI の gh が見つかりません。
    echo gh をインストールしてログインしてください。
    pause
    exit /b 1
)

echo [2/5] Building development Resources...

start "" /wait "%OUT_DIR%\Game.exe" 1

if errorlevel 1 (
    echo Resourcesの生成に失敗しました。
    pause
    exit /b 1
)

if not exist "%OUT_DIR%\Resources\ResourceManifest.ini" (
    echo Resources\ResourceManifest.ini が作成されませんでした。
    pause
    exit /b 1
)

echo [3/5] Initializing distribution folder...

rmdir /s /q "%DIST_DIR%" 2>nul
mkdir "%DIST_DIR%"

echo [4/5] Creating zip from Bin\Development...

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CreateZip.ps1" -SourceDirectory "%OUT_DIR%" -DestinationFile "%DIST_DIR%\%ZIP_NAME%"

if errorlevel 1 (
    echo zip の作成に失敗しました。
    pause
    exit /b 1
)

if not exist "%DIST_DIR%\%ZIP_NAME%" (
    echo zip ファイルが作成されませんでした。
    pause
    exit /b 1
)

echo [5/5] Uploading GitHub Release...

gh release view "%RELEASE_TAG%" --repo "%RELEASE_REPO%" >nul 2>nul

if errorlevel 1 (
    gh release create "%RELEASE_TAG%" "%DIST_DIR%\%ZIP_NAME%" --repo "%RELEASE_REPO%" --title "VVV Latest" --notes "Latest VVV development build"
) else (
    gh release upload "%RELEASE_TAG%" "%DIST_DIR%\%ZIP_NAME%" --repo "%RELEASE_REPO%" --clobber
)

if errorlevel 1 (
    echo GitHub Release へのアップロードに失敗しました。
    pause
    exit /b 1
)

echo 完了しました。
echo https://github.com/iiiiiinnnnnnnn/shuushoku-download-site/releases/download/vvv-latest/VVV.zip
pause
