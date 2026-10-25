param(
    [switch]$FastTxTest,
    [switch]$IsotpBenchTx,
    [switch]$IsotpBenchRx,
    [ValidateRange(1, 300)][int]$IsotpBenchLength = 100,
    [switch]$IsotpForceTxBusy,
    [switch]$ParamSaveFailureTest,
    [switch]$WwdgSelfTest,
    [switch]$WwdgEarlyFeedTest
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $projectRoot)
. (Join-Path $repoRoot 'firmware/common/toolchain.ps1')
$toolchain = Resolve-ArmGccToolchain
$buildDir = Join-Path $projectRoot 'Build'
$gcc = $toolchain.Gcc
$objcopy = $toolchain.Objcopy
$size = $toolchain.Size
$linker = Join-Path $projectRoot 'STM32F407ZGTX_FLASH.ld'
$elf = Join-Path $buildDir 'ecu-f407.elf'
$map = Join-Path $buildDir 'ecu-f407.map'

Write-Host "TOOLCHAIN: $($toolchain.Bin)"

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
    "-I$(Join-Path $projectRoot 'Core/Inc')",
    "-I$(Join-Path $repoRoot 'firmware/common')",
    "-I$(Join-Path $projectRoot 'Drivers/STM32F4xx_HAL_Driver/Inc')",
    "-I$(Join-Path $projectRoot 'Drivers/STM32F4xx_HAL_Driver/Inc/Legacy')",
    "-I$(Join-Path $projectRoot 'Drivers/CMSIS/Device/ST/STM32F4xx/Include')",
    "-I$(Join-Path $projectRoot 'Drivers/CMSIS/Include')",
    '-O0',
    '-ffunction-sections',
    '-fdata-sections',
    '-Wall',
    '--specs=nano.specs',
    '-mfpu=fpv4-sp-d16',
    '-mfloat-abi=hard',
    '-mthumb'
)

if ($FastTxTest) {
    $commonFlags += '-DAPP_CAN_FAST_TX_TEST=1U'
    Write-Host 'FAST TX TEST: APP_CAN_FAST_TX_TEST=1U'
}

if ($WwdgSelfTest -and $WwdgEarlyFeedTest) {
    throw 'Choose either -WwdgSelfTest or -WwdgEarlyFeedTest, not both.'
}
if ($IsotpBenchTx -and $IsotpBenchRx) {
    throw 'Choose either -IsotpBenchTx or -IsotpBenchRx, not both.'
}
if ($IsotpBenchTx) {
    $commonFlags += '-DAPP_ISOTP_BENCH_TX=1U'
    Write-Host 'ISO-TP BENCH: sender'
}
if ($IsotpBenchRx) {
    $commonFlags += '-DAPP_ISOTP_BENCH_RX=1U'
    Write-Host 'ISO-TP BENCH: receiver'
}
if ($IsotpBenchTx -or $IsotpBenchRx) {
    $commonFlags += "-DAPP_ISOTP_BENCH_PAYLOAD_SIZE=$IsotpBenchLength"
    Write-Host "ISO-TP BENCH LENGTH: $IsotpBenchLength"
}
if ($IsotpForceTxBusy) {
    $commonFlags += '-DAPP_ISOTP_FORCE_TX_BUSY=1U'
    Write-Host 'ISO-TP FAULT: force transmit-busy path'
}
if ($ParamSaveFailureTest) {
    $commonFlags += '-DAPP_PARAM_FORCE_SAVE_FAILURE=1U'
    Write-Host 'PARAM TEST: force persistence failure'
}
if ($WwdgSelfTest) {
    $commonFlags += '-DAPP_WWDG_SELFTEST=1U'
    Write-Host 'WWDG SELF TEST: stop refreshing WWDG'
}
if ($WwdgEarlyFeedTest) {
    $commonFlags += '-DAPP_WWDG_EARLY_FEED_TEST=1U'
    Write-Host 'WWDG EARLY FEED TEST: refresh before the window opens'
}

$sources = @()
$commonRoot = Join-Path $repoRoot 'firmware/common'
$sources += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Core/Src') -Filter '*.c' -File
$sources += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Drivers/STM32F4xx_HAL_Driver/Src') -Filter '*.c' -File
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'can_driver.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'can_health.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'can_rx_queue.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'isotp.c')
if ($IsotpBenchTx -or $IsotpBenchRx) {
    $sources += Get-Item -LiteralPath (Join-Path $commonRoot 'isotp_bench.c')
}
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'watchdog.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'param_store.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'param_log.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'uds_server.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'uds_reference_routines.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'uds_did_db.c')
$sources += Get-Item -LiteralPath (Join-Path $commonRoot 'uds_ecu.c')
$objectFiles = @()
$repoPrefix = $repoRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar

foreach ($source in $sources | Sort-Object FullName) {
    $relative = $source.FullName.Substring($repoPrefix.Length).Replace('\', '/')
    $objectRoot = Join-Path $buildDir 'obj'
    $object = Join-Path $objectRoot ($relative -replace '\.c$', '.o')
    $objectDir = Split-Path -Parent $object
    New-Item -ItemType Directory -Path $objectDir -Force | Out-Null

    Write-Host "CC  $relative"
    & $gcc @commonFlags -c $source.FullName -o $object
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed: $relative"
    }
    $objectFiles += $object
}

$startup = Join-Path $projectRoot 'Core/Startup/startup_stm32f407zgtx.s'
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
