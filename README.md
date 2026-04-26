# superConsole IO — USB Composite HID Firmware

STM32F072RBTx 기반 USB 복합 HID 장치 펌웨어입니다.  
키보드 매트릭스, 듀얼 아날로그 조이스틱, 홀 센서 트리거를 하나의 PCB에 통합하여 단일 USB 케이블로 PC에 연결합니다.

---

## 하드웨어 사양

| 항목 | 내용 |
|---|---|
| MCU | STM32F072RBTx |
| 클럭 | 48 MHz (HSI48, USB CRS 동기화) |
| USB | Full Speed 2.0 — Composite HID |
| VID / PID | 0x0483 / 0x5758 |

### 키보드 매트릭스 (14 × 7)

- **Column** (출력, Active-HIGH): PB9·PB8·PB7·PB6·PB5·PB4·PB3 / PD2·PC12·PC11·PC10·PA15·PB11·PA6
- **Row** (입력, Pull-DOWN): PA7 · PC4 · PC5 · PB0 · PB1 · PB2 · PB10
- 다이오드 배치: **애노드 → Column, 캐소드 → 스위치 → Row** (안티 고스팅)

| | R0 | R1 | R2 | R3 | R4 | R5 | R6 |
|---|---|---|---|---|---|---|---|
| **C0** | GP L1 | Esc | ` | Tab | Caps | Shift | LCtrl |
| **C1** | GP L3 | F1 | 1 | Q | A | Z | LWin |
| **C2** | GP Left | F2 | 2 | W | S | X | LAlt |
| **C3** | GP Up | F3 | 3 | E | D | C | Space |
| **C4** | GP Down | F4 | 4 | R | F | V | RAlt |
| **C5** | GP Right | F5 | 5 | T | G | B | RWin |
| **C6** | GP Select | F6 | 6 | Y | H | N | Menu |
| **C7** | GP Start | F7 | 7 | U | J | M | RCtrl |
| **C8** | GP X | F8 | 8 | I | K | , | **Fn** |
| **C9** | GP Y | F9 | 9 | O | L | . | Left |
| **C10** | GP A | F10 | 0 | P | ; | / | Down |
| **C11** | GP B | F11 | - | [ | ' | RShift | Right |
| **C12** | GP R3 | F12 | = | ] | Enter | Up | PgUp |
| **C13** | GP R1 | Del | Backspace | \ | Home | End | PgDn |

> **Fn 조합**: `Fn + I` → PrintScreen / `Fn + O` → Scroll Lock

### 아날로그 입력

| 기능 | 핀 | 센서 |
|---|---|---|
| L-Stick X | PA1 | 가변저항 |
| L-Stick Y | PA2 | 가변저항 |
| R-Stick X | PA4 | 가변저항 |
| R-Stick Y | PA3 | 가변저항 |
| L2 Trigger | PA0 | Hall 센서 (SS49E) |
| R2 Trigger | PA5 | Hall 센서 (SS49E) |

ADC: 12비트 → 8비트 변환, 샘플링 55.5 사이클, HALEx 캘리브레이션 적용

### 모드 스위치 및 LED

| 핀 | 기능 |
|---|---|
| PF1 | 모드 스위치 (Pull-UP) — HIGH: Gamepad / LOW: Mouse |
| PC0 | 모드 상태 LED |
| PC1 | Scroll Lock LED |
| PC2 | Caps Lock LED |
| PC3 | Num Lock LED |

### 기타

| 핀 | 기능 |
|---|---|
| PB13 / PB14 | I2C2 SCL / SDA (OLED 128×32) |
| PB12 | OLED RST |

---

## USB 구조 — Composite HID (3 인터페이스)

단일 엔드포인트에서 3가지 입력을 공유하면 서로 블록이 생기는 문제를 해결하기 위해 인터페이스별 독립 엔드포인트를 사용합니다.

```
Interface 0 — Keyboard  EP1 IN  (1ms)  8 bytes  modifier + reserved + 6 keycodes
                         EP1 OUT (1ms)  1 byte   LED 상태 (NumLock / CapsLock / ScrollLock)
