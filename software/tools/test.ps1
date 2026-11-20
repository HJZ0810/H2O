param([string]$Cxx='g++',[string]$Python='python',[string]$TestPath='',[string]$ServerSource='',[string]$GlyphAtlas='')
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
$jsonInclude='-I'+(Join-Path $packageRoot 'libraries\ArduinoJson\src')
$harness=Join-Path $TestPath 'core_harness.exe'
Invoke-Checked $Cxx ($flags+@($jsonInclude,(Join-Path $PSScriptRoot 'core_harness.cpp'),(Join-Path $firmware 'ExamCore.cpp'),(Join-Path $firmware 'AutoDemo.cpp'),'-o',$harness))
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
$cloud=Join-Path $TestPath 'test_thingscloud.exe'
Invoke-Checked $Cxx ($flags+@($jsonInclude,('-I'+(Join-Path $PSScriptRoot 'host')),(Join-Path $PSScriptRoot 'test_thingscloud.cpp'),(Join-Path $firmware 'ThingsCloudProtocol.cpp'),(Join-Path $firmware 'Actuators.cpp'),'-o',$cloud))
Invoke-Checked $cloud @()
$display=Join-Path $TestPath 'test_display.exe'
$font=Join-Path $packageRoot 'libraries\U8g2_for_Adafruit_GFX-1.8.0\src'
$fontObject=Join-Path $TestPath 'u8g2_fonts.o'
Invoke-Checked $Cxx @('-x','c','-DU8G2_USE_LARGE_FONTS','-fdata-sections','-c',(Join-Path $font 'u8g2_fonts.c'),'-o',$fontObject)
Invoke-Checked $Cxx ($flags+@('-Wl,--gc-sections',('-I'+(Join-Path $PSScriptRoot 'host')),('-I'+$font),(Join-Path $PSScriptRoot 'test_display.cpp'),(Join-Path $firmware 'Display.cpp'),(Join-Path $font 'U8g2_for_Adafruit_GFX.cpp'),$fontObject,'-o',$display))
$displayArgs=@($TestPath)
if ($GlyphAtlas) { $displayArgs+= $GlyphAtlas }
Invoke-Checked $display $displayArgs
if ($ServerSource) {
  Invoke-Checked 'node' @((Join-Path $PSScriptRoot 'replay_exam.cjs'),$harness,$ServerSource)
}
Write-Host "Local firmware checks passed. Evidence path: $TestPath"
