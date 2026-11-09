param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$OutputDirectory = 'Build/ReactionValidation'
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $ProjectRoot
$PreviousSettings = $env:MDSS_PERF_SETTINGS
try {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $ArtifactDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $Executable = Join-Path $ProjectRoot "Build/bin/$Configuration/MDSS.exe"
    $SettingsPath = Join-Path $ArtifactDirectory 'capture.json'
    @{
        capture_image = $true
        presentation_mode = $true
        replay_demo = $true
        render_mesh_resolution = 128
        sparse_solver = $true
        active_channel_mask = $true
        per_workgroup_channel_mask = $true
    } | ConvertTo-Json | Set-Content -LiteralPath $SettingsPath -Encoding utf8
    $env:MDSS_PERF_SETTINGS = $SettingsPath
    foreach ($Capture in @(@{ Name='early'; Warmup=179 }, @{ Name='final'; Warmup=839 })) {
        $ResultPath = Join-Path $ArtifactDirectory ($Capture.Name + '.jsonl')
        $LogPath = Join-Path $ArtifactDirectory ($Capture.Name + '.log')
        $Arguments = @('--benchmark-scene', 'Assets/Scenes/IceReactionLab.Scene',
            '--benchmark-resolution', '128', '--benchmark-warmup-frames', $Capture.Warmup,
            '--benchmark-measure-frames', '1', '--benchmark-output', $ResultPath)
        & $Executable @Arguments > $LogPath 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Scene execution failed: $LogPath" }
        $Validation = Get-Content -LiteralPath ($ResultPath + '.validation.json') -Raw | ConvertFrom-Json
        foreach ($Instance in $Validation.instances) {
            if ($Instance.nonfinite -or $Instance.negative -or $Instance.reaction_budget_exhausted_texels -or $Instance.reaction_arithmetic_failure_texels) {
                throw "Reaction correctness check failed: $ResultPath"
            }
        }
        if (Select-String -LiteralPath $LogPath -Pattern '\[ERROR\]' -Quiet) { throw "Runtime error: $LogPath" }
        Write-Output "Verified $($Capture.Name): $($Validation.simulation_step_serial) solver steps; image $ResultPath.ppm"
    }
} finally {
    $env:MDSS_PERF_SETTINGS = $PreviousSettings
    Pop-Location
}
