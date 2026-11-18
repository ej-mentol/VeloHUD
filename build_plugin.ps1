param(
    [string]$Configuration = "Release",
    [string]$MetaHookDir = "",
    [string]$VGUI2ExtensionDir = "",
    [switch]$Install,
    [switch]$Clean,
    [switch]$Diagnostics
)

$ErrorActionPreference = "Stop"
$projectDir = $PSScriptRoot
$buildDir = Join-Path $projectDir "build"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing build tree: $buildDir"
    Remove-Item $buildDir -Recurse -Force
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake.exe not found in PATH. Install CMake 3.21 or newer."
}

# The SDK is an external read-only input. Prefer an explicit path, then the
# environment, then the surrounding aggregate checkout, then let CMake fetch it.
if (-not $MetaHookDir) {
    if ($env:METAHOOK_SOURCE_PATH) {
        $MetaHookDir = $env:METAHOOK_SOURCE_PATH
    } elseif (Test-Path "$projectDir\..\..\MetaHook\include\metahook.h") {
        $MetaHookDir = "$projectDir\..\..\MetaHook"
    } elseif (Test-Path "$projectDir\..\..\include\metahook.h") {
        $MetaHookDir = "$projectDir\..\.."
    }
}
if ($MetaHookDir) {
    $MetaHookDir = (Resolve-Path $MetaHookDir).Path
}

# VGUI2Extension only contributes interface headers; its sources are not built.
if (-not $VGUI2ExtensionDir) {
    if ($env:VGUI2EXTENSION_SOURCE_PATH) {
        $VGUI2ExtensionDir = $env:VGUI2EXTENSION_SOURCE_PATH
    } elseif (Test-Path "$projectDir\..\VGUI2Extension\include\Interface\IVGUI2Extension.h") {
        $VGUI2ExtensionDir = "$projectDir\..\VGUI2Extension"
    } elseif (Test-Path "$projectDir\..\..\Plugins\VGUI2Extension\include\Interface\IVGUI2Extension.h") {
        $VGUI2ExtensionDir = "$projectDir\..\..\Plugins\VGUI2Extension"
    }
}
if ($VGUI2ExtensionDir) {
    $VGUI2ExtensionDir = (Resolve-Path $VGUI2ExtensionDir).Path
}

# Formatting tooling is optional: without it CMake fetches the pinned commit.
$formatValidationDir = $env:FORMAT_VALIDATION_SOURCE_PATH
if (-not $formatValidationDir -and (Test-Path "$projectDir\..\..\thirdparty\FormatValidation\cmake\FormatValidation.cmake")) {
    $formatValidationDir = "$projectDir\..\..\thirdparty\FormatValidation"
}
if ($formatValidationDir) {
    $formatValidationDir = (Resolve-Path $formatValidationDir).Path
}

Write-Host "ProjectDir:      $projectDir"
Write-Host "Configuration:   $Configuration"
Write-Host "Diagnostics:     $(if ($Diagnostics) { 'ON' } else { 'OFF' })"
Write-Host "MetaHookDir:     $(if ($MetaHookDir) { $MetaHookDir } else { '(fetch from GitHub)' })"
Write-Host "VGUI2Extension:  $(if ($VGUI2ExtensionDir) { $VGUI2ExtensionDir } else { '(fetch pinned commit)' })"

$configureArgs = @(
    "-S", $projectDir,
    "-B", $buildDir,
    "-A", "Win32"
)
# Always set it explicitly: the build directory caches the last value, so a
# previous -Diagnostics run must not leak into the default build.
$diagnosticsValue = if ($Diagnostics) { "ON" } else { "OFF" }
$configureArgs += "-DVELOHUD_DIAGNOSTICS=$diagnosticsValue"
if ($MetaHookDir) { $configureArgs += "-DMETAHOOK_SOURCE_PATH=$MetaHookDir" }
if ($VGUI2ExtensionDir) { $configureArgs += "-DVGUI2EXTENSION_SOURCE_PATH=$VGUI2ExtensionDir" }
if ($formatValidationDir) { $configureArgs += "-DFORMAT_VALIDATION_SOURCE_PATH=$formatValidationDir" }

Write-Host "`n>>> Configuring..." -ForegroundColor Yellow
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)." }

Write-Host "`n>>> Building [$Configuration | Win32]..." -ForegroundColor Yellow
& cmake --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)." }

$dll = Join-Path $buildDir "$Configuration\VeloHUD.dll"
if (Test-Path $dll) {
    $item = Get-Item $dll
    Write-Host ">>> Built: $($item.FullName) ($($item.Length) bytes)" -ForegroundColor Green
} else {
    throw "Expected output not found: $dll"
}

if ($Install) {
    $prefix = Join-Path $projectDir "install"
    Write-Host "`n>>> Installing to $prefix..." -ForegroundColor Yellow
    & cmake --install $buildDir --config $Configuration --prefix $prefix
    if ($LASTEXITCODE -ne 0) { throw "CMake install failed (exit $LASTEXITCODE)." }
}

Write-Host "`nAll requested configurations built successfully!" -ForegroundColor Green
