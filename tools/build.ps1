$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$CMake = Join-Path $ProjectRoot ".tools\python\Scripts\cmake.exe"
$CTest = Join-Path $ProjectRoot ".tools\python\Scripts\ctest.exe"

Push-Location $ProjectRoot
try {
    & "$PSScriptRoot\bootstrap-toolchain.ps1"

    & $CMake --preset windows-debug
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    & $CMake --build --preset windows-debug
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

    & $CTest --preset windows-debug
    if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
} finally {
    Pop-Location
}
