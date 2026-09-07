# CacheBuilder

生成処理は `Source/Resource/CacheBuilder.h/.cpp` に共通化しています
同じ `Game.sln` の開発ツールからDebug・Releaseのビルド後に呼び出します

元素材はルートの `Resources`、出力は `Bin/Debug/Resources` または `Bin/Release/Resources` です
ルートの `Resources/ResourceSettings.ini` は編集する除外・先読み設定です。
ビルド時に先読み設定を `Bin/<構成>/Resources/ResourceManifest.ini` に含めます。
このManifestは、その出力先に生成済みのファイル・更新日時・先読み設定の一覧です。
一覧はルートからコピーするものではなく、出力先ごとに生成します。
ゲームはBin側のManifestだけを読み込み、Bin単独で配布できます。
除外設定はビルド時に適用し、除外対象はManifestにも含めません。
旧Bin側の `ResourceSettings.ini` はビルド成功時に削除します。直下Resourcesの設定は残ります。
ゲーム起動時は生成せず、生成済みの一覧と先読み対象を読み込みます
Debugではシーン移動を受け付ける前にも更新時刻を確認します
変更分を生成して古いメモリキャッシュを破棄した後、次のシーンを読み込みます
未変更なら再生成せず、更新失敗時は現在のシーンに留まります
Release実行時は配布済みのキャッシュを読み込み、生成は行いません

Debugの起動画面にある `CACHE MANAGER` で候補一覧を検索し、除外とプレを切り替えます
チェックは元素材側の `Resources/ResourceSettings.ini` に自動保存され、ビルド時にも使われます
戻る、キャッシュに反映、次のシーン移動のいずれかで変更を反映します
除外は先読みより優先し、元素材を残して生成済みファイルとメモリキャッシュを外します
使用中の素材を除外するとゲームから読み込めなくなります
`Resources/Image` 以下の画像は初期状態で先読みON、個別にOFFへ変更できます
モデルと画像は実行用リソースを先読みし、VSTGなど他のファイルはバイト列を保持します
VSTGは保持したバイト列を利用しますが、地形や当たり判定の組み立てはシーン生成時に行います
UI・ステージのパーティクル画像はResourceManagerのテクスチャを共有します
先読み設定はManifestに含まれるので、Releaseの先読みにも反映されます

```ini
[Resources/Model/Enemy/aracore.vmdl]
exclude=0
preload=1
```

`Resources` を再帰的に調べ、モデルなどはコピー、`Terrain/Layers` の画像はDDSへ変換します
GLB・GLTF、開発ツール、`ninclude_` で始まる項目などは除外します
地形vxは正本の `Resources` から通常のリソースとしてコピーします
シェーダーは従来どおりVisual Studioでコンパイルします

`ResourceManifest.ini` は自動生成する一覧です

```ini
[resources]
model=Resources/Model/example.vmdl
updated=2026-08-31T03:00:00.1234567Z
preload=1
```

素材の更新時刻は100ns単位まで比較します
未変更の生成物は再生成しません
`updated` が空、または行がない項目は再生成します
元ファイルがなくなった場合は管理対象の出力だけを整理します
未知のファイルやシェーダーをまとめて削除することはありません

Debugのエディタ保存時は該当ファイルの時刻を一旦無効化して再生成し、
ResourceManager内の古いモデル・テクスチャも破棄します
次回読み込みから保存後の内容になります
既に配置済みのインスタンスをその場で置き換える処理は行いません

保存失敗は成功扱いせず、モデル本体は一時ファイルから置き換えます
キャッシュ更新に失敗した場合も未生成の時刻を残し、次回再試行します
外部ツールが内容だけを変更して時刻もサイズも保った場合は、`updated` を消すか `--force` で生成してください

手動実行は `Bin/Tools/Debug/CacheBuilder.exe` または `Bin/Tools/Release/CacheBuilder.exe` です
引数なしで対応する固定フォルダーを処理し、`--force` で全件再生成します
配布時にCacheBuilder本体を含める必要はありません

検証はツールのReleaseビルド後に `Test-CacheBuilder.ps1`、
GameのDebugビルド後に `Test-ResourceRefresh.ps1` を実行します
後者は描画なしのVMDLで、保存・再読込・メモリキャッシュ更新を検証します
テスト素材と実行ファイルは `Obj` に作成します
