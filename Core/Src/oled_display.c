#include "oled_display.h"

#include "battery_monitor.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

#define OLED_I2C_ADDR_7BIT       0x3CU
#define OLED_I2C_ADDR_HAL        (OLED_I2C_ADDR_7BIT << 1)
#define OLED_WIDTH               128U
#define OLED_HEIGHT              64U
#define OLED_PAGES               8U
#define OLED_PAGE_PACKET_SIZE    (1U + OLED_WIDTH)
#define OLED_FRAMEBUFFER_SIZE    (OLED_WIDTH * OLED_PAGES)
#define OLED_RETRY_INTERVAL_MS   10000U
#define OLED_RENDER_INTERVAL_MS  500U
#define OLED_PAGE_INTERVAL_MS    20U
#define OLED_CONTROL_OVERLAY_MS  1600U

typedef enum {
    OLED_STATE_RESET_LOW = 0,
    OLED_STATE_RESET_HIGH_WAIT,
    OLED_STATE_INIT,
    OLED_STATE_IDLE,
    OLED_STATE_SEND_PAGE,
    OLED_STATE_OFFLINE,
} OLED_State_t;

typedef enum {
    OLED_OVERLAY_NONE = 0,
    OLED_OVERLAY_BACKLIGHT,
    OLED_OVERLAY_MOUSE_SENSITIVITY,
} OLED_Overlay_t;

static OLED_State_t oled_state;
static OLED_Overlay_t oled_overlay;
static uint8_t oled_online;
static uint8_t oled_tx_busy;
static uint8_t oled_dirty;
static uint8_t oled_render_requested;
static uint8_t oled_overlay_value;
static uint8_t oled_init_index;
static uint8_t oled_page_index;
static uint8_t oled_page_phase;
static uint8_t oled_page_packet[OLED_PAGE_PACKET_SIZE];
static uint8_t framebuffer[OLED_FRAMEBUFFER_SIZE];
static uint32_t state_time_ms;
static uint32_t next_retry_ms;
static uint32_t last_render_ms;
static uint32_t last_page_ms;
static uint32_t overlay_until_ms;

static const uint8_t oled_init_cmds[] = {
    0xAE,       /* display off */
    0x20, 0x00, /* horizontal addressing */
    0xB0,
    0xC8,       /* COM scan direction remapped */
    0x00,
    0x10,
    0x40,
    0x81, 0x7F,
    0xA1,       /* segment remap */
    0xA6,
    0xA8, 0x3F,
    0xA4,
    0xD3, 0x00,
    0xD5, 0x80,
    0xD9, 0xF1,
    0xDA, 0x12,
    0xDB, 0x40,
    0x8D, 0x14, /* charge pump on */
    0xAF        /* display on */
};

static uint8_t Font5x7(char ch, uint8_t col)
{
    static const uint8_t digits[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
        {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    };
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
        {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
    };

    if (col >= 5U) return 0x00U;
    if (ch >= '0' && ch <= '9') return digits[ch - '0'][col];
    if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'][col];
    if (ch >= 'a' && ch <= 'z') return letters[ch - 'a'][col];

    switch (ch) {
        case ':': return (col == 1U || col == 3U) ? 0x36U : 0x00U;
        case '.': return (col == 2U) ? 0x40U : 0x00U;
        case '-': return (col >= 1U && col <= 3U) ? 0x08U : 0x00U;
        case '+': return (col == 2U) ? 0x3EU : ((col >= 1U && col <= 3U) ? 0x08U : 0x00U);
        case '%': return (uint8_t[]){0x23,0x13,0x08,0x64,0x62}[col];
        case '/': return (uint8_t[]){0x20,0x10,0x08,0x04,0x02}[col];
        default: return 0x00U;
    }
}

static void DrawPixel(uint8_t x, uint8_t y)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    framebuffer[((uint16_t)(y / 8U) * OLED_WIDTH) + x] |= (uint8_t)(1U << (y & 7U));
}

