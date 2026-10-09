param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$HeaderPath
)
$ErrorActionPreference = 'Stop'
$effects = @(Get-ChildItem -LiteralPath $SourceDirectory -Filter '*.efkpkg' -File | Sort-Object Name)
$names = @($effects | ForEach-Object { $_.BaseName.ToUpperInvariant() -replace '[^A-Z0-9_]', '_' })
if (@($names | Select-Object -Unique).Count -ne $names.Count) { throw 'Duplicate effect identifiers.' }
foreach ($name in $names) {
    if ($name -notmatch '^[A-Z_][A-Z0-9_]*$') { throw "Invalid effect identifier: $name" }
}
$header = [IO.File]::ReadAllText($HeaderPath)
$enum = "enum class EffectId`r`n{`r`n" + (($names | ForEach-Object { "`t$_," }) -join "`r`n") + "`r`n};"
$definitions = for ($i = 0; $i -lt $effects.Count; ++$i) {
    $name = $names[$i]
    $file = $effects[$i].Name
    if ($file.Contains('"') -or $file.Contains('\')) { throw "Invalid effect filename: $file" }
    "`tEffectDefinition{EffectId::$name, `"$name`", `"Resources/Effect/$file`"},"
}
$table = "inline constexpr std::array EffectDefinitions =`r`n{`r`n" + ($definitions -join "`r`n") + "`r`n};"
$updated = [regex]::Replace($header, 'enum class EffectId\s*\{.*?\};', [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $enum }, 'Singleline')
$updated = [regex]::Replace($updated, 'inline constexpr std::array EffectDefinitions =\s*\{.*?\};', [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $table }, 'Singleline')
if ($updated -ne $header) { [IO.File]::WriteAllText($HeaderPath, $updated, [Text.UTF8Encoding]::new($false)) }
