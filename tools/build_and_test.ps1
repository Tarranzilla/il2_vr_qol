param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$preset = if ($Configuration -eq "Release") { "windows-msvc-release" } else { "windows-msvc-debug" }
$binDir = Join-Path $PSScriptRoot "..\build\$preset"

Write-Host "==> Configurando CMake ($preset)..." -ForegroundColor Cyan
cmake --preset $preset

Write-Host "==> Compilando ($preset)..." -ForegroundColor Cyan
cmake --build --preset $preset

Write-Host "==> Executando testes unitarios (CTest)..." -ForegroundColor Cyan
ctest --preset $preset --output-on-failure

Write-Host "==> Executando self-tests dos spikes e smoke-test..." -ForegroundColor Cyan
& (Join-Path $binDir "spike_s4_vjoy.exe") --self-test
& (Join-Path $binDir "spike_s5_telemetry.exe") --self-test
& (Join-Path $binDir "spike_s6_keyboard.exe") --self-test
& (Join-Path $binDir "spike_s2_seated_pose.exe") --status
& (Join-Path $binDir "il2vr_companion.exe") --smoke-test

Write-Host "==> Build e verificacoes concluidos com sucesso!" -ForegroundColor Green
