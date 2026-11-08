#include "bsp_relay.h"

/**
  * @brief  按有效电平把逻辑开关量转换成引脚电平
  * @param  pin: 继电器引脚
  * @param  on:  1-吸合  0-断开
  * @retval 无
  */
static void Relay_WritePin(uint16_t pin, uint8_t on)
{
#if RELAY_ACTIVE_LEVEL
	if(on)
	{
		GPIO_SetBits(RELAY_GPIO_PORT, pin);
	}
	else
	{
		GPIO_ResetBits(RELAY_GPIO_PORT, pin);
	}
#else
	if(on)
	{
		GPIO_ResetBits(RELAY_GPIO_PORT, pin);
	}
	else
	{
		GPIO_SetBits(RELAY_GPIO_PORT, pin);
	}
#endif
}

/**
  * @brief  继电器模块初始化，上电默认两路断开
  * @param  无
  * @retval 无
  */
void Relay_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RELAY_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin = RELAY_TEMP_PIN | RELAY_HUM_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(RELAY_GPIO_PORT, &GPIO_InitStructure);

	Relay_All_Off();
}

/**
  * @brief  TEMP 继电器（PA1）控制
  * @param  on: 1-吸合  0-断开
  * @retval 无
  */
void Relay_Temp_Ctrl(uint8_t on)
{
	Relay_WritePin(RELAY_TEMP_PIN, on);
}

/**
  * @brief  HUM 继电器（PA0）控制
  * @param  on: 1-吸合  0-断开
  * @retval 无
  */
void Relay_Hum_Ctrl(uint8_t on)
{
	Relay_WritePin(RELAY_HUM_PIN, on);
}

/**
  * @brief  两路继电器全部断开
  * @param  无
  * @retval 无
  */
void Relay_All_Off(void)
{
	Relay_Temp_Ctrl(0);
	Relay_Hum_Ctrl(0);
}
