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
char oled_Temp[16];
char oled_Hum[16];

uint8_t key_value = 0;
uint16_t TimeCount = 0;

uint8_t Temp_Low = 30;   //�¶����ޣ���������������
uint8_t Temp_High = 33;  //�¶����ޣ�����������ɢ��
uint8_t Hum_Low = 80;    //ʪ�����ޣ�������������ʪ
uint8_t Hum_High = 85;   //ʪ�����ޣ�����������ɢ��
uint8_t Alarm_flag = 1;   //����ʹ�ܣ��ϵ�Ĭ�Ͽ���
uint8_t Humidifier_flag;
uint8_t Heater_flag;
int16_t pwm_duty;
uint8_t fan_base = FAN_LEVEL_OFF;   //�û��趨�ķ��ȵ�λ

// ����ģʽ��0-�Զ�ģʽ���ϵ�Ĭ�ϣ�  1-�ֶ�ģʽ
uint8_t Work_Mode = 0;

char PUBLIS_BUF[400];
const char devPubTopic[] = "$sys/151aEKcUs2/test1/thing/property/post"; 
const char *devSubTopic[] = {"$sys/151aEKcUs2/test1/thing/property/set",
                               "$sys/151aEKcUs2/test1/thing/property/post/reply"};
unsigned char *dataPtr = NULL;

/* ������������ / �� / �� / �� */
static const uint8_t Fan_Levels[4] = {FAN_LEVEL_OFF, FAN_LEVEL_LOW, FAN_LEVEL_MID, FAN_LEVEL_HIGH};

static uint8_t Fan_GetLevel(int16_t duty)
{
	if(duty <= FAN_LEVEL_OFF) return 0;
	if(duty < (FAN_LEVEL_LOW + FAN_LEVEL_MID) / 2) return 1;
	if(duty < (FAN_LEVEL_MID + FAN_LEVEL_HIGH) / 2) return 2;
	return 3;
}


typedef enum{
	SCREEN_MENU,        //������
	SCREEN_TEMP_LOW,    //�¶���������
	SCREEN_TEMP_HIGH,   //�¶���������
	SCREEN_HUM_LOW,     //ʪ����������
	SCREEN_HUM_HIGH,    //ʪ����������
	SCREEN_FAN_SET,     //���ȵ�λ����
	SCREEN_HEATER,      //����������
	SCREEN_HUMIDIFIER   //��ʪ������
}DisplayState;

DisplayState currentState = SCREEN_MENU;

uint8_t Heater_On = 0, Hum_On = 0;             //�̵��������趨��KEY2 �� / KEY3 �أ�
uint8_t Alarm_On = 0;                          //��������ǰ״̬�������ϱ���ƽ̨

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
	OLED_ShowCH(20,0, Work_Mode ? (u8*)"�ֶ�ģʽ" : (u8*)"�Զ�ģʽ");
	OLED_ShowCH(20,2,(u8*)"���ܷ�����");
	sprintf(oled_Temp,"Temp:%d.%d��",DHT11_Data.temp_int,DHT11_Data.temp_deci);
	OLED_ShowCH(20,4,(u8*)(char*)oled_Temp);

	sprintf(oled_Hum,"Hum:%d��",DHT11_Data.humi_int);
	OLED_ShowCH(20,6,(u8*)(char*)oled_Hum);

}

static void Oled_ShowThreshold(const char *title, uint8_t *value)
{
	char buf[16];

	OLED_ShowCH(30,2,(u8*)title);

	if(key_value == 2)
	{
		if(*value < 100) (*value)++;
	}
	else if(key_value == 3)
	{
		if(*value > 0) (*value)--;
	}

	sprintf(buf, "%d", *value);
	OLED_ShowCH(50,4,(u8*)buf);

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"����");   //�Զ�ģʽ�������ϲ��ܸ���ֵ
}

void Oled_ShowFanSet()
{
	uint8_t level;

	OLED_ShowCH(30,2,(u8*)"���ȵ���");

	level = Fan_GetLevel(fan_base);

	if(key_value == 2)			//����
	{
		if(level < 3)
		{
			level++;
			fan_base = Fan_Levels[level];
			pwm_duty = fan_base;
			PWM_SetSpeed((u8)pwm_duty);
		}
	}
	else if(key_value == 3)		//����
	{
		if(level > 0)
		{
			level--;
			fan_base = Fan_Levels[level];
			pwm_duty = fan_base;
			PWM_SetSpeed((u8)pwm_duty);
		}
	}

	level = Fan_GetLevel(pwm_duty);

	if(level == 0)				OLED_ShowCH(30,4,(u8*)"����:��");
	else if(level == 1)			OLED_ShowCH(30,4,(u8*)"����:LOW");
	else if(level == 2)			OLED_ShowCH(30,4,(u8*)"����:MID");
	else						OLED_ShowCH(30,4,(u8*)"����:HIGH");

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"����");   //�Զ�ģʽ�������ϲ��ܸķ���
}

/* ���ȿ��ƣ��Զ�ģʽֻ�������Զ�ɢ�ȣ��ֶ�ģʽ���û��������� */
void Fan_Control()
{
	uint8_t target;

	if(Work_Mode == 0)
	{
		target = (DHT11_Data.temp_int > Temp_High || DHT11_Data.humi_int > Hum_High)
				 ? FAN_LEVEL_HIGH : FAN_LEVEL_OFF;
	}
	else
	{
		target = fan_base;
	}

	if(target != pwm_duty)
	{
		pwm_duty = target;
		PWM_SetSpeed((u8)pwm_duty);
	}
}

