# IO 모듈 테스트 보드 개발 가이드

이 파일은 **STM32F072RBTx** 기반 USB 통합 인터페이스(키보드, 마우스, 조이스틱) 프로젝트의 하드웨어 명세 및 개발 지침을 담고 있습니다.

## 1. MCU 하드웨어 기본 설정
* **MCU:** STM32F072RBTx (USB 2.0 최적화)
* **HCLK:** 48MHz
* **USB 세팅:** Custom HID (Human Interface Device) Class
    * **USB_DM:** PA11
    * **USB_DP:** PA12
* **I2C OLED (Wisevision X096-2864KSWPG01-H30, SSD1315, 128x64):**
    * **SDA:** PB9 (I2C1)
    * **SCL:** PB8 (I2C1)
    * **RST:** PB12 (GPIO Output)
    * **I2C Address:** 0x3C
    * **Update:** interrupt-based page transfer, non-blocking for HID loop
* **Battery Monitor (INA219):**
    * **SDA:** PB14 (I2C2)
    * **SCL:** PB13 (I2C2)
    * **I2C Address:** 0x40 (A0/A1 = GND)
    * **Shunt:** 0.1Ω 2512, between 2S battery + and MP2672 BATT
    * **Default Direction:** IN+ = battery +, IN- = MP2672 BATT; positive current means discharge
* **Battery Charger Presence (MP2672AGD):**
    * **SDA:** PB14 (I2C2)
    * **SCL:** PB13 (I2C2)
    * **I2C Address:** 0x4B
    * **Use:** presence check only
* **LCD/OLED Brightness PWM:**
    * **PWM:** PC6 (TIM3 CH1)
    * **Default:** 60%
    * **Range:** 20% to 100%, 20% step
    * **Mouse Mode:** GP Y increases brightness, GP A decreases brightness

## 2. 키보드 매트릭스 설정 (14 Columns x 7 Rows)
스캔 방식: Column에서 출력 신호를 보내고 Row에서 입력을 읽음.

### 핀 배정 (GPIO)
| 구분 | 핀 번호 |
| :--- | :--- |
| **Columns (00-06)** | PC14, PC13, PB7, PB6, PB5, PB4, PB3 |
| **Columns (07-13)** | PD2, PC12, PC11, PC10, PA15, PB11, PA6 |
| **Rows (00-06)** | PA7, PC4, PC5, PB0, PB1, PB2, PB10 |

### 키 맵핑 (C: Column, R: Row)
| | R0 | R1 | R2 | R3 | R4 | R5 | R6 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
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
| **C11** | GP B | F11 | - | [ | ‘ | RShift | Right |
| **C12** | GP R2 | F12 | = | ] | Enter | Up | - |
| **C13** | GP R1 | GP L3 | Backspace | \ | GP R3 | End | - |

**특수 키 조합:**
- `Fn` + `I` : PrintScreen
- `Fn` + `O` : Scroll Lock
- `Fn` + `Backspace` : Delete

## 3. 아날로그 입력 (조이스틱 & 트리거)
| 기능 | 핀 번호 | 센서 방식 |
| :--- | :--- | :--- |
| Gamepad L-Joy X | PA1 | 가변저항 |
| Gamepad L-Joy Y | PA2 | 가변저항 |
| Gamepad R-Joy X | PA4 | 가변저항 |
| Gamepad R-Joy Y | PA3 | 가변저항 |
| Gamepad L2 Trigger | C1/R0 | 매트릭스 디지털 버튼 |
| Gamepad R2 Trigger | C12/R0 | 매트릭스 디지털 버튼 |

## 4. 모드 스위치 및 LED
* **모드 스위치 (PF1):** 토글 방식 (Internal Pull-up 사용).
    * **Open (High):** Gamepad 모드 (조이스틱이 게임패드로 동작)
    * **GND (Low):** Mouse 모드 (L-Joy: 스크롤, R-Joy: 이동 / GP R2: 우클릭 / GP Y/A: 밝기 조절)
* **상태 LED:**
    * NumLock: PC3
    * CapsLock: PC2
    * ScrollLock: PC1
    * Mode Status: PC0

---
**Gemini CLI 지시문:** 위 명세를 바탕으로 STM32 HAL 라이브러리를 사용하여 USB 복합 장치(Keyboard + Mouse + Gamepad)의 디스크립터 및 스캔 로직을 생성하시오.
