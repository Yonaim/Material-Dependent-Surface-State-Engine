param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs = @()
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "Build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake is required but was not found in PATH."
    exit 1
}

& cmake -S $ProjectRoot -B $BuildDir
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& cmake --build $BuildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$ExecutableCandidates = @(
    (Join-Path $BuildDir "bin/MDSS.exe"),
    (Join-Path $BuildDir "bin/$Configuration/MDSS.exe")
)
$Executable = $ExecutableCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $Executable) {
    Write-Error "Build succeeded, but MDSS.exe was not found in Build/bin."
    exit 1
}

& $Executable @AppArgs
exit $LASTEXITCODE
