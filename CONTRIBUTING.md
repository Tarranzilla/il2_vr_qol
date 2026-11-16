# Contributing & Contributor License Agreement / Contribuições e CLA

Thank you for helping improve **IL-2 VR QoL Companion**!  
Obrigado por contribuir com o **IL-2 VR QoL Companion**!

---

## How to Contribute / Como Contribuir

1. Fork https://github.com/Tarranzilla/il2_vr_qol and create a branch for your feature, bug fix, or translation.
2. Make sure unit tests and the debug build pass cleanly before opening a Pull Request:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\tools\build_and_test.ps1
   ```
3. To add a new HUD language, duplicate [`config/locales/en-US.json`](config/locales/en-US.json) as `config/locales/<bcp47-code>.json` and ensure all keys match `pt-BR.json` and `en-US.json`.

---

## Contributor License Agreement (CLA) / Termo de Contribuição

Because **IL-2 VR QoL Companion** is distributed under a **Dual License** (GNU GPLv3 for the open-source community + paid Commercial License for proprietary integration — see [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md)), **by submitting a Pull Request, patch, or translation to this repository, you agree to the following terms:**

- **English:** You confirm that your contribution is your original work and you grant the project author (**Tarranzilla**, `joaotarran@gmail.com`) a perpetual, worldwide, non-exclusive, royalty-free, irrevocable license to use, modify, distribute, and sublicense your contribution both under the **GNU GPLv3** and as part of **Commercial / Proprietary License agreements** for this project. You retain full copyright ownership of your original contribution.
- **Português (BR):** Ao enviar um Pull Request, patch ou arquivo de tradução para este repositório, você declara que a contribuição é de sua autoria original e concede ao mantenedor e detentor dos direitos do projeto (**Tarranzilla**, `joaotarran@gmail.com`) uma licença perpétua, mundial, não exclusiva, irrevogável e livre de royalties para utilizar, modificar, distribuir e sublicenciar sua contribuição tanto sob a licença **GNU GPLv3** quanto em contratos de **Licença Comercial** deste software, mantendo você a titularidade sobre a sua contribuição original.
