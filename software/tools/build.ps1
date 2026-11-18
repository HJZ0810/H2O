param(
  [string]$ArduinoCli = 'arduino-cli',
  [string]$BuildPath = '',
  [ValidateRange(1,16)][int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
  $bundledCli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
  if (Test-Path -LiteralPath $bundledCli) { $ArduinoCli = $bundledCli }
  else { throw 'Arduino CLI is required. Install ESP32 core 3.3.12 before building.' }
}
if (-not $BuildPath) { $BuildPath = Join-Path $packageRoot 'build' }
$binaryPath = Join-Path $packageRoot 'binaries'
$validationPath = Join-Path $packageRoot 'validation'
New-Item -ItemType Directory -Path $BuildPath,$binaryPath,$validationPath -Force | Out-Null
$fqbn = 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none'
# Replace prior output immediately, including when library discovery is silent.
[System.IO.File]::WriteAllText((Join-Path $validationPath 'arduino-build.txt'),'')
& $ArduinoCli compile --fqbn $fqbn --libraries (Join-Path $packageRoot 'libraries') --build-path $BuildPath --output-dir $binaryPath --jobs $Jobs --warnings default (Join-Path $packageRoot 'ExamHandheld') 2>&1 | Tee-Object -FilePath (Join-Path $validationPath 'arduino-build.txt')
if ($LASTEXITCODE -ne 0) { throw "Firmware build failed ($LASTEXITCODE). See validation\arduino-build.txt" }
$fqbn | Set-Content -LiteralPath (Join-Path $validationPath 'fqbn.txt') -Encoding utf8
Write-Host 'Firmware build passed. No upload was performed.'