Interface 1 — Mouse     EP2 IN  (1ms)  4 bytes  buttons + X + Y + Wheel
Interface 2 — Gamepad   EP3 IN  (4ms)  8 bytes  16 buttons + 6 axes (LX/LY/RX/RY/L2/R2)
```

- Report ID 없음 — 인터페이스당 단일 리포트
- Configuration Descriptor: 91 bytes
- PMA 배치: EP0 OUT @ 0x40 / EP0 IN @ 0x80 / EP1 IN @ 0xC0 / EP1 OUT @ 0xD0 / EP2 IN @ 0xE0 / EP3 IN @ 0xF0

---

## 동작 모드

### Gamepad 모드 (PF1 = HIGH)
- 매트릭스의 GP 버튼 → 버튼 비트필드 (16버튼)
- L/R Stick → 절대값 축 (0–255, Y축 반전 적용)
- L2/R2 Hall 센서 → Z / Rz 축

### Mouse 모드 (PF1 = LOW)
| 입력 | 마우스 동작 |
|---|---|
| R-Stick (PA4/PA3) | 커서 이동 (데드존 ±25) |
| L-Stick Y (PA2) | 스크롤 휠 (데드존 ±25, ÷32 감속) |
| GP R1 (매트릭스) | 좌클릭 |
| R2 Hall 센서 | 우클릭 (중심 편차 >64 시 인식) |
| GP L3 (매트릭스) | 휠 클릭 |

---

## 펌웨어 동작 흐름

```
IO_Control_Process() — 메인 루프에서 블로킹 없이 반복
 ├─ [매트릭스 스캔] 14컬럼 × IDR 3회 읽기 (직접 레지스터 접근)
 │    └─ 비대칭 디바운스: Press 2ms / Release 6ms
 ├─ [키보드 리포트] 변화 감지 시 EP1 IN 즉시 전송
 └─ [ADC, 8ms 주기] Read_ADC() → Gamepad 또는 Mouse 리포트 전송
