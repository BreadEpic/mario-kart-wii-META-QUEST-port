[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SetupExecutable,
    [Parameter(Mandatory=$true)][string]$Version,
    [string]$Dotnet='dotnet',
    [string]$WheelWizardRepository='https://github.com/TeamWheelWizard/WheelWizard.git',
    [switch]$AllowDirty
)
$ErrorActionPreference='Stop'
function Get-VrFileHash([string]$LiteralPath,[string]$Algorithm='SHA256') {
    $stream=[IO.File]::OpenRead($LiteralPath)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return @{Hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()} }
    finally { $sha.Dispose();$stream.Dispose() }
}
Set-StrictMode -Version 3
$repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$setup=[IO.Path]::GetFullPath($SetupExecutable)
if(-not (Test-Path -LiteralPath $setup -PathType Leaf)) { throw 'Build the ROM-free installer first.' }
if($Version -notmatch '^\d+\.\d+\.\d+([-.][A-Za-z0-9.-]+)?$') { throw 'Invalid portable version.' }
$revision=(& git -C $repoRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0) { throw 'Unable to identify the source revision.' }
if(-not $AllowDirty -and (& git -C $repoRoot status --porcelain)) { throw 'Release packaging requires a clean source checkout. Use -AllowDirty only for local development.' }
$pin='86618e7367df935d78401583136e492c6f00fa27'
$work=Join-Path $PSScriptRoot ('artifacts/portable-'+[Guid]::NewGuid().ToString('N'))
$upstream=Join-Path $work 'WheelWizard-source'
$bundle=Join-Path $work ('WiiCompiled-VR-Portable-v'+$Version)
$launcher=Join-Path $bundle 'WheelWizard'
New-Item -ItemType Directory -Path $launcher -Force | Out-Null
& git clone --no-checkout $WheelWizardRepository $upstream
if($LASTEXITCODE -ne 0) { throw 'Wheel Wizard checkout failed.' }
& git -C $upstream checkout --detach $pin
if($LASTEXITCODE -ne 0) { throw 'Pinned Wheel Wizard revision is unavailable.' }
$patch=Join-Path $repoRoot 'integrations/wheelwizard-vr.patch'
& git -C $upstream apply --check $patch
if($LASTEXITCODE -ne 0) { throw 'Wheel Wizard patch does not match the pinned revision.' }
& git -C $upstream apply $patch
if($LASTEXITCODE -ne 0) { throw 'Wheel Wizard patch failed.' }
& $Dotnet publish (Join-Path $upstream 'WheelWizard/WheelWizard.csproj') -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true -p:CSharpier_Check=false -o $launcher
if($LASTEXITCODE -ne 0) { throw 'Wheel Wizard publish failed.' }
Copy-Item -LiteralPath $setup -Destination (Join-Path $bundle 'WiiCompiled-VR-Setup.exe')
Copy-Item -LiteralPath (Join-Path $upstream 'LICENSE') -Destination (Join-Path $launcher 'LICENSE-WheelWizard.txt')
Set-Content -LiteralPath (Join-Path $launcher 'vr-local.txt') -Value 'portable' -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination (Join-Path $bundle 'README.md')
Copy-Item -LiteralPath (Join-Path $repoRoot 'OPENXR.md') -Destination (Join-Path $bundle 'OPENXR.md')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Update-VR.ps1') -Destination $bundle
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Update-VR.cmd') -Destination $bundle
@'
WiiCompiled VR Portable

Run WiiCompiled-VR-Setup.exe and select your own clean PAL RMCP01 disc image.
Keep portable installation enabled. Automatic Retro Rewind download is optional
and enabled by default. Run WheelWizard/WheelWizard.exe after compilation.

Default controls: right trigger accelerates; left trigger brakes/reverses;
Y uses items; X performs tricks; A drifts in first person; grips hold the wheel.
Right stick click changes cameras. X+Y or F10 opens VR settings.
See README.md and OPENXR.md for the full controls and available settings.

Keep the entire folder together. No ROM, game assets or translated game
executable are distributed. Your personal disc image stays on your PC.
'@ | Set-Content -LiteralPath (Join-Path $bundle 'README.txt') -Encoding UTF8
$files=@(Get-ChildItem -LiteralPath $bundle -Recurse -File)
foreach($file in $files) {
    if($file.Extension -match '^\.(iso|rvz|wbfs|dol|rel|pul)$' -or $file.Name -in @('WiiCompiled.exe','RetroRewind.exe')) {
        throw ('Game content is forbidden in the portable bundle: '+$file.Name)
    }
}
$manifest=[ordered]@{
    version=$Version; sourceRevision=$revision; development=[bool]$AllowDirty; wheelWizardRevision=$pin
    patchSha256=(Get-VrFileHash -LiteralPath $patch -Algorithm SHA256).Hash.ToLowerInvariant()
    files=@($files | Sort-Object FullName | ForEach-Object {
        @{path=$_.FullName.Substring($bundle.Length+1).Replace('\','/');sha256=(Get-VrFileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$_.Length}
    })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $bundle 'portable-manifest.json') -Encoding UTF8
$out=Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$archive=Join-Path $out ('WiiCompiled-VR-Portable-v'+$Version+'.zip')
if(Test-Path -LiteralPath $archive) { throw ('Refusing to overwrite an existing archive: '+$archive) }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($bundle,$archive,[IO.Compression.CompressionLevel]::Optimal,$true)
$hash=(Get-VrFileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath ($archive+'.sha256') -Value ($hash+'  '+[IO.Path]::GetFileName($archive)) -Encoding ASCII
Write-Host ('Portable ready: '+$archive)
