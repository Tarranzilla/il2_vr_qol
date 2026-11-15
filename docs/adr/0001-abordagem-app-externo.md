# ADR-0001 – Abordagem de aplicativo externo (sem modificar o jogo)

- **Status:** Aceito
- **Data:** 2026-11-15

## Contexto
O IL-2 GB não oferece SDK de modding para VR/input. Modificar binários ou arquivos do jogo quebra a verificação de integridade no multiplayer e é frágil a atualizações.

## Decisão
Implementar o "mod" como um **aplicativo companheiro externo** que se integra via:
- runtime VR (SteamVR: overlays, chaperone, input dos controles, háptica);
- dispositivos de entrada padrão (vJoy e teclado via `SendInput`);
- telemetria UDP oficial do jogo.

## Consequências
- ✅ Compatível com MP e atualizações; instalação/desinstalação limpa.
- ✅ Desenvolvimento e depuração fora do processo do jogo.
- ⚠️ Visualização limitada a overlays (quads 2D posicionados em 3D).
- ⚠️ Interação com cockpit depende de mapeamento manual (hotspots + binds).
