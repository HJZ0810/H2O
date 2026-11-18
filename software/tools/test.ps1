param([string]$Cxx='g++',[string]$Python='python',[string]$TestPath='')
$ErrorActionPreference='Stop'
$packageRoot=Split-Path $PSScriptRoot -Parent
$firmware=Join-Path $packageRoot 'ExamHandheld'
if(-not $TestPath){$TestPath=Join-Path $env:TEMP ('zlc-firmware-test-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Path $TestPath -Force | Out-Null
if(-not (Get-Command $Cxx -ErrorAction SilentlyContinue)){
  $localCompiler='D:\PRO\codeblocks-25.03mingw-nosetup\MinGW\bin\g++.exe'
  if(Test-Path -LiteralPath $localCompiler){$Cxx=$localCompiler}else{throw 'A C++17 compiler is required. Pass -Cxx with its path.'}
}
function Invoke-Checked([string]$program,[string[]]$arguments){& $program @arguments;if($LASTEXITCODE -ne 0){throw "Failed ($LASTEXITCODE): $program"}}
$flags=@('-std=c++17','-static','-O2','-Wall',('-I'+$firmware))
$harness=Join-Path $TestPath 'core_harness.exe'
Invoke-Checked $Cxx ($flags+@(('-I'+(Join-Path $packageRoot 'libraries\ArduinoJson\src')),(Join-Path $PSScriptRoot 'core_harness.cpp'),(Join-Path $firmware 'ExamCore.cpp'),(Join-Path $firmware 'AutoDemo.cpp'),'-o',$harness))
$priorCoreBin=$env:EXAM_CORE_BIN
try{$env:EXAM_CORE_BIN=$harness;Invoke-Checked $Python @('-B',(Join-Path $PSScriptRoot 'test_core.py'));Invoke-Checked $Python @('-B',(Join-Path $PSScriptRoot 'test_v2.py'))}finally{$env:EXAM_CORE_BIN=$priorCoreBin}
foreach($name in @('test_buttons','button-stop')){
  $binary=Join-Path $TestPath ($name+'.exe')
  Invoke-Checked $Cxx ($flags+@((Join-Path $PSScriptRoot ($name+'.cpp')),(Join-Path $firmware 'ButtonLogic.cpp'),'-o',$binary))
  Invoke-Checked $binary @()
}
$portal=Join-Path $TestPath 'test_portal.exe'
Invoke-Checked $Cxx ($flags+@((Join-Path $PSScriptRoot 'test_portal.cpp'),(Join-Path $firmware 'PortalHtml.cpp'),'-o',$portal))
Invoke-Checked $portal @((Join-Path $TestPath 'portal.html'))
Write-Host "Local firmware checks passed. Evidence path: $TestPath"
