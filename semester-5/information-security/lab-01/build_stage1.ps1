$ErrorActionPreference = 'Stop'

$projectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$asciiProjectPath = Join-Path $env:USERPROFILE 'bsit_lab1_workspace'
$mingwDirectory = 'D:\CLion 2025.2.4\bin\mingw\bin'
$gccPath = Join-Path $mingwDirectory 'gcc.exe'
$cmakePath = 'D:\CLion 2025.2.4\bin\cmake\win\x64\bin\cmake.exe'
$ninjaPath = 'D:\CLion 2025.2.4\bin\ninja\win\x64\ninja.exe'
$buildDirectory = Join-Path $asciiProjectPath 'build-ok'

foreach ($requiredPath in @($gccPath, $cmakePath, $ninjaPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required build tool was not found: $requiredPath"
    }
}

if (Test-Path -LiteralPath $asciiProjectPath) {
    $existingItem = Get-Item -LiteralPath $asciiProjectPath
    if (-not ($existingItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "The ASCII build path exists but is not a junction: $asciiProjectPath"
    }
    $resolvedTarget = [System.IO.Path]::GetFullPath([string]$existingItem.Target)
    $resolvedProject = [System.IO.Path]::GetFullPath($projectDirectory)
    if ($resolvedTarget -ne $resolvedProject) {
        throw "The existing junction points to another directory: $resolvedTarget"
    }
} else {
    New-Item -ItemType Junction `
        -Path $asciiProjectPath `
        -Target $projectDirectory | Out-Null
}

$env:Path = "$mingwDirectory;$env:Path"

& $cmakePath `
    -S $asciiProjectPath `
    -B $buildDirectory `
    -G Ninja `
    "-DCMAKE_C_COMPILER=$gccPath" `
    "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

& $cmakePath --build $buildDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

& (Join-Path (Split-Path $cmakePath) 'ctest.exe') `
    --test-dir $buildDirectory `
    --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
}

Write-Host ''
Write-Host 'Build and tests completed successfully.'
Write-Host "Server: $(Join-Path $buildDirectory 'lab_server.exe')"
Write-Host "Client: $(Join-Path $buildDirectory 'lab_client.exe')"
