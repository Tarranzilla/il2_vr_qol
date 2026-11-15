@echo off
cd /d "%~dp0"
taskkill /F /IM il2vr_companion.exe >nul 2>&1
if not exist "%~dp0build\windows-msvc-debug\il2vr_companion.exe" (
    echo [ERRO] Executavel nao encontrado em build\windows-msvc-debug\il2vr_companion.exe
    pause
    exit /b 1
)
start "IL-2 VR QoL Companion" "%~dp0build\windows-msvc-debug\il2vr_companion.exe"
