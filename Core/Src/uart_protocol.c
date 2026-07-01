#include "uart_protocol.h"

#include "backlight_control.h"
#include "battery_monitor.h"
#include "oled_display.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

#define UART_RX_LINE_SIZE     64U
#define UART_TX_LINE_SIZE     128U
#define UART_BRIGHTNESS_MIN   0U
#define UART_BRIGHTNESS_MAX   100U

static uint8_t rx_byte;
static char rx_line[UART_RX_LINE_SIZE];
static volatile uint8_t rx_index;
static volatile uint8_t rx_line_ready;
static volatile uint8_t rx_overflow;
static char command_line[UART_RX_LINE_SIZE];

static uint8_t tx_busy;
static uint8_t tx_buffer[UART_TX_LINE_SIZE];

static uint8_t AsciiToUpper(uint8_t ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return (uint8_t)(ch - ('a' - 'A'));
    }
    return ch;
}

static uint8_t TokenEquals(const char *token, const char *expected)
{
    while (*token && *expected) {
        if (AsciiToUpper((uint8_t)*token) != (uint8_t)*expected) {
            return 0U;
        }
        token++;
        expected++;
    }
    return (*token == '\0' && *expected == '\0') ? 1U : 0U;
}

static char *NextToken(char **cursor)
{
    char *start = *cursor;

    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (*start == '\0') {
        *cursor = start;
        return NULL;
    }

    char *end = start;
    while (*end != '\0' && *end != ' ' && *end != '\t') {
        end++;
    }
    if (*end != '\0') {
        *end++ = '\0';
    }
    *cursor = end;
    return start;
}

static uint8_t ParsePercent(const char *text, uint8_t *value)
{
    uint16_t parsed = 0U;
    uint8_t digits = 0U;

    while (*text) {
        if (*text < '0' || *text > '9') {
            return 0U;
        }
        parsed = (uint16_t)((parsed * 10U) + (uint8_t)(*text - '0'));
        digits++;
        if (parsed > 100U) {
            return 0U;
        }
        text++;
    }

    if (digits == 0U ||
        parsed < UART_BRIGHTNESS_MIN ||
        parsed > UART_BRIGHTNESS_MAX ||
        (parsed % 20U) != 0U) {
        return 0U;
    }
    *value = (uint8_t)parsed;
    return 1U;
}

static void UART_SendText(const char *text)
{
    if (tx_busy) {
        return;
    }

    size_t len = strlen(text);
    if (len >= sizeof(tx_buffer)) {
        len = sizeof(tx_buffer) - 1U;
    }
    memcpy(tx_buffer, text, len);
    tx_busy = 1U;
    if (HAL_UART_Transmit_IT(&huart1, tx_buffer, (uint16_t)len) != HAL_OK) {
        tx_busy = 0U;
    }
}

static void AppendChar(char *buf, uint8_t *idx, uint8_t max, char ch)
{
    if (*idx < (uint8_t)(max - 1U)) {
        buf[(*idx)++] = ch;
        buf[*idx] = '\0';
    }
}

static void AppendText(char *buf, uint8_t *idx, uint8_t max, const char *text)
{
    while (*text) {
        AppendChar(buf, idx, max, *text++);
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
    }
    AppendUnsigned(buf, idx, max, (uint32_t)value);
}

static void UART_SendStatus(void)
{
    UART_SendText("OK SCIO UART 1\r\n");
}

static void UART_SendHelp(void)
{
    UART_SendText("OK CMDS PING HELP GET BL SET BL <0|20|40|60|80|100> BL <0|20|40|60|80|100> GET BAT\r\n");
}

static void UART_SendBacklight(void)
{
    char out[24];
    uint8_t percent = Backlight_GetPercent();
    uint8_t idx = 0U;
    AppendText(out, &idx, sizeof(out), "OK BL ");
    AppendUnsigned(out, &idx, sizeof(out), percent);
    AppendText(out, &idx, sizeof(out), "\r\n");
    UART_SendText(out);
}

