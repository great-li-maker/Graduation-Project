#include <stdio.h>
#include "stm32f10x.h"
#include "bsp_delay.h"
#include "bsp_key.h"
#include "bsp_oled.h"
#include "bsp_dht11.h"
#include "bsp_Alarm.h"
#include "bsp_led.h"
#include "bsp_usart.h"
#include "esp8266.h"
#include "onenet.h"
#include "bsp_pwm.h"
#include "bsp_relay.h"
#include "string.h"

DHT11_Data_TypeDef DHT11_Data;
char oled_Temp[16],oled_TempThr[16];
char oled_Hum[16],oled_HumThr[16];
char oled_Pwm[16];

uint8_t key_value = 0;
uint16_t TimeCount = 0;

uint8_t Temp_Thr = 30;  //温度阈值设置
uint8_t Hum_Thr = 80;   //湿度阈值设置
uint8_t Alarm_flag;
uint8_t Humidifier_flag;
uint8_t Heater_flag;
int16_t pwm_duty;

// 工作模式：0-自动模式（上电默认）  1-手动模式
uint8_t Work_Mode = 0;

char PUBLIS_BUF[320];
const char devPubTopic[] = "$sys/151aEKcUs2/test1/thing/property/post"; 
const char *devSubTopic[] = {"$sys/151aEKcUs2/test1/thing/property/set",
                               "$sys/151aEKcUs2/test1/thing/property/post/reply"};
unsigned char *dataPtr = NULL;


typedef enum{
	SCREEN_MENU,        //主界面
	SCREEN_TEMP_SET,    //温度阈值设置
	SCREEN_HUM_SET,     //湿度阈值设置
	SCREEN_FAN_SET,     //风扇PWM设置
	SCREEN_HEATER,      //加热器控制
	SCREEN_HUMIDIFIER   //加湿器控制
}DisplayState;

DisplayState currentState = SCREEN_MENU;

uint8_t Heater_On = 0, Hum_On = 0;             //继电器开关设定（KEY2 开 / KEY3 关）
uint8_t Alarm_On = 0;                          //蜂鸣器当前状态，用于上报云平台

void Bsp_init()
{
	DHT11_Init();
	OLED_Init();
	Delay_Init();
	Key_Init();
	Alarm_Init();
	LED_Init();
	Usart_Init();
	PWM_Init();
	PWM_SetSpeed(pwm_duty);
	Relay_Init();

}

void Oled_ShowMenu()
{
	OLED_ShowCH(20,0, Work_Mode ? (u8*)"手动模式" : (u8*)"自动模式");
	OLED_ShowCH(20,2,(u8*)"智能孵化箱");
	sprintf(oled_Temp,"Temp:%d.%d℃",DHT11_Data.temp_int,DHT11_Data.temp_deci);
	OLED_ShowCH(20,4,(u8*)(char*)oled_Temp);

	sprintf(oled_Hum,"Hum:%d％",DHT11_Data.humi_int);
	OLED_ShowCH(20,6,(u8*)(char*)oled_Hum);

}

void Oled_ShowTempSet()
{
	OLED_ShowCH(30,2,(u8*)"温度阈值");
	if(key_value == 2)
	{
		if(Temp_Thr < 100)
		{
			Temp_Thr++;
		}
	}
	else if(key_value == 3)
	{
		if(Temp_Thr > 0)
		{
			Temp_Thr--;
		}
	}

	sprintf(oled_TempThr,"Temp:%d℃",Temp_Thr);
	OLED_ShowCH(30,4,(u8*)oled_TempThr);

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"锁定");   //自动模式：板子上不能改阈值
}

void Oled_ShowHumSet()
{
    
	OLED_ShowCH(30,2,(u8*)"湿度阈值");
	if(key_value == 2)
	{
		if(Hum_Thr < 100)
		{
			Hum_Thr++;
		}
	}
	else if(key_value == 3)
	{
		if(Hum_Thr > 0)
		{
			Hum_Thr--;
		}
	}

	sprintf(oled_HumThr,"Hum:%d％",Hum_Thr);
	OLED_ShowCH(30,4,(u8*)oled_HumThr);

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"锁定");   //自动模式：板子上不能改阈值
}

void Oled_ShowFanSet()
{
	OLED_ShowCH(30,2,(u8*)"风扇调节");
	if(key_value == 2)
	{
		if(pwm_duty < 100)
		{
			pwm_duty += 20;
			if(pwm_duty > 100) pwm_duty = 100;
			PWM_SetSpeed(pwm_duty);
		}
	}
	else if(key_value == 3)
	{
		if(pwm_duty > 0)
		{
			pwm_duty -= 20;
			if(pwm_duty < 0) pwm_duty = 0;
			PWM_SetSpeed(pwm_duty);
		}
	}  

	sprintf(oled_Pwm,"风速:%-3d％",pwm_duty);
	OLED_ShowCH(30,4,(u8*)oled_Pwm);

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"锁定");   //自动模式：板子上不能改风速
}

/* 加热器控制界面：KEY2 吸合，KEY3 断开 */
void Oled_ShowHeater()
{
	if(key_value == 2)  { Heater_On = 1; Heater_flag = 0; }
	if(key_value == 3)  { Heater_On = 0; Heater_flag = 0; }

	OLED_ShowCH(40,2,(u8*)"加热器");

	if(Heater_On)     OLED_ShowCH(0,4,(u8*)"状态:开");
	else              OLED_ShowCH(0,4,(u8*)"状态:关");

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"锁定");   //自动模式：加热器由阈值控制，不可操作
}

