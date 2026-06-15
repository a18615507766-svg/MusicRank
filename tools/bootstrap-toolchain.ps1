param(
    [switch]$CheckOnly,
    [string]$PythonPath
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ToolsRoot = Join-Path $ProjectRoot ".tools"
$VenvRoot = Join-Path $ToolsRoot "python"
$VenvPython = Join-Path $VenvRoot "Scripts\python.exe"
$QtInstallRoot = Join-Path $ToolsRoot "Qt"
$QtRoot = Join-Path $QtInstallRoot "6.8.3\mingw_64"
$QtVersion = "6.8.3"
$CMakeVersion = "3.31.6"
$NinjaVersion = "1.11.1"
$GxxVersion = "13.1.0"
$QtBaseConfigs = @(
    (Join-Path $QtRoot "lib\cmake\Qt6Core\Qt6CoreConfig.cmake"),
    (Join-Path $QtRoot "lib\cmake\Qt6Widgets\Qt6WidgetsConfig.cmake"),
    (Join-Path $QtRoot "lib\cmake\Qt6Sql\Qt6SqlConfig.cmake"),
    (Join-Path $QtRoot "lib\cmake\Qt6Test\Qt6TestConfig.cmake")
)
$QtMultimediaConfig = Join-Path $QtRoot "lib\cmake\Qt6Multimedia\Qt6MultimediaConfig.cmake"
$QMake = Join-Path $QtRoot "bin\qmake.exe"
$CMake = Join-Path $VenvRoot "Scripts\cmake.exe"
$Ninja = Join-Path $VenvRoot "Scripts\ninja.exe"
$CTest = Join-Path $VenvRoot "Scripts\ctest.exe"
$Gxx = Join-Path $QtInstallRoot "Tools\mingw1310_64\bin\g++.exe"

function Resolve-Python {
    if ($PythonPath) {
        return $PythonPath
    }

    if ($env:MUSICRANK_PYTHON) {
        return $env:MUSICRANK_PYTHON
    }

    $bundledPython = Join-Path $env:USERPROFILE `
        ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
    if (Test-Path -LiteralPath $bundledPython) {
        return $bundledPython
    }

    $command = Get-Command python -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw "Python not found. Use -PythonPath or set MUSICRANK_PYTHON."
}

function Get-CommandOutput {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    $output = & $Executable @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $Executable $($Arguments -join ' ')"
    }
    return ($output -join "`n").Trim()
}

function Assert-Version {
    param(
        [string]$Name,
        [string]$Actual,
        [string]$Expected,
        [switch]$AllowSuffix
    )

    $matches = if ($AllowSuffix) {
        $Actual -match "^$([regex]::Escape($Expected))(\D|$)"
    } else {
        $Actual -eq $Expected
    }

    if (-not $matches) {
        throw "$Name version mismatch: expected $Expected, got $Actual"
    }
}

function Assert-Python {
    param([string]$Executable)

    if (-not (Test-Path -LiteralPath $Executable)) {
        throw "Python not found: $Executable"
    }

    $versionOutput = Get-CommandOutput $Executable @(
        "-c",
        "import sys; print('.'.join(map(str, sys.version_info[:3])))"
    )
    $version = [version]$versionOutput
    if ($version -lt [version]"3.10") {
        throw "Python 3.10 or newer is required, got $versionOutput"
    }
}

function Get-MissingPaths {
    param([string[]]$Paths)
    return @($Paths | Where-Object { -not (Test-Path -LiteralPath $_) })
}

function Assert-Toolchain {
    $requiredPaths = @(
        $QtBaseConfigs
        $QtMultimediaConfig
        $QMake
        $CMake
        $Ninja
        $CTest
        $Gxx
    )
    $missing = @(Get-MissingPaths $requiredPaths)
    if ($missing.Count -gt 0) {
        throw "Qt 6 toolchain missing: $($missing -join ', ')"
    }

    $actualQtVersion = Get-CommandOutput $QMake @("-query", "QT_VERSION")
    $cmakeOutput = Get-CommandOutput $CMake @("--version")
    $actualCMakeVersion = ($cmakeOutput -split "`n")[0] -replace "^cmake version ", ""
    $actualNinjaVersion = Get-CommandOutput $Ninja @("--version")
    $actualGxxVersion = Get-CommandOutput $Gxx @("-dumpfullversion")

    Assert-Version "Qt" $actualQtVersion $QtVersion
    Assert-Version "CMake" $actualCMakeVersion $CMakeVersion
    Assert-Version "Ninja" $actualNinjaVersion $NinjaVersion -AllowSuffix
    Assert-Version "G++" $actualGxxVersion $GxxVersion
}

if ($CheckOnly) {
    Assert-Toolchain
} else {
    $Python = Resolve-Python
    Assert-Python $Python

    New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null

    if (-not (Test-Path -LiteralPath $VenvPython)) {
        & $Python -m venv $VenvRoot
        if ($LASTEXITCODE -ne 0) { throw "Failed to create Python virtual environment." }
    }

    & $VenvPython -m pip install --disable-pip-version-check `
        "aqtinstall==3.3.0" "cmake==3.31.6" "ninja==1.11.1.3"
    if ($LASTEXITCODE -ne 0) { throw "Failed to install build tools." }

    $missingBaseConfigs = @(Get-MissingPaths $QtBaseConfigs)
    if ($missingBaseConfigs.Count -gt 0) {
        & $VenvPython -m aqt install-qt windows desktop 6.8.3 win64_mingw `
            -O $QtInstallRoot --timeout 30
        if ($LASTEXITCODE -ne 0) { throw "Failed to install Qt 6.8.3 base package." }
    }

    if (-not (Test-Path -LiteralPath $QtMultimediaConfig)) {
        & $VenvPython -m aqt install-qt windows desktop 6.8.3 win64_mingw `
            -O $QtInstallRoot --timeout 30 --noarchives -m qtmultimedia
        if ($LASTEXITCODE -ne 0) { throw "Failed to install Qt Multimedia." }
    }

    if (-not (Test-Path -LiteralPath $Gxx)) {
        & $VenvPython -m aqt install-tool windows desktop tools_mingw1310 `
            qt.tools.win64_mingw1310 -O $QtInstallRoot
        if ($LASTEXITCODE -ne 0) { throw "Failed to install MinGW 13.1." }
    }

    Assert-Toolchain
}

Write-Host "QtRoot=$QtRoot"
Write-Host "CMake=$CMake"
Write-Host "Ninja=$Ninja"
Write-Host "Gxx=$Gxx"
