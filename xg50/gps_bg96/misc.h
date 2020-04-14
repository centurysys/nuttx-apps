#ifndef _LIB_MISC_H
#define _LIB_MISC_H

#include <time.h>

#define I2C_ADDR_TCA6507  0x45
#define I2C_ADDR_PCA9534  0x20
#define I2C_ADDR_EEPROM   0x51
#define I2C_ADDR_BME280   0x76
#define I2C_ADDR_MPU9250  0x68

int find_i2c_device(uint8_t addr);
void init_leds(void);
int set_signal_level(int level);
int set_signal_level_by_LQI(uint8_t LQI);
uint16_t rtc_sleep(char *task_name, uint16_t seconds);
char *isoformat(const time_t *timer, char *buf);
int get_battery_level(uint16_t *level);

#endif
