# Pilar 1 – Posicionamento do Headset

## Objetivo
Permitir que o usuário, sentado na cadeira, alinhe **posição (X/Y/Z)** e **orientação (yaw)** da cabeça virtual em relação ao assento do cockpit, com ajustes práticos (um clique) e finos (milímetros/décimos de grau), por UI desktop, painel VR e atalhos de teclado.

## Como funciona
O Companion altera o **Seated Zero Pose** do SteamVR (a origem do espaço sentado). Como o IL-2 posiciona a câmera relativa a essa origem, mover a origem move o piloto dentro do cockpit.

```
SeatPose_final = RecenterBase ∘ Offset(x, y, z, yaw)
```
- **RecenterBase:** capturado no "Recentralizar" a partir da pose atual do HMD (apenas posição + yaw; pitch/roll descartados).
- **Offset:** ajustes do usuário, salvos em perfil.

## Funcionalidades

| ID | Funcionalidade | Prioridade |
|----|----------------|:----------:|
| P1.1 | Recentralizar em um clique/atalho (com contagem regressiva opcional de 3 s para o usuário se posicionar) | Must |
| P1.2 | Nudge X/Y/Z e yaw: passo **grosso** (padrão 2 cm / 5°) e **fino** (padrão 2 mm / 0,5°), configuráveis | Must |
| P1.3 | Atalhos globais funcionando com o jogo em foco | Must |
| P1.4 | Painel desktop com sliders, campos numéricos, botões ± e "Reset" | Must |
| P1.5 | Restaurar o chaperone original ao sair (e no próximo start, se houve crash) | Must |
| P1.6 | Perfis: default + por aeronave; troca rápida | Should |
| P1.7 | Toast no VR mostrando o ajuste aplicado | Should |
| P1.8 | Painel VR com os mesmos controles (operado pelo laser) | Should |
| P1.9 | "Arrastar o mundo": segurar os dois grips e mover/girar o espaço | Could |
| P1.10 | Ajuste de altura "olhar para o horizonte": nivelar com referência de altura de olhos | Could |

## Atalhos padrão (configuráveis)

Modificador base: **`Ctrl + Alt`** (adicione **`Shift`** para passo fino).

| Ação | Tecla |
|------|-------|
| Recentralizar | `Numpad 5` |
| Frente / Trás | `Numpad 8` / `Numpad 2` |
| Esquerda / Direita | `Numpad 4` / `Numpad 6` |
| Subir / Descer | `Numpad 9` / `Numpad 3` |
| Girar yaw esq./dir. | `Numpad 7` / `Numpad 1` |
| Salvar perfil atual | `Numpad 0` |
| Zerar offset | `Numpad .` |
| Mostrar/ocultar painel VR | `Numpad +` |

## UI (desktop) – wireframe

```
┌─ Assento ───────────────────────────────────────────┐
│ Perfil: [ Default        ▼]  [Salvar] [Salvar como] │
│                                                     │
│ [  RECENTRALIZAR (Ctrl+Alt+Num5)  ]  ☐ contagem 3s  │
│                                                     │
│ Passo:  (•) Grosso  ( ) Fino      Grosso: [2.0] cm  │
│                                   Fino:   [2.0] mm  │
│ Frente/Trás  [−] ──────●────── [+]   +1.20 cm       │
│ Esq./Dir.    [−] ───●───────── [+]   −0.40 cm       │
│ Altura       [−] ────────●──── [+]   +3.10 cm       │
│ Yaw          [−] ─────●─────── [+]   −1.5 °         │
│                                                     │
│ [Zerar offset]  [Restaurar chaperone original]      │
└─────────────────────────────────────────────────────┘
```

## Critérios de aceite
- Com o IL-2 em voo, cada atalho altera a vista em < 100 ms.
- Precisão: passo fino aplica exatamente o valor configurado (verificado no log da pose).
- Fechar o app (ou matar o processo e reabrir) restaura o chaperone original.
- Perfil salvo é restaurado automaticamente ao iniciar.

## Questões em aberto
- Usar universo *Seated* ou *Standing*? (depende do S2)
- Interação com o "Reset VR view" do jogo (S7).
