# CreateZip.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string]$DestinationFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
$destinationDirectory = Split-Path -Parent $DestinationFile
$destinationName = Split-Path -Leaf $DestinationFile
$destinationRoot = (Resolve-Path -LiteralPath $destinationDirectory).Path
$destinationPath = Join-Path $destinationRoot $destinationName

if (Test-Path -LiteralPath $destinationPath) {
    throw "Destination already exists: $destinationPath"
}

[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $sourcePath,
    $destinationPath,
    [System.IO.Compression.CompressionLevel]::Optimal,
    $false)

$archive = [System.IO.Compression.ZipFile]::OpenRead($destinationPath)
try {
    $entryPaths = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
    if ($entryPaths -notcontains 'Game.exe') { throw 'Game.exe is missing from the zip.' }
    if ($entryPaths -notcontains 'Data/cached.ini') { throw 'Data/cached.ini is missing from the zip.' }
    $entryCount = $archive.Entries.Count
}
finally {
    $archive.Dispose()
}

Write-Host "Created and validated $destinationName ($entryCount entries)."
