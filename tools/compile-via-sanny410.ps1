param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName Microsoft.VisualBasic
Add-Type -AssemblyName System.Windows.Forms

$sannyPath = "C:\Tools\SannyBuilder4_410\sanny.exe"
$inputPath = (Resolve-Path $InputFile).Path
$workdir = Split-Path $inputPath -Parent
$baseName = [System.IO.Path]::GetFileNameWithoutExtension($inputPath)
$scmOut = Join-Path $workdir ($baseName + ".scm")
$imgOut = Join-Path $workdir "script.img"
$oldScm = if (Test-Path $scmOut) { (Get-Item $scmOut).LastWriteTimeUtc } else { Get-Date "2000-01-01" }
$oldImg = if (Test-Path $imgOut) { (Get-Item $imgOut).LastWriteTimeUtc } else { Get-Date "2000-01-01" }

Get-Process sanny -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$args = "--mode sa_sbl_coopandreas `"$inputPath`""
$proc = Start-Process -FilePath $sannyPath -ArgumentList $args -WorkingDirectory $workdir -PassThru
Start-Sleep -Seconds 6

$alive = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
if (-not $alive) {
    [pscustomobject]@{
        Input = $inputPath
        Started = $false
        Compiled = $false
        MainScmUpdated = $false
        ScriptImgUpdated = $false
    }
    exit 0
}

[Microsoft.VisualBasic.Interaction]::AppActivate($proc.Id) | Out-Null
Start-Sleep -Milliseconds 700
[System.Windows.Forms.SendKeys]::SendWait('{F6}')
Start-Sleep -Seconds 18

$scm = Get-Item $scmOut -ErrorAction SilentlyContinue
$img = Get-Item $imgOut -ErrorAction SilentlyContinue

[pscustomobject]@{
    Input = $inputPath
    Started = $true
    Compiled = [bool]($scm -and $scm.LastWriteTimeUtc -gt $oldScm)
    MainScmUpdated = [bool]($scm -and $scm.LastWriteTimeUtc -gt $oldScm)
    ScriptImgUpdated = [bool]($img -and $img.LastWriteTimeUtc -gt $oldImg)
    ScmPath = if ($scm) { $scm.FullName } else { $null }
    ScmTime = if ($scm) { $scm.LastWriteTime } else { $null }
}
