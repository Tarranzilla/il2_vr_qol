@echo off
chcp 65001 >nul
echo Reconfiguring vJoy Device 1 with all 8 axes (X, Y, Z, Rx, Ry, Rz, Sl0, Sl1) and 16 buttons...
powershell -Command "Start-Process -FilePath 'C:\Program Files\vJoy\x64\vJoyConfig.exe' -ArgumentList '1 -f -a x y z rx ry rz sl0 sl1 -b 16' -Verb RunAs -Wait"
"C:\Program Files\vJoy\x64\vJoyConfig.exe" -t 1
pause
