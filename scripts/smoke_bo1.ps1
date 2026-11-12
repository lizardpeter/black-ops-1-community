param(
    [Parameter(Mandatory)] [string] $GameDirectory,
    [Parameter(Mandatory)] [string] $Executable,
    [Parameter(Mandatory)] [string] $Screenshot
)

$ErrorActionPreference = 'Stop'
$game = (Resolve-Path $GameDirectory).Path
Copy-Item $Executable $game
$launch = Join-Path $game (Split-Path $Executable -Leaf)
$env:BO1_LAUNCH_CHECK = '1'
$env:BO1_CAPTURE_PATH = [System.IO.Path]::GetFullPath($Screenshot)
Remove-Item -LiteralPath $env:BO1_CAPTURE_PATH -ErrorAction SilentlyContinue
$process = Start-Process $launch -WorkingDirectory $game -PassThru -ArgumentList @(
    'allowdupe', '+set', 'logfile', '2', '+set', 'r_fullscreen', '0',
    '+set', 'r_aaSamples', '1', '+set', 'g_gametype', 'dm', '+map', 'mp_nuked'
)
$start = Get-Date
$deadline = $start.AddMinutes(4)
$log = $null
$passed = $false
try {
    while ((Get-Date) -lt $deadline -and !$process.HasExited) {
        $log = Get-ChildItem $game -Filter console_mp.log -File -Recurse |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($log -and (Select-String -LiteralPath $log.FullName -SimpleMatch 'BO1_LAUNCH_CHECK: captured and presented multiplayer frame' -Quiet)) {
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0 -and (Test-Path -LiteralPath $env:BO1_CAPTURE_PATH -PathType Leaf)) {
                $passed = $true
                break
            }
        }
        if ($log) {
            $textureFailure = Select-String -LiteralPath $log.FullName -SimpleMatch 'BO1_TEXTURE_FAILURE:' |
                Select-Object -Last 1
            if ($textureFailure) {
                throw $textureFailure.Line
            }
        }
        Start-Sleep -Seconds 2
    }
    if (!$passed) {
        $log = Get-ChildItem $game -Filter console_mp.log -File -Recurse |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($log) {
            $tail = (Get-Content -LiteralPath $log.FullName -Tail 20) -join ' | '
            Write-Host "::warning title=Last BO1 console lines::$tail"
        }
        $events = Get-WinEvent -FilterHashtable @{
            LogName = 'Application'
            StartTime = $start.AddSeconds(-2)
            Id = 1000, 1001
        } -ErrorAction SilentlyContinue
        foreach ($event in ($events | Where-Object { $_.Message -match 'BlackOpsMP-Community' } | Select-Object -First 2)) {
            $message = ($event.Message -replace '[\r\n]+', ' ')
            Write-Host "::warning title=Windows application event::$message"
        }
        if ($process.HasExited) {
            throw ('BO1 exited before capturing and presenting a multiplayer frame. Exit code 0x{0:X8}' -f $process.ExitCode)
        }
        throw 'BO1 did not produce a game window and capture a presented multiplayer frame before the timeout.'
    }
    $bitmap = [System.Drawing.Bitmap]::new($env:BO1_CAPTURE_PATH)
    try {
        $sampleColors = [System.Collections.Generic.HashSet[int]]::new()
        for ($y = 0; $y -lt $bitmap.Height; $y += 16) {
            for ($x = 0; $x -lt $bitmap.Width; $x += 16) {
                [void] $sampleColors.Add($bitmap.GetPixel($x, $y).ToArgb() -band 0xFFFFFF)
            }
        }
        if ($sampleColors.Count -lt 8) {
            throw 'The captured frame has fewer than eight distinct sampled colors.'
        }
        Write-Output "BO1 presented and captured a $($bitmap.Width)x$($bitmap.Height) multiplayer frame on Nuketown."
    } finally {
        $bitmap.Dispose()
    }
} finally {
    if ($log) {
        Get-Content -LiteralPath $log.FullName -Tail 100
    }
    if (!$process.HasExited) {
        Stop-Process -Id $process.Id
    }
}
