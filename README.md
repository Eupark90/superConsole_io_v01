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

## Windows 최초 연결 시 주의사항

이전에 동일 VID/PID로 구 펌웨어(단일 HID 인터페이스)를 사용한 적이 있다면, Windows가 캐시된 드라이버를 사용해 정상 인식되지 않을 수 있습니다.

1. **장치 관리자** → 인체 공학적 입력 장치
2. 기존 `superConsole IO` 항목 우클릭 → **장치 제거**
3. USB 케이블 재연결

정상 인식 시 **3개**의 HID 장치가 등록됩니다:
- HID 키보드 장치
- HID 규격 마우스
- HID 규격 게임 컨트롤러