static void DrawChar2x(uint8_t x, uint8_t y, char ch)
{
    for (uint8_t col = 0U; col < 5U; col++) {
        uint8_t bits = Font5x7(ch, col);
        for (uint8_t row = 0U; row < 7U; row++) {
            if (bits & (1U << row)) {
                uint8_t px = (uint8_t)(x + (col * 2U));
                uint8_t py = (uint8_t)(y + (row * 2U));
                DrawPixel(px, py);
                DrawPixel((uint8_t)(px + 1U), py);
                DrawPixel(px, (uint8_t)(py + 1U));
                DrawPixel((uint8_t)(px + 1U), (uint8_t)(py + 1U));
            }
        }
    }
}

static void DrawText2x(uint8_t x, uint8_t y, const char *text)
{
    while (*text && x < OLED_WIDTH) {
        DrawChar2x(x, y, *text++);
        x = (uint8_t)(x + 12U);
    }
}

static void DrawCenteredText2x(uint8_t y, const char *text)
{
    uint8_t len = (uint8_t)strlen(text);
    uint8_t width = (uint8_t)(len * 12U);
    uint8_t x = (width >= OLED_WIDTH) ? 0U : (uint8_t)((OLED_WIDTH - width) / 2U);
    DrawText2x(x, y, text);
}

static void AppendChar(char *buf, uint8_t *idx, uint8_t max, char ch)
{
    if (*idx < (uint8_t)(max - 1U)) {
        buf[(*idx)++] = ch;
        buf[*idx] = '\0';
    }
}

static void AppendUnsigned(char *buf, uint8_t *idx, uint8_t max, uint32_t value)
{
    char tmp[10];
    uint8_t n = 0U;
    do {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value && n < sizeof(tmp));
    while (n > 0U) {
        AppendChar(buf, idx, max, tmp[--n]);
    }
}

static void AppendSigned(char *buf, uint8_t *idx, uint8_t max, int32_t value)
{
    if (value < 0) {
        AppendChar(buf, idx, max, '-');
        value = -value;
    } else {
        AppendChar(buf, idx, max, '+');
    }
    AppendUnsigned(buf, idx, max, (uint32_t)value);
}

static void FormatVoltage(char *buf, uint8_t max, uint16_t mv)
{
    uint8_t idx = 0U;
    AppendChar(buf, &idx, max, 'V');
    AppendChar(buf, &idx, max, ':');
    AppendUnsigned(buf, &idx, max, mv / 1000U);
    AppendChar(buf, &idx, max, '.');
    uint16_t frac = (mv % 1000U) / 10U;
    AppendChar(buf, &idx, max, (char)('0' + (frac / 10U)));
    AppendChar(buf, &idx, max, (char)('0' + (frac % 10U)));
    AppendChar(buf, &idx, max, 'V');
}

static void FormatCurrent(char *buf, uint8_t max, int16_t ma)
{
    uint8_t idx = 0U;
    AppendChar(buf, &idx, max, 'I');
    AppendChar(buf, &idx, max, ':');
    AppendSigned(buf, &idx, max, ma);
    AppendChar(buf, &idx, max, 'm');
    AppendChar(buf, &idx, max, 'A');
}

static void FormatPower(char *buf, uint8_t max, uint16_t mw)
{
    uint8_t idx = 0U;
    AppendChar(buf, &idx, max, 'P');
    AppendChar(buf, &idx, max, ':');
    AppendUnsigned(buf, &idx, max, mw);
    AppendChar(buf, &idx, max, 'm');
    AppendChar(buf, &idx, max, 'W');
}

static void FormatPercent(char *buf, uint8_t max, uint8_t percent)
{
    uint8_t idx = 0U;
    AppendChar(buf, &idx, max, 'B');
    AppendChar(buf, &idx, max, ':');
    if (percent == 0xFFU) {
        AppendChar(buf, &idx, max, '-');
        AppendChar(buf, &idx, max, '-');
    } else {
        AppendUnsigned(buf, &idx, max, percent);
    }
    AppendChar(buf, &idx, max, '%');
}

