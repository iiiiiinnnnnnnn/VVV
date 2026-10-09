param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$HeaderPath
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $SourceDirectory).Path.TrimEnd('\', '/')
$tracks = @{}
foreach ($file in Get-ChildItem -LiteralPath $root -Filter '*.wav' -File -Recurse | Sort-Object FullName) {
    $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
    $relative = $relative -replace '\[\d+\](?=\.wav$)', ''
    $name = ($relative -replace '\.wav$', '').ToUpperInvariant() -replace '[^A-Z0-9_]', '_'
    if ($name -notmatch '^[A-Z_][A-Z0-9_]*$') { throw "Invalid sound identifier: $name" }
    $path = "Resources/Sound/$relative"
    if ($tracks.ContainsKey($name) -and $tracks[$name] -ne $path) { throw "Duplicate sound identifier: $name" }
    $tracks[$name] = $path
}
$header = [IO.File]::ReadAllText($HeaderPath)
# Existing identifiers retain their order; newly discovered tracks are appended.
$names = @()
foreach ($match in [regex]::Matches($header, 'SoundDefinition\{SoundTrack::([A-Z0-9_]+),')) {
    $name = $match.Groups[1].Value
    if ($tracks.ContainsKey($name)) { $names += $name }
}
$names += @($tracks.Keys | Sort-Object | Where-Object { $_ -notin $names })
$enum = "enum class SoundTrack`r`n{`r`n" + (($names | ForEach-Object { "`t$_," }) -join "`r`n") + "`r`n};"
$definitions = foreach ($name in $names) {
    $path = $tracks[$name]
    "`tSoundDefinition{SoundTrack::$name, `"$name`", `"$path`"},"
}
$table = "inline constexpr std::array SoundDefinitions =`r`n{`r`n" + ($definitions -join "`r`n") + "`r`n};"
$updated = [regex]::Replace($header, 'enum class SoundTrack\s*\{.*?\};', [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $enum }, 'Singleline')
$updated = [regex]::Replace($updated, 'inline constexpr std::array SoundDefinitions =\s*\{.*?\};', [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $table }, 'Singleline')
if ($updated -ne $header) { [IO.File]::WriteAllText($HeaderPath, $updated, [Text.UTF8Encoding]::new($false)) }
