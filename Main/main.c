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

DHT11_Data_TypeDef DHT11_Data;
char oled_Temp[16],oled_TempThr[16];
char oled_Hum[16],oled_HumThr[16];
char oled_Pwm[16];

uint8_t key_value = 0;
uint8_t Temp_Thr = 30;  //温度阈值设置
uint8_t Hum_Thr = 80;   //湿度阈值设置
uint16_t TimeCount = 0;
uint8_t Alarm_flag;
uint8_t pwm_duty;

char PUBLIS_BUF[256];
const char devPubTopic[] = "$sys/151aEKcUs2/test1/thing/property/post"; 
const char *devSubTopic[] = {"$sys/151aEKcUs2/test1/thing/property/set"};
unsigned char *dataPtr = NULL;


typedef enum{
	MAIN_MENU,        //主菜单界面
	TEMP_SET,		  //温度阈值设置界面
	HUM_SET,          //湿度阈值设置界面
	PWM_SET           //风扇PWM阈值设置界面
}DispalyState;

DispalyState currentState = MAIN_MENU;

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
	PWM_SetBright(pwm_duty);

}

void Oled_show()
{
	OLED_ShowCH(5,0,"智能家居系统");
	if(DHT11_Read_TempAndHumidity(&DHT11_Data) == 1)
	{
		sprintf(oled_Temp,"Temp:%d.%d℃",DHT11_Data.temp_int,DHT11_Data.temp_deci);
		OLED_ShowCH(20,2,(char*)oled_Temp);
		
		sprintf(oled_Hum,"Hum:%d％",DHT11_Data.humi_int);
		OLED_ShowCH(20,4,(char*)oled_Hum);
		
		sprintf(oled_Pwm,"LED亮度:%d％", pwm_duty);
		OLED_ShowCH(20,6,(char*)oled_Pwm);
		
	}
}

void Oled_show1()
{
	OLED_ShowCH(30,2,"温度阈值");
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
}

void Oled_show2()
{
	OLED_ShowCH(30,2,"湿度阈值");
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
} 

void Oled_show3()
{
	OLED_ShowCH(30,2,"LED亮度调节");
	if(key_value == 2)
	{
		if(pwm_duty < 100)
		{
			pwm_duty += 20;
			if(pwm_duty > 100) pwm_duty = 100;
			PWM_SetBright(pwm_duty);
		}
	}
	else if(key_value == 3)
	{
		if(pwm_duty > 0)
		{
			pwm_duty -= 20;
			if(pwm_duty < 0) pwm_duty = 0;
			PWM_SetBright(pwm_duty);
		}
	}
	sprintf(oled_Pwm,"风扇PWM:%-3d％",pwm_duty);
	OLED_ShowCH(30,4,(u8*)oled_Pwm);
}

void Oled_Switch()
{
	key_value = Key_Scan(0);
	if(key_value == 1)
	{
		currentState = (currentState +1)%4;
		OLED_Clear();
	}
	
	switch(currentState)
	{
		case MAIN_MENU:
			Oled_show();
		break;
		
		case TEMP_SET:
			Oled_show1();
		break;
		
		case HUM_SET:
			Oled_show2();
		break;

		case PWM_SET:
			Oled_show3();
	}
	
}

void Alarm_State()
{
	if(Alarm_flag == 1)
	{
			if(DHT11_Data.temp_int > Temp_Thr || DHT11_Data.humi_int > Hum_Thr)
			{
				Alarm_ON();
			}
	}
	else
	{
		Alarm_OFF();
	}
}

void JsonValue()
{
	uint8_t Temp = DHT11_Data.temp_int;
	uint8_t Hum = DHT11_Data.humi_int;
	
	memset(PUBLIS_BUF, 0, sizeof(PUBLIS_BUF));
	
	sprintf(PUBLIS_BUF,"{\"id\":\"123\",\"params\":{\"Temp\":{\"value\":%d},\"Hum\":{\"value\":%d} }}",
					DHT11_Data.temp_int,DHT11_Data.humi_int);	
}

int main()
{
	Bsp_init();
	OLED_ShowCH(25,4,"网络连接中...");
	ESP8266_Init();
	while(OneNet_DevLink())
	{
		DelayXms(500);
	}
	OLED_Clear();
	OLED_ShowCH(25,4,"网络连接成功");
	DelayXms(3000);
/*订阅Topic主题*/
	OneNet_Subscribe(devSubTopic, 1);	
	OLED_Clear();
	while(1)
	{
		Oled_Switch();
		Alarm_State();
		
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
