# Test-CacheBuilder.ps1
param([string]$Executable = "$PSScriptRoot/../../Bin/Tools/Release/CacheBuilder.exe")
$ErrorActionPreference = 'Stop'
$tool = (Resolve-Path -LiteralPath $Executable).Path
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ('../../Obj/CacheBuilderTests/' + [guid]::NewGuid())))
$source = Join-Path $root 'Resources'
$output = Join-Path $root 'Output'
foreach ($directory in @('Resources/Model', 'Resources/Image', 'Resources/Terrain/Layers', 'Output')) {
    [IO.Directory]::CreateDirectory((Join-Path $root $directory)) | Out-Null
}
function Check([bool]$Condition, [string]$Name) {
    if (!$Condition) { throw "FAIL: $Name" }
    Write-Output "PASS: $Name"
}
function Run-Builder([int]$Expected = 0, [string]$Saved = '', [string]$OutputPath = $output) {
    $start = [Diagnostics.ProcessStartInfo]::new($tool)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('--source', $source, '--output', $OutputPath)) { $start.ArgumentList.Add($argument) }
    if($Saved) { $start.ArgumentList.Add('--saved-source'); $start.ArgumentList.Add($Saved) }
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $script:LastResult = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
    if ($process.ExitCode -ne $Expected) { throw "Unexpected exit $($process.ExitCode): $script:LastResult" }
}
function Write-Bitmap([string]$Path, [byte]$Color) {
    $stream = [IO.File]::Create($Path)
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([byte[]](66,77))
        $writer.Write([uint32]102)
        $writer.Write([uint32]0)
        $writer.Write([uint32]54)
        $writer.Write([uint32]40)
        $writer.Write([int32]4)
        $writer.Write([int32]4)
        $writer.Write([uint16]1)
        $writer.Write([uint16]24)
        $writer.Write([uint32]0)
        $writer.Write([uint32]48)
        foreach ($i in 1..4) { $writer.Write([uint32]0) }
        foreach ($i in 1..48) { $writer.Write($Color) }
    } finally { $writer.Dispose() }
}

