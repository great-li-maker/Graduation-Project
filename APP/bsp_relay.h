#ifndef _BSP_RELAY_H
#define _BSP_RELAY_H

#include "stm32f10x.h"

/* 继电器模块
 * TEMP 继电器 -> PA1   （温控执行机构）
 * HUM  继电器 -> PA0   （湿控执行机构）
 */
#define RELAY_GPIO_PORT       GPIOA
#define RELAY_GPIO_CLK        RCC_APB2Periph_GPIOA

#define RELAY_TEMP_PIN        GPIO_Pin_1
#define RELAY_HUM_PIN         GPIO_Pin_0

/* 继电器有效电平：0-低电平吸合，1-高电平吸合
 * 本工程使用高电平吸合的继电器模块
 */
#define RELAY_ACTIVE_LEVEL    1

void Relay_Init(void);
void Relay_Temp_Ctrl(uint8_t on);
void Relay_Hum_Ctrl(uint8_t on);
void Relay_All_Off(void);

#endif
