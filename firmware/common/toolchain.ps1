function Resolve-ArmGccToolchain {
    param(
        [string]$BinDirectory = $env:ARM_GCC_BIN
    )

    $isWindowsHost = $env:OS -eq 'Windows_NT'
    $exeSuffix = if ($isWindowsHost) { '.exe' } else { '' }

    function Test-ToolchainDirectory([string]$candidate) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            return $null
        }
        $resolved = Resolve-Path -LiteralPath $candidate -ErrorAction SilentlyContinue
        if ($null -eq $resolved) {
            return $null
        }
        $gcc = Join-Path $resolved.Path ("arm-none-eabi-gcc" + $exeSuffix)
        $objcopy = Join-Path $resolved.Path ("arm-none-eabi-objcopy" + $exeSuffix)
        $size = Join-Path $resolved.Path ("arm-none-eabi-size" + $exeSuffix)
        if ((Test-Path -LiteralPath $gcc -PathType Leaf) -and
            (Test-Path -LiteralPath $objcopy -PathType Leaf) -and
            (Test-Path -LiteralPath $size -PathType Leaf)) {
            return [pscustomobject]@{
                Bin = $resolved.Path
                Gcc = $gcc
                Objcopy = $objcopy
                Size = $size
            }
        }
        return $null
    }

    $direct = Test-ToolchainDirectory $BinDirectory
    if ($null -ne $direct) {
        return $direct
    }

    $gccCommand = Get-Command ("arm-none-eabi-gcc" + $exeSuffix) -ErrorAction SilentlyContinue
    if ($null -ne $gccCommand) {
        $fromPath = Test-ToolchainDirectory (Split-Path -Parent $gccCommand.Source)
        if ($null -ne $fromPath) {
            return $fromPath
        }
    }

    $roots = @()
    if (-not [string]::IsNullOrWhiteSpace($env:STM32CUBEIDE_HOME)) {
        $roots += $env:STM32CUBEIDE_HOME
    }
    if ($isWindowsHost) {
        $roots += Get-PSDrive -PSProvider FileSystem |
            ForEach-Object { Join-Path $_.Root 'ST' }
        $roots += @(
            (Join-Path $env:ProgramFiles 'STMicroelectronics'),
            (Join-Path ${env:ProgramFiles(x86)} 'STMicroelectronics'),
            (Join-Path $env:LOCALAPPDATA 'Programs')
        )
    } else {
        $roots += @('/opt/st', '/opt/STM32CubeIDE', (Join-Path $HOME 'st'))
    }

    foreach ($root in ($roots | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)) {
        $match = Get-ChildItem -LiteralPath $root `
            -Filter ("arm-none-eabi-gcc" + $exeSuffix) `
            -File -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $match) {
            $found = Test-ToolchainDirectory $match.DirectoryName
            if ($null -ne $found) {
                return $found
            }
        }
    }

    throw "ARM GCC not found. Set ARM_GCC_BIN to the directory containing arm-none-eabi-gcc."
}
