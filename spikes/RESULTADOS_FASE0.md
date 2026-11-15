# Resultados da Fase 0 – Fundação & Spikes

**Data:** 2026-11-15

## 1. Descobertas Verificadas no Ambiente

- **IL-2 Sturmovik: Battle of Stalingrad:** instalado em `C:\Program Files (x86)\Steam\steamapps\common\IL-2 Sturmovik Battle of Stalingrad`.
  - `data\startup.cfg` contém `or_enable = 1` (VR habilitado) e `[KEY = input] exclusive = 1`.
  - `data\input\devices.txt` já lista o dispositivo virtual: `1,%22113bac80-e0b2-11f1-0000545345440280%22,vJoy%20Device`.
- **vJoy (Virtual Joystick):** versão `2.2.2.0` (`0x222`) instalada em `C:\Program Files\vJoy\x64\vJoyInterface.dll`, com `Device 1: Status: FREE` (`1`) e exports confirmados.
- **SteamVR:** instalado em `C:\Program Files (x86)\Steam\steamapps\common\SteamVR` (estava fechado durante os testes automatizados).
- **Toolchain & Dependências:** MSVC 14.51 (Visual Studio 2026 Insiders), CMake 4.3.1, Ninja e vcpkg instalados. Todas as dependências do [vcpkg.json](../vcpkg.json) (`openvr`, `imgui[dx11-binding,win32-binding]`, `nlohmann-json`, `spdlog`, `glm`, `catch2`) compiladas para `x64-windows`.

## 2. Testes Automatizados Executados (pós-desbloqueio do Smart App Control)

Executado `.\tools\build_and_test.ps1` + `spike_s3_vr_input.exe --status`:

| Verificação | Resultado | Observação |
|-------------|-----------|------------|
| `cmake --preset windows-msvc-debug` & `--build` | ✅ Passou | 7 executáveis + `il2vr_core` compilados sem erros/avisos |
| `ctest --preset windows-msvc-debug` (`il2vr_tests`) | ✅ Passou (1/1, 0.07 s) | `MathTypes`, `SeatManager`, `VJoyClient::normalizedToVJoyAxis`, `KeyboardInjector::resolveKey` |
| `spike_s4_vjoy.exe --self-test` | ✅ Passou | Carregou `vJoyInterface.dll`, versão `0x222`, adquiriu Device 1, enviou X/Y/Z/Rz/Btn1, reset e relinquish |
| `spike_s5_telemetry.exe --self-test` | ✅ Passou | Listener UDP loopback recebeu 8 bytes na porta efêmera `50793` |
| `spike_s6_keyboard.exe --self-test` | ✅ Passou | Resolveu scancodes de todas as teclas do perfil LaGG-3 (`G`, `F`, `LShift`, `S`, `E`, `RAlt`, `C`, `RCtrl`, `L`) |
| `spike_s2_seated_pose.exe --status` | ✅ Passou (código 0) | Reportou SteamVR offline (`Not starting vrserver for background app (121)`) sem abrir SteamVR à força |
| `spike_s3_vr_input.exe --status` | ✅ Passou (código 0) | Reportou SteamVR offline (`Not starting vrserver for background app (121)`) |
| `il2vr_companion.exe --smoke-test` | ✅ Passou | Validou `SeatManager` (save/load JSON), `vJoy` Device 1, `TelemetryListener` loopback e `KeyboardInjector` |

## 3. Validação ao Vivo (Quest 3 + SteamVR + IL-2 abertos)

- **S1 (Runtime SteamVR + IL-2):** ✅ Confirmado com `Il-2` (PID `3152`), `vrmonitor` (PID `9900`) e `vrserver` (PID `16976`) ativos em conjunto.
- **Conexão inicial de S2 e S3 com o jogo aberto:**
  - `spike_s2_seated_pose.exe --status` conectou em modo overlay (`overlay=sim`), leu a matriz `SeatedZeroPose` e a pose bruta do HMD (`pos=(-0.0464204, 1.17858, 0.292642)`, `yaw=-41.4051 deg`).
  - `spike_s3_vr_input.exe --samples 3` leu simultaneamente as poses e estados de ambos os controles Quest 3 (`L` e `R`) com o `Il-2.exe` em execução.

## 4. Próximos Spikes Interativos (com voo iniciado no cockpit)

Os testes acima validaram toda a pilha técnica no Windows sem o headset ligado. Para marcar S1–S8 como concluídos com o jogo aberto:

1. **S1 / S2 (Seated Zero Pose no IL-2):** com SteamVR, Quest 3 e IL-2 em voo, executar:
   ```powershell
   .\build\windows-msvc-debug\spike_s2_seated_pose.exe --nudge-y 0.15
   ```
   Confirmar deslocamento vertical imediato de 15 cm por 2 s e retorno automático.
2. **S3 (Controles Quest 3 com o IL-2 em foco):** manter o foco na janela do IL-2 e executar:
   ```powershell
   .\build\windows-msvc-debug\spike_s3_vr_input.exe --samples 50
   ```
   Confirmar leitura de pose, grip, gatilho e thumbsticks.
3. **S4 (Mapeamento no IL-2):** executar:
   ```powershell
   .\build\windows-msvc-debug\spike_s4_vjoy.exe --sweep 15
   ```
   Mapear Pitch/Roll/Throttle/Rudder em *Settings → Key Mapping* do IL-2.
4. **S5 (Telemetria UDP real):** configurar `[KEY = motiondevice]` / `[KEY = telemetrydevice]` em `data\startup.cfg` apontando para `127.0.0.1` e executar:
   ```powershell
   .\build\windows-msvc-debug\spike_s5_telemetry.exe --listen 4321 15
   ```
5. **S6 (`SendInput` scancode no IL-2):** executar:
   ```powershell
   .\build\windows-msvc-debug\spike_s6_keyboard.exe --send-tap G
   ```
   Focar a janela do IL-2 durante a contagem de 3 s e confirmar acionamento do trem de pouso.
6. **S7 / S8:** testar o comportamento do *Reset VR view* do próprio IL-2 em relação ao espaço *Seated* e prototipar o primeiro painel de overlay sobre o jogo (já no início da Fase 1 / Fase 2).
