#ifndef BSP_KEY_H
#define BSP_KEY_H

#include "stm32f10x.h"

#define KEY1_PORT_CLK  RCC_APB2Periph_GPIOB
#define KEY1_PORT	  	 GPIOB
#define KEY1_PORT_PIN  GPIO_Pin_12

#define KEY2_PORT_CLK  RCC_APB2Periph_GPIOB
#define KEY2_PORT	  	 GPIOB
#define KEY2_PORT_PIN  GPIO_Pin_13

#define KEY3_PORT_CLK  RCC_APB2Periph_GPIOB
#define KEY3_PORT	     GPIOB
#define KEY3_PORT_PIN  GPIO_Pin_14

#define KEY1    GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12) //读取按键1
#define KEY2    GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13) //读取按键1
#define KEY3    GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_14) //读取按键1

void Key_Init(void);
uint8_t Key_Scan(uint8_t mode);
#endif
