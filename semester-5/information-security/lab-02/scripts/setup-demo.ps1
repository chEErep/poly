param([string]$Destination = 'C:\BSITLab2')
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Open PowerShell as administrator inside the Windows 10 VM.'
}
$packageDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$Destination = [IO.Path]::GetFullPath($Destination).TrimEnd('\')
if ($Destination -notmatch '^[A-Za-z]:\\.+') {
    throw 'Destination must be a local folder, for example C:\BSITLab2.'
}
$marker = Join-Path $Destination '.bsit-lab2-demo'
if ((Test-Path -LiteralPath $Destination) -and -not (Test-Path -LiteralPath $marker)) {
    throw "Destination already exists and was not created by this script: $Destination"
}
$service = Get-Service -Name 'BSITLab2Backup' -ErrorAction SilentlyContinue
if ($service -and $service.Status -ne 'Stopped') {
    throw 'Stop BSITLab2Backup before copying a new EXE.'
}
if ((Get-Service -Name LanmanServer).Status -ne 'Running') {
    throw 'The Server (LanmanServer) service must be running for localhost UNC paths.'
}
$driveName = $Destination.Substring(0, 1)
$shareName = $driveName + '$'
if (-not (Get-SmbShare -Name $shareName -ErrorAction SilentlyContinue)) {
    throw "The Windows administrative share $shareName is unavailable."
}
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
Set-Content -LiteralPath $marker -Value 'BSIT Lab 2 demo directory' -Encoding ASCII
Copy-Item -LiteralPath (Join-Path $packageDirectory 'backup_service.exe') -Destination $Destination -Force
Copy-Item -LiteralPath (Join-Path $packageDirectory 'README.md') -Destination $Destination -Force
Copy-Item -LiteralPath (Join-Path $packageDirectory 'verify-service.ps1') -Destination $Destination -Force
Copy-Item -LiteralPath (Join-Path $packageDirectory 'verify-complete.ps1') -Destination $Destination -Force

# Повторный запуск не затирает документы, настройки или готовый архив.
$sourceDirectory = Join-Path $Destination 'source'
$backupDirectory = Join-Path $Destination 'backup'
New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
$folders = @('', 'папка1', 'папка1\папка2', 'папка1\папка2\папка3')
foreach ($relative in $folders) {
    $folder = if ($relative) { Join-Path $sourceDirectory $relative } else { $sourceDirectory }
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    $text = Join-Path $folder 'заметка.txt'
    if (-not (Test-Path -LiteralPath $text)) {
        Set-Content -LiteralPath $text -Value "Backup test. Folder: $relative" -Encoding UTF8
    }
    foreach ($name in @('report_1.docx', 'report_12.docx')) {
        $file = Join-Path $folder $name
        if (-not (Test-Path -LiteralPath $file)) {
            Copy-Item -LiteralPath (Join-Path $packageDirectory 'sample.docx') -Destination $file
        }
    }
    foreach ($name in @('demo.exe', 'other.exe')) {
        $file = Join-Path $folder $name
        if (-not (Test-Path -LiteralPath $file)) {
            Copy-Item -LiteralPath (Join-Path $packageDirectory 'demo.exe') -Destination $file
        }
    }
}
$uncRoot = '\\localhost\' + $shareName + $Destination.Substring(2)
$config = Join-Path $Destination 'backup.ini'
if (-not (Test-Path -LiteralPath $config)) {
    @(
        '# Automatic backup; check source files every 2 seconds.',
        "source=$uncRoot\source",
        "archive=$uncRoot\backup\backup.zip",
        'mask=*.txt',
        'mask=report_?.docx',
        'mask=demo.exe',
        'interval_seconds=2'
    ) | Set-Content -LiteralPath $config -Encoding UTF8
}
if (-not (Test-Path -LiteralPath ($uncRoot + '\source'))) {
    throw "UNC access failed: $uncRoot\source"
}
Write-Host "Prepared: $Destination"
Write-Host "Next: cd $Destination"
Write-Host '.\backup_service.exe once'
Write-Host 'For service commands, see README.md. The service is not installed yet.'
