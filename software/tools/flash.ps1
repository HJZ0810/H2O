param(
  [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]+$')][string]$Port,
  [string]$ArduinoCli = 'arduino-cli'
)
$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
  $bundledCli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
  if (Test-Path -LiteralPath $bundledCli) { $ArduinoCli = $bundledCli }
  else { throw 'Arduino CLI is required. Pass -ArduinoCli with its absolute path.' }
}
$binaryPath = Join-Path $packageRoot 'binaries'
foreach ($binaryName in @('ExamHandheld.ino.bin','ExamHandheld.ino.bootloader.bin','ExamHandheld.ino.partitions.bin')) {
  if (-not (Test-Path -LiteralPath (Join-Path $binaryPath $binaryName))) { throw "Missing binary: $binaryName. Run tools\build.ps1 first." }
}
$fqbn = 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none'
Write-Host "Uploading to $Port. Disconnect the handheld external 5 V before connecting the ESP32 to USB."
& $ArduinoCli upload --fqbn $fqbn --port $Port --input-dir $binaryPath --verify
if ($LASTEXITCODE -ne 0) { throw "Firmware upload failed ($LASTEXITCODE)." }
Write-Host 'Upload and verification passed. Configure the device and validate it on hardware.'
