# IL-2 VR QoL

Companion app de **Quality of Life e Interação em VR** para *IL-2 Sturmovik: Battle of Stalingrad* (série *Great Battles*), focado em Meta Quest 3.

> [!IMPORTANT]
> Este projeto **não modifica arquivos do jogo**. Ele roda como um aplicativo externo (overlay VR + dispositivos virtuais de entrada), o que o mantém compatível com atualizações e com servidores multiplayer (que verificam a integridade dos arquivos).

## Os 3 pilares

| # | Pilar | Resumo | Spec |
|---|-------|--------|------|
| 1 | **Posicionamento do Headset** | Ajustar posição/altura/orientação do "assento" VR com ajustes rápidos e finos via UI e atalhos de teclado. Perfis por aeronave. | [spec](docs/specs/pilar-1-posicionamento-headset.md) |
| 2 | **Manche e Manete Virtuais** | Controlar manche e acelerador com os controles do Quest 3 (joystick virtual via vJoy), com feedback háptico vindo da telemetria do jogo e camadas de debug ligáveis. | [spec](docs/specs/pilar-2-controles-vr.md) |
| 3 | **Interação com o Cockpit** | Apontar para painéis/alavancas com um *raycaster* e acioná-los. Hotspots calibrados por aeronave, mapeados para teclas/eixos do IL-2. | [spec](docs/specs/pilar-3-interacao-cockpit.md) |

## Documentação

| Documento | Conteúdo |
|-----------|----------|
| [01 – Visão e Escopo](docs/01-visao-e-escopo.md) | Objetivos, não-objetivos, público, premissas |
| [02 – Arquitetura](docs/02-arquitetura.md) | Módulos, fluxo de dados, stack, IPC |
| [03 – Roadmap](docs/03-roadmap.md) | Fases, marcos e backlog com checkboxes |
| [04 – Riscos e Spikes](docs/04-riscos-e-spikes.md) | Incertezas técnicas e experimentos para validá-las |
| [ADRs](docs/adr/) | Registro das decisões de arquitetura |
| [Exemplos de config](config/examples/) | Formatos de perfil, atalhos, mapeamento e hotspots |

## Estrutura do repositório

```
il2_vr_qol/
├── README.md
├── .editorconfig
├── .gitignore
├── CMakePresets.json
├── vcpkg.json
├── docs/
│   ├── 01-visao-e-escopo.md
│   ├── 02-arquitetura.md
│   ├── 03-roadmap.md
│   ├── 04-riscos-e-spikes.md
│   ├── adr/                  # Architecture Decision Records
│   └── specs/                # Especificação de cada pilar
├── config/
│   └── examples/             # JSONs de referência (perfis, atalhos, hotspots)
├── src/                      # Código (criado na Fase 0) – ver src/README.md
├── spikes/                   # Protótipos descartáveis da Fase 0
└── tools/                    # Scripts utilitários (captura de telemetria etc.)
```

## Requisitos (previstos)

- Windows 10/11, IL-2 Sturmovik: Battle of Stalingrad rodando em modo **SteamVR** (ver [ADR-0002](docs/adr/0002-backend-vr-openvr-primeiro.md))
- Meta Quest 3 via Quest Link / Air Link / Virtual Desktop / Steam Link
- [vJoy](https://github.com/BrunnerInnovation/vJoy) (driver de joystick virtual)
- Visual Studio 2022+ com C++20, CMake, vcpkg

## Status

🟡 **Fase 0 – Fundação & Spikes** (ver [roadmap](docs/03-roadmap.md)).
