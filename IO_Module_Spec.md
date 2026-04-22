# IO 모듈 테스트 보드 개발 가이드

이 파일은 **STM32F072RBTx** 기반 USB 통합 인터페이스(키보드, 마우스, 조이스틱) 프로젝트의 하드웨어 명세 및 개발 지침을 담고 있습니다.

## 1. MCU 하드웨어 기본 설정
* **MCU:** STM32F072RBTx (USB 2.0 최적화)
* **HCLK:** 48MHz
* **USB 세팅:** Custom HID (Human Interface Device) Class
    * **USB_DM:** PA11
    * **USB_DP:** PA12
* **I2C OLED (Adafruit 128x32):**
    * **SDA:** PB14
    * **SCL:** PB13
    * **RST:** PB12 (GPIO Output)

## 2. 키보드 매트릭스 설정 (14 Columns x 7 Rows)
스캔 방식: Column에서 출력 신호를 보내고 Row에서 입력을 읽음.

### 핀 배정 (GPIO)
| 구분 | 핀 번호 |
| :--- | :--- |
| **Columns (00-06)** | PB9, PB8, PB7, PB6, PB5, PB4, PB3 |
| **Columns (07-13)** | PD2, PC12, PC11, PC10, PA15, PB11, PA6 |
| **Rows (00-06)** | PA7, PC4, PC5, PB0, PB1, PB2, PB10 |

### 키 맵핑 (C: Column, R: Row)
| | R0 | R1 | R2 | R3 | R4 | R5 | R6 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
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
| **C11** | GP B | F11 | - | [ | ‘ | RShift | Right |
| **C12** | GP R3 | F12 | = | ] | Enter | Up | PgUp |
| **C13** | GP R1 | Delete | Backspace | \ | Home | End | PgDn |

**특수 키 조합:**
- `Fn` + `I` : PrintScreen
- `Fn` + `O` : Scroll Lock

## 3. 아날로그 입력 (조이스틱 & 트리거)
| 기능 | 핀 번호 | 센서 방식 |
| :--- | :--- | :--- |
| Gamepad L-Joy X | PA1 | 가변저항 |
| Gamepad L-Joy Y | PA2 | 가변저항 |
| Gamepad R-Joy X | PA4 | 가변저항 |
| Gamepad R-Joy Y | PA3 | 가변저항 |
| Gamepad L2 Trigger | PA0 | 홀센서 (SS49E) |
| Gamepad R2 Trigger | PA5 | 홀센서 (SS49E) |

## 4. 모드 스위치 및 LED
* **모드 스위치 (PF1):** 토글 방식 (Internal Pull-up 사용).
    * **Open (High):** Gamepad 모드 (조이스틱이 게임패드로 동작)
    * **GND (Low):** Mouse 모드 (L-Joy: 스크롤, R-Joy: 이동 / 트리거 깊이에 따른 가속도 적용)
* **상태 LED:**
    * NumLock: PC3
    * CapsLock: PC2
    * ScrollLock: PC1
    * Mode Status: PC0

---
**Gemini CLI 지시문:** 위 명세를 바탕으로 STM32 HAL 라이브러리를 사용하여 USB 복합 장치(Keyboard + Mouse + Gamepad)의 디스크립터 및 스캔 로직을 생성하시오.
