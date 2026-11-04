#include "bsp_Alarm.h"
void Alarm_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(FMQ_GPIO_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(FMQ_led_GPIO_CLK,ENABLE);
	
	GPIO_InitStructure.GPIO_Pin=FMQ_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(FMQ_GPIO_PORT,&GPIO_InitStructure);
	GPIO_ResetBits(FMQ_GPIO_PORT, FMQ_GPIO_PIN);	
	
	GPIO_InitStructure.GPIO_Pin=FMQ_led_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(FMQ_led_GPIO_PORT,&GPIO_InitStructure);
	GPIO_SetBits(FMQ_led_GPIO_PORT, FMQ_led_GPIO_PIN);	
}

void Alarm_OFF()
{
	GPIO_ResetBits(FMQ_GPIO_PORT, FMQ_GPIO_PIN);
	GPIO_SetBits(FMQ_led_GPIO_PORT, FMQ_led_GPIO_PIN);	
}

void Alarm_ON()
{
	GPIO_SetBits(FMQ_GPIO_PORT, FMQ_GPIO_PIN);
	GPIO_ResetBits(FMQ_led_GPIO_PORT, FMQ_led_GPIO_PIN);
}
