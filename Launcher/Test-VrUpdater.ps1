$ErrorActionPreference='Stop'
# Load only the transaction function: this test never queries GitHub or touches an installation.
$tokens=$null
$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Update-VR.ps1'),[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count) { throw ($parseErrors -join '; ') }
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Install-VrManagedFiles'},$true)
if(-not $function) { throw 'Updater transaction function missing.' }
. ([scriptblock]::Create($function.Extent.Text))
$temporary=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$root=Join-Path $temporary ('vr-updater-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
function Assert-Text([string]$Path,[string]$Expected) {
    if([IO.File]::ReadAllText($Path) -ne $Expected) { throw ('Incorrect contents: '+$Path) }
}
try {
    $source=Join-Path $root 'source'
    $destination=Join-Path $root 'installed'
    New-Item -ItemType Directory -Path $source,$destination | Out-Null
    [IO.File]::WriteAllText((Join-Path $source 'program'),'new')
    [IO.File]::WriteAllText((Join-Path $destination 'program'),'old')
    [IO.File]::WriteAllText((Join-Path $destination 'Config.toml'),'personal settings')
    $replace=@{From=(Join-Path $source 'program');To=(Join-Path $destination 'program');Relative='program'}
    Install-VrManagedFiles @($replace) (Join-Path $root 'success-backup')
    Assert-Text $replace.To 'new'
    Assert-Text (Join-Path $root 'success-backup/program') 'old'
    Assert-Text (Join-Path $destination 'Config.toml') 'personal settings'

    # A missing source fails after replacing one old file and creating one new file.
    # Both must roll back, without touching unrelated user data.
    [IO.File]::WriteAllText($replace.To,'old')
    $added=@{From=$replace.From;To=(Join-Path $destination 'added');Relative='added'}
    $failure=@{From=(Join-Path $source 'missing');To=(Join-Path $destination 'broken');Relative='broken'}
    $failed=$false
    try { Install-VrManagedFiles @($replace,$added,$failure) (Join-Path $root 'failure-backup') } catch { $failed=$true }
    if(-not $failed) { throw 'Expected the update failure to be reported.' }
    Assert-Text $replace.To 'old'
    if((Test-Path $added.To) -or (Test-Path $failure.To)) { throw 'New files survived rollback.' }
    Assert-Text (Join-Path $destination 'Config.toml') 'personal settings'

    # A file in use must fail without leaving earlier replacements installed.
    $lockedPath=Join-Path $destination 'locked'
    [IO.File]::WriteAllText($lockedPath,'locked old')
    $locked=@{From=$replace.From;To=$lockedPath;Relative='locked'}
    $handle=[IO.File]::Open($lockedPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
    $failed=$false
    try {
        try { Install-VrManagedFiles @($replace,$locked) (Join-Path $root 'locked-backup') } catch { $failed=$true }
    } finally { $handle.Dispose() }
    if(-not $failed) { throw 'Expected an exclusively locked destination to reject the update.' }
    Assert-Text $replace.To 'old'
    Assert-Text $lockedPath 'locked old'
    Write-Host 'PASS: portable updater replacement, backups, missing/locked-file rollback and user-data preservation.'
} finally {
    $resolved=[IO.Path]::GetFullPath($root)
    if(-not $resolved.StartsWith($temporary.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolved) -notlike 'vr-updater-test-*') { throw 'Unsafe test cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
