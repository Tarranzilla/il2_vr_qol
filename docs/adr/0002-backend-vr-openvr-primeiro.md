# ADR-0002 – Backend VR: OpenVR (SteamVR) primeiro

- **Status:** Aceito (validar detalhes técnicos nos Spikes S1–S3)
- **Data:** 2026-11-15

## Contexto
O Quest 3 pode rodar o IL-2 pelo runtime Oculus (Link) ou pelo SteamVR. Apenas o SteamVR permite, de forma suportada, que um processo externo: desenhe overlays, altere o espaço de rastreamento (chaperone) e leia os controles enquanto outro app está em foco.

## Opções
| Opção | Prós | Contras |
|-------|------|---------|
| **A. App de overlay OpenVR** | APIs documentadas (IVROverlay, IVRChaperoneSetup, IVRInput); ferramentas como OVR Advanced Settings provam a viabilidade | Exige jogo em modo SteamVR |
| B. OpenXR API Layer (DLL no processo do jogo) | Controle total do frame (composition layers, poses); funciona com qualquer runtime OpenXR | Só se o jogo usar OpenXR (nativo/OpenComposite); maior complexidade e risco de crash no jogo |
| C. Runtime Oculus | Nativo do Quest | Sem API para overlays/offset de terceiros |

## Decisão
Implementar **A** atrás de uma interface `IVrBackend`, mantendo **B** como evolução futura.

## Consequências
- Usuário precisa iniciar o IL-2 em modo SteamVR (documentar no setup).
- A interface `IVrBackend` deve ser desenhada sem vazar tipos OpenVR para os módulos de domínio.
