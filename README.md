# superConsole IO — USB Composite HID Firmware

STM32F072RBTx 기반 USB 복합 HID 장치 펌웨어입니다.  
키보드 매트릭스, 듀얼 아날로그 조이스틱, 디지털 게임패드 버튼을 하나의 PCB에 통합하여 단일 USB 케이블로 PC에 연결합니다.

---

## 하드웨어 사양

| 항목 | 내용 |
|---|---|
| MCU | STM32F072RBTx |
| 클럭 | 48 MHz (HSI48, USB CRS 동기화) |
| USB | Full Speed 2.0 — Composite HID |
| VID / PID | 0x0483 / 0x5758 |

### 키보드 매트릭스 (14 × 7)

- **Column** (출력, Active-HIGH): PC14·PC13·PB7·PB6·PB5·PB4·PB3 / PD2·PC12·PC11·PC10·PA15·PB11·PA6
- **Row** (입력, Pull-DOWN): PA7 · PC4 · PC5 · PB0 · PB1 · PB2 · PB10
- 다이오드 배치: **애노드 → Column, 캐소드 → 스위치 → Row** (안티 고스팅)

#### 현재 Column / Row 핀 정의

| 인덱스 | Column 핀 | 인덱스 | Row 핀 |
|---|---|---|---|
| C0 | PC14 | R0 | PA7 |
| C1 | PC13 | R1 | PC4 |
| C2 | PB7 | R2 | PC5 |
| C3 | PB6 | R3 | PB0 |
| C4 | PB5 | R4 | PB1 |
| C5 | PB4 | R5 | PB2 |
| C6 | PB3 | R6 | PB10 |
| C7 | PD2 |  |  |
| C8 | PC12 |  |  |
| C9 | PC11 |  |  |
| C10 | PC10 |  |  |
| C11 | PA15 |  |  |
| C12 | PB11 |  |  |
| C13 | PA6 |  |  |

