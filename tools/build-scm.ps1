param(
    [string]$SannyPath = "C:\Tools\SannyBuilder4\sanny.exe",
    [string]$ProjectRoot = "C:\CoopAndreas",
    [string]$GamePath = "C:\GTA San Andreas"
)

$ErrorActionPreference = "Stop"

$mainFile = Join-Path $ProjectRoot "scm\main.txt"
$outputFile = Join-Path $ProjectRoot "scm\main.compiled.scm"

if (-not (Test-Path $SannyPath)) {
    throw "Sanny Builder not found at $SannyPath"
}

if (-not (Test-Path $mainFile)) {
    throw "SCM source not found at $mainFile"
}

if (-not (Test-Path $GamePath)) {
    throw "Game path not found at $GamePath"
}

& $SannyPath --no-splash --mode sa_sbl_coopandreas --option "GamePath=$GamePath" --compile $mainFile $outputFile

if (Test-Path $outputFile) {
    Get-Item $outputFile | Select-Object FullName, Length, LastWriteTime
} else {
    Write-Warning "Compilation finished without producing $outputFile. Open Sanny Builder once and verify the compiler mode is set to 'GTA SA (v1.0 - CoopAndreas)'."
}
