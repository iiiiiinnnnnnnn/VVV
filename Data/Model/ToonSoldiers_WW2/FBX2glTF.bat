@echo off
cd /d "%~dp0"

echo === FBX → GLB 変換開始 ===

rem --- 再帰的に全ての .fbx を処理 ---
for /r %%i in (*.fbx) do (
    echo Converting: %%i
    .\FBX2glTF-windows-x64.exe --binary --verbose --input "%%i" --output "%%~dpni.glb"

    rem 変換成功したら元のFBXを削除
    if exist "%%~dpni.glb" (
        del "%%i"
    )
)

echo === .meta 削除中 ===

rem --- 再帰的に全ての .meta を削除 ---
for /r %%m in (*.meta) do (
    echo Deleting meta: %%m
    del "%%m"
)

echo === 完了 ===
pause
