#include "battery_monitor.h"

#include <string.h>

extern I2C_HandleTypeDef hi2c2;

#define INA219_I2C_ADDR_7BIT        0x40U
#define INA219_I2C_ADDR_HAL         (INA219_I2C_ADDR_7BIT << 1)
#define INA219_REG_CONFIG           0x00U
#define INA219_REG_SHUNT_VOLTAGE    0x01U
#define INA219_REG_BUS_VOLTAGE      0x02U
#define INA219_REG_POWER            0x03U
#define INA219_REG_CURRENT          0x04U
#define INA219_REG_CALIBRATION      0x05U
#define MP2672_I2C_ADDR_7BIT        0x4BU
#define MP2672_I2C_ADDR_HAL         (MP2672_I2C_ADDR_7BIT << 1)

/* 32V bus range, +/-320mV shunt range, 12-bit bus/shunt ADC, continuous mode. */
#define INA219_CONFIG_VALUE         0x399FU
/* Rshunt=0.1 ohm, current_lsb=100uA -> calibration=0.04096/(0.0001*0.1). */
#define INA219_CALIBRATION_VALUE    4096U
#define INA219_CURRENT_LSB_UA       100L
#define INA219_POWER_LSB_MW         2U
#define INA219_SHUNT_LSB_UV         10L
#define INA219_BUS_LSB_MV           4U

/* Default wiring: INA219 IN+ = battery positive, IN- = MP2672 BATT side.
   Positive current means battery discharge into the system. */
#define INA219_IN_PLUS_AT_BATTERY_POSITIVE 1

#define BATTERY_POLL_INTERVAL_MS    1000U
#define INA219_RETRY_INTERVAL_MS    10000U
#define MP2672_RETRY_INTERVAL_MS    10000U
#define BATTERY_I2C_TIMEOUT_MS      1U
#define BATTERY_PERCENT_UNKNOWN     0xFFU

static BatteryStatus_t battery_status;
static uint32_t last_poll_ms;
static uint32_t next_ina219_retry_ms;
static uint32_t next_mp2672_retry_ms;

static HAL_StatusTypeDef I2C_WriteReg16(uint16_t dev_addr, uint8_t reg, uint16_t value)
{
    uint8_t data[2] = {
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFFU),
    };

    return HAL_I2C_Mem_Write(&hi2c2,
                             dev_addr,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             data,
                             sizeof(data),
                             BATTERY_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef I2C_ReadReg16(uint16_t dev_addr, uint8_t reg, uint16_t *value)
{
    uint8_t data[2] = { 0U, 0U };
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c2,
                                                dev_addr,
                                                reg,
                                                I2C_MEMADD_SIZE_8BIT,
                                                data,
                                                sizeof(data),
                                                BATTERY_I2C_TIMEOUT_MS);
    if (status == HAL_OK) {
        *value = ((uint16_t)data[0] << 8) | data[1];
    }
    return status;
}

static HAL_StatusTypeDef INA219_Init(void)
{
    if (I2C_WriteReg16(INA219_I2C_ADDR_HAL,
                       INA219_REG_CONFIG,
                       INA219_CONFIG_VALUE) != HAL_OK) {
        return HAL_ERROR;
    }

    return I2C_WriteReg16(INA219_I2C_ADDR_HAL,
                          INA219_REG_CALIBRATION,
                          INA219_CALIBRATION_VALUE);
}

static uint8_t CheckMP2672(uint32_t now)
{
    if (HAL_I2C_IsDeviceReady(&hi2c2, MP2672_I2C_ADDR_HAL, 1U, BATTERY_I2C_TIMEOUT_MS) == HAL_OK) {
        battery_status.mp2672_online = 1U;
        battery_status.i2c_online = 1U;
        return 1U;
    }

    battery_status.mp2672_online = 0U;
    battery_status.mp2672_error_count++;
    next_mp2672_retry_ms = now + MP2672_RETRY_INTERVAL_MS;
    return 0U;
}

static uint8_t EstimatePercentFromVoltage(uint16_t voltage_mv)
{
    typedef struct {
        uint16_t mv;
        uint8_t percent;
    } Point_t;

    static const Point_t table[] = {
        { 8400U, 100U },
        { 8200U,  90U },
        { 8000U,  80U },
        { 7800U,  70U },
        { 7600U,  60U },
        { 7400U,  50U },
        { 7200U,  40U },
        { 7000U,  30U },
        { 6800U,  20U },
        { 6600U,  10U },
        { 6200U,   0U },
    };

    if (voltage_mv >= table[0].mv) {
        return table[0].percent;
    }

    for (uint8_t i = 1U; i < (sizeof(table) / sizeof(table[0])); i++) {
        if (voltage_mv >= table[i].mv) {
            uint16_t high_mv = table[i - 1U].mv;
            uint16_t low_mv = table[i].mv;
            uint8_t high_pct = table[i - 1U].percent;
            uint8_t low_pct = table[i].percent;
            uint16_t span_mv = high_mv - low_mv;
            uint16_t pos_mv = voltage_mv - low_mv;
            return (uint8_t)(low_pct + ((uint32_t)(high_pct - low_pct) * pos_mv) / span_mv);
        }
    }

    return 0U;
}

