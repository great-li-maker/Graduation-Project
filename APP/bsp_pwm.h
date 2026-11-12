#ifndef __BSP_PWM_H
#define __BSP_PWM_H
#include "stm32f10x.h"

#define PWM_TIM        TIM4
#define PWM_TIM_RCC    RCC_APB1Periph_TIM4
#define PWM_GPIO_RCC   RCC_APB2Periph_GPIOB
#define PWM_GPIO_PORT  GPIOB
#define PWM_GPIO_PIN   GPIO_Pin_9

void PWM_Init(void);
void PWM_SetSpeed(u8 level);

/* 风扇三挡：关 / 低 / 中 / 高 */
#define FAN_LEVEL_OFF    0
#define FAN_LEVEL_LOW    30
#define FAN_LEVEL_MID    60
#define FAN_LEVEL_HIGH   100

uint8_t PWM_SnapFanLevel(uint8_t duty);

#endif
