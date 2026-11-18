@echo off
cd /d "%~dp0"
taskkill /F /IM il2vr_companion.exe >nul 2>&1

if exist "%~dp0il2vr_companion.exe" (
    start "IL-2 VR QoL Companion" "%~dp0il2vr_companion.exe"
    exit /b 0
)

if exist "%~dp0build\windows-msvc-release\il2vr_companion.exe" (
    start "IL-2 VR QoL Companion" "%~dp0build\windows-msvc-release\il2vr_companion.exe"
    exit /b 0
)

if exist "%~dp0build\windows-msvc-debug\il2vr_companion.exe" (
    start "IL-2 VR QoL Companion" "%~dp0build\windows-msvc-debug\il2vr_companion.exe"
    exit /b 0
)

echo [ERROR] il2vr_companion.exe not found next to this script or in build\windows-msvc-{release,debug}.
pause
exit /b 1
