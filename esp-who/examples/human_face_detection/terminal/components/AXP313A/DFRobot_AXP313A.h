#ifndef _DFROBOT_AXP313A_H_
#define _DFROBOT_AXP313A_H_
#include "stdio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OV2640 0
#define OV7725 1
#define TIME6S 0
#define TIME10S 0

#ifdef __cplusplus
extern "C" {
#endif

void begin(i2c_port_t i2c_num, uint8_t addr);
void enableCameraPower(uint8_t camera);
void setCameraPower(float DVDD, float AVDDorDOVDD);
void disablePower(void);
void setShutdownKeyLevelTime(uint8_t offLevelTime);

#ifdef __cplusplus
}
#endif

#endif