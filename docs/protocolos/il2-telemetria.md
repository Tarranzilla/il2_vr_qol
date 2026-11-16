# Protocolo de Telemetria UDP — IL-2 Sturmovik: Great Battles

Documentação do protocolo binário utilizado pela **`v0.8.2`** para acionar feedback háptico nos controladores VR do Meta Quest 3 a partir dos pacotes enviados nativamente pelo IL-2 (sem modificação de binários do jogo).

## Configuração (`data/startup.cfg`)

```ini
[KEY = motiondevice]
	addr = "127.0.0.1"
	decimation = 1
	enable = true
	port = 29373
[END]

[KEY = telemetrydevice]
	addr = "127.0.0.1"
	decimation = 1
	enable = true
	port = 29373
[END]
```

Ambas as seções podem apontar para a mesma porta UDP (`29373`), pois cada datagrama inicia com um identificador único de 32 bits (`uint32` little-endian).

---

## 1. Pacote `motiondevice` (`0x494C0100` / `1229717760`, 44 bytes)

| Offset | Tipo | Campo | Descrição |
|---|---|---|---|
| `0` | `uint32` | `packetId` | `0x494C0100` |
| `4` | `uint32` | `tick` | Tick de simulação (`50 Hz / decimation`) |
| `8..19` | `3 x float32` | `yaw, pitch, roll` | Orientação em radianos (mundo) |
| `20..31` | `3 x float32` | `spinX, spinY, spinZ` | Velocidade angular em rad/s (corpo local) |
| `32..43` | `3 x float32` | `accX, accY, accZ` | Aceleração linear em m/s² (lateral, longitudinal, vertical no corpo local) |

---

## 2. Pacote `telemetrydevice` (`0x54000101` / `1409286401`)

### Cabeçalho (11 bytes)
- `[0..3]`: `uint32 packetId = 0x54000101`
- `[4..5]`: `uint16 packetSize` (`packetSize == 12` indica simulação pausada)
- `[6..9]`: `uint32 tick`
- `[10]`: `uint8 stateCount`

### Blocos de Estado (`stateCount` entradas)
Cada entrada começa com `[uint16 stateType][uint8 floatCount]`, seguida por `floatCount * 4` bytes (`float32` little-endian):

| `stateType` | Nome | Conteúdo |
|---|---|---|
| `0` | `RPM` | RPM de cada motor (`floatCount` motores) |
| `1` | `IntakeManifoldPressurePa` | Pressão de admissão por motor (Pa) |
| `4` | `LandingGearPosition` | Posição do trem de pouso (`0..1`) |
| `5` | `LandingGearPressure` | Carga nos trens (`0..1`) |
| `6` | `IndicatedAirspeedMetresSecond` | Velocidade indicada (m/s) |
| `8` | `Acceleration` | `3 x float32` (longitudinal, vertical, lateral) |
| `9` | `StallBuffet` | `2 x float32` (`frequency`, `amplitude`) |
| `10` | `AboveGroundLevelMetres` | Altura AGL (m) |
| `11` | `FlapsPosition` | Posição dos flaps (`0..1`) |
| `12` | `AirBrakePosition` | Freio aerodinâmico (`0..1`) |

### Blocos de Eventos (`uint8 eventCount` após o último bloco de estado)
Cada evento começa com `[uint16 eventType][uint8 byteLength]`, seguido por `byteLength` bytes:

| `eventType` | Nome | Payload |
|---|---|---|
| `0` | `VehicleName` | String ASCII do modelo da aeronave |
| `4` | `BombRelease` | Lançamento de bomba |
| `5` | `RocketLaunch` | Disparo de foguete |
| `7` | `Hit` | `offset (3f) + force (3f)` (24 bytes) |
| `8` | `Damage` | `offset (3f) + intensity (1f)` (16 bytes) |
| `9` | `GunFired` | `uint8 gunIndex` (índice da arma disparada) |
