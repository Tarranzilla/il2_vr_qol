# IL-2 VR QoL Companion (`v0.8.0`)

**VR Quality of Life & Virtual Flight Controls Companion** for *IL-2 Sturmovik: Great Battles* (Meta Quest 3 / SteamVR).

Companion app de **Qualidade de Vida, Controles Virtuais 6DoF e Mouse VR** para *IL-2 Sturmovik: Great Battles* no Meta Quest 3 / SteamVR.

> [!IMPORTANT]
> **Zero game file modification / Zero alteração em arquivos do jogo:** Runs completely externally via SteamVR Overlays (`OpenVR`) + `vJoy` virtual joystick + Windows `SendInput` (`en-US` & `pt-BR` multi-language UI).

---

## Implemented Features / Recursos Implementados (`v0.1.0` → `v0.8.0`)

- **3D Seat / VR Origin Positioning (`v0.1.0` + `v0.2.0`):**
  - Triple-click **`3x R3`** (Right Stick Click): Toggle between **Flight Mode** and **Origin 3D Drag Mode** (grab anywhere with Grip to translate the cockpit origin with zero rotation).
  - Left **`Y`** Button: Instant head-pose recenter (`TrackingUniverseStanding` chaperone-safe).
  - Fine & coarse nudge buttons + Global Windows Hotkeys + `NumPad 5` IL-2 native recenter helper.
- **Quest 3-Style Draggable VR HUD (`v0.2.0`):**
  - Triple-click **`3x L3`** (Left Stick Click) or `Ctrl+Alt+O`: Open/Close the VR HUD overlay.
  - Grab the **bottom handle bar** with Trigger or Grip + Raycaster (and thumbstick Y for distance) to move and anchor the HUD anywhere in 3D cockpit space.
- **Two Virtual Flight Control Modes (`v0.3.0`):**
  - **Free Mode (`Modo Livre`):** Hold Right/Left Grip anywhere in mid-air for 6DoF VTOL-VR-style virtual stick (with wrist-twist rudder & One-Euro jitter filtering) and latched throttle lever.
  - **Anchored Mode (`Modo Ancorado`):** Fixed 3D cockpit stick & throttle anchors persisted in `config/controls.anchors.json`, with haptic proximity cues and a **`Position Controls` (`Posicionar Controles`)** calibration submode (neutralizes flight axes while dragging/rotating anchors to match the cockpit).
  - Automatic `vJoy` recovery, 8-axis admin configurator, IL-2 `devices.txt` profile sync (`joyN`), and jitter-free **Auto-Bind** calibration sweeps.
- **Toggleable 3D Hand Arrows & Euler Rotation Gizmo (`v0.4.0`):**
  - Real-time 3D OpenVR directional overlays (`Forward -Z`, `Right +X`) on both controllers and anchored cockpit controls, plus live Pitch / Roll / Yaw angle readouts in degrees.
- **VR Raycaster Mouse in IL-2 Menus (`v0.5.0` → `v0.8.0`):**
  - Press **`X` (Left controller)** to toggle VR Menu Mouse (automatically hiding centered HUD panels so the menu stays clear).
  - Projects onto an interactive **3D Virtual Menu Screen** (with cyan frame & 3D ray-intersection cursor dot) using DirectInput-compatible smooth relative deltas (`MOUSEEVENTF_MOVE`).
  - Hold **Grip** in VR Mouse Mode to drag/align the virtual screen directly over IL-2's menu (`Stick Y` = distance, `Stick X` = width), persisted automatically in `config/settings.json`.
  - **Trigger** = Left Click, **`A`** = Right Click, **Stick Y** = Scroll, with automatic vJoy weapon lock.
- **Modular i18n Translation System (`v0.6.0` & `v0.7.0`):**
  - Community-extensible JSON locale files in [`config/locales/`](config/locales/) (`pt-BR` and `en-US` included out of the box) with live in-VR switching in the **Languages / Idiomas** tab.

---

## Controls Summary / Resumo dos Atalhos VR (Quest 3)

| Action / Ação | Input |
|---|---|
| Open / Close VR HUD (`Abrir/Fechar HUD VR`) | **`3x L3`** (Left stick triple-click) or `Ctrl+Alt+O` |
| Flight Mode ↔ Edit VR Origin (`Modo Voo <-> Origem 3D`) | **`3x R3`** (Right stick triple-click) |
| Toggle VR Menu Mouse (`Ativar/Desativar Mouse VR`) | **`X`** Button (Left controller) |
| Move / Resize Mouse Screen (`Ajustar Tela do Mouse VR`) | Hold **Grip** (+ **Stick Y** depth, **Stick X** size) |
| Reposition HUD Window (`Mover Painel HUD`) | Hold **Trigger** or **Grip** on the bottom HUD Grab Bar (+ **Stick Y** for depth) |
| Grab Virtual Stick / Throttle (`Manche / Manete`) | Hold **Right Grip** / **Left Grip** |
| Recenter Origin at Head (`Recentralizar na Cabeça`) | **`Y`** Button (Left controller) |
| VR Menu Mouse (`Mouse VR nos Menus`) | **Laser** + **Trigger** (L-Click) / **`A`** (R-Click) / **Stick Y** (Scroll) |

---

## Build & Test (`Windows MSVC + vcpkg + CMake`)

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_and_test.ps1
```

- Run unit tests: `ctest --test-dir build/windows-msvc-debug --output-on-failure`
- Run companion app: `.\build\windows-msvc-debug\il2vr_companion.exe`

## Requisitos / Requirements

- Windows 10/11, *IL-2 Sturmovik: Great Battles* running in **SteamVR / OpenVR** mode
- Meta Quest 3 (Quest Link / Air Link / Virtual Desktop / Steam Link)
- [vJoy](https://github.com/BrunnerInnovation/vJoy) virtual joystick driver (Device 1 with X, Y, Z, Rz axes)
- Visual Studio 2022+ (MSVC, C++20), CMake, vcpkg (`nlohmann-json`, `spdlog`, `glm`, `imgui`, `catch2`)

## Documentação / Docs

| Documento | Conteúdo |
|---|---|
| [01 – Visão e Escopo](docs/01-visao-e-escopo.md) | Objetivos, não-objetivos, premissas |
| [02 – Arquitetura](docs/02-arquitetura.md) | Módulos, fluxo de dados, stack |
| [03 – Roadmap](docs/03-roadmap.md) | Fases e marcos |
| [04 – Riscos e Spikes](docs/04-riscos-e-spikes.md) | Validação técnica e riscos |
| [ADRs](docs/adr/) | Architecture Decision Records |
| [Specs](docs/specs/) | Especificação dos 3 pilares |
| [Locales](config/locales/) | JSON translations (`pt-BR.json`, `en-US.json`) |