$model = Join-Path $source 'Model/model.vmdl'
$runtimeModel = Join-Path $output 'Model/model.vmdl'
$bitmap = Join-Path $source 'Terrain/Layers/tile.bmp'
$dds = Join-Path $output 'Terrain/Layers/tile.dds'
$manifestPath = Join-Path $output 'ResourceManifest.ini'
[IO.File]::WriteAllText($model, 'model old')
[IO.File]::WriteAllText((Join-Path $source 'Image/remove.bin'), 'remove fixture')
[IO.File]::WriteAllText((Join-Path $source 'Model/model.glb'), 'excluded')
[IO.File]::WriteAllText((Join-Path $source 'Model/ninclude_hidden.vmdl'), 'excluded')
Write-Bitmap $bitmap 10
[IO.File]::SetLastWriteTimeUtc($bitmap, [datetime]::Parse('2026-08-01T00:00:00.1234567Z').ToUniversalTime())
Run-Builder
$manifest = [IO.File]::ReadAllText($manifestPath)
Check ($manifest.Contains('model=Resources/Model/model.vmdl') -and $manifest.Contains('mipmap=Resources/Terrain/Layers/tile.dds')) 'fixed source layout without config'
Check ($manifest.Contains('updated=2026-08-01T00:00:00.1234567Z')) 'source timestamp includes subsecond precision'
Check (!(Test-Path (Join-Path $output 'Model/model.glb')) -and !(Test-Path (Join-Path $output 'Model/ninclude_hidden.vmdl'))) 'development files excluded'
$bytes=[IO.File]::ReadAllBytes($dds)
Check ([Text.Encoding]::ASCII.GetString($bytes,0,4) -eq 'DDS ' -and [BitConverter]::ToUInt32($bytes,28) -eq 3) 'native DDS mip chain'
$before=[IO.File]::GetLastWriteTimeUtc($manifestPath)
Run-Builder
Check ($script:LastResult.Contains('3 unchanged') -and [IO.File]::GetLastWriteTimeUtc($manifestPath) -eq $before) 'unchanged build preserves manifest and outputs'
$oldHash=(Get-FileHash $dds).Hash
$time=[IO.File]::GetLastWriteTimeUtc($bitmap)
Write-Bitmap $bitmap 200
[IO.File]::SetLastWriteTimeUtc($bitmap,$time.AddTicks(1))
Run-Builder
Check ($script:LastResult.Contains('1 converted') -and (Get-FileHash $dds).Hash -ne $oldHash) '100ns source timestamp change regenerates DDS'
$time=[IO.File]::GetLastWriteTimeUtc($model)
[IO.File]::WriteAllText($model, 'model new')
[IO.File]::SetLastWriteTimeUtc($model,$time)
Run-Builder 0 $model
Check ([IO.File]::ReadAllText($runtimeModel) -eq 'model new') 'editor save refreshes even with identical timestamp and size'
$manifest=[IO.File]::ReadAllText($manifestPath)
$manifest=[regex]::Replace($manifest,'(model=Resources/Model/model.vmdl\n)updated=[^\n]*','$1updated=')
[IO.File]::WriteAllText($manifestPath,$manifest)
Run-Builder
Check ($script:LastResult.Contains('1 copied')) 'cleared ResourceManifest.ini timestamp forces regeneration'
[IO.File]::WriteAllText($runtimeModel,'damaged')
Run-Builder
Check ([IO.File]::ReadAllText($runtimeModel) -eq 'model new') 'damaged copied file repaired'
Remove-Item -LiteralPath $dds
Run-Builder
Check ($script:LastResult.Contains('1 converted')) 'missing output regenerated'
$time=[IO.File]::GetLastWriteTimeUtc($bitmap)
[IO.File]::WriteAllText($bitmap,'broken image')
[IO.File]::SetLastWriteTimeUtc($bitmap,$time)
Run-Builder 1 $bitmap
Check ([IO.File]::ReadAllText($manifestPath).Contains("mipmap=Resources/Terrain/Layers/tile.dds`nupdated=`n")) 'conversion failure leaves timestamp invalidated'
Write-Bitmap $bitmap 180
[IO.File]::SetLastWriteTimeUtc($bitmap,$time)
Run-Builder
Check ($script:LastResult.Contains('1 converted')) 'failed conversion retries without manual ini edit'
[IO.File]::WriteAllText((Join-Path $output 'untouched.cso'),'unmanaged')
Remove-Item -LiteralPath (Join-Path $source 'Image/remove.bin')
Run-Builder
Check (!(Test-Path (Join-Path $output 'Image/remove.bin')) -and (Test-Path (Join-Path $output 'untouched.cso'))) 'only obsolete managed output removed'
$manifest=[IO.File]::ReadAllText($manifestPath)
[IO.File]::WriteAllText($manifestPath,"[resources]`nfile=Resources/../outside.bin`nupdated=`n")
Run-Builder 1
Check ($script:LastResult.Contains('Parent paths')) 'manifest traversal rejected'
[IO.File]::WriteAllText($manifestPath,$manifest)
Run-Builder 1 '' $source
Check ($script:LastResult.Contains('overlap')) 'source output overlap rejected'
[IO.File]::Copy($bitmap,(Join-Path $source 'Terrain/Layers/tile.png'))
Run-Builder 1
Check ($script:LastResult.Contains('Duplicate output')) 'DDS output collision rejected'
Remove-Item -LiteralPath (Join-Path $source 'Terrain/Layers/tile.png')
$prebuilt=Join-Path $source 'Terrain'
[IO.Directory]::CreateDirectory($prebuilt)|Out-Null
[IO.File]::WriteAllText((Join-Path $prebuilt 'terrain.vx'),'broken')
Run-Builder 1
Check ($script:LastResult.Contains('Invalid terrain vx')) 'invalid source vx rejected'
$vx=[byte[]]::new(120)
[BitConverter]::GetBytes([uint32]0x58565656).CopyTo($vx,0)
[BitConverter]::GetBytes([uint32]3).CopyTo($vx,4)
[BitConverter]::GetBytes([uint32]3).CopyTo($vx,8)
[BitConverter]::GetBytes([uint32]3).CopyTo($vx,12)
[IO.File]::WriteAllBytes((Join-Path $prebuilt 'terrain.vx'),$vx)
Run-Builder
Check (Test-Path (Join-Path $output 'Terrain/terrain.vx')) 'vx copied from source Resources'
Write-Bitmap (Join-Path $source 'Terrain/Layers/second.bmp') 40
Write-Bitmap $bitmap 80
Run-Builder
Check ($script:LastResult.Contains('2 converted')) 'multiple DDS conversions in one process'
$settingsPath=Join-Path $source 'ResourceSettings.ini'
[IO.File]::WriteAllText($settingsPath,"[Resources/Model/model.vmdl]`nexclude=1`npreload=1`n[Resources/Terrain/Layers/tile.dds]`nexclude=1`npreload=0`n")
Run-Builder
Check (!(Test-Path $runtimeModel) -and !(Test-Path $dds) -and (Test-Path $model) -and (Test-Path $bitmap)) 'configured exclusion removes generated files only'
Check (![IO.File]::ReadAllText($manifestPath).Contains('model=Resources/Model/model.vmdl')) 'excluded asset omitted from manifest'
Check (!(Test-Path (Join-Path $output 'ResourceSettings.ini'))) 'runtime settings are not deployed'
[IO.File]::WriteAllText($settingsPath,"[Resources/Model/model.vmdl]`nexclude=0`npreload=1`n")
Run-Builder
Check ((Test-Path $runtimeModel) -and (Test-Path $dds)) 'unchecking exclusion restores generated files'
$manifest=[IO.File]::ReadAllText($manifestPath)
Check ($manifest -match 'model=Resources/Model/model.vmdl\nupdated=[^\n]*\npreload=1') 'manifest contains preload setting'
$settingsTime=[IO.File]::GetLastWriteTimeUtc($manifestPath)
Run-Builder
Check ([IO.File]::GetLastWriteTimeUtc($manifestPath) -eq $settingsTime) 'unchanged manifest is not rewritten'
[IO.File]::WriteAllText((Join-Path $output 'ResourceSettings.ini'), 'legacy runtime settings')
[IO.File]::WriteAllText($settingsPath,"[Resources/Model/model.vmdl]`nexclude=0`npreload=0`n")
Run-Builder
Check ($script:LastResult.Contains('0 converted, 0 copied') -and
    [IO.File]::ReadAllText($manifestPath) -match 'model=Resources/Model/model.vmdl\nupdated=[^\n]*\npreload=0') 'preload-only edit updates manifest without rebuilding assets'
Check (!(Test-Path (Join-Path $output 'ResourceSettings.ini')) -and (Test-Path $settingsPath)) 'legacy runtime settings removed and source settings preserved'
[IO.File]::WriteAllText($settingsPath,"[Resources/../outside.bin]`nexclude=1`n")
Run-Builder 1
Check ($script:LastResult.Contains('Invalid ResourceSettings.ini path')) 'invalid settings path rejected'
Write-Output "Fixtures: $root"