/* 加湿器控制界面：KEY2 吸合，KEY3 断开 */
void Oled_ShowHumidifier()
{
	if(key_value == 2)  { Hum_On = 1; Humidifier_flag = 0; }
	if(key_value == 3)  { Hum_On = 0; Humidifier_flag = 0; }

	OLED_ShowCH(40,2,(u8*)"加湿器");

	if(Hum_On)        OLED_ShowCH(0,4,(u8*)"状态:开");
	else              OLED_ShowCH(0,4,(u8*)"状态:关");

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"锁定");   //自动模式：加湿器由阈值控制，不可操作
}

void Oled_Switch()
{
	static uint8_t last_mode = 0xFF;

	if(Work_Mode != last_mode)          //模式切换（由小程序控制）：清屏重画，避免残留
	{
		last_mode = Work_Mode;
		OLED_Clear();
	}

	key_value = Key_Scan(1);  

	/* 自动模式：板子上的按键只能翻页，KEY2/KEY3 一律不响应 */
	if(Work_Mode == 0 && key_value != 1) key_value = 0;

	if(key_value == 1)
	{
		currentState = (currentState +1)%6;
		OLED_Clear();
	}
	
	switch(currentState)
	{
		case SCREEN_MENU:
			Oled_ShowMenu();
		break;
		
		case SCREEN_TEMP_SET:
			Oled_ShowTempSet();
		break;
		
		case SCREEN_HUM_SET:
			Oled_ShowHumSet();
		break;

		case SCREEN_FAN_SET:
			Oled_ShowFanSet();
		break;

		case SCREEN_HEATER:
			Oled_ShowHeater();
		break;

		case SCREEN_HUMIDIFIER:
			Oled_ShowHumidifier();
		break;
	}
	
}

void Alarm_State()
{	
	if(Alarm_flag == 1 && (DHT11_Data.temp_int > Temp_Thr 
						|| DHT11_Data.humi_int > Hum_Thr))
	{
		Alarm_ON();
		Alarm_On = 1;
	}
	else
	{
		Alarm_OFF();
		Alarm_On = 0;
	}
}

/* 加热器控制：自动模式下按温度阈值设置 Heater_On 标志，再驱动继电器 */
void Heater_Control()
{
	if(Heater_flag)
	{
        Heater_On = 1;
	}
	else if(Work_Mode == 0)
	{
		if(DHT11_Data.temp_int < Temp_Thr)    Heater_On = 1;      //低于温度阈值：加热
		else                                  Heater_On = 0;
	}

	Relay_Temp_Ctrl(Heater_On);
}

/* 加湿器控制：自动模式下按湿度阈值设置 Hum_On 标志，再驱动继电器 */
void Humidifier_Control()
{
    if (Humidifier_flag)
	{
        Hum_On = 1;
	}
	else if(Work_Mode == 0)
	{
		if(DHT11_Data.humi_int < Hum_Thr)    Hum_On = 1;      //低于湿度阈值：加湿
		else                                 Hum_On = 0;
	}
	Relay_Hum_Ctrl(Hum_On);
}

void JsonValue()
{
	uint8_t Temp = DHT11_Data.temp_int;
	uint8_t Hum = DHT11_Data.humi_int;
	static uint16_t pub_id = 0;

	pub_id++;

	memset(PUBLIS_BUF, 0, sizeof(PUBLIS_BUF));

	sprintf(PUBLIS_BUF,
			"{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
			"\"Temp\":{\"value\":%d},"
			"\"Hum\":{\"value\":%d},"
			"\"Temp_th\":{\"value\":%d},"
			"\"Hum_th\":{\"value\":%d},"
			"\"Heater\":{\"value\":%s},"
			"\"Humidifier\":{\"value\":%s},"
			"\"Alarm\":{\"value\":%s},"
			"\"Mode\":{\"value\":%s},"
			"\"Fan\":{\"value\":%d}}}",
			pub_id, Temp, Hum,
			Temp_Thr, Hum_Thr,
			Heater_On ? "true" : "false",
			Hum_On ? "true" : "false",
			Alarm_On ? "true" : "false",
			Work_Mode ? "true" : "false",
			pwm_duty);
}

int main()
{
	static uint16_t dht_cnt = 0;
	static DHT11_Data_TypeDef dht_tmp;

	Bsp_init();
	OLED_ShowCH(25,4,(u8*)"网络连接中...");
	ESP8266_Init();
	while(OneNet_DevLink())
	{
		DelayXms(500);
	}
	OLED_Clear();
	OLED_ShowCH(25,4,(u8*)"网络连接成功");
	DelayXms(3000);
/*订阅Topic主题*/
	OneNet_Subscribe(devSubTopic, 2);	
	OLED_Clear();
	while(1)
	{
		/* DHT11 两次采样至少间隔 1 秒；读失败时保留上一次有效值 */
		if(++dht_cnt >= 80)
		{
			dht_cnt = 0;
			if(DHT11_Read_TempAndHumidity(&dht_tmp) == SUCCESS)
				DHT11_Data = dht_tmp;
		}

		Oled_Switch();
		Alarm_State();
		Heater_Control();
		Humidifier_Control();

		if(++TimeCount >= 100)
		{
			JsonValue();
			OneNet_Publish(devPubTopic, PUBLIS_BUF);
			ESP8266_Clear();
			TimeCount = 0;
		}

		dataPtr = ESP8266_GetIPD(2);
		if(dataPtr != NULL)
			OneNet_RevPro(dataPtr);
	}
}
