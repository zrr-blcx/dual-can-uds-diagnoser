param(
    [switch]$IsotpBenchTx,
    [switch]$IsotpBenchRx,
    [ValidateRange(1, 300)][int]$IsotpBenchLength = 100,
    [switch]$IsotpForceTxBusy,
    [switch]$ParamSaveFailureTest,
    [switch]$WwdgSelfTest,
    [switch]$WwdgEarlyFeedTest,
    [switch]$DbcBench,
    [switch]$BenchTest,
    [switch]$StressMaster
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
$linker = Join-Path $projectRoot 'STM32F103C8Tx_FLASH.ld'
$elf = Join-Path $buildDir 'ecu-f103.elf'
$map = Join-Path $buildDir 'ecu-f103.map'

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
    '-mcpu=cortex-m3',
    '-std=gnu11',
    '-g3',
    '-DDEBUG',
    '-DUSE_HAL_DRIVER',
    '-DSTM32F103xB',
    "-I$(Join-Path $projectRoot 'Core/Inc')",
    "-I$(Join-Path $repoRoot 'firmware/common')",
    "-I$(Join-Path $projectRoot 'Drivers/STM32F1xx_HAL_Driver/Inc')",
    "-I$(Join-Path $projectRoot 'Drivers/STM32F1xx_HAL_Driver/Inc/Legacy')",
    "-I$(Join-Path $projectRoot 'Drivers/CMSIS/Device/ST/STM32F1xx/Include')",
    "-I$(Join-Path $projectRoot 'Drivers/CMSIS/Include')",
    '-O0',
    '-ffunction-sections',
    '-fdata-sections',
    '-Wall',
    '--specs=nano.specs',
    '-mthumb'
)

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
if ($DbcBench) {
    $commonFlags += '-DAPP_DBC_BENCH=1U'
    Write-Host 'DBC BENCH: F103 VehicleStatus on 0x280'
}
if ($BenchTest) {
    if ($DbcBench -or $IsotpBenchTx -or $IsotpBenchRx) {
        throw 'BenchTest cannot be combined with DBC or ISO-TP bench modes.'
    }
    $commonFlags += '-DAPP_BENCH_TEST=1U'
    Write-Host 'BENCH TEST: background sequence probe and UDS fault hook'
}
if ($StressMaster) {
    if ($DbcBench -or $IsotpBenchTx -or $IsotpBenchRx -or $BenchTest) {
        throw 'StressMaster must be built as its own test mode.'
    }
    $commonFlags += '-DAPP_STRESS_MASTER=1U'
    Write-Host 'STRESS MASTER: 90% background load and F407 UDS test'
}

$sources = @()
$commonRoot = Join-Path $repoRoot 'firmware/common'
$sources += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Core/Src') -Filter '*.c' -File
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
if ($DbcBench) {
    $sources += Get-Item -LiteralPath (Join-Path $commonRoot 'project_dbc.c')
    $sources += Get-Item -LiteralPath (Join-Path $commonRoot 'dbc_bench.c')
}
if ($BenchTest) {
    $sources += Get-Item -LiteralPath (Join-Path $commonRoot 'background_probe.c')
    $sources += Get-Item -LiteralPath (Join-Path $commonRoot 'uds_fault_test.c')
}
if ($StressMaster) {
    $sources += Get-Item -LiteralPath (Join-Path $commonRoot 'stress_master.c')
}

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
$halSourceRoot = Join-Path $projectRoot 'Drivers/STM32F1xx_HAL_Driver/Src'
foreach ($name in $halNames) {
    $sources += Get-Item -LiteralPath (Join-Path $halSourceRoot $name)
}

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

$startup = Join-Path $projectRoot 'Core/Startup/startup_stm32f103xb.s'
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
