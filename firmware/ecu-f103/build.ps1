$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $projectRoot 'Build'
$toolRoot = 'E:\ST\STM32CubeIDE_2.1.0\STM32CubeIDE\plugins'
$gccBin = Join-Path $toolRoot 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin'
$gcc = Join-Path $gccBin 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $gccBin 'arm-none-eabi-objcopy.exe'
$size = Join-Path $gccBin 'arm-none-eabi-size.exe'
$linker = Join-Path $projectRoot 'STM32F103C8Tx_FLASH.ld'
$elf = Join-Path $buildDir 'ecu-f103.elf'
$map = Join-Path $buildDir 'ecu-f103.map'

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
    '-mcpu=cortex-m3',
    '-std=gnu11',
    '-g3',
    '-DDEBUG',
    '-DUSE_HAL_DRIVER',
    '-DSTM32F103xB',
    "-I$(Join-Path $projectRoot 'Core\Inc')",
    "-I$(Join-Path $projectRoot '..\common')",
    "-I$(Join-Path $projectRoot 'Drivers\STM32F1xx_HAL_Driver\Inc')",
    "-I$(Join-Path $projectRoot 'Drivers\STM32F1xx_HAL_Driver\Inc\Legacy')",
    "-I$(Join-Path $projectRoot 'Drivers\CMSIS\Device\ST\STM32F1xx\Include')",
    "-I$(Join-Path $projectRoot 'Drivers\CMSIS\Include')",
    '-O0',
    '-ffunction-sections',
    '-fdata-sections',
    '-Wall',
    '--specs=nano.specs',
    '-mthumb'
)

$sources = @()
$sources += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Core\Src') -Filter '*.c' -File
$sources += Get-Item -LiteralPath (Join-Path $projectRoot '..\common\can_driver.c')

$halNames = @(
    'stm32f1xx_hal.c',
    'stm32f1xx_hal_can.c',
    'stm32f1xx_hal_cortex.c',
    'stm32f1xx_hal_flash.c',
    'stm32f1xx_hal_flash_ex.c',
    'stm32f1xx_hal_gpio.c',
    'stm32f1xx_hal_gpio_ex.c',
    'stm32f1xx_hal_pwr.c',
    'stm32f1xx_hal_rcc.c',
    'stm32f1xx_hal_rcc_ex.c'
)
$halSourceRoot = Join-Path $projectRoot 'Drivers\STM32F1xx_HAL_Driver\Src'
foreach ($name in $halNames) {
    $sources += Get-Item -LiteralPath (Join-Path $halSourceRoot $name)
}

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

$startup = Join-Path $projectRoot 'Core\Startup\startup_stm32f103xb.s'
$startupObject = Join-Path $buildDir 'startup_stm32f103xb.o'
Write-Host 'AS  Core\Startup\startup_stm32f103xb.s'
& $gcc '-mcpu=cortex-m3' '-g3' '-DDEBUG' '-DUSE_HAL_DRIVER' '-DSTM32F103xB' '-c' $startup '-o' $startupObject
if ($LASTEXITCODE -ne 0) {
    throw 'Startup assembly failed'
}
$objectFiles += $startupObject

Write-Host 'LD  ecu-f103.elf'
& $gcc @objectFiles '-mcpu=cortex-m3' "-T$linker" '--specs=nosys.specs' "--specs=nano.specs" "-Wl,-Map=$map" '-Wl,--gc-sections' '-static' '-mthumb' '-Wl,--start-group' '-lc' '-lm' '-Wl,--end-group' '-o' $elf
if ($LASTEXITCODE -ne 0) {
    throw 'Link failed'
}

$bin = Join-Path $buildDir 'ecu-f103.bin'
$hex = Join-Path $buildDir 'ecu-f103.hex'
& $objcopy '-O' 'binary' $elf $bin
& $objcopy '-O' 'ihex' $elf $hex
& $size $elf

Write-Host "ELF: $elf"
Write-Host "BIN: $bin"
Write-Host "HEX: $hex"
