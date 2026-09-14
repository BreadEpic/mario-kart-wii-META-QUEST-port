[CmdletBinding()]
param([switch]$Install)
$ErrorActionPreference='Stop'
function Get-VrFileHash([string]$LiteralPath,[string]$Algorithm='SHA256') {
    $stream=[IO.File]::OpenRead($LiteralPath)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return @{Hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()} }
    finally { $sha.Dispose();$stream.Dispose() }
}
function Install-VrManagedFiles($Operations, [string]$Backup) {
    $changed=@()
    try {
        foreach($operation in $Operations) {
            $old=Join-Path $Backup $operation.Relative
            $existed=Test-Path -LiteralPath $operation.To
            if($existed) {
                New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($old)) -Force | Out-Null
                Copy-Item -LiteralPath $operation.To -Destination $old -ErrorAction Stop
            }
            $changed+=@{Target=$operation.To;Backup=$old;Existed=$existed}
            New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($operation.To)) -Force | Out-Null
            Copy-Item -LiteralPath $operation.From -Destination $operation.To -Force -ErrorAction Stop
        }
    } catch {
        $failure=$_
        $restoreErrors=@()
        for($i=$changed.Count-1;$i -ge 0;$i--) {
            $item=$changed[$i]
            try {
                if($item.Existed) { Copy-Item -LiteralPath $item.Backup -Destination $item.Target -Force -ErrorAction Stop }
                elseif(Test-Path -LiteralPath $item.Target) { Remove-Item -LiteralPath $item.Target -Force -ErrorAction Stop }
            } catch { $restoreErrors+=$_.Exception.Message }
        }
        if($restoreErrors.Count) { throw ('Update failed: '+$failure.Exception.Message+'. Some files could not be restored. Previous files are preserved at '+$Backup+'. '+($restoreErrors -join '; ')) }
        throw $failure
    }
}
$bundle=[IO.Path]::GetFullPath($PSScriptRoot)
$log=Join-Path $bundle 'VR-update.log'
try {
    $headers=@{'User-Agent'='WiiCompiled-VR-Updater';Accept='application/vnd.github+json'}
    $release=Invoke-RestMethod -Uri 'https://api.github.com/repos/heurazy/mario-kart-wii-VR-port/releases/latest' -Headers $headers -TimeoutSec 30
    $latestVersion=[version]($release.tag_name.TrimStart('v'))
    $installedManifest=Join-Path $bundle 'portable-manifest.json'
    if(Test-Path -LiteralPath $installedManifest) {
        $installed=Get-Content -LiteralPath $installedManifest -Raw | ConvertFrom-Json
        $number=[regex]::Match($installed.version,'^\d+\.\d+\.\d+').Value
        if($number -and [version]$number -ge $latestVersion -and -not $installed.development) {
            Write-Host 'The portable VR tools are already up to date.'
            exit 0
        }
    }
    $asset=@($release.assets | Where-Object { $_.name -match '^WiiCompiled-VR-Portable-v[0-9].*\.zip$' })
    if($asset.Count -ne 1) { throw 'The release does not have a unique portable archive.' }
    $checksum=@($release.assets | Where-Object { $_.name -eq ($asset[0].name+'.sha256') })
    if($checksum.Count -ne 1) { throw 'This release has no verified update checksum. Download it manually from the project release page.' }
    Write-Host ('Latest VR version: '+$release.tag_name)
    if(-not $Install) { Write-Host 'Run Update-VR.cmd to install it. Game files and personal settings are preserved.';exit 0 }
    $running=@(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($bundle+'\',[StringComparison]::OrdinalIgnoreCase) -and $_.Id -ne $PID })
    if($running.Count) {
        Write-Host 'Close Wheel Wizard and both VR games, then press Enter to continue.'
        Read-Host | Out-Null
        foreach($process in $running) { if(Get-Process -Id $process.Id -ErrorAction SilentlyContinue) { throw 'A portable application is still running. Close it and retry.' } }
    }
    $stage=Join-Path $bundle ('.vr-update-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage | Out-Null
    $zip=Join-Path $stage 'update.zip'
    Invoke-WebRequest -Uri $asset[0].browser_download_url -Headers $headers -OutFile $zip -TimeoutSec 1800 -UseBasicParsing
    $expected=(Invoke-WebRequest -Uri $checksum[0].browser_download_url -Headers $headers -TimeoutSec 30 -UseBasicParsing).Content.Trim().Split(' ')[0].ToLowerInvariant()
    if($expected -notmatch '^[0-9a-f]{64}$' -or (Get-VrFileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Update checksum does not match. No installed files were changed.' }
    $extract=Join-Path $stage 'extracted'
    New-Item -ItemType Directory -Path $extract | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($zip)
    try {
        foreach($entry in $archive.Entries) {
            $target=[IO.Path]::GetFullPath((Join-Path $extract $entry.FullName.Replace('/','\')))
            if(-not $target.StartsWith($extract+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid archive path.' }
            if(-not $entry.Name) { continue }
            New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
        }
    } finally { $archive.Dispose() }
    $manifests=@(Get-ChildItem -LiteralPath $extract -Filter portable-manifest.json -Recurse -File)
    if($manifests.Count -ne 1) { throw 'Update manifest is missing or ambiguous.' }
    $source=$manifests[0].DirectoryName
    $manifest=Get-Content -LiteralPath $manifests[0].FullName -Raw | ConvertFrom-Json
    if($manifest.version -ne $release.tag_name.TrimStart('v') -or $manifest.development) { throw 'Update manifest does not describe the requested stable release.' }
    if(-not @($manifest.files).Count) { throw 'Update manifest contains no files.' }
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $operations=@()
    foreach($file in $manifest.files) {
        $relative=$file.path.Replace('/','\')
        if(-not $seen.Add($relative)) { throw ('Duplicate managed file: '+$relative) }
        if($relative -notmatch '^(WheelWizard\\[^\\]+|WiiCompiled-VR-Setup\.exe|README\.(txt|md)|OPENXR\.md|Update-VR\.(ps1|cmd))$') { throw ('Unexpected managed file: '+$relative) }
        $from=[IO.Path]::GetFullPath((Join-Path $source $relative))
        $to=[IO.Path]::GetFullPath((Join-Path $bundle $relative))
        if(-not $from.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase) -or -not $to.StartsWith($bundle+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid managed path.' }
        if((Get-VrFileHash -LiteralPath $from -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256) { throw ('Invalid update file: '+$relative) }
        $operations+=@{From=$from;To=$to;Relative=$relative}
    }
    $operations+=@{From=$manifests[0].FullName;To=(Join-Path $bundle 'portable-manifest.json');Relative='portable-manifest.json'}
    $backup=Join-Path $stage 'previous'
    Install-VrManagedFiles -Operations $operations -Backup $backup
    Write-Host 'VR tools updated. Run setup to recompile both games with this version using your own ROM.'
    Write-Host ('Previous program files are preserved at '+$backup)
    'Update completed; run setup to rebuild the games.' | Set-Content -LiteralPath $log
} catch {
    $_.Exception.Message | Set-Content -LiteralPath $log
    Write-Host ('Update failed: '+$_.Exception.Message)
    exit 1
}
