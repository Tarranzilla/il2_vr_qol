# ADR-0003 – Stack: C++20 + Dear ImGui + CMake/vcpkg

- **Status:** Aceito
- **Data:** 2026-11-15

## Contexto
Precisamos de baixa latência no loop de input, acesso a SDKs nativos (OpenVR, vJoy, Win32) e uma UI rica em painéis de debug, idealmente com o mesmo código no desktop e no VR.

## Opções
| Opção | Prós | Contras |
|-------|------|---------|
| **C++20 + Dear ImGui** | Um só processo e linguagem; ImGui renderiza em textura para overlays VR; reaproveitável num futuro OpenXR layer | UI menos "polida" que WPF |
| C# .NET 8 (WPF/Avalonia) + C++ nativo | UI desktop bonita, produtividade | Duas linguagens, interop, UI VR teria que ser feita à parte |
| Unity/Godot overlay | Renderização 3D fácil | Pesado, concorre por GPU com o jogo |

## Decisão
**C++20 + Dear ImGui (D3D11)**, build com **CMake + vcpkg (manifest mode)**, testes com **Catch2**.

## Consequências
- Um tema ImGui customizado será criado para a UI de usuário final.
- Possível migração da UI desktop para outra tecnologia no futuro sem afetar o core.