| | R0 | R1 | R2 | R3 | R4 | R5 | R6 |
|---|---|---|---|---|---|---|---|
| **C0** | GP L1 | Esc | ` | Tab | Caps | Shift | LCtrl |
| **C1** | GP L2 | F1 | 1 | Q | A | Z | LWin |
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
| **C12** | GP R2 | F12 | = | ] | Enter | Up | - |
| **C13** | GP R1 | GP L3 | Backspace | \ | GP R3 | End | - |

> **Fn 조합**: `Fn + I` → PrintScreen / `Fn + O` → Scroll Lock / `Fn + Backspace` → Delete

### 아날로그 입력

| 기능 | 핀 | 센서 |
|---|---|---|
| L-Stick X | PA1 | 가변저항 |
| L-Stick Y | PA2 | 가변저항 |
| R-Stick X | PA4 | 가변저항 |
| R-Stick Y | PA3 | 가변저항 |
| L2 Trigger | C1/R0 | 매트릭스 디지털 버튼 |
| R2 Trigger | C12/R0 | 매트릭스 디지털 버튼 |

ADC: 12비트 → 8비트 변환, 샘플링 55.5 사이클, HALEx 캘리브레이션 적용. PA0/PA5 기반 Hall 트리거 입력은 비활성화.

### 모드 스위치 및 LED

| 핀 | 기능 |
|---|---|
| PF1 | 모드 스위치 (Pull-UP) — HIGH: Gamepad / LOW: Mouse |
| PC0 | 모드 상태 LED |
| PC1 | Scroll Lock LED |
| PC2 | Caps Lock LED |
| PC3 | Num Lock LED |
| PC6 | LCD/OLED 밝기 PWM (TIM3 CH1, 기본 60%) |

### 기타

| 핀 | 기능 |
|---|---|
| PB13 / PB14 | I2C2 SCL / SDA (INA219 전압/전류 모니터 + MP2672AGD presence 확인) |
| PB8 / PB9 | I2C1 SCL / SDA (Wisevision X096-2864KSWPG01-H30 OLED) |
| PB12 | OLED RST |

### 배터리 전압/전류 모니터 (INA219)

| 항목 | 내용 |
|---|---|
| 연결 | I2C2 (`PB13=SCL`, `PB14=SDA`) |
| I2C 주소 | `0x40` (A0/A1 = GND 기준) |
| 션트 | 0.1Ω 2512 |
| 측정 위치 | 2S 배터리 `+` ↔ 0.1Ω 션트 ↔ MP2672 `BATT` |
| 폴링 주기 | 1000ms |

- 기본 배선 가정: `INA219 IN+ = 2S 배터리 +`, `INA219 IN- = MP2672 BATT`.
- 위 배선에서 `current_ua/current_ma`가 양수이면 배터리에서 시스템 쪽으로 흐르는 방전 전류, 음수이면 MP2672에서 배터리로 들어가는 충전 전류입니다.
- INA219 bus voltage는 `IN-` 기준 전압이므로, 기본 배선에서는 `battery_voltage_mv = bus_voltage_mv + shunt_voltage_uv / 1000`으로 보정합니다.
- 반대로 배선했다면 `battery_monitor.c`의 `INA219_IN_PLUS_AT_BATTERY_POSITIVE` 값을 `0`으로 바꾸면 전류 부호와 배터리 전압 보정이 맞춰집니다.
- INA219 calibration은 `Rshunt=0.1Ω`, `current_lsb=100uA`, `calibration=4096` 기준입니다.
- 읽는 값: bus voltage, shunt voltage, current, power.
- 잔여량 `percent`는 INA219 배터리 전압 기반의 대략 추정값입니다. 정확한 SOC가 필요하면 fuel gauge IC가 필요합니다.
- INA219가 장착되지 않은 테스트 보드에서는 `ina219_online=0`으로 두고 10초마다만 재탐색합니다. 이 경우 HID 입력 처리는 계속 동작하며, 전압/전류 값은 `0`, 잔량은 `unknown(0xFF)`입니다.
- 같은 I2C2 버스의 MP2672AGD는 주소 `0x4B`에 대해 연결 여부만 확인합니다. 충전 상태/전압 측정은 하지 않습니다.
- 부팅 후 메인 루프 첫 회전에서 INA219/MP2672AGD presence를 확인하고, 이후 미응답 소자는 10초마다 재탐색합니다.

### OLED 표시 장치 (Wisevision X096-2864KSWPG01-H30)

| 항목 | 내용 |
|---|---|
| 연결 | I2C1 (`PB8=SCL`, `PB9=SDA`) |
| Reset | `PB12` GPIO |
| 해상도 | 128×64 |
| 컨트롤러 | SSD1315 |
| I2C 주소 | `0x3C` |

- INA219에서 읽은 배터리 전압, 전류, 전력, 전압 기반 잔량 추정치와 현재 밝기 퍼센트를 표시합니다.
- OLED 초기화와 화면 전송은 `HAL_I2C_Master_Transmit_IT()` 기반 interrupt 전송만 사용합니다.
- 메인 루프에서는 HID 처리 이후 `OLED_Display_Process()`가 짧게 상태만 진행합니다.
- 화면은 500ms마다 렌더링하고, 실제 I2C 전송은 128바이트 page 단위로 20ms 이상 간격을 두고 나누어 보냅니다.
- OLED가 없거나 응답하지 않으면 `offline`으로 전환하고 10초 뒤 reset/init을 다시 시도합니다.
- USB interrupt 우선순위가 I2C1보다 높으므로 OLED 갱신은 HID 입력 전송을 막지 않습니다.

### LCD/OLED 밝기 PWM

| 항목 | 내용 |
|---|---|
| 핀 | `PC6` |
| 타이머 | `TIM3 CH1` (`GPIO_AF0_TIM3`) |
| PWM 주파수 | 1kHz |
| 시작 밝기 | 60% |
| 조절 범위 | 20%–100%, 20% 단위 |

- `TIM3`를 직접 레지스터 설정으로 구동합니다.
- STM32F0의 `PC6 -> TIM3_CH1` alternate function은 `GPIO_AF0_TIM3`입니다.
- 마우스 모드에서 `GP Y` 버튼을 누르면 밝기가 20% 증가합니다.
- 마우스 모드에서 `GP A` 버튼을 누르면 밝기가 20% 감소합니다.
- 버튼을 누른 순간만 처리하므로 누르고 있는 동안 반복 증감하지 않습니다.

### 현재 핀 사용 요약

| 기능 | 핀 |
|---|---|
| USB FS | PA11 DM / PA12 DP |
| Column 출력 | PC14, PC13, PB7, PB6, PB5, PB4, PB3, PD2, PC12, PC11, PC10, PA15, PB11, PA6 |
| Row 입력 | PA7, PC4, PC5, PB0, PB1, PB2, PB10 |
| Stick ADC | PA1, PA2, PA4, PA3 |
| Mode switch | PF1 |
| LED | PC0, PC1, PC2, PC3 |
| OLED I2C1 | PB8 SCL / PB9 SDA |
| OLED reset | PB12 |
| INA219 / MP2672 presence I2C2 | PB13 SCL / PB14 SDA |
| Brightness PWM | PC6 / TIM3 CH1 |

### 핀아웃 점검 결과

- 현재 코드와 `.ioc` 기준으로 기능 간 중복 할당된 핀은 없습니다.
- `PC6` 백라이트 PWM은 STM32F0 기준 `TIM3_CH1 / GPIO_AF0_TIM3`로 설정합니다.
- `PB3`는 `Column_06` GPIO 출력입니다. 이전 라벨 오타 `Cplumn_06`은 수정했습니다.
- `PA0/PA5`는 기존 Hall 트리거 입력 자리였지만 현재는 사용하지 않으며 `GPIO_Analog`로 남겨 둡니다.
- `PA13/PA14`는 SWD 디버그용으로 유지합니다.
- `PC13/PC14` Column은 GPIO settle 특성을 고려해 스캔 시 약 2us settle delay를 둡니다.

---

## USB 구조 — Composite HID (3 인터페이스)

단일 엔드포인트에서 3가지 입력을 공유하면 서로 블록이 생기는 문제를 해결하기 위해 인터페이스별 독립 엔드포인트를 사용합니다.

```
Interface 0 — Keyboard  EP1 IN  (1ms)  8 bytes  modifier + reserved + 6 keycodes
                         EP1 OUT (1ms)  1 byte   LED 상태 (NumLock / CapsLock / ScrollLock)
