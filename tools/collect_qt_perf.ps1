[CmdletBinding()]
param(
    [string]$ExePath,
    [ValidateRange(1, 20)]
    [int]$Runs = 3,
    [ValidateRange(10, 3600)]
    [int]$RunSeconds = 300,
    [ValidateRange(0, 600)]
    [int]$WarmupSeconds = 30,
    [ValidateRange(0, 600)]
    [int]$BetweenRunSeconds = 30,
    [ValidateRange(0, 10)]
    [int]$ReconnectTrials = 3,
    [ValidateRange(0, 600)]
    [int]$ReconnectStableSeconds = 35
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$resultRoot = Join-Path $repoRoot "perf-results"
$sessionName = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDir = Join-Path $resultRoot $sessionName

function Resolve-ViewerExecutable {
    param([string]$RequestedPath)

    if ($RequestedPath) {
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

    $defaultPath = Join-Path $repoRoot "build-perf\qt_4ch_viewer.exe"
    if (-not (Test-Path -LiteralPath $defaultPath)) {
        throw "build-perf\qt_4ch_viewer.exe를 찾지 못했습니다. run_qt_perf.cmd로 Release 빌드부터 실행하세요."
    }
    return (Resolve-Path -LiteralPath $defaultPath).Path
}

function Get-Percentile {
    param(
        [double[]]$Values,
        [double]$Percentile
    )

    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) {
        return 0.0
    }
    $index = [Math]::Max(0, [Math]::Ceiling($sorted.Count * $Percentile) - 1)
    return [double]$sorted[$index]
}

function Measure-ProcessRun {
    param(
        [int]$ProcessId,
        [int]$RunNumber,
        [int]$DurationSeconds,
        [int]$LogicalCpuCount,
        [string]$OutputPath
    )

    $rows = @()
    $qtProcess = Get-Process -Id $ProcessId
    $previousCpuSeconds = [double]$qtProcess.CPU
    $previousTime = Get-Date

    try {
        for ($second = 1; $second -le $DurationSeconds; $second++) {
            Start-Sleep -Seconds 1
            $now = Get-Date
            $qtProcess = Get-Process -Id $ProcessId
            $elapsedSeconds = ($now - $previousTime).TotalSeconds
            $cpuPercent = (($qtProcess.CPU - $previousCpuSeconds) / $elapsedSeconds) *
                100.0 / $LogicalCpuCount

            $rows += [pscustomobject]@{
                run            = $RunNumber
                elapsed_s      = $second
                timestamp      = [DateTimeOffset]::Now.ToString("o")
                cpu_percent    = [Math]::Round($cpuPercent, 2)
                working_set_mb = [Math]::Round($qtProcess.WorkingSet64 / 1MB, 2)
            }

            $previousCpuSeconds = [double]$qtProcess.CPU
            $previousTime = $now

            if (($second % 30) -eq 0 -or $second -eq $DurationSeconds) {
                Write-Host ("  Run {0}: {1}/{2}초" -f $RunNumber, $second, $DurationSeconds)
            }
        }
    }
    finally {
        if ($rows.Count -gt 0) {
            $rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8
        }
    }

    return $rows
}

$viewerExe = Resolve-ViewerExecutable -RequestedPath $ExePath
$existingViewer = Get-Process -Name "qt_4ch_viewer" -ErrorAction SilentlyContinue
if ($existingViewer) {
    throw "실행 중인 qt_4ch_viewer를 먼저 종료하세요. 기존 프로세스의 Application Output은 가로챌 수 없습니다."
}

New-Item -ItemType Directory -Path $sessionDir -Force | Out-Null
$appLog = Join-Path $sessionDir "application.log"
$metricsLog = Join-Path $sessionDir "metrics.log"
$runWindowsPath = Join-Path $sessionDir "run-windows.csv"
$reconnectWindowsPath = Join-Path $sessionDir "reconnect-windows.csv"
$summaryPath = Join-Path $sessionDir "process-summary.csv"
$logicalCpuCount = [Environment]::ProcessorCount
$gitCommit = (& git -C $repoRoot rev-parse --short HEAD 2>$null)

$runtimeDirectories = @(
    "C:\Qt\6.11.0\mingw_64\bin",
    "C:\dev\vcpkg\installed\x64-mingw-dynamic\bin"
) | Where-Object { Test-Path -LiteralPath $_ }
$childPath = (($runtimeDirectories + $env:Path) -join ";")

@{
    created_at        = [DateTimeOffset]::Now.ToString("o")
    git_commit        = $gitCommit
    executable        = $viewerExe
    runs              = $Runs
    run_seconds       = $RunSeconds
    warmup_seconds    = $WarmupSeconds
    between_run_s     = $BetweenRunSeconds
    reconnect_trials  = $ReconnectTrials
    reconnect_stable_s = $ReconnectStableSeconds
    logical_cpu_count = $logicalCpuCount
    cpu_definition    = "프로세스 CPU 시간 변화량 / 논리 CPU 수 (0~100% 정규화)"
    memory_definition = "Windows WorkingSet64 (RSS 상당)"
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $sessionDir "metadata.json") -Encoding UTF8

$appJob = $null
$launchedProcess = $null
$runWindows = @()
$reconnectWindows = @()
$summaries = @()

try {
    Write-Host "Qt VMS 성능 계측"
    Write-Host ("  실행 파일: {0}" -f $viewerExe)
    Write-Host ("  결과 폴더: {0}" -f $sessionDir)
    Write-Host ""
    Write-Host "앱을 실행하고 Application Output을 별도 파일에 저장합니다."

    $appJob = Start-Job -ArgumentList $viewerExe, (Split-Path -Parent $viewerExe), `
        $appLog, $metricsLog, $childPath -ScriptBlock {
        param($Executable, $WorkingDirectory, $AppLogPath, $MetricsLogPath, $RuntimePath)

        Set-Location -LiteralPath $WorkingDirectory
        $env:Path = $RuntimePath
        $utf8 = New-Object System.Text.UTF8Encoding($false)
        $appWriter = New-Object System.IO.StreamWriter($AppLogPath, $false, $utf8)
        $metricsWriter = New-Object System.IO.StreamWriter($MetricsLogPath, $false, $utf8)
        $appWriter.AutoFlush = $true
        $metricsWriter.AutoFlush = $true
        try {
            & $Executable 2>&1 | ForEach-Object {
                $message = $_.ToString()
                $line = "{0} {1}" -f ([DateTimeOffset]::Now.ToString("o")), $message
                $appWriter.WriteLine($line)
                if ($message -match '^\[(ch[1-4]|ui-ch[1-4]|live-retry)\]') {
                    $metricsWriter.WriteLine($line)
                }
            }
        }
        finally {
            $metricsWriter.Dispose()
            $appWriter.Dispose()
        }
    }

    $launchDeadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 250
        $launchedProcess = Get-Process -Name "qt_4ch_viewer" -ErrorAction SilentlyContinue |
            Sort-Object StartTime -Descending | Select-Object -First 1
        if ($launchedProcess) {
            break
        }
        if ($appJob.State -in @("Completed", "Failed", "Stopped")) {
            $jobMessage = (Receive-Job -Job $appJob -Keep 2>&1 | Out-String).Trim()
            throw "앱 실행에 실패했습니다. $jobMessage"
        }
    } while ((Get-Date) -lt $launchDeadline)

    if (-not $launchedProcess) {
        throw "20초 안에 qt_4ch_viewer 프로세스가 시작되지 않았습니다. application.log를 확인하세요."
    }

    Write-Host ""
    Read-Host "Qt 창에서 4채널이 모두 연결된 것을 확인한 뒤 Enter를 누르세요"

    for ($run = 1; $run -le $Runs; $run++) {
        if ($WarmupSeconds -gt 0) {
            Write-Host ("Run {0} 워밍업 {1}초..." -f $run, $WarmupSeconds)
            Start-Sleep -Seconds $WarmupSeconds
        }

        $runStart = [DateTimeOffset]::Now
        Write-Host ("Run {0}/{1} 측정 시작 ({2}초)" -f $run, $Runs, $RunSeconds)
        $runCsv = Join-Path $sessionDir ("run-{0:D2}-process.csv" -f $run)
        $rows = Measure-ProcessRun -ProcessId $launchedProcess.Id -RunNumber $run `
            -DurationSeconds $RunSeconds -LogicalCpuCount $logicalCpuCount -OutputPath $runCsv
        $runEnd = [DateTimeOffset]::Now

        $runWindows += [pscustomobject]@{
            run       = $run
            started   = $runStart.ToString("o")
            ended     = $runEnd.ToString("o")
            csv_file  = Split-Path -Leaf $runCsv
        }
        $runWindows | Export-Csv -LiteralPath $runWindowsPath -NoTypeInformation -Encoding UTF8

        $cpuValues = [double[]]@($rows | ForEach-Object { $_.cpu_percent })
        $memoryValues = [double[]]@($rows | ForEach-Object { $_.working_set_mb })
        $summaries += [pscustomobject]@{
            run            = $run
            samples        = $rows.Count
            cpu_avg_pct    = [Math]::Round(($cpuValues | Measure-Object -Average).Average, 2)
            cpu_p95_pct    = [Math]::Round((Get-Percentile $cpuValues 0.95), 2)
            cpu_max_pct    = [Math]::Round(($cpuValues | Measure-Object -Maximum).Maximum, 2)
            working_avg_mb = [Math]::Round(($memoryValues | Measure-Object -Average).Average, 2)
            working_p95_mb = [Math]::Round((Get-Percentile $memoryValues 0.95), 2)
            working_max_mb = [Math]::Round(($memoryValues | Measure-Object -Maximum).Maximum, 2)
        }
        $summaries | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8

        if ($run -lt $Runs -and $BetweenRunSeconds -gt 0) {
            Write-Host ("다음 Run까지 {0}초 대기..." -f $BetweenRunSeconds)
            Start-Sleep -Seconds $BetweenRunSeconds
        }
    }

    if ($ReconnectTrials -gt 0) {
        Write-Host ""
        Write-Host "정상 재생 계측 완료. 이제 재연결 시험을 별도로 진행합니다."
        for ($trial = 1; $trial -le $ReconnectTrials; $trial++) {
            Write-Host ""
            Write-Host ("재연결 {0}/{1}: Pi RTSPS proxy를 중단한 뒤 retry 로그를 확인하고 다시 시작하세요." -f $trial, $ReconnectTrials)
            $trialStart = [DateTimeOffset]::Now
            Read-Host "영상이 다시 Playing 상태로 복구되면 Enter를 누르세요"
            $trialEnd = [DateTimeOffset]::Now
            $reconnectWindows += [pscustomobject]@{
                trial   = $trial
                started = $trialStart.ToString("o")
                ended   = $trialEnd.ToString("o")
            }
            $reconnectWindows | Export-Csv -LiteralPath $reconnectWindowsPath `
                -NoTypeInformation -Encoding UTF8

            if ($trial -lt $ReconnectTrials -and $ReconnectStableSeconds -gt 0) {
                Write-Host ("retry attempt 초기화를 위해 정상 재생 {0}초 대기..." -f $ReconnectStableSeconds)
                Start-Sleep -Seconds $ReconnectStableSeconds
            }
        }
    }

    Write-Host ""
    Write-Host "계측이 완료되었습니다."
    Write-Host ("결과: {0}" -f $sessionDir)
    Write-Host "그래프 작성에는 이 결과 폴더 전체를 사용하면 됩니다."
}
finally {
    if ($launchedProcess -and -not $launchedProcess.HasExited) {
        Stop-Process -Id $launchedProcess.Id -ErrorAction SilentlyContinue
    }
    if ($appJob) {
        Stop-Job -Job $appJob -ErrorAction SilentlyContinue
        Remove-Job -Job $appJob -Force -ErrorAction SilentlyContinue
    }
}