/* ���������ƽ��棺KEY2 ���ϣ�KEY3 �Ͽ� */
void Oled_ShowHeater()
{
	if(key_value == 2)  { Heater_On = 1; Heater_flag = 0; }
	if(key_value == 3)  { Heater_On = 0; Heater_flag = 0; }

	OLED_ShowCH(40,2,(u8*)"������");

	if(Heater_On)     OLED_ShowCH(0,4,(u8*)"״̬:��");
	else              OLED_ShowCH(0,4,(u8*)"״̬:��");

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"����");   //�Զ�ģʽ������������ֵ���ƣ����ɲ���
}

/* ��ʪ�����ƽ��棺KEY2 ���ϣ�KEY3 �Ͽ� */
void Oled_ShowHumidifier()
{
	if(key_value == 2)  { Hum_On = 1; Humidifier_flag = 0; }
	if(key_value == 3)  { Hum_On = 0; Humidifier_flag = 0; }

	OLED_ShowCH(40,2,(u8*)"��ʪ��");

	if(Hum_On)        OLED_ShowCH(0,4,(u8*)"״̬:��");
	else              OLED_ShowCH(0,4,(u8*)"״̬:��");

	if(Work_Mode == 0) OLED_ShowCH(0,6,(u8*)"����");   //�Զ�ģʽ����ʪ������ֵ���ƣ����ɲ���
}

void Oled_Switch()
{
	static uint8_t last_mode = 0xFF;

	if(Work_Mode != last_mode)          //ģʽ�л�����С������ƣ��������ػ����������
	{
		last_mode = Work_Mode;
		OLED_Clear();
	}

	key_value = Key_Scan(0);  

	/* �Զ�ģʽ�������ϵİ���ֻ�ܷ�ҳ��KEY2/KEY3 һ�ɲ���Ӧ */
	if(Work_Mode == 0 && key_value != 1) key_value = 0;

	if(key_value == 1)
	{
		currentState = (currentState +1)%8;
		OLED_Clear();
	}
	
	switch(currentState)
	{
		case SCREEN_MENU:
			Oled_ShowMenu();
		break;
		
		case SCREEN_TEMP_LOW:
			Oled_ShowThreshold("�¶�LOW", &Temp_Low);
		break;
		case SCREEN_TEMP_HIGH:
			Oled_ShowThreshold("�¶�HIGH", &Temp_High);
		break;
		case SCREEN_HUM_LOW:
			Oled_ShowThreshold("ʪ��LOW", &Hum_Low);
		break;
		
		case SCREEN_HUM_HIGH:
			Oled_ShowThreshold("ʪ��HIGH", &Hum_High);
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
	if(Alarm_flag == 1 && (DHT11_Data.temp_int > Temp_High
						|| DHT11_Data.humi_int > Hum_High))
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

/* ���������ƣ��Զ�ģʽ�°��¶���ֵ���� Heater_On ��־���������̵��� */
void Heater_Control()
{
	if(Heater_flag)
	{
        Heater_On = 1;
	}
	else if(Work_Mode == 0)
	{
		if(DHT11_Data.temp_int < Temp_Low)    Heater_On = 1;      //�����¶����ޣ�����
		else                                  Heater_On = 0;
	}

	Relay_Temp_Ctrl(Heater_On);
}

/* ��ʪ�����ƣ��Զ�ģʽ�°�ʪ����ֵ���� Hum_On ��־���������̵��� */
void Humidifier_Control()
{
    if (Humidifier_flag)
	{
        Hum_On = 1;
	}
	else if(Work_Mode == 0)
	{
		if(DHT11_Data.humi_int < Hum_Low)    Hum_On = 1;      //����ʪ�����ޣ���ʪ
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
			"\"Temp_low\":{\"value\":%d},"
			"\"Temp_high\":{\"value\":%d},"
			"\"Hum_low\":{\"value\":%d},"
			"\"Hum_high\":{\"value\":%d},"
			"\"Heater\":{\"value\":%s},"
			"\"Humidifier\":{\"value\":%s},"
			"\"Alarm\":{\"value\":%s},"
			"\"Mode\":{\"value\":%s},"
			"\"Fan\":{\"value\":%d}}}",
			pub_id, Temp, Hum,
			Temp_Low, Temp_High, Hum_Low, Hum_High,
			Heater_On ? "true" : "false",
			Hum_On ? "true" : "false",
			Alarm_flag ? "true" : "false",
			Work_Mode ? "true" : "false",
			pwm_duty);
}

int main()
{
	static uint16_t dht_cnt = 0;
	static DHT11_Data_TypeDef dht_tmp;

	Bsp_init();
	OLED_ShowCH(25,4,(u8*)"����������...");
	ESP8266_Init();
	while(OneNet_DevLink())
	{
		DelayXms(500);
	}
	OLED_Clear();
	OLED_ShowCH(25,4,(u8*)"�������ӳɹ�");
	DelayXms(3000);
/*����Topic����*/
	OneNet_Subscribe(devSubTopic, 2);	
	OLED_Clear();
	while(1)
	{
		/* DHT11 ���β������ټ�� 1 �룻��ʧ��ʱ������һ����Чֵ */
		if(++dht_cnt >= 80)
		{
			dht_cnt = 0;
			if(DHT11_Read_TempAndHumidity(&dht_tmp) == SUCCESS)
				DHT11_Data = dht_tmp;
		}

		Fan_Control();
		Oled_Switch();
		Alarm_State();
		Heater_Control();
		Humidifier_Control();

		/* �ȴ������У����������ϱ���ս��ջ���ʱ©�� set */
		dataPtr = ESP8266_GetIPD(2);
		if(dataPtr != NULL)
			OneNet_RevPro(dataPtr);

		if(++TimeCount >= 100)
		{
			JsonValue();
			OneNet_Publish(devPubTopic, PUBLIS_BUF);
			TimeCount = 0;
		}
	}
}
