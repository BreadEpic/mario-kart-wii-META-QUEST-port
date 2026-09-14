$ErrorActionPreference='Stop'
$repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source=Get-Content -LiteralPath (Join-Path $repoRoot 'runtime/include/runtime_config.h') -Raw -Encoding UTF8
$docs=Get-Content -LiteralPath (Join-Path $repoRoot 'OPENXR.md') -Raw -Encoding UTF8
foreach($key in @('first_person_units_per_meter','first_person_head_up_meters','first_person_head_forward_meters','first_person_head_right_meters','native_steering_wheel')) {
    $pattern=[regex]::Escape($key)+' = ([0-9.]+|true|false)'
    $code=[regex]::Match($source,$pattern)
    $example=[regex]::Match($docs,$pattern)
    if(-not $code.Success -or -not $example.Success -or $code.Groups[1].Value -ne $example.Groups[1].Value) { throw ('VR documentation/default mismatch: '+$key) }
}
Write-Host 'VR documentation matches generated cockpit defaults.'
