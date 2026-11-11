/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	onenet.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2017-05-08
	*
	*	版本： 		V1.1
	*
	*	说明： 		与onenet平台的数据交互接口层
	*
	*	修改记录：	V1.0：协议封装、返回判断都在同一个文件，并且不同协议接口不同。
	*				V1.1：提供统一接口供应用层使用，根据不同协议文件来封装协议相关的内容。
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f10x.h"

//网络设备
#include "esp8266.h"

//协议文件
#include "onenet.h"
#include "mqttkit.h"

//硬件驱动
#include "bsp_usart.h"
#include "bsp_delay.h"
#include "bsp_Alarm.h"
#include "bsp_pwm.h"

//C库
#include <string.h>
#include <stdio.h>

#include "cJSON.h"

/*产品ID*/
#define PROID		"151aEKcUs2"

//鉴权Token
#define TOKEN	"version=2018-10-31&res=products%2F151aEKcUs2%2Fdevices%2Ftest1&et=1919763606&method=md5&sign=gcmPfWj4gtIySNn8OudKDg%3D%3D"

//设备名称
#define DEVID		"test1"

/* 属性设置回复主题：平台下发 property/set 后，设备必须往 set_reply 回一条确认，
   否则 set-device-property 接口会报 10411“设备响应超时” */
#define SET_TOPIC			"$sys/" PROID "/" DEVID "/thing/property/set"
#define SET_REPLY_TOPIC		"$sys/" PROID "/" DEVID "/thing/property/set_reply"

extern uint8_t Alarm_flag;
extern uint8_t pwm_duty;
extern uint8_t Temp_Thr;       //温度阈值（主程序里定义）
extern uint8_t Hum_Thr;        //湿度阈值（主程序里定义）
extern uint8_t Work_Mode;      //工作模式：0-自动  1-手动（主程序里定义）
extern uint8_t Heater_flag;    //加热器手动开关标志（主程序里定义）
extern uint8_t Humidifier_flag;    //加湿器手动开关标志（主程序里定义）
extern uint8_t Heater_On;      //加热器继电器状态（主程序里定义）
extern uint8_t Hum_On;         //加湿器继电器状态（主程序里定义）


//==========================================================
//	函数名称：	OneNet_DevLink
//
//	函数功能：	与onenet创建连接
//
//	入口参数：	无
//
//	返回参数：	1-成功	0-失败
//
//	说明：		与onenet平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					//协议包

	unsigned char *dataPtr;
	_Bool status = 1;
	
	UsartPrintf(USART_DEBUG, "OneNet_DevLink\r\n"
							"PROID: %s,	TOKEN: %s, DEVID:%s\r\n"
                        , PROID, TOKEN, DEVID);
	
	if(MQTT_PacketConnect(PROID, TOKEN, DEVID, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//上传平台
		dataPtr = ESP8266_GetIPD(250);									//等待平台响应

		if(dataPtr != NULL)
		{	
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n");status = 0;break;
					
					case 1:UsartPrintf(USART_DEBUG, "WARN:	连接失败：协议错误\r\n");break;
					case 2:UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法的clientid\r\n");break;
					case 3:UsartPrintf(USART_DEBUG, "WARN:	连接失败：服务器失败\r\n");break;
					case 4:UsartPrintf(USART_DEBUG, "WARN:	连接失败：用户名或密码错误\r\n");break;
					case 5:UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法链接(比如token非法)\r\n");break;
					
					default:UsartPrintf(USART_DEBUG, "ERR:	连接失败：未知错误\r\n");break;
				}
			}
		}
		
		MQTT_DeleteBuffer(&mqttPacket);								//删包
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");
	
	return status;
	
}

//==========================================================
//	函数名称：	OneNet_Subscribe
//
//	函数功能：	订阅
//
//	入口参数：	topics：订阅的topic
//				topic_cnt：topic个数
//
//	返回参数：	SEND_TYPE_OK-成功	SEND_TYPE_SUBSCRIBE-需要重发
//
//	说明：		
//==========================================================
void OneNet_Subscribe(const char *topics[], unsigned char topic_cnt)
{
	
	unsigned char i = 0;
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};							//协议包
	
	for(; i < topic_cnt; i++)
		UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topics[i]);
	
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, topics, topic_cnt, &mqttPacket) == 0)
	{
		unsigned char *subPtr = NULL;

		ESP8266_SendData(mqttPacket._data, mqttPacket._len);

		MQTT_DeleteBuffer(&mqttPacket);											//删包

		/* 等 SUBACK：拿到了说明订阅成功，之后才能收到属性下发 */
		subPtr = ESP8266_GetIPD(100);
		if(subPtr != NULL)
			OneNet_RevPro(subPtr);
	}

}

