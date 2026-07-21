# PackageData.ps1

param(
	[Parameter(Mandatory = $true)]
	[string]$RuntimeData,
	[Parameter(Mandatory = $true)]
	[string]$PackageData
)

$ErrorActionPreference = 'Stop'
$runtimeRoot = [IO.Path]::GetFullPath($RuntimeData)
$packageRoot = [IO.Path]::GetFullPath($PackageData)
$runtimePrefix = $runtimeRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
$packagePrefix = $packageRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
$pathList = Join-Path $runtimeRoot 'cached.ini'

if (!(Test-Path -LiteralPath $pathList -PathType Leaf)) {
	throw 'Data/cached.ini was not found. Run the Debug build once to create resource caches.'
}

New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
Copy-Item -LiteralPath $pathList -Destination (Join-Path $packageRoot 'cached.ini') -Force

$lines = Get-Content -LiteralPath $pathList
$inResources = $false
foreach ($line in $lines) {
	if ([string]::IsNullOrWhiteSpace($line)) { continue }
	if ($line -eq '[resources]') {
		$inResources = $true
		continue
	}
	if ($line.StartsWith('[')) {
		$inResources = $false
		continue
	}
	if (!$inResources -or $line.StartsWith(';') -or $line.StartsWith('#')) { continue }
	$columns = $line -split '=', 2
	if ($columns.Count -ne 2) { throw "Invalid Data/cached.ini line: $line" }
	if ($columns[0] -eq 'updated') { continue }

	$cachedPath = $columns[1].Replace('/', [IO.Path]::DirectorySeparatorChar)
	if (!$cachedPath.StartsWith("Data$([IO.Path]::DirectorySeparatorChar)")) {
		throw "Cached path is outside Data: $cachedPath"
	}

	$relativePath = $cachedPath.Substring(5)
	$sourcePath = [IO.Path]::GetFullPath((Join-Path $runtimeRoot $relativePath))
	$destinationPath = [IO.Path]::GetFullPath((Join-Path $packageRoot $relativePath))
	if (!$sourcePath.StartsWith($runtimePrefix, [StringComparison]::OrdinalIgnoreCase)) {
		throw "Source path escaped runtime Data: $cachedPath"
	}
	if (!$destinationPath.StartsWith($packagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
		throw "Destination path escaped package Data: $cachedPath"
	}
	if (!(Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
		throw "Cached resource was not found: $sourcePath"
	}

	New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destinationPath)) -Force | Out-Null
	Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}
