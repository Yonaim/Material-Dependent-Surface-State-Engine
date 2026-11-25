# 현재 직물 장면의 온도·용융·흡수·건조를 CPU 기준과 GPU에서 비교한다.
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$OutputDirectory = 'Build/ReactionValidation'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $ProjectRoot
try {
    $Executable = Join-Path $ProjectRoot "Build/bin/$Configuration/MDSS.exe"
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $ArtifactDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
    & $Executable --check-surface --surface-scene textile --surface-output $ArtifactDirectory
    if ($LASTEXITCODE -ne 0) { throw "Surface Simulation reaction validation failed ($LASTEXITCODE)." }
} finally {
    Pop-Location
}
