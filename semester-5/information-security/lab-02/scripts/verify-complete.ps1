param([string]$EvidenceDirectory = '', [switch]$KeepInstalled)
$ErrorActionPreference = 'Stop'
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an administrator PowerShell inside the VM.'
}
$demoDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$executable = Join-Path $demoDirectory 'backup_service.exe'
$archive = Join-Path $demoDirectory 'backup\backup.zip'
$log = Join-Path $demoDirectory 'backup.log'
if (-not $EvidenceDirectory) { $EvidenceDirectory = Join-Path $demoDirectory 'verification' }
if (-not (Test-Path -LiteralPath $EvidenceDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
}
$existing = Get-Service -Name BSITLab2Backup -ErrorAction SilentlyContinue
if ($existing) { $existing.Dispose(); throw 'Stop/remove the service first, or use update-demo.ps1.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$checks = New-Object 'System.Collections.Generic.List[string]'
$installed = $false
$testRoot = Join-Path $demoDirectory ('source\auto-check-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
$relative = (Split-Path -Leaf $testRoot) + '/папка1/папка2/папка3'
$deep = Join-Path $testRoot 'папка1\папка2\папка3'
Start-Transcript -LiteralPath (Join-Path $EvidenceDirectory 'verification.txt') -Force | Out-Null

# Проверяет код завершения команды нашего SCP.
function Invoke-BackupCommand([string]$Command) {
    & $executable $Command
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $Command" }
}

# Каждый раз закрывает ZIP; сравнивает сохранённые данные, а не только имя файла.
function Read-ZipText([string]$Name) {
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $entry = $zip.GetEntry($Name)
        if (-not $entry) { return $null }
        $reader = New-Object IO.StreamReader($entry.Open(), [Text.Encoding]::UTF8)
        try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally { $zip.Dispose() }
}

# Ждёт нужные данные, не перезапуская службу.
function Wait-Text([string]$Name, [AllowNull()][object]$Expected) {
    $deadline = [DateTime]::UtcNow.AddSeconds(40)
    do {
        $service = Get-Service -Name BSITLab2Backup
        try { if ($service.Status -ne 'Running') { throw 'Service stopped during update.' } }
        finally { $service.Dispose() }
        $actual = $null
        $readOk = $false
        try { $actual = Read-ZipText $Name; $readOk = $true } catch [IO.IOException] { }
        if ($readOk -and (($null -eq $Expected -and $null -eq $actual) -or
            ($null -ne $Expected -and $actual -ceq $Expected))) { return }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Archive did not update: $Name"
}

# Новая строка журнала подтверждает завершение именно текущего запуска.
function Wait-Ready([int]$Before) {
    $deadline = [DateTime]::UtcNow.AddSeconds(40)
    do {
        if ((Test-Path -LiteralPath $log) -and
            @(Select-String -LiteralPath $log -Pattern 'Backup ready:').Count -gt $Before) { return }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Initial backup did not finish. See backup.log.'
}

# Собирает результаты для текста отчёта и проверки JSON.
function Pass([string]$Text) { $checks.Add($Text); Write-Host "PASS: $Text" }

try {
    Invoke-BackupCommand install
    $installed = $true
    $before = if (Test-Path -LiteralPath $log) { @(Select-String -LiteralPath $log -Pattern 'Backup ready:').Count } else { 0 }
    Invoke-BackupCommand start
    Wait-Ready $before
    $pidBefore = (Get-CimInstance Win32_Service -Filter "Name='BSITLab2Backup'").ProcessId
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $names = @($zip.Entries | ForEach-Object { $_.FullName })
        $initialCount = $names.Count
        if ($names -notcontains 'папка1/папка2/папка3/demo.exe') { throw 'Nested EXE is missing.' }
        if (@($names | Where-Object { $_ -like '*report_12.docx' -or $_ -like '*other.exe' }).Count) {
            throw 'An excluded demo file was archived.'
        }
        foreach ($entry in $zip.Entries) {
            $stream = $entry.Open()
            try { $stream.CopyTo([IO.Stream]::Null) } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
    Pass "Installed/running; ZIP readable; masks and 3-level nesting verified ($initialCount files)."

    New-Item -ItemType Directory -Path $deep -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $deep 'changed.txt'), 'BEFORE', [Text.Encoding]::UTF8)
    Wait-Text "$relative/changed.txt" 'BEFORE'
    [IO.File]::WriteAllText((Join-Path $deep 'changed.txt'), 'AFTER!', [Text.Encoding]::UTF8)
    Wait-Text "$relative/changed.txt" 'AFTER!'
    Pass 'Existing file updated automatically, including an edit of the same length.'
    [IO.File]::WriteAllText((Join-Path $deep 'новый.txt'), 'NEW NESTED FILE', [Text.Encoding]::UTF8)
    Wait-Text "$relative/новый.txt" 'NEW NESTED FILE'
    $pidAfter = (Get-CimInstance Win32_Service -Filter "Name='BSITLab2Backup'").ProcessId
    if ($pidBefore -ne $pidAfter) { throw 'Service process changed during automatic update.' }
    Pass "New nested Unicode file added; same ZIP path and service PID $pidAfter."

    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    $held = [IO.File]::Open($archive, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        [IO.File]::WriteAllText((Join-Path $deep 'changed.txt'), 'AFTER LOCK', [Text.Encoding]::UTF8)
        Start-Sleep -Seconds 4
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $hash) { throw 'Locked ZIP changed.' }
    } finally { $held.Dispose() }
    Wait-Text "$relative/changed.txt" 'AFTER LOCK'
    Pass 'Locked destination preserved; automatic retry succeeded after release.'

    # Удаляет только тестовую папку этого запуска, проверив границы source.
    $sourceRoot = [IO.Path]::GetFullPath((Join-Path $demoDirectory 'source')).TrimEnd('\') + '\'
    $resolvedTest = [IO.Path]::GetFullPath($testRoot)
    if (-not $resolvedTest.StartsWith($sourceRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test path.' }
    Remove-Item -LiteralPath $resolvedTest -Recurse -Force
    Wait-Text "$relative/changed.txt" $null

    $before = @(Select-String -LiteralPath $log -Pattern 'Backup ready:').Count
    Restart-Service -Name BSITLab2Backup
    Wait-Ready $before
    Pass 'Restart through SCM completed and created a fresh backup.'
    Stop-Service -Name BSITLab2Backup
    Invoke-BackupCommand start
    Invoke-BackupCommand stop
    Pass 'Stop works through SCM and through our own stop command.'
    Invoke-BackupCommand remove
    $installed = $false
    $deleted = Get-Service -Name BSITLab2Backup -ErrorAction SilentlyContinue
    if ($deleted) { $deleted.Dispose(); throw 'Deletion pending; close Services and retry.' }
    Pass 'Registration removed; EXE, configuration and ZIP retained.'

    [ordered]@{
        success = $true; timestamp = (Get-Date).ToString('o'); initial_files = $initialCount
        process_before_update = $pidBefore; process_after_update = $pidAfter
        archive = $archive; service_exe_sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
        checks = $checks.ToArray()
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'verification-result.json') -Encoding UTF8
    $summary = @('BSIT LAB 2 - COMPLETE VERIFICATION', (Get-Date).ToString('yyyy-MM-dd HH:mm:ss'),
                 "Archive: $archive", "Same service PID during update: $pidAfter", '') + @($checks | ForEach-Object { "PASS: $_" })
    $summary | Set-Content -LiteralPath (Join-Path $demoDirectory 'verification-summary.txt') -Encoding UTF8
    Copy-Item -LiteralPath $log -Destination (Join-Path $EvidenceDirectory 'backup.log') -Force
    if ($KeepInstalled) {
        Invoke-BackupCommand install
        $installed = $true
        $before = @(Select-String -LiteralPath $log -Pattern 'Backup ready:').Count
        Invoke-BackupCommand start
        Wait-Ready $before
        $installed = $false
        Write-Host 'Service left running for manual demonstration.'
    }
} finally {
    if ($installed) {
        Stop-Service -Name BSITLab2Backup -ErrorAction SilentlyContinue
        & $executable remove
    }
    Stop-Transcript | Out-Null
}
