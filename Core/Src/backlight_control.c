#include "backlight_control.h"

#define BACKLIGHT_MIN_PERCENT      20U
#define BACKLIGHT_MAX_PERCENT      100U
#define BACKLIGHT_STEP_PERCENT     20U
#define BACKLIGHT_DEFAULT_PERCENT  60U
#define BACKLIGHT_PWM_PERIOD       999U

static uint8_t backlight_percent = BACKLIGHT_DEFAULT_PERCENT;

static void Backlight_Apply(void)
{
    TIM3->CCR1 = ((uint32_t)(BACKLIGHT_PWM_PERIOD + 1U) * backlight_percent) / 100U;
}

void Backlight_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    GPIO_InitStruct.Pin = LCD_PWM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM3;
    HAL_GPIO_Init(LCD_PWM_GPIO_Port, &GPIO_InitStruct);

    TIM3->PSC = 47U;                 /* 48MHz / 48 = 1MHz timer clock */
    TIM3->ARR = BACKLIGHT_PWM_PERIOD; /* 1kHz PWM */
    TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
    TIM3->CCMR1 |= (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
    TIM3->CCER |= TIM_CCER_CC1E;
    TIM3->CR1 |= TIM_CR1_ARPE;
    Backlight_Apply();
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 |= TIM_CR1_CEN;
}

void Backlight_SetPercent(uint8_t percent)
{
    if (percent < BACKLIGHT_MIN_PERCENT) {
        percent = BACKLIGHT_MIN_PERCENT;
    }
    if (percent > BACKLIGHT_MAX_PERCENT) {
        percent = BACKLIGHT_MAX_PERCENT;
    }
    backlight_percent = percent;
    Backlight_Apply();
}

void Backlight_Increase(void)
{
    Backlight_SetPercent((uint8_t)(backlight_percent + BACKLIGHT_STEP_PERCENT));
}

void Backlight_Decrease(void)
{
    if (backlight_percent <= (BACKLIGHT_MIN_PERCENT + BACKLIGHT_STEP_PERCENT)) {
        Backlight_SetPercent(BACKLIGHT_MIN_PERCENT);
    } else {
        Backlight_SetPercent((uint8_t)(backlight_percent - BACKLIGHT_STEP_PERCENT));
    }
}

uint8_t Backlight_GetPercent(void)
{
    return backlight_percent;
}
