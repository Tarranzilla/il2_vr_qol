# 04 – Riscos e Spikes

Spikes são protótipos curtos e descartáveis (pasta `spikes/`) para responder uma pergunta técnica antes de investir na implementação. Cada spike termina com uma nota em `spikes/<id>/RESULT.md` (pergunta, método, resultado, decisão).

## Matriz de riscos

| ID | Risco | Prob. | Impacto | Mitigação / Spike |
|----|-------|:-----:|:-------:|-------------------|
| R1 | IL-2 rodando no runtime Oculus impede overlays e alteração do espaço | Média | Alto | Exigir modo SteamVR (S1); backend OpenXR layer como plano B |
| R2 | Alterar o *seated zero* não afeta a câmera do IL-2 em tempo real | Baixa | Alto | S2; fallback: ajustar o standing zero ou usar layer OpenXR |
| R3 | Overlay app não recebe input dos controles com o jogo em foco | Média | Alto | S3; testar IVRInput vs. `GetControllerState` legado |
| R4 | IL-2 não aceita vJoy ou tem limite de dispositivos | Baixa | Alto | S4 |
| R5 | Formato da telemetria pouco documentado | Média | Médio | S5: engenharia reversa com captura + docs da comunidade (SimTools, SimHub) |
| R6 | `SendInput` ignorado pelo jogo (DirectInput exclusivo) | Baixa | Alto | S6; fallback: botões vJoy em vez de teclado |
| R7 | Recenter do jogo desalinha hotspots | Alta | Médio | S7; documentar, bloquear/remapear o atalho do jogo, recalibração rápida |
| R8 | Overlays 2D insuficientes para laser/gizmos 3D convincentes | Média | Baixo | S8; laser como quad em billboard axial; render models do OpenVR |
| R9 | Jitter do rastreamento gera tremor no manche | Média | Médio | Filtro One-Euro, deadzone, suavização configurável |
| R10 | Fadiga do braço ("gorilla arm") ao pilotar | Alta | Médio | Pivot ajustável perto da coxa/apoio; modo rotação de pulso; ganho alto |
| R11 | Mudanças de versão do IL-2 quebram binds/telemetria | Baixa | Médio | Parser versionado, testes com capturas gravadas |

## Spikes

### S1 – Caminho de runtime
- **Pergunta:** Como rodar o IL-2 + Quest 3 sob SteamVR de forma estável (Link/Air Link + SteamVR, Virtual Desktop → SteamVR, Steam Link)?
- **Método:** Iniciar o jogo em cada caminho; confirmar no SteamVR que o app ativo é o IL-2; medir frame time.
- **Saída:** caminho recomendado + instruções de setup.

### S2 – Seated zero pose em runtime
- **Pergunta:** `IVRChaperoneSetup::SetWorkingSeatedZeroPoseToRawTrackingPose` + `CommitWorkingCopy(EChaperoneConfigFile_Live)` desloca a câmera do IL-2 imediatamente?
- **Método:** Spike console: aplicar +10 cm em Y a cada 2 s com o jogo rodando. Testar também a variante *standing*.
- **Saída:** API correta, latência, efeitos colaterais (guardian, outros apps).

### S3 – Input em segundo plano
- **Pergunta:** Um app `VRApplication_Overlay` lê poses, grip, trigger e thumbsticks com o IL-2 em foco?
- **Método:** Spike que imprime o estado a 90 Hz usando IVRInput (action manifest) e, em paralelo, a API legada.
- **Saída:** API escolhida, binding padrão para "Oculus Touch".

### S4 – vJoy no IL-2
- **Método:** Instalar vJoy, alimentar eixos senoidais, mapear em *Settings → Key Mapping*.
- **Saída:** eixos/botões suportados, presença em `input/devices.txt`, impacto de reordenar dispositivos.

### S5 – Telemetria UDP
- **Método:** Habilitar `motiondevice` e `telemetrydevice` em `data/startup.cfg` (endereço `127.0.0.1`, porta, decimation); capturar pacotes com ferramenta em `tools/`; mapear campos (cabeçalho, ids de indicadores e eventos).
- **Saída:** `docs/protocolos/il2-telemetria.md` + amostras gravadas para testes.

### S6 – Injeção de teclado
- **Método:** `SendInput` com `KEYEVENTF_SCANCODE`, tap (press 30–50 ms) e hold; testar combinações com modificadores (LShift, LCtrl, RAlt).
- **Saída:** duração mínima de press, combos funcionais.

### S7 – Recenter do jogo × espaço Seated
- **Pergunta:** O "Reset VR view" do IL-2 chama o runtime ou aplica offset interno? Isso desalinha nossos hotspots?
- **Saída:** recomendação ao usuário e, se possível, detecção automática.

### S8 – Overlays sobre o jogo
- **Método:** Overlay de painel ImGui (textura D3D11), quad fino como laser, quad de highlight; avaliar legibilidade e custo de GPU.
- **Saída:** parâmetros (resolução de painel, largura do laser, ordem de sort).