//==========================================================
//	函数名称：	OneNet_Publish
//
//	函数功能：	发布消息
//
//	入口参数：	topic：发布的主题
//				msg：消息内容
//
//	返回参数：	SEND_TYPE_OK-成功	SEND_TYPE_PUBLISH-需要重送
//
//	说明：		
//==========================================================
void OneNet_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};							//协议包
	
	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);
	
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg),MQTT_QOS_LEVEL0, 0, 1, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);					//向平台发送订阅请求
		
		MQTT_DeleteBuffer(&mqttPacket);											//删包
	}
	else
	{
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketPublish Failed\r\n");
	}

}

//==========================================================
//	函数名称：	OneNet_GetValue
//
//	函数功能：	取出属性真正的“值”节点
//
//	入口参数：	item：属性节点
//
//	返回参数：	值节点（bool / number / string），无则返回 NULL
//
//	说明：		OneNET 下发有两种写法，都要兼容：
//				扁平：  "Fan": 30
//				嵌套：  "Fan": {"value": 30, "time": 123456}
//==========================================================
static cJSON *OneNet_GetValue(cJSON *item)
{
	cJSON *v = NULL;

	if(item == NULL) return NULL;

	v = cJSON_GetObjectItem(item, "value");		//嵌套格式：取 value
	return (v != NULL) ? v : item;				//扁平格式：节点本身就是值
}

//==========================================================
//	函数名称：	OneNet_GetBool
//
//	函数功能：	把属性节点转成布尔值
//
//	入口参数：	item：属性节点
//
//	返回参数：	0/1-成功  -1-类型不支持
//
//	说明：		兼容 true/false、0/1 以及 "true"/"false"/"1"/"0"
//==========================================================
static int OneNet_GetBool(cJSON *item)
{
	cJSON *v = OneNet_GetValue(item);

	if(v == NULL)
		return -1;

	if(v->type == cJSON_True)
		return 1;

	if(v->type == cJSON_False)
		return 0;

	if(v->type == cJSON_Number)
		return (v->valuedouble != 0) ? 1 : 0;

	if(v->type == cJSON_String && v->valuestring != NULL)
	{
		if(strcmp(v->valuestring, "true") == 0 || strcmp(v->valuestring, "1") == 0)
			return 1;

		if(strcmp(v->valuestring, "false") == 0 || strcmp(v->valuestring, "0") == 0)
			return 0;
	}

	return -1;
}

//==========================================================
//	函数名称：	OneNet_GetNumber
//
//	函数功能：	把属性节点转成数值，并限制在给定范围内
//
//	入口参数：	item：属性节点
//				min/max：允许范围
//				out：解析结果
//
//	返回参数：	0-成功  -1-类型不支持
//==========================================================
static int OneNet_GetNumber(cJSON *item, double min, double max, double *out)
{
	cJSON *v = OneNet_GetValue(item);

	if(v == NULL || v->type != cJSON_Number)
		return -1;

	*out = v->valuedouble;

	if(*out < min) *out = min;
	if(*out > max) *out = max;

	return 0;
}