Interface 1 — Mouse     EP2 IN  (1ms)  4 bytes  buttons + X + Y + Wheel
Interface 2 — Gamepad   EP3 IN  (4ms)  8 bytes  16 buttons + 6 axes (LX/LY/RX/RY, L2/R2 axes reserved)
```

- Report ID 없음 — 인터페이스당 단일 리포트
- Configuration Descriptor: 91 bytes
- PMA 배치: EP0 OUT @ 0x40 / EP0 IN @ 0x80 / EP1 IN @ 0xC0 / EP1 OUT @ 0xD0 / EP2 IN @ 0xE0 / EP3 IN @ 0xF0

---

## 동작 모드

### Gamepad 모드 (PF1 = HIGH)
- 매트릭스의 GP 버튼 → 버튼 비트필드 (16버튼)
- L/R Stick → 절대값 축 (0–255, Y축 반전 적용)
- L2/R2 매트릭스 입력 → 버튼 비트필드

### Mouse 모드 (PF1 = LOW)
| 입력 | 마우스 동작 |
|---|---|
| R-Stick (PA4/PA3) | 커서 이동 (데드존 ±25) |
| L-Stick Y (PA2) | 스크롤 휠 (데드존 ±25, ÷32 감속) |
| GP R1 (매트릭스) | 좌클릭 |
| GP R2 (매트릭스) | 우클릭 |
| GP L3 (매트릭스) | 휠 클릭 |
| GP Y (매트릭스) | 밝기 +20% |
| GP A (매트릭스) | 밝기 -20% |

---

## 펌웨어 동작 흐름

```
IO_Control_Process() — 메인 루프에서 블로킹 없이 반복
 ├─ [매트릭스 스캔] 14컬럼 × IDR 3회 읽기 (직접 레지스터 접근)
 │    └─ 비대칭 디바운스: Press 2ms / Release 6ms
 ├─ [키보드 리포트] 변화 감지 시 EP1 IN 즉시 전송
 ├─ [ADC, 8ms 주기] Read_ADC() 4채널 → Gamepad 또는 Mouse 리포트 전송
 ├─ [배터리, 1000ms 주기] INA219 전압/전류 폴링 → BatteryStatus_t 갱신
 └─ [OLED, 비차단] BatteryStatus_t를 page 단위 interrupt I2C 전송으로 표시
