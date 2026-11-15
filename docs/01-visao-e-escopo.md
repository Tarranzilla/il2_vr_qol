# 01 – Visão e Escopo

## Visão

> Tornar o IL-2 em VR com Meta Quest 3 **confortável de configurar** e **tátil de pilotar**: sentar, alinhar o cockpit com o corpo em segundos, pegar o manche e a manete com as mãos e, por fim, operar o painel do avião sem tirar o headset.

## Objetivos

| ID | Objetivo | Métrica de sucesso |
|----|----------|-------------------|
| O1 | Alinhar a posição do piloto ao assento real | Recentralizar + ajuste fino em < 10 s; precisão de 5 mm / 0,5° |
| O2 | Persistência do alinhamento | Offset salvo por perfil (global e por aeronave) e restaurado automaticamente |
| O3 | Pilotar com os controles do Quest 3 | Pitch/roll/throttle via vJoy com latência percebida < 1 frame de input (~11 ms a 90 Hz) |
| O4 | Feedback háptico | Vibração correlacionada a eventos do jogo (tiro, impacto, motor, buffet) |
| O5 | Visibilidade/diagnóstico | Camadas de debug individuais, ligáveis por UI e atalho |
| O6 | Interagir com o cockpit | Apontar e acionar ≥ 10 controles por aeronave via hotspots calibrados |

## Não-objetivos (por ora)

- Modificar binários, shaders ou arquivos `.gtp` do jogo.
- Renderizar modelos 3D do cockpit dentro do jogo (o jogo não expõe API para isso; usamos overlays).
- Cockpits "clicáveis" nativos (o IL-2 GB não os possui; emulamos via teclas/eixos).
- Suporte a outros headsets além do Quest 3 na v1 (a arquitetura não impede, mas não será testado).
- Rastreamento de mãos (hand tracking) na v1 – candidato para versão futura.

## Público

- Piloto virtual de IL-2 GB em VR, sem HOTAS ou que queira imersão "mão no manche".
- Usuário avançado que tolera instalar vJoy e configurar keybinds no jogo.

## Premissas

1. O jogo base é o **IL-2 Sturmovik: Battle of Stalingrad** (módulo base da série *Great Battles*, versão 5.x).
2. O jogo será executado no **modo SteamVR (OpenVR)** (decisão confirmada; o modo Oculus nativo não permite que apps externos alterem o espaço de rastreamento nem desenhem overlays). → validar detalhes de setup no Spike S1.
3. O IL-2 GB **não** usa os controles VR para nada, então não há conflito de input.
4. O IL-2 aceita joysticks DirectInput virtuais (vJoy) e teclado via `SendInput` com scancodes.
5. O IL-2 envia telemetria UDP configurável em `data/startup.cfg` (seções `motiondevice` / `telemetrydevice`).
6. **Aeronave de referência (Pilar 3):** **LaGG-3 Series 29 (`lagg3s29`)**, primeiro caça soviético utilizado na campanha de *Battle of Stalingrad* (com o **Yak-1 Series 69 (`yak1s69`)** também presente na fase inicial do modo Carreira).

## Glossário

| Termo | Significado |
|-------|-------------|
| **Seat Pose / Assento** | Transformação (posição + yaw) que define onde o "zero" do espaço sentado fica no mundo real |
| **Offset** | Delta aplicado sobre o Seat Pose pelos ajustes do usuário |
| **Grab** | Ato de segurar (botão grip) um controle virtual (manche/manete/alavanca) |
| **Pivot** | Ponto de rotação virtual do manche |
| **Hotspot** | Região 3D (no espaço do assento) associada a um controle do cockpit |
| **Cockpit Anchor** | Referencial do perfil de aeronave; coincide com o Seat Pose calibrado |
| **Camada (Layer)** | Overlay de visualização/debug que pode ser ligado e desligado |