static void FormatOverlayPercent(char *buf, uint8_t max, uint8_t percent)
{
    uint8_t idx = 0U;
    AppendUnsigned(buf, &idx, max, percent);
    AppendChar(buf, &idx, max, '%');
}

static void RenderOverlayFrame(void)
{
    char line[8];

    memset(framebuffer, 0, sizeof(framebuffer));
    DrawCenteredText2x(4U, (oled_overlay == OLED_OVERLAY_BACKLIGHT) ? "LIGHT" : "SENS");
    FormatOverlayPercent(line, sizeof(line), oled_overlay_value);
    DrawCenteredText2x(34U, line);
    oled_dirty = 1U;
}

static void RenderStatusFrame(void)
{
    const BatteryStatus_t *bat = BatteryMonitor_GetStatus();
    char line[22];

    memset(framebuffer, 0, sizeof(framebuffer));

    if (bat->ina219_online) {
        FormatVoltage(line, sizeof(line), bat->battery_voltage_mv);
        DrawText2x(0U, 0U, line);
        FormatCurrent(line, sizeof(line), bat->current_ma);
        DrawText2x(0U, 16U, line);
        FormatPower(line, sizeof(line), bat->power_mw);
        DrawText2x(0U, 32U, line);
        FormatPercent(line, sizeof(line), bat->percent);
        DrawText2x(0U, 48U, line);
    } else {
        DrawCenteredText2x(6U, "INA219");
        DrawCenteredText2x(26U, "OFFLINE");
        DrawCenteredText2x(46U, "HID OK");
    }

    oled_dirty = 1U;
}

static void RenderFrame(void)
{
    uint32_t now = HAL_GetTick();

    if (oled_overlay != OLED_OVERLAY_NONE) {
        if ((int32_t)(now - overlay_until_ms) < 0) {
            RenderOverlayFrame();
            return;
        }
        oled_overlay = OLED_OVERLAY_NONE;
    }

    RenderStatusFrame();
}

static void OLED_MarkOffline(uint32_t now)
{
    oled_online = 0U;
    oled_tx_busy = 0U;
    oled_state = OLED_STATE_OFFLINE;
    next_retry_ms = now + OLED_RETRY_INTERVAL_MS;
}

static uint8_t OLED_TrySend(const uint8_t *data, uint16_t len)
{
    if (oled_tx_busy || hi2c1.State != HAL_I2C_STATE_READY) {
        return 0U;
    }
    if (HAL_I2C_Master_Transmit_IT(&hi2c1, OLED_I2C_ADDR_HAL, (uint8_t *)data, len) != HAL_OK) {
        return 0U;
    }
    oled_tx_busy = 1U;
    return 1U;
}

static void OLED_SendNextInit(uint32_t now)
{
    static uint8_t packet[2];

    if (oled_init_index >= sizeof(oled_init_cmds)) {
        oled_online = 1U;
        oled_state = OLED_STATE_IDLE;
        RenderFrame();
        return;
    }

    packet[0] = 0x00U;
    packet[1] = oled_init_cmds[oled_init_index];
    if (OLED_TrySend(packet, sizeof(packet))) {
        oled_init_index++;
    } else if (hi2c1.ErrorCode != HAL_I2C_ERROR_NONE) {
        OLED_MarkOffline(now);
    }
}