```

### 디바운스
기계식 스위치 바운싱을 처리하기 위해 비대칭 디바운스를 사용합니다.
- **Press (눌림)**: 2ms 안정 후 인식 → 빠른 응답
- **Release (놓음)**: 6ms 안정 후 인식 → 바운스로 인한 오입력 방지

---

## 빌드 환경

- **IDE**: STM32CubeIDE (또는 VS Code + ST Debug 확장)
- **HAL**: STM32F0 HAL Library (STM32CubeMX 생성)
- **Middleware**: ST USB Device Library (CustomHID 기반 수정)
- **툴체인**: arm-none-eabi-gcc

### 플래시 방법

ST-Link 또는 `.vscode/launch.json` 설정을 통해 디버그/업로드:

```
Run → Start Debugging  (F5)
```

---

## 버그 수정 및 변경 이력

### [fix] PMA BTable 충돌 → 게임패드 버튼 미작동 (`usbd_conf.c`)

**증상**: 키보드·마우스는 정상, 게임패드 버튼 입력이 전혀 없음

**원인**: STM32F0 USB 하드웨어는 PMA 오프셋 0x00부터 BTable을 배치합니다.
각 엔드포인트는 8바이트 항목을 가지며, EP3의 항목은 **PMA 0x18–0x1F**에 위치합니다.
기존 코드는 EP0 OUT 버퍼도 **PMA 0x18**에 배치하여 두 용도가 완전히 겹쳤습니다.
Windows가 열거(enumeration) 중 SET_IDLE, SET_PROTOCOL 등 SETUP 패킷을 보낼 때마다
8바이트가 PMA 0x18에 기록되어 **EP3의 TX 버퍼 주소와 전송 카운트가 파괴**되었습니다.
EP1(키보드)·EP2(마우스) BTable은 각각 0x08, 0x10에 있어 충돌 범위 밖이므로 정상 동작했습니다.

| BTable 항목 | PMA 주소 | 이전 EP0 OUT 버퍼 | 충돌 여부 |
|---|---|---|---|
| EP1 BTable | 0x08–0x0F | 시작 0x18 → 범위 밖 | 없음 ✓ |
| EP2 BTable | 0x10–0x17 | 시작 0x18 → 범위 밖 | 없음 ✓ |
| **EP3 BTable** | **0x18–0x1F** | **시작 0x18 → 완전 겹침** | **충돌 ✗** |

**수정**: 모든 PMA 버퍼를 BTable 영역(0x00–0x3F) 이후로 이동

```
이전: EP0 OUT@0x18 / EP0 IN@0x58 / EP1 IN@0x98 / EP1 OUT@0xA8 / EP2 IN@0xB8 / EP3 IN@0xC8
수정: EP0 OUT@0x40 / EP0 IN@0x80 / EP1 IN@0xC0 / EP1 OUT@0xD0 / EP2 IN@0xE0 / EP3 IN@0xF0
```

---

### [feat] 단일 엔드포인트 복합 HID → 3 인터페이스 독립 엔드포인트 (`usbd_customhid.*`, `usbd_custom_hid_if.c`)

**증상**: 키보드 입력이 지연되거나 누락됨

**원인**: 키보드·마우스·게임패드가 EP1 IN 하나를 공유하고 `state` 플래그도 하나뿐이었습니다.
마우스/게임패드 전송 중 EP1이 BUSY 상태면 키보드 리포트가 `USBD_BUSY`로 전부 버려졌습니다.

**수정**:
- Interface 0 (Keyboard): EP1 IN + EP1 OUT 독립 할당
- Interface 1 (Mouse): EP2 IN 독립 할당
- Interface 2 (Gamepad): EP3 IN 독립 할당
- `state[3]` 배열로 인터페이스별 전송 상태 분리
- Report Descriptor 3개 분리 (Report ID 없음)
- `SendReport(itf_idx, ...)` 인터페이스 인덱스 기반으로 재설계

---

### [fix] 비대칭 디바운스 도입 (`io_control.c`)

**증상**: 키보드 입력 지연

**원인**: 8ms 균일 디바운스 적용 → 모든 키 입력에 최소 8ms 지연 누적

**수정**: Press / Release 방향을 분리하여 서로 다른 임계값 적용

| 방향 | 임계값 | 목적 |
|---|---|---|
| Press (눌림) | 2ms | 빠른 응답 |
| Release (놓음) | 6ms | 반동(바운스)으로 인한 오입력 방지 |

---

### [fix] USB PID 변경 → Windows 드라이버 재열거 강제 (`usbd_desc.c`)

단일 인터페이스(구 펌웨어) → 3 인터페이스(신 펌웨어)로 구조가 바뀐 후에도
Windows가 VID/PID가 동일하면 캐시된 드라이버를 재사용하여 인터페이스 수 불일치 발생.

PID를 `0x5750` → `0x5758` (22360)으로 변경하여 Windows가 새 장치로 인식하도록 강제.

---

## Windows 최초 연결 시 주의사항

이전에 동일 VID/PID로 구 펌웨어(단일 HID 인터페이스)를 사용한 적이 있다면, Windows가 캐시된 드라이버를 사용해 정상 인식되지 않을 수 있습니다.

1. **장치 관리자** → 인체 공학적 입력 장치
2. 기존 `superConsole IO` 항목 우클릭 → **장치 제거**
3. USB 케이블 재연결

정상 인식 시 **3개**의 HID 장치가 등록됩니다:
- HID 키보드 장치
- HID 규격 마우스
- HID 규격 게임 컨트롤러