static void ClearINA219Measurements(void)
{
    battery_status.valid = 0U;
    battery_status.i2c_online = 0U;
    battery_status.ina219_online = 0U;
    battery_status.percent = BATTERY_PERCENT_UNKNOWN;
    battery_status.battery_voltage_mv = 0U;
    battery_status.bus_voltage_mv = 0U;
    battery_status.shunt_voltage_uv = 0;
    battery_status.current_ua = 0;
    battery_status.current_ma = 0;
    battery_status.power_mw = 0U;
}

static uint8_t ReadINA219(uint32_t now)
{
    uint16_t bus_raw = 0U;
    uint16_t shunt_raw = 0U;
    uint16_t current_raw = 0U;
    uint16_t power_raw = 0U;

    if (I2C_ReadReg16(INA219_I2C_ADDR_HAL, INA219_REG_BUS_VOLTAGE, &bus_raw) != HAL_OK ||
        I2C_ReadReg16(INA219_I2C_ADDR_HAL, INA219_REG_SHUNT_VOLTAGE, &shunt_raw) != HAL_OK ||
        I2C_ReadReg16(INA219_I2C_ADDR_HAL, INA219_REG_CURRENT, &current_raw) != HAL_OK ||
        I2C_ReadReg16(INA219_I2C_ADDR_HAL, INA219_REG_POWER, &power_raw) != HAL_OK) {
        ClearINA219Measurements();
        battery_status.ina219_error_count++;
        battery_status.read_error_count++;
        next_ina219_retry_ms = now + INA219_RETRY_INTERVAL_MS;
        return 0U;
    }

    uint16_t bus_voltage_mv = (uint16_t)(((bus_raw >> 3) & 0x1FFFU) * INA219_BUS_LSB_MV);
    int32_t shunt_voltage_uv = (int32_t)((int16_t)shunt_raw) * INA219_SHUNT_LSB_UV;
    int32_t current_ua = (int32_t)((int16_t)current_raw) * INA219_CURRENT_LSB_UA;

#if INA219_IN_PLUS_AT_BATTERY_POSITIVE
    uint16_t battery_voltage_mv = (uint16_t)(bus_voltage_mv + (shunt_voltage_uv / 1000L));
#else
    uint16_t battery_voltage_mv = bus_voltage_mv;
    shunt_voltage_uv = -shunt_voltage_uv;
    current_ua = -current_ua;
#endif

    battery_status.valid = 1U;
    battery_status.i2c_online = 1U;
    battery_status.ina219_online = 1U;
    battery_status.bus_voltage_mv = bus_voltage_mv;
    battery_status.battery_voltage_mv = battery_voltage_mv;
    battery_status.shunt_voltage_uv = shunt_voltage_uv;
    battery_status.current_ua = current_ua;
    battery_status.current_ma = (int16_t)(current_ua / 1000L);
    battery_status.power_mw = (uint16_t)(power_raw * INA219_POWER_LSB_MW);
    battery_status.percent = EstimatePercentFromVoltage(battery_voltage_mv);
    battery_status.last_update_ms = now;
    return 1U;
}

void BatteryMonitor_Init(void)
{
    memset(&battery_status, 0, sizeof(battery_status));
    battery_status.percent = BATTERY_PERCENT_UNKNOWN;
    last_poll_ms = HAL_GetTick() - BATTERY_POLL_INTERVAL_MS;
    next_ina219_retry_ms = 0U;
    next_mp2672_retry_ms = 0U;
}

void BatteryMonitor_Process(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - last_poll_ms) < BATTERY_POLL_INTERVAL_MS) {
        return;
    }
    last_poll_ms = now;

    if (!battery_status.mp2672_online && ((int32_t)(now - next_mp2672_retry_ms) >= 0)) {
        CheckMP2672(now);
    }

    if (battery_status.ina219_online || ((int32_t)(now - next_ina219_retry_ms) >= 0)) {
        if (!battery_status.ina219_online) {
            if (INA219_Init() != HAL_OK) {
                battery_status.ina219_error_count++;
                battery_status.read_error_count++;
                next_ina219_retry_ms = now + INA219_RETRY_INTERVAL_MS;
                battery_status.i2c_online = battery_status.mp2672_online;
                battery_status.valid = battery_status.ina219_online;
                return;
            }
        }
        ReadINA219(now);
    }

    battery_status.i2c_online = (battery_status.ina219_online || battery_status.mp2672_online) ? 1U : 0U;
}

const BatteryStatus_t *BatteryMonitor_GetStatus(void)
{
    return &battery_status;
}
