param([string]$Destination = 'C:\BSITLab2')
$ErrorActionPreference = 'Stop'
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script in an administrator PowerShell inside the Windows 10 VM.'
}

# Обновляет только нашу учебную службу; исходные документы и настройки сохраняет.
$oldExe = Join-Path $Destination 'backup_service.exe'
$service = Get-Service -Name BSITLab2Backup -ErrorAction SilentlyContinue
if ($service) {
    if ($service.Status -ne 'Stopped') {
        Stop-Service -Name BSITLab2Backup
        $service.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(30))
    }
    $service.Dispose()
    if (-not (Test-Path -LiteralPath $oldExe)) { throw "Old service EXE is missing: $oldExe" }
    & $oldExe remove
    if ($LASTEXITCODE -ne 0) { throw 'Cannot remove the old registration. Close the Services window.' }
}
& (Join-Path $PSScriptRoot 'setup-demo.ps1') -Destination $Destination
$config = Join-Path $Destination 'backup.ini'
$text = Get-Content -LiteralPath $config -Raw -Encoding UTF8
$text = $text -replace '(?m)^#.*Stage 1.*$', '# Automatic backup; checks every interval_seconds.'
if ($text -notmatch '(?m)^\s*interval_seconds\s*=') { $text += "`r`ninterval_seconds=2`r`n" }
Set-Content -LiteralPath $config -Value $text -Encoding UTF8
$evidence = '\\VBOXSVR\lab2-results'
if (-not (Test-Path -LiteralPath $evidence)) { $evidence = Join-Path $Destination 'verification' }
& (Join-Path $Destination 'verify-service.ps1') -EvidenceDirectory $evidence -KeepInstalled
Write-Host 'COMPLETE: updated service is running in C:\BSITLab2. See verification-summary.txt.'