//==========================================================
//	函数名称：	OneNet_RevPro
//
//	函数功能：	平台下发数据解析
//
//	入口参数：	dataPtr：平台返回的数据
//
//	返回参数：	无
//
//	说明：
//==========================================================
void OneNet_RevPro(unsigned char *cmd)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};								//协议包

	char *req_payload = NULL;
	char *cmdid_topic = NULL;

	unsigned short topic_len = 0;
	unsigned short req_len = 0;

	unsigned char type = 0;
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;

	short result = 0;
	cJSON *json = NULL, *params_json = NULL, *id_json = NULL;
	cJSON *fan_json = NULL, *Mode_json = NULL, *Alarm_json = NULL;
	cJSON *Heater_json = NULL, *Humidifier_json = NULL;
	cJSON *Tempth_json = NULL, *Humth_json = NULL;
	cJSON *Temp_json = NULL, *Hum_json = NULL;
	int bval = 0;
	double dval = 0.0;
	char idBuf[40];
	char replyBuf[96];

	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_CMD:															//命令下发

			result = MQTT_UnPacketCmd(cmd, &cmdid_topic, &req_payload, &req_len);	//解出topic和消息体
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "cmdid: %s, req: %s, req_len: %d\r\n", cmdid_topic, req_payload, req_len);

				if(MQTT_PacketCmdResp(cmdid_topic, req_payload, &mqttPacket) == 0)	//命令回复组包
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send CmdResp\r\n");

					ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//回复命令
					MQTT_DeleteBuffer(&mqttPacket);									//删包
				}
			}

		break;

		case MQTT_PKT_PUBLISH:														//接收的Publish消息

			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																	cmdid_topic, topic_len, req_payload, req_len);

				/* 只处理属性设置主题，避免把平台其它消息当成属性解析 */
				if(strcmp((char *)cmdid_topic, SET_TOPIC) == 0)
				{
					json = cJSON_Parse(req_payload);

					if(json == NULL)
					{
						UsartPrintf(USART_DEBUG, "WARN:\tJSON parse failed\r\n");
					}
					else
					{
						id_json = cJSON_GetObjectItem(json, "id");
						params_json = cJSON_GetObjectItem(json, "params");

						if(params_json != NULL)
						{
							fan_json = cJSON_GetObjectItem(params_json, "Fan");
							Mode_json = cJSON_GetObjectItem(params_json, "Mode");
							Alarm_json = cJSON_GetObjectItem(params_json, "Alarm");
							Heater_json = cJSON_GetObjectItem(params_json, "Heater");
							Humidifier_json = cJSON_GetObjectItem(params_json, "Humidifier");
							Tempth_json = cJSON_GetObjectItem(params_json, "Temp_th");
							Humth_json = cJSON_GetObjectItem(params_json, "Hum_th");
							Temp_json = cJSON_GetObjectItem(params_json, "Temp");
							Hum_json = cJSON_GetObjectItem(params_json, "Hum");

							/* 工作模式：单独下发也能生效 */
							bval = OneNet_GetBool(Mode_json);
							if(bval >= 0)
							{
								Work_Mode = (uint8_t)bval;

								if(Work_Mode == 0)
								{
									Heater_flag = 0;		//清掉手动强制标志
									Humidifier_flag = 0;
								}
								//Alarm 由云端单独控制，模式切换不修改 Alarm_flag

								UsartPrintf(USART_DEBUG, "Work Mode: %s\r\n", Work_Mode ? "Manual" : "Automatic");
							}

							/* Temp/Hum 是测量值：云端即使下发也不修改传感器数据，仅记录并回复成功 */
							if(OneNet_GetNumber(Temp_json, 0.0, 100.0, &dval) == 0)
								UsartPrintf(USART_DEBUG, "WARN:\tTemp is measured, ignore set: %d\r\n", (int)(dval + 0.5));

							if(OneNet_GetNumber(Hum_json, 0.0, 100.0, &dval) == 0)
								UsartPrintf(USART_DEBUG, "WARN:\tHum is measured, ignore set: %d\r\n", (int)(dval + 0.5));

							/* 报警使能：单独下发也能生效 */
							bval = OneNet_GetBool(Alarm_json);
							if(bval >= 0)
							{
								Alarm_flag = (uint8_t)bval;
								UsartPrintf(USART_DEBUG, "Alarm Flag: %d\r\n", Alarm_flag);
							}

							/* 加热器：单独下发也能生效 */
							bval = OneNet_GetBool(Heater_json);
							if(bval >= 0)
							{
								Heater_flag = (uint8_t)bval;

								if(bval == 0)
									Heater_On = 0;		//手动模式下要能真正关断

								UsartPrintf(USART_DEBUG, "Heater Flag: %d\r\n", Heater_flag);
							}

							/* 加湿器：单独下发也能生效 */
							bval = OneNet_GetBool(Humidifier_json);
							if(bval >= 0)
							{
								Humidifier_flag = (uint8_t)bval;

								if(bval == 0)
									Hum_On = 0;

								UsartPrintf(USART_DEBUG, "Humidifier Flag: %d\r\n", Humidifier_flag);
							}

							/* 风扇档位 0~100 */
							if(OneNet_GetNumber(fan_json, 0.0, 100.0, &dval) == 0)
							{
								pwm_duty = (uint8_t)(dval + 0.5);
								PWM_SetSpeed(pwm_duty);
								UsartPrintf(USART_DEBUG, "Fan Speed: %d\r\n", pwm_duty);
							}

							/* 温度阈值 0~100 */
							if(OneNet_GetNumber(Tempth_json, 0.0, 100.0, &dval) == 0)
							{
								Temp_Thr = (uint8_t)(dval + 0.5);
								UsartPrintf(USART_DEBUG, "Temperature Threshold: %d\r\n", Temp_Thr);
							}

							/* 湿度阈值 0~100 */
							if(OneNet_GetNumber(Humth_json, 0.0, 100.0, &dval) == 0)
							{
								Hum_Thr = (uint8_t)(dval + 0.5);
								UsartPrintf(USART_DEBUG, "Humidity Threshold: %d\r\n", Hum_Thr);
							}
						}
						else
						{
							UsartPrintf(USART_DEBUG, "WARN:\tno params\r\n");
						}

						/* 无论参数是否合法都要回 set_reply，否则平台 set-device-property 报 10411 */
						idBuf[0] = '\0';
						if(id_json != NULL)
						{
							if(id_json->type == cJSON_String && id_json->valuestring != NULL)
							{
								strncpy(idBuf, id_json->valuestring, sizeof(idBuf) - 1);
								idBuf[sizeof(idBuf) - 1] = '\0';
							}
							else if(id_json->type == cJSON_Number)
							{
								sprintf(idBuf, "%d", (int)id_json->valuedouble);
							}
						}

						sprintf(replyBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", idBuf);
						OneNet_Publish(SET_REPLY_TOPIC, replyBuf);

						cJSON_Delete(json);
					}
				}
			}
			else
			{
				UsartPrintf(USART_DEBUG, "WARN:\tUnPacketPublish failed: %d\r\n", result);
			}
		break;

		case MQTT_PKT_PUBACK:														//发送Publish消息，平台回复的Ack

			if(MQTT_UnPacketPublishAck(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Publish Send OK\r\n");

		break;

		case MQTT_PKT_PUBREC:														//发送Publish消息，平台回复的Rec，设备回复Rel消息

			if(MQTT_UnPacketPublishRec(cmd) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishRec\r\n");
				if(MQTT_PacketPublishRel(MQTT_PUBLISH_ID, &mqttPacket) == 0)
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send PublishRel\r\n");
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);
					MQTT_DeleteBuffer(&mqttPacket);
				}
			}

		break;

		case MQTT_PKT_PUBREL:														//收到Publish消息，设备回复Rec，平台回复Rel，设备再回复Comp

			if(MQTT_UnPacketPublishRel(cmd, pkt_id) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishRel\r\n");
				if(MQTT_PacketPublishComp(MQTT_PUBLISH_ID, &mqttPacket) == 0)
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send PublishComp\r\n");
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);
					MQTT_DeleteBuffer(&mqttPacket);
				}
			}

		break;

		case MQTT_PKT_PUBCOMP:													//发送Publish消息，平台回复Rec，设备回复Rel，平台再回复Comp

			if(MQTT_UnPacketPublishComp(cmd) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishComp\r\n");
			}

		break;

		case MQTT_PKT_SUBACK:														//发送Subscribe消息的Ack

			if(MQTT_UnPacketSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe Err\r\n");

		break;

		case MQTT_PKT_UNSUBACK:													//发送UnSubscribe消息的Ack

			if(MQTT_UnPacketUnSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT UnSubscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT UnSubscribe Err\r\n");

		break;

		default:
			result = -1;
		break;
	}

	ESP8266_Clear();									//清空缓存

	if(result == -1)
		return;

	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}

}
