# src/

Layout planejado (criado na Fase 0). Ver [docs/02-arquitetura.md](../docs/02-arquitetura.md).

```
src/
├── app/            # main.cpp, loop principal, composição dos módulos
├── core/           # event bus, logging, tempo, math helpers
├── vr/             # IVrBackend + OpenVrBackend (poses, input, chaperone, overlays, haptics)
├── seat/           # Pilar 1 – offset do assento, recenter, perfis
├── controls/       # Pilar 2 – manche/manete virtuais, curvas, filtros
├── cockpit/        # Pilar 3 – hotspots, raycaster, ações, editor
├── telemetry/      # listener UDP e parsers do IL-2
├── haptics/        # motor de háptica
├── output/         # vJoy e injeção de teclado
├── ui/             # ImGui desktop + painéis VR
├── layers/         # camadas de debug
├── config/         # load/save JSON, schema, migrações
└── hotkeys/        # atalhos globais
tests/              # Catch2 (math, curvas, raycast, parsers)
resources/          # action manifest OpenVR, bindings, ícones, .vrmanifest
```

Regra: módulos de domínio (`seat`, `controls`, `cockpit`, `haptics`) **não** incluem headers do OpenVR — apenas a interface `vr/IVrBackend.h`.
