param(
    [string]$ClionDirectory = 'D:\CLion 2025.2.4',
    [string]$PythonPath = 'C:\Users\chere\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$asciiDirectory = Join-Path $env:USERPROFILE 'bsit_lab2_workspace'
$mingwDirectory = Join-Path $ClionDirectory 'bin\mingw\bin'
$compiler = Join-Path $mingwDirectory 'gcc.exe'
$cmake = Join-Path $ClionDirectory 'bin\cmake\win\x64\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
$ninja = Join-Path $ClionDirectory 'bin\ninja\win\x64\ninja.exe'
foreach ($tool in @($compiler, $cmake, $ctest, $ninja, $PythonPath)) {
    if (-not (Test-Path -LiteralPath $tool)) { throw "Build tool not found: $tool" }
}

# Junction нужен только инструментам сборки, которым мешает кириллица в пути.
if (Test-Path -LiteralPath $asciiDirectory) {
    $existing = Get-Item -LiteralPath $asciiDirectory
    if (-not ($existing.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        [IO.Path]::GetFullPath([string]$existing.Target) -ne [IO.Path]::GetFullPath($projectDirectory)) {
        throw "This build path belongs to another directory: $asciiDirectory"
    }
} else {
    New-Item -ItemType Junction -Path $asciiDirectory -Target $projectDirectory | Out-Null
}
$buildDirectory = Join-Path $asciiDirectory 'build'
$savedPath = $env:Path
try {
    $env:Path = "$mingwDirectory;$env:Path"
    & $cmake -S $asciiDirectory -B $buildDirectory -G Ninja '-DCMAKE_BUILD_TYPE=Release' `
        "-DCMAKE_C_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & $cmake --build $buildDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
    & $ctest --test-dir $buildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    & $PythonPath (Join-Path $projectDirectory 'tests\integration.py') `
        (Join-Path $buildDirectory 'backup_probe.exe') (Join-Path $buildDirectory 'demo.exe')
    if ($LASTEXITCODE -ne 0) { throw 'ZIP integration tests failed' }
    & $PythonPath (Join-Path $projectDirectory 'tests\monitor_integration.py') `
        (Join-Path $buildDirectory 'backup_probe.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Automatic update tests failed' }
    & $PythonPath (Join-Path $projectDirectory 'tools\package.py') $projectDirectory $buildDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
    & $PythonPath (Join-Path $projectDirectory 'tests\cli_smoke.py') (Join-Path $projectDirectory 'dist\Lab2')
    if ($LASTEXITCODE -ne 0) { throw 'Packaged EXE check failed' }
} finally {
    $env:Path = $savedPath
}
Write-Host 'Ready: dist\Lab2 and dist\Lab2.zip'
