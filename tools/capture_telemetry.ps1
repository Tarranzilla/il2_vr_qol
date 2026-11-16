param(
    [int]$Port = 29373,
    [int]$Seconds = 15,
    [string]$OutFile = ""
)

$ErrorActionPreference = 'Stop'
$captureDir = Join-Path $PSScriptRoot 'captures'
if (-not (Test-Path $captureDir)) {
    New-Item -ItemType Directory -Path $captureDir | Out-Null
}
if (-not $OutFile) {
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $OutFile = Join-Path $captureDir "il2_telemetry_${stamp}.bin"
}

$udp = New-Object System.Net.Sockets.UdpClient($Port)
$udp.Client.ReceiveTimeout = 200
$ep = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$fs = [System.IO.File]::Open($OutFile, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
$bw = New-Object System.IO.BinaryWriter($fs)

Write-Host "Capturando UDP na porta $Port por ${Seconds}s -> $OutFile"
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$count = 0
$motionCount = 0
$stateCount = 0

try {
    while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $bytes = $udp.Receive([ref]$ep)
            if ($bytes.Length -ge 4) {
                $pidHex = [System.BitConverter]::ToUInt32($bytes, 0)
                if ($pidHex -eq 0x494C0100) { $motionCount++ }
                elseif ($pidHex -eq 0x54000101) { $stateCount++ }
                $bw.Write([uint32]$bytes.Length)
                $bw.Write($bytes)
                $count++
            }
        } catch [System.Net.Sockets.SocketException] {
            # Timeout waiting for packet
        }
    }
} finally {
    $bw.Dispose()
    $fs.Dispose()
    $udp.Dispose()
}

Write-Host "Concluido: $count pacotes (Motion=$motionCount, Telemetry=$stateCount) salvos em $OutFile"