```

### 디바운스
기계식 스위치 바운싱을 처리하기 위해 비대칭 디바운스를 사용합니다.
- **Press (눌림)**: 2ms 안정 후 인식 → 빠른 응답
- **Release (놓음)**: 6ms 안정 후 인식 → 바운스로 인한 오입력 방지

### 매트릭스 스캔 안정화

- 각 컬럼 스캔 전후에 모든 Column 출력을 LOW로 정리합니다.
- 대상 Column만 HIGH로 올린 뒤 약 2us settle delay를 둡니다.
- 특히 `PC13/PC14`는 저속 도메인 특성이 있어 settle 시간을 두지 않으면 인접 컬럼 오검출이 발생할 수 있습니다.
- Column GPIO 출력 속도는 High speed로 설정했습니다.

---

## 빌드 환경

- **IDE**: STM32CubeIDE (또는 VS Code + ST Debug 확장)
- **HAL**: STM32F0 HAL Library (STM32CubeMX 생성)
- **Middleware**: ST USB Device Library (CustomHID 기반 수정)
- **툴체인**: arm-none-eabi-gcc

### 플래시 방법

ST-Link 연결 후 VS Code 태스크 또는 디버그 설정을 사용합니다.

```
Terminal → Run Task → Upload Debug
```

현재 `Upload Debug` 태스크는 macOS Homebrew OpenOCD 경로를 기준으로 설정되어 있습니다.

```
/opt/homebrew/bin/openocd
  -f interface/stlink.cfg
  -f target/stm32f0x.cfg
  -c "program build/Debug/superConsoleV01_IO.elf verify reset exit"
