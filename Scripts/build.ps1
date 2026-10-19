param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CMakeArgs = @()
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "Build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake is required but was not found in PATH."
    exit 1
}

# Match the Bash helper's behavior when a project directory has moved and the
# existing CMake cache still points at its old location.
if (Test-Path -LiteralPath $BuildDir -PathType Container) {
    $StaleCache = $false
    $CacheFiles = Get-ChildItem -LiteralPath $BuildDir -Filter "CMakeCache.txt" -File -Recurse
    foreach ($CacheFile in $CacheFiles) {
        $CacheText = Get-Content -LiteralPath $CacheFile.FullName
        $CachedBuild = ($CacheText | Where-Object { $_ -like "CMAKE_CACHEFILE_DIR:INTERNAL=*" } | Select-Object -First 1) -replace '^CMAKE_CACHEFILE_DIR:INTERNAL=', ''
        $CachedSource = ($CacheText | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:INTERNAL=*" } | Select-Object -First 1) -replace '^CMAKE_HOME_DIRECTORY:INTERNAL=', ''
        $CacheDirectory = Split-Path -Parent $CacheFile.FullName

        if (($CachedBuild -and [System.IO.Path]::GetFullPath($CachedBuild) -ne [System.IO.Path]::GetFullPath($CacheDirectory)) -or
            ($CacheFile.FullName -eq (Join-Path $BuildDir "CMakeCache.txt") -and $CachedSource -and [System.IO.Path]::GetFullPath($CachedSource) -ne [System.IO.Path]::GetFullPath($ProjectRoot))) {
            $StaleCache = $true
            break
        }
    }

    if ($StaleCache) {
        $BackupDir = Join-Path $ProjectRoot ("build-backup-" + [guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path $BackupDir | Out-Null
        Move-Item -LiteralPath $BuildDir -Destination (Join-Path $BackupDir "Build")
        Write-Output "Project path changed; previous build saved to $(Join-Path $BackupDir 'Build')"
        Write-Output "CMake cache options will be reset; pass any required options again."
    }
}

# Let CMake choose the platform's default generator, matching Scripts/build.sh.
# Additional arguments can override cache entries, for example:
# -CMakeArgs @('-DMDSS_ENABLE_VALIDATION=OFF').
& cmake -S $ProjectRoot -B $BuildDir "-DCMAKE_BUILD_TYPE=$Configuration" @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& cmake --build $BuildDir --config $Configuration --parallel
exit $LASTEXITCODE