static void UART_SendBattery(void)
{
    const BatteryStatus_t *bat = BatteryMonitor_GetStatus();
    char out[96];
    uint8_t idx = 0U;

    AppendText(out, &idx, sizeof(out), "OK BAT V=");
    AppendUnsigned(out, &idx, sizeof(out), bat->battery_voltage_mv);
    AppendText(out, &idx, sizeof(out), "mV I=");
    AppendSigned(out, &idx, sizeof(out), bat->current_ma);
    AppendText(out, &idx, sizeof(out), "mA P=");
    AppendUnsigned(out, &idx, sizeof(out), bat->power_mw);
    AppendText(out, &idx, sizeof(out), "mW B=");
    if (bat->percent == 0xFFU) {
        AppendText(out, &idx, sizeof(out), "NA");
    } else {
        AppendUnsigned(out, &idx, sizeof(out), bat->percent);
        AppendChar(out, &idx, sizeof(out), '%');
    }
    AppendText(out, &idx, sizeof(out), "\r\n");
    UART_SendText(out);
}

static void HandleSetBacklight(char *value_token)
{
    uint8_t percent = 0U;
    if (!ParsePercent(value_token, &percent)) {
        UART_SendText("ERR RANGE BL 0/20/40/60/80/100\r\n");
        return;
    }

    Backlight_SetPercent(percent);
    OLED_Display_ShowBacklightPercent(Backlight_GetPercent());
    UART_SendBacklight();
}

static void HandleCommand(char *line)
{
    char *cursor = line;
    char *cmd = NextToken(&cursor);
    if (cmd == NULL) {
        return;
    }

    if (TokenEquals(cmd, "PING")) {
        UART_SendText("OK PONG\r\n");
        return;
    }
    if (TokenEquals(cmd, "HELP")) {
        UART_SendHelp();
        return;
    }
    if (TokenEquals(cmd, "SCIO?")) {
        UART_SendStatus();
        return;
    }
    if (TokenEquals(cmd, "BL")) {
        char *value = NextToken(&cursor);
        if (value == NULL) {
            UART_SendBacklight();
            return;
        }
        HandleSetBacklight(value);
        return;
    }
    if (TokenEquals(cmd, "GET")) {
        char *target = NextToken(&cursor);
        if (target != NULL && TokenEquals(target, "BL")) {
            UART_SendBacklight();
            return;
        }
        if (target != NULL && TokenEquals(target, "BAT")) {
            UART_SendBattery();
            return;
        }
        UART_SendText("ERR GET TARGET\r\n");
        return;
    }
    if (TokenEquals(cmd, "SET")) {
        char *target = NextToken(&cursor);
        char *value = NextToken(&cursor);
        if (target != NULL && value != NULL && TokenEquals(target, "BL")) {
            HandleSetBacklight(value);
            return;
        }
        UART_SendText("ERR SET TARGET\r\n");
        return;
    }

    UART_SendText("ERR UNKNOWN\r\n");
}

void UART_Protocol_Init(void)
{
    rx_index = 0U;
    rx_line_ready = 0U;
    rx_overflow = 0U;
    tx_busy = 0U;
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    UART_SendStatus();
}

void UART_Protocol_Process(void)
{
    if (rx_overflow) {
        rx_overflow = 0U;
        UART_SendText("ERR LINE\r\n");
    }

    if (!rx_line_ready) {
        return;
    }

    __disable_irq();
    memcpy(command_line, rx_line, sizeof(command_line));
    rx_line_ready = 0U;
    __enable_irq();

    HandleCommand(command_line);
}

void UART_Protocol_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart1) {
        return;
    }

    uint8_t ch = rx_byte;
    if (!rx_line_ready) {
        if (ch == '\n' || ch == '\r') {
            if (rx_index > 0U) {
                rx_line[rx_index] = '\0';
                rx_line_ready = 1U;
                rx_index = 0U;
            }
        } else if (rx_index < (UART_RX_LINE_SIZE - 1U)) {
            rx_line[rx_index++] = (char)ch;
        } else {
            rx_index = 0U;
            rx_overflow = 1U;
        }
    }

    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
}

void UART_Protocol_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        tx_busy = 0U;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    UART_Protocol_RxCpltCallback(huart);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    UART_Protocol_TxCpltCallback(huart);
}
