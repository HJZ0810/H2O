param(
  [ValidateSet('handheld','monitor')][string]$Target = 'handheld',
  [ValidateSet('yd2540','verify')][string]$Board = 'yd2540',
  [string]$OutputRoot = '',
  [ValidateRange(1,16)][int]$Jobs = 4,
  [string]$ArduinoCli = 'arduino-cli'
)
$ErrorActionPreference = 'Stop'
$software = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
  $ArduinoCli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
}
if (-not $OutputRoot) { $OutputRoot = Join-Path $env:TEMP 'h2o-r10' }
$dest = Join-Path $OutputRoot "$Target-$Board"
New-Item -ItemType Directory -Force $dest | Out-Null
$sketch = Join-Path $software 'ExamHandheldV2'
if ($Target -eq 'monitor') {
  # Arduino requires the folder to have the same name as the primary sketch.
  $sketch = Join-Path $dest 'stage\BMonitor'
  New-Item -ItemType Directory -Force $sketch | Out-Null
  Get-ChildItem -LiteralPath (Join-Path $software 'B-monitor') -File | Where-Object Extension -in '.h','.ino','.cpp' | Copy-Item -Destination $sketch -Force
}
$fqbn = 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none'
$options = @('compile','--fqbn',$fqbn,'--jobs',"$Jobs",'--libraries',(Join-Path $software 'libraries'),'--build-path',(Join-Path $dest 'build'),'--output-dir',(Join-Path $dest 'bin'))
if ($Board -eq 'verify') { $options += @('--build-property','compiler.cpp.extra_flags=-DBOARD_VERIFY') }
& $ArduinoCli @options $sketch
if ($LASTEXITCODE -ne 0) { throw "Arduino compilation failed ($LASTEXITCODE)" }
Write-Output "Built $Target / $Board in $dest\bin"
