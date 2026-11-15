@echo off
chcp 65001 >nul
echo Reconfigurando vJoy Device 1 com todos os 8 eixos (X, Y, Z, Rx, Ry, Rz, Sl0, Sl1) e 16 botoes...
powershell -Command "Start-Process -FilePath 'C:\Program Files\vJoy\x64\vJoyConfig.exe' -ArgumentList '1 -f -a x y z rx ry rz sl0 sl1 -b 16' -Verb RunAs -Wait"
"C:\Program Files\vJoy\x64\vJoyConfig.exe" -t 1
pause
