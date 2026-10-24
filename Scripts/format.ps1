$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ExcludedDirectoryNames = @(".git", "Build", "build", "CMakeFiles", "ThirdParty")
$SourceExtensions = @(".cpp", ".h", ".hpp", ".c", ".cc", ".cxx")

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    [Console]::Error.WriteLine("Error: clang-format is required but was not found in PATH.")
    exit 1
}

# Walk directories explicitly so excluded trees are pruned before they are read.
# Skip reparse points to match find's default behavior of not following symlinks.
$SourceFiles = [System.Collections.Generic.List[string]]::new()
$PendingDirectories = [System.Collections.Generic.Stack[string]]::new()
$PendingDirectories.Push($ProjectRoot)

while ($PendingDirectories.Count -gt 0) {
    $Directory = $PendingDirectories.Pop()
    foreach ($Entry in Get-ChildItem -LiteralPath $Directory -Force) {
        if ($Entry.PSIsContainer) {
            if ($Entry.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                continue
            }

            if ($ExcludedDirectoryNames -ccontains $Entry.Name -or $Entry.Name -clike "build-*") {
                continue
            }

            $PendingDirectories.Push($Entry.FullName)
            continue
        }

        if ($SourceExtensions -ccontains $Entry.Extension) {
            $SourceFiles.Add($Entry.FullName)
        }
    }
}

foreach ($SourceFile in $SourceFiles) {
    & clang-format -i $SourceFile
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
