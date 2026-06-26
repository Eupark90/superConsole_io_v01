#ifndef __BATTERY_MONITOR_H
#define __BATTERY_MONITOR_H

#include "main.h"

typedef struct {
    uint8_t valid;
    uint8_t i2c_online;
    uint8_t ina219_online;
    uint8_t mp2672_online;
    uint8_t percent;          /* voltage-based estimate, 0xFF when unavailable */
    uint16_t battery_voltage_mv;
    uint16_t bus_voltage_mv;
    int32_t shunt_voltage_uv;
    int32_t current_ua;
    int16_t current_ma;
    uint16_t power_mw;
    uint32_t last_update_ms;
    uint32_t read_error_count;
    uint32_t ina219_error_count;
    uint32_t mp2672_error_count;
} BatteryStatus_t;

void BatteryMonitor_Init(void);
void BatteryMonitor_Process(void);
const BatteryStatus_t *BatteryMonitor_GetStatus(void);

#endif /* __BATTERY_MONITOR_H */
