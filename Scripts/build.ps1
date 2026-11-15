# 의존성과 빌드 캐시를 확인하고 프로젝트의 CMake 빌드를 실행한다.
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CMakeArgs = @()
)

# ============================================================
# 00. Environment and Prerequisites
# ============================================================
#region 00_Environment_and_Prerequisites
# 도구·SDK·실행 입력을 검사해 빠진 전제조건에서 즉시 중단한다.

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "Build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake is required but was not found in PATH."
    exit 1
}

#endregion

# ============================================================
# 01. Relocated Cache Preservation
# ============================================================
#region 01_Relocated_Cache_Preservation
# 다른 프로젝트 경로의 CMake cache가 발견되면 기존 Build를 보관한다.

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

#endregion

# ============================================================
# 02. Configure and Build
# ============================================================
#region 02_Configure_and_Build
# 선택한 구성 옵션으로 CMake 설정과 빌드를 순서대로 실행한다.

# Let CMake choose the platform's default generator, matching Scripts/build.sh.
# Additional arguments can override cache entries, for example:
# -CMakeArgs @('-DMDSS_ENABLE_VALIDATION=OFF').
& cmake -S $ProjectRoot -B $BuildDir "-DCMAKE_BUILD_TYPE=$Configuration" @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& cmake --build $BuildDir --config $Configuration --parallel
exit $LASTEXITCODE

#endregion