static void OLED_SendPage(uint32_t now)
{
    static const uint8_t set_page_cmd_prefix[4] = { 0x00U, 0x21U, 0x00U, 0x7FU };
    static uint8_t page_cmd[7];
    if ((now - last_page_ms) < OLED_PAGE_INTERVAL_MS) {
        return;
    }
    last_page_ms = now;

    if (oled_page_phase == 0U) {
        memcpy(page_cmd, set_page_cmd_prefix, sizeof(set_page_cmd_prefix));
        page_cmd[4] = 0x22U;
        page_cmd[5] = oled_page_index;
        page_cmd[6] = oled_page_index;
        if (OLED_TrySend(page_cmd, sizeof(page_cmd))) {
            oled_page_phase = 1U;
        }
        return;
    }

    oled_page_packet[0] = 0x40U;
    memcpy(&oled_page_packet[1], &framebuffer[oled_page_index * OLED_WIDTH], OLED_WIDTH);
    if (OLED_TrySend(oled_page_packet, sizeof(oled_page_packet))) {
        oled_page_phase = 0U;
        oled_page_index++;
        if (oled_page_index >= OLED_PAGES) {
            oled_page_index = 0U;
            oled_dirty = 0U;
            oled_state = OLED_STATE_IDLE;
        }
    }
}

void OLED_Display_Init(void)
{
    oled_state = OLED_STATE_RESET_LOW;
    oled_overlay = OLED_OVERLAY_NONE;
    oled_online = 0U;
    oled_tx_busy = 0U;
    oled_dirty = 0U;
    oled_render_requested = 1U;
    oled_init_index = 0U;
    oled_page_index = 0U;
    oled_page_phase = 0U;
    state_time_ms = HAL_GetTick();
    next_retry_ms = 0U;
    last_render_ms = 0U;
    last_page_ms = 0U;
    overlay_until_ms = 0U;
    HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
}

void OLED_Display_Process(void)
{
    uint32_t now = HAL_GetTick();

    switch (oled_state) {
        case OLED_STATE_RESET_LOW:
            if ((now - state_time_ms) >= 10U) {
                HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
                state_time_ms = now;
                oled_state = OLED_STATE_RESET_HIGH_WAIT;
            }
            break;

        case OLED_STATE_RESET_HIGH_WAIT:
            if ((now - state_time_ms) >= 10U) {
                oled_init_index = 0U;
                oled_state = OLED_STATE_INIT;
            }
            break;

        case OLED_STATE_INIT:
            OLED_SendNextInit(now);
            break;

        case OLED_STATE_IDLE:
            if (oled_overlay != OLED_OVERLAY_NONE && (int32_t)(now - overlay_until_ms) >= 0) {
                oled_overlay = OLED_OVERLAY_NONE;
                oled_render_requested = 1U;
            }
            if (oled_render_requested || (now - last_render_ms) >= OLED_RENDER_INTERVAL_MS) {
                oled_render_requested = 0U;
                last_render_ms = now;
                RenderFrame();
            }
            if (oled_dirty && !oled_tx_busy) {
                oled_page_index = 0U;
                oled_page_phase = 0U;
                oled_state = OLED_STATE_SEND_PAGE;
            }
            break;

        case OLED_STATE_SEND_PAGE:
            OLED_SendPage(now);
            break;

        case OLED_STATE_OFFLINE:
            if ((int32_t)(now - next_retry_ms) >= 0) {
                OLED_Display_Init();
            }
            break;

        default:
            OLED_MarkOffline(now);
            break;
    }
}

void OLED_Display_RequestRefresh(void)
{
    oled_render_requested = 1U;
}

void OLED_Display_ShowBacklightPercent(uint8_t percent)
{
    oled_overlay = OLED_OVERLAY_BACKLIGHT;
    oled_overlay_value = percent;
    overlay_until_ms = HAL_GetTick() + OLED_CONTROL_OVERLAY_MS;
    oled_render_requested = 1U;
}

void OLED_Display_ShowMouseSensitivityPercent(uint8_t percent)
{
    oled_overlay = OLED_OVERLAY_MOUSE_SENSITIVITY;
    oled_overlay_value = percent;
    overlay_until_ms = HAL_GetTick() + OLED_CONTROL_OVERLAY_MS;
    oled_render_requested = 1U;
}

void OLED_Display_I2C_TxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c1) {
        oled_tx_busy = 0U;
    }
}

void OLED_Display_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c1) {
        OLED_MarkOffline(HAL_GetTick());
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    OLED_Display_I2C_TxCpltCallback(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    OLED_Display_I2C_ErrorCallback(hi2c);
}
