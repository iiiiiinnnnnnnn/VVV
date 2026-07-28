# PackageData.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$RuntimeData,

    [Parameter(Mandatory = $true)]
    [string]$PackageData
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$runtimeDataRoot = (Resolve-Path -LiteralPath $RuntimeData).Path
$manifestPath = Join-Path $runtimeDataRoot 'cached.ini'
if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Manifest not found: $manifestPath"
}

New-Item -ItemType Directory -Path $PackageData -Force | Out-Null
$packageDataRoot = (Resolve-Path -LiteralPath $PackageData).Path
$copiedCount = 0

foreach ($line in Get-Content -LiteralPath $manifestPath -Encoding UTF8) {
    if ($line -notmatch '^(file|model|mipmap)=(.+)$') { continue }

    $listedPath = $Matches[2].Trim().Replace('/', '\')
    if ($listedPath -notmatch '^Data\\(.+)$') {
        throw "Resource path must start with Data/: $listedPath"
    }

    $relativePath = $Matches[1]
    if ([System.IO.Path]::IsPathRooted($relativePath) -or $relativePath.Split('\') -contains '..') {
        throw "Resource path escapes Data/: $listedPath"
    }

    $sourcePath = Join-Path $runtimeDataRoot $relativePath
    if (!(Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Resource not found: $sourcePath"
    }

    $destinationPath = Join-Path $packageDataRoot $relativePath
    $destinationDirectory = Split-Path -Parent $destinationPath
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
    $copiedCount++
}

Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $packageDataRoot 'cached.ini') -Force
Write-Host "Copied $copiedCount resources and cached.ini."
