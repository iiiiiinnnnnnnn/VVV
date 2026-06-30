@echo off
setlocal
cd /d "%~dp0"

set RELEASE_REPO=iiiiiinnnnnnnn/shuushoku-download-site
set RELEASE_TAG=vvv-latest
set ZIP_NAME=VVV.zip
set OUT_DIR=bin\x64\Debug
set PACKAGE_DIR=package
set DIST_DIR=dist

echo [1/6] ビルド済みファイルを確認中...

if not exist "%OUT_DIR%\Game.exe" (
    echo Game.exe が見つかりません。
    echo 先に Visual Studio で Debug x64 ビルドしてください。
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

echo [2/6] 作業フォルダーを初期化中...

rmdir /s /q "%PACKAGE_DIR%" 2>nul
rmdir /s /q "%DIST_DIR%" 2>nul

mkdir "%PACKAGE_DIR%"
mkdir "%DIST_DIR%"

echo [3/6] exe と dll をコピー中...

copy "%OUT_DIR%\PhysXCommon_64.dll" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\PhysXCooking_64.dll" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\PhysXFoundation_64.dll" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\PVDRuntime_64.dll" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\PhysX_64.dll" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\Game.exe" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\Game.exp" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\Game.lib" "%PACKAGE_DIR%\" /Y
copy "%OUT_DIR%\Game.pdb" "%PACKAGE_DIR%\" /Y

if not exist "Data" (
    echo Data フォルダーが見つかりません。
    pause
    exit /b 1
)

echo [4/6] Data をコピー中...
echo .glb、指定ツール、ninclude_、Data\Terrain\Layers の png は除外します。

mkdir "%PACKAGE_DIR%\Data"

robocopy "Data" "%PACKAGE_DIR%\Data" /E /R:2 /W:1 ^
 /XF "*.glb" "DDSAssistant.bat" "texconv.exe" "FBX2glTF-windows-x64.exe" "FBX2glTF.bat" "ninclude_*" ^
 /XD "ninclude_*"

if %ERRORLEVEL% GEQ 8 (
    echo Data のコピーに失敗しました。
    pause
    exit /b 1
)

if exist "%PACKAGE_DIR%\Data\Terrain\Layers" (
    del /q "%PACKAGE_DIR%\Data\Terrain\Layers\*.png" 2>nul
)

if exist "imgui.ini" (
    copy "imgui.ini" "%PACKAGE_DIR%\" /Y
)

echo [5/6] zip を作成中...
echo Data が大きい場合、ここでしばらく止まって見えます。

pushd "%PACKAGE_DIR%"
tar -a -cf "..\%DIST_DIR%\%ZIP_NAME%" *
popd

if not exist "%DIST_DIR%\%ZIP_NAME%" (
    echo zip の作成に失敗しました。
    pause
    exit /b 1
)

echo [6/6] GitHub Release にアップロード中...

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