param(
  [ValidateSet('handheld','monitor')][string]$Target = 'handheld',
  [ValidateSet('yd2540','verify')][string]$Board = 'yd2540',
  [string]$OutputRoot = '',
  [ValidateRange(1,16)][int]$Jobs = 4,
  [string]$ArduinoCli = 'arduino-cli'
)
$ErrorActionPreference = 'Stop'
$software = Split-Path $PSScriptRoot -Parent
if (-not $OutputRoot) { $OutputRoot = Join-Path $env:TEMP 'h2o-r10' }
$root = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
$dest = Join-Path $root "$Target-$Board"
[IO.Directory]::CreateDirectory($dest) | Out-Null
$run = [guid]::NewGuid().ToString('N')
$runDir = Join-Path (Join-Path $dest 'runs') $run
$build = Join-Path $runDir 'build'
$bin = Join-Path $runDir 'bin'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$statusPath = Join-Path $dest 'status.json'
$receipt = [ordered]@{state='running'; run=$run; target=$Target; board=$Board; sketchDigest=$null}

function Get-InputHash([string]$path) {
  $stream = [IO.File]::OpenRead($path)
  $sha = [Security.Cryptography.SHA256]::Create()
  try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
  finally { $sha.Dispose(); $stream.Dispose() }
}

function Write-BuildState([string]$state) {
  $receipt.state = $state
  $json = ($receipt | ConvertTo-Json) + "`n"
  [IO.File]::WriteAllText((Join-Path $runDir 'status.json'), $json, $utf8)
  $temp = Join-Path $dest ('.status-' + [guid]::NewGuid().ToString('N'))
  [IO.File]::WriteAllText($temp, $json, $utf8)
  if ([IO.File]::Exists($statusPath)) {
    [IO.File]::Replace($temp, $statusPath, "$temp.previous")
    [IO.File]::Delete("$temp.previous")
  } else { [IO.File]::Move($temp, $statusPath) }
}

try {
  $lock = [IO.File]::Open((Join-Path $dest '.build.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
} catch [IO.IOException] { throw "Cannot acquire build lock for $dest. $($_.Exception.Message)" }
try {
  [IO.Directory]::CreateDirectory($runDir) | Out-Null
  Write-BuildState 'running'
  if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
    if ($ArduinoCli -ne 'arduino-cli') { throw "Arduino CLI not found: $ArduinoCli" }
    $ArduinoCli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
  }
  $sourceName = if ($Target -eq 'monitor') { 'B-monitor' } else { 'ExamHandheldV2' }
  $name = if ($Target -eq 'monitor') { 'BMonitor' } else { 'ExamHandheldV2' }
  $source = Join-Path $software $sourceName
  $sketch = Join-Path (Join-Path $runDir 'stage') $name
  [IO.Directory]::CreateDirectory($sketch) | Out-Null
  # These two sketches use top-level sources. Do not copy historical binaries or captures.
  $files = @(Get-ChildItem -LiteralPath $source -File | Where-Object Extension -in '.ino','.h','.hpp','.hh','.c','.cc','.cpp','.cxx','.s','.tpp','.inc' | Sort-Object Name)
  foreach ($file in $files) { Copy-Item -LiteralPath $file.FullName -Destination $sketch }
  $manifest = foreach ($file in $files) {
    $hash = Get-InputHash (Join-Path $sketch $file.Name)
    "$hash  $($file.Name)"
  }
  $manifestPath = Join-Path $runDir 'sketch-sha256.txt'
  [IO.File]::WriteAllText($manifestPath, ($manifest -join "`n") + "`n", $utf8)
  $receipt.sketchDigest = Get-InputHash $manifestPath
  $fqbn = 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none'
  $options = @('compile','--fqbn',$fqbn,'--jobs',"$Jobs",'--libraries',(Join-Path $software 'libraries'),'--build-path',$build,'--output-dir',$bin)
  if ($Board -eq 'verify') { $options += @('--build-property','compiler.cpp.extra_flags=-DBOARD_VERIFY') }
  & $ArduinoCli @options $sketch
  if ($LASTEXITCODE -ne 0) { throw "Arduino compilation failed ($LASTEXITCODE)" }
  # Arduino CLI omits boot_app0.bin and flash_args from --output-dir. Deliver all four images together.
  $required = @("$name.ino.bootloader.bin", "$name.ino.partitions.bin", 'boot_app0.bin', "$name.ino.bin")
  $seen = @()
  $argsFile = Join-Path $build 'flash_args'
  foreach ($line in (Get-Content -LiteralPath $argsFile)) {
    if ($line -notmatch '^\s*0x') { continue }
    if ($line -notmatch '^\s*0x[0-9a-fA-F]+\s+(\S+)\s*$') { throw "Invalid flash_args entry: $line" }
    $image = $Matches[1]
    if ($image -cnotin $required -or $seen -contains $image) { throw "Unexpected flash image: $image" }
    $seen += $image
    $builtFile = Get-Item -LiteralPath (Join-Path $build $image)
    if ($builtFile.Length -le 0) { throw "Empty flash image: $image" }
    $export = Join-Path $bin $image
    if ([IO.File]::Exists($export)) {
      if ((Get-InputHash $export) -ne (Get-InputHash $builtFile.FullName)) { throw "Export mismatch: $image" }
    } else { Copy-Item -LiteralPath $builtFile.FullName -Destination $export }
  }
  if ($seen.Count -ne $required.Count) { throw 'Incomplete flash_args image list' }
  Copy-Item -LiteralPath $argsFile -Destination $bin -Force
  Write-BuildState 'succeeded'
  Write-Output "Built $Target / $Board in $bin"
} catch {
  if ([IO.Directory]::Exists($runDir)) { Write-BuildState 'failed' }
  throw
} finally { $lock.Dispose() }
