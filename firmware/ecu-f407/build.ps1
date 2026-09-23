$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $projectRoot 'Build'
$toolRoot = 'E:\ST\STM32CubeIDE_2.1.0\STM32CubeIDE\plugins'
$gccBin = Join-Path $toolRoot 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin'
$gcc = Join-Path $gccBin 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $gccBin 'arm-none-eabi-objcopy.exe'
$size = Join-Path $gccBin 'arm-none-eabi-size.exe'
$linker = Join-Path $projectRoot 'STM32F407ZGTX_FLASH.ld'
$elf = Join-Path $buildDir 'ecu-f407.elf'
$map = Join-Path $buildDir 'ecu-f407.map'

if (-not (Test-Path -LiteralPath $gcc -PathType Leaf)) {
    throw "ARM GCC not found: $gcc"
}

if (Test-Path -LiteralPath $buildDir) {
    $resolvedBuild = (Resolve-Path -LiteralPath $buildDir).Path
    $resolvedProject = (Resolve-Path -LiteralPath $projectRoot).Path
    if (-not $resolvedBuild.StartsWith($resolvedProject, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean build directory outside project: $resolvedBuild"
    }
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$commonFlags = @(
    '-mcpu=cortex-m4',
    '-std=gnu11',
    '-g3',
    '-DDEBUG',
    '-DUSE_HAL_DRIVER',
    '-DSTM32F407xx',
    "-I$(Join-Path $projectRoot 'Core\Inc')",
    "-I$(Join-Path $projectRoot '..\common')",
    "-I$(Join-Path $projectRoot 'Drivers\STM32F4xx_HAL_Driver\Inc')",
    "-I$(Join-Path $projectRoot 'Drivers\STM32F4xx_HAL_Driver\Inc\Legacy')",
    "-I$(Join-Path $projectRoot 'Drivers\CMSIS\Device\ST\STM32F4xx\Include')",
    "-I$(Join-Path $projectRoot 'Drivers\CMSIS\Include')",
    '-O0',
    '-ffunction-sections',
    '-fdata-sections',
    '-Wall',
    '--specs=nano.specs',
    '-mfpu=fpv4-sp-d16',
    '-mfloat-abi=hard',
    '-mthumb'
)

$sources = @()
$sources += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Core\Src') -Filter '*.c' -File
$sources += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Drivers\STM32F4xx_HAL_Driver\Src') -Filter '*.c' -File
$sources += Get-Item -LiteralPath (Join-Path $projectRoot '..\common\can_driver.c')
$objectFiles = @()

foreach ($source in $sources | Sort-Object FullName) {
    $relative = $source.FullName.Substring($projectRoot.Length).TrimStart('\')
    $object = Join-Path $buildDir ($relative -replace '\.c$', '.o')
    $objectDir = Split-Path -Parent $object
    New-Item -ItemType Directory -Path $objectDir -Force | Out-Null

    Write-Host "CC  $relative"
    & $gcc @commonFlags -c $source.FullName -o $object
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed: $relative"
    }
    $objectFiles += $object
}

$startup = Join-Path $projectRoot 'Core\Startup\startup_stm32f407zgtx.s'
$startupObject = Join-Path $buildDir 'startup_stm32f407zgtx.o'
Write-Host 'AS  Core\Startup\startup_stm32f407zgtx.s'
& $gcc '-mcpu=cortex-m4' '-g3' '-DDEBUG' '-DUSE_HAL_DRIVER' '-DSTM32F407xx' '-c' $startup '-o' $startupObject
if ($LASTEXITCODE -ne 0) {
    throw 'Startup assembly failed'
}
$objectFiles += $startupObject

Write-Host 'LD  ecu-f407.elf'
& $gcc @objectFiles '-mcpu=cortex-m4' "-T$linker" '--specs=nosys.specs' "--specs=nano.specs" "-Wl,-Map=$map" '-Wl,--gc-sections' '-static' '-mfpu=fpv4-sp-d16' '-mfloat-abi=hard' '-mthumb' '-Wl,--start-group' '-lc' '-lm' '-Wl,--end-group' '-o' $elf
if ($LASTEXITCODE -ne 0) {
    throw 'Link failed'
}

$bin = Join-Path $buildDir 'ecu-f407.bin'
$hex = Join-Path $buildDir 'ecu-f407.hex'
& $objcopy '-O' 'binary' $elf $bin
& $objcopy '-O' 'ihex' $elf $hex
& $size $elf

Write-Host "ELF: $elf"
Write-Host "BIN: $bin"
Write-Host "HEX: $hex"
