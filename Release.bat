@rem Release.bat
@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

set RELEASE_REPO=iiiiiinnnnnnnn/shuushoku-download-site
set RELEASE_TAG=vvv-latest
set ZIP_NAME=VVV.zip
set OUT_DIR=bin\x64\Release
set DIST_DIR=dist

echo [1/4] ビルド済みファイルを確認中...

if not exist "%OUT_DIR%\Game.exe" (
    echo Game.exe が見つかりません。
    echo 先に Visual Studio で Release x64 ビルドしてください。
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

echo [2/4] 作業フォルダーを初期化中...

rmdir /s /q "%DIST_DIR%" 2>nul

mkdir "%DIST_DIR%"

echo [3/4] bin\x64\Release 全体の zip を作成中...

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

echo [4/4] GitHub Release にアップロード中...

gh release view "%RELEASE_TAG%" --repo "%RELEASE_REPO%" >nul 2>nul

if errorlevel 1 (
    gh release create "%RELEASE_TAG%" "%DIST_DIR%\%ZIP_NAME%" --repo "%RELEASE_REPO%" --title "VVV Latest" --notes "Latest VVV build"
) else (
    gh release upload "%RELEASE_TAG%" "%DIST_DIR%\%ZIP_NAME%" --repo "%RELEASE_REPO%" --clobber
)

if errorlevel 1 (
    echo Release へのアップロードに失敗しました。
    pause
    exit /b 1
)

echo 完了しました。
echo https://github.com/iiiiiinnnnnnnn/shuushoku-download-site/releases/download/vvv-latest/VVV.zip
pause
