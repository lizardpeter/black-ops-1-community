param(
    [Parameter(Mandatory)] [string] $GameDirectory,
    [Parameter(Mandatory)] [string] $Executable
)

$ErrorActionPreference = 'Stop'
$game = (Resolve-Path $GameDirectory).Path
Copy-Item $Executable $game
$launch = Join-Path $game (Split-Path $Executable -Leaf)
$env:BO1_LAUNCH_CHECK = '1'
$process = Start-Process $launch -WorkingDirectory $game -PassThru -ArgumentList @(
    'allowdupe', '+set', 'logfile', '2', '+set', 'r_fullscreen', '0',
    '+set', 'g_gametype', 'dm', '+map', 'mp_nuked'
)
$start = Get-Date
$deadline = $start.AddMinutes(4)
$log = $null
$passed = $false
try {
    while ((Get-Date) -lt $deadline -and !$process.HasExited) {
        $log = Get-ChildItem $game -Filter console_mp.log -File -Recurse |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($log -and (Select-String -LiteralPath $log.FullName -SimpleMatch 'BO1_LAUNCH_CHECK: active multiplayer frame' -Quiet)) {
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0) {
                $passed = $true
                break
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
            throw ('BO1 exited before rendering an active multiplayer frame. Exit code 0x{0:X8}' -f $process.ExitCode)
        }
        throw 'BO1 did not produce a game window and an active multiplayer frame before the timeout.'
    }
    Write-Output 'BO1 rendered an active local multiplayer frame on Nuketown.'
} finally {
    if ($log) {
        Get-Content -LiteralPath $log.FullName -Tail 100
    }
    if (!$process.HasExited) {
        Stop-Process -Id $process.Id
    }
}