```

디버그 실행은 `.vscode/launch.json`의 ST-Link 설정을 사용합니다. 보드 전원이 들어오지 않았거나 ST-Link `VTref`가 0V에 가까우면 업로드가 실패합니다.

---

## 버그 수정 및 변경 이력

### [hw] 입력 보드 리비전 반영 — Column/I2C/트리거/키맵 변경 (`main.*`, `stm32f0xx_hal_msp.c`, `io_control.c`)

**Column 핀 변경**
- `Column0`: `PB9` → `PC14`
- `Column1`: `PB8` → `PC13`
- `PB8/PB9`는 Column에서 해제하고 OLED용 `I2C1 SCL/SDA`로 설정
- 현재 전체 Column 순서: `C0=PC14`, `C1=PC13`, `C2=PB7`, `C3=PB6`, `C4=PB5`, `C5=PB4`, `C6=PB3`, `C7=PD2`, `C8=PC12`, `C9=PC11`, `C10=PC10`, `C11=PA15`, `C12=PB11`, `C13=PA6`

**I2C 용도 분리**
- `I2C1`: `PB8=SCL`, `PB9=SDA` — Wisevision 128×64 OLED
- `I2C2`: `PB13=SCL`, `PB14=SDA` — INA219 전압/전류 모니터 + MP2672AGD presence 확인
- `PB12`는 OLED RST GPIO로 유지
- `PC6`은 LCD/OLED 밝기 PWM으로 사용

**L2/R2 트리거 변경**
- 기존 `PA0/PA5` Hall 센서 기반 ADC 트리거는 비활성화
- ADC는 스틱용 4채널만 사용: `PA1=LX`, `PA2=LY`, `PA4=RX`, `PA3=RY`
- `L2`: 매트릭스 `C1/R0`
- `R2`: 매트릭스 `C12/R0`
- USB Gamepad 리포트 구조의 L2/R2 축 바이트는 호환성을 위해 유지하지만 현재 값은 `0`으로 고정

**게임패드 버튼 위치 변경**
- `GP R1`: `C13/R0`
- `GP L3`: `C13/R1`
- `GP R3`: `C13/R4`
- `GP R2`: `C12/R0`
- Mouse 모드에서 `GP R1=좌클릭`, `GP R2=우클릭`, `GP L3=휠 클릭`

**키보드 키맵 변경**
- 일반 `Del`, `PgUp`, `PgDn` 키 제거
- `Fn + Backspace`를 `Delete`로 할당
- 기존 `Fn + I=PrintScreen`, `Fn + O=Scroll Lock` 유지

---

### [fix] Column 스캔 안정화 (`io_control.c`, `main.c`)

**증상**: Column 변경 후 일부 키가 인접 Column/Row로 잘못 감지되거나 입력이 불안정할 수 있음.

**원인**: `PC13/PC14` Column이 포함되면서 Column 전환 직후 Row 입력을 바로 읽으면 GPIO settle 이전 상태가 섞일 수 있음.

**수정**
- 스캔 시작/종료 및 각 Column 전환 전에 모든 Column을 LOW로 정리
- 대상 Column을 HIGH로 올린 후 약 2us settle delay 적용
- Row IDR을 3회 읽고 OR 처리하여 짧은 글리치를 완화
- Column GPIO 출력 속도를 High speed로 설정

---

### [chore] VS Code ST-Link 업로드 설정 정리 (`.vscode/tasks.json`, `.vscode/launch.json`)

- `Upload Debug` 태스크를 OpenOCD 기반으로 변경
- 사용 경로: `/opt/homebrew/bin/openocd`
- 타겟 설정: `interface/stlink.cfg`, `target/stm32f0x.cfg`
- 동작: `build/Debug/superConsoleV01_IO.elf`를 `program → verify → reset → exit` 순서로 업로드
- ST-Link가 타겟 전압을 감지하지 못하면 펌웨어 설정과 무관하게 업로드가 실패함

---

### [feat] INA219 배터리 전압/전류 모니터링 추가 (`battery_monitor.*`)

**구성**
- `I2C2`의 `PB13/PB14`에 연결된 INA219 `0x40`을 1초마다 폴링
- 같은 I2C2 버스의 MP2672AGD `0x4B`는 presence만 확인
- INA219는 0.1Ω 션트, calibration `4096`, current LSB `100uA`, power LSB `2mW` 기준
- 기본 배선은 `IN+=배터리 +`, `IN-=MP2672 BATT`; 양수 전류는 방전, 음수 전류는 충전

**상태 저장**
- `BatteryMonitor_Process()`가 메인 루프에서 주기적으로 상태를 갱신
- `BatteryMonitor_GetStatus()`로 최신 `BatteryStatus_t` 조회 가능
- INA219 read 실패는 `ina219_online`, `ina219_error_count`, `read_error_count`에 반영
- INA219 미장착 보드에서도 HID 입력 지연이 생기지 않도록 재탐색은 10초 간격으로 제한
- MP2672AGD 미응답도 동일하게 10초 간격으로만 재확인

**잔여량 제한**
- INA219 배터리 전압 기반 lookup table로 `percent`를 대략 추정
- 전압 기반 추정은 부하/충전 상태에 영향을 받으므로 정확한 SOC가 아님
- 정확한 SOC/잔여량 %가 필요하면 전류 적산 fuel gauge IC가 필요

---

### [feat] Wisevision SSD1315 OLED 상태 표시 추가 (`oled_display.*`)

**구성**
- `I2C1`의 `PB8/PB9`에 Wisevision X096-2864KSWPG01-H30 OLED 연결
- `PB12`를 OLED reset GPIO로 사용
- SSD1315/SSD1306 호환 초기화 시퀀스 사용
- 기본 I2C 주소는 `0x3C`

**표시 내용**
- INA219 online 상태
- 배터리 전압 `battery_voltage_mv`
- 배터리 전류 `current_ma`
- 전력 `power_mw`
- 전압 기반 잔량 추정 `percent`
- 현재 밝기 `Backlight_GetPercent()`

**HID 지연 방지**
- OLED reset은 `HAL_Delay()` 없이 tick 기반 상태 머신으로 처리
- OLED I2C 전송은 blocking API를 사용하지 않고 `HAL_I2C_Master_Transmit_IT()`만 사용
- 128바이트 page 단위로 나누어 20ms 이상 간격으로 전송
- I2C1 interrupt priority는 USB보다 낮게 설정
- OLED 미장착/오류 시 10초마다만 재시도

---

### [feat] PC6 PWM 밝기 제어 추가 (`backlight_control.*`, `io_control.c`)

**구성**
- `PC6`을 `TIM3 CH1` PWM 출력으로 사용
- STM32F0의 `PC6 -> TIM3_CH1` alternate function은 `GPIO_AF0_TIM3`
- PWM 주파수는 1kHz
- 기본 시작 밝기는 60%
- 최소 20%, 최대 100%, 20% 단위 조절

**마우스 모드 조작**
- `GP Y`: 밝기 20% 증가
- `GP A`: 밝기 20% 감소
- rising edge만 처리하여 버튼을 길게 눌러도 한 단계만 변경

---

### [fix] 핀아웃 설정 점검 및 정리 (`main.*`, `backlight_control.c`, `superConsoleV01_IO.ioc`)

- `PC6` 백라이트 PWM의 alternate function을 `GPIO_AF1_TIM3`에서 `GPIO_AF0_TIM3`로 수정
- `TIM3` 레지스터 초기화 순서를 명확히 정리
- `.ioc`에 `PC6=S_TIM3_CH1`, `TIM3 CH1 PWM` 설정 반영
- `PB3` 라벨 오타 `Cplumn_06`을 `Column_06`으로 수정
- 전체 핀아웃 점검 결과 기능 간 직접 충돌 없음 확인

---

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
