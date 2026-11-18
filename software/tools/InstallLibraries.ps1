param([string]$SketchbookPath='', [string]$ArduinoCli='arduino-cli')
$ErrorActionPreference='Stop'
$packageRoot=Split-Path $PSScriptRoot -Parent
if(-not $SketchbookPath){
  if(-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)){
    $bundledCli=Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
    if(Test-Path -LiteralPath $bundledCli){$ArduinoCli=$bundledCli}
  }
  if(Get-Command $ArduinoCli -ErrorAction SilentlyContinue){
    $configuredPath= & $ArduinoCli config get directories.user 2>$null
    if($LASTEXITCODE -eq 0){$SketchbookPath=($configuredPath | Out-String).Trim()}
  }
  if(-not $SketchbookPath){$SketchbookPath=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino'}
}
$libraryTarget=Join-Path $SketchbookPath 'libraries'
$lock=Get-Content -LiteralPath (Join-Path $packageRoot 'libraries\lock.json') -Raw | ConvertFrom-Json
function Same-Tree([string]$source,[string]$target){
  $left=@(Get-ChildItem -LiteralPath $source -Recurse -File)
  $right=@(Get-ChildItem -LiteralPath $target -Recurse -File)
  if($left.Count -ne $right.Count){return $false}
  foreach($file in $left){
    $relative=$file.FullName.Substring($source.TrimEnd('\').Length+1)
    $other=Join-Path $target $relative
    if(-not (Test-Path -LiteralPath $other -PathType Leaf)){return $false}
    if((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $other -Algorithm SHA256).Hash){return $false}
  }
  return $true
}
# Check every conflict before writing any library.
foreach($lib in $lock.libraries){
  $source=Join-Path $packageRoot ('libraries\'+$lib.directory)
  $target=Join-Path $libraryTarget $lib.directory
  if((Test-Path -LiteralPath $target) -and -not (Same-Tree $source $target)){throw "Different existing library: $target. Keep a backup and resolve the conflict, or pass -SketchbookPath matching a separate Arduino IDE sketchbook."}
}
New-Item -ItemType Directory -Path $libraryTarget -Force | Out-Null
foreach($lib in $lock.libraries){
  $source=Join-Path $packageRoot ('libraries\'+$lib.directory)
  $target=Join-Path $libraryTarget $lib.directory
  if(-not (Test-Path -LiteralPath $target)){Copy-Item -LiteralPath $source -Destination $libraryTarget -Recurse}
  Write-Host ($lib.name+' '+$lib.version)
}
Write-Host "Libraries ready in: $libraryTarget. Restart Arduino IDE and open ExamHandheld\ExamHandheld.ino."
