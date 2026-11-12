#include "bsp_pwm.h"

/**
 * @brief  PB9 PWM初始化 频率固定
 */
void PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;

    RCC_APB2PeriphClockCmd(PWM_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(PWM_TIM_RCC, ENABLE);

    // PB9 复用推挽输出
    GPIO_InitStruct.GPIO_Pin = PWM_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStruct);

    // 时基: ARR=99  PSC=72  档位0~99 对应占空比0~100%
    TIM_TimeBaseStruct.TIM_Period = 99;
    TIM_TimeBaseStruct.TIM_Prescaler = 72;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(PWM_TIM, &TIM_TimeBaseStruct);

    // PWM1模式
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC4Init(PWM_TIM, &TIM_OCInitStruct);

    TIM_Cmd(PWM_TIM, ENABLE);
}

/**
 * @brief  设置风扇档位 范围 0~100
 * @param  level: 档位值 0(停) ~ 100(最大)
 */
void PWM_SetSpeed(u8 level)
{
    u16 duty;
    // 边界限制
    if(level > 100) level = 100;
    // 映射: 0~100 -> 0~99
    duty = (u16)level * 99 / 100;
    TIM_SetCompare4(PWM_TIM, duty);
}



/**
 * @brief  把 0~100 的任意值归到三挡
 * @param  duty: 0~100
 * @retval 0 / 30 / 60 / 100
 */
uint8_t PWM_SnapFanLevel(uint8_t duty)
{
    if(duty == 0)
        return FAN_LEVEL_OFF;

    if(duty < (FAN_LEVEL_LOW + FAN_LEVEL_MID) / 2)
        return FAN_LEVEL_LOW;

    if(duty < (FAN_LEVEL_MID + FAN_LEVEL_HIGH) / 2)
        return FAN_LEVEL_MID;

    return FAN_LEVEL_HIGH;
}
