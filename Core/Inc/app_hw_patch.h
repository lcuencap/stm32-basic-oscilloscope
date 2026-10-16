#ifndef INC_APP_HW_PATCH_H_
#define INC_APP_HW_PATCH_H_

#include "main.h"

/* Reaplica la configuracio sensible dels periferics fora del codi generat. */
HAL_StatusTypeDef AppHwPatch_ApplyAdcTriggeredRegularConfig(
    ADC_HandleTypeDef *hadc,
    const ADC_ChannelConfTypeDef *channel_config
);

HAL_StatusTypeDef AppHwPatch_ApplyDmaNormalMode(DMA_HandleTypeDef *hdma);

HAL_StatusTypeDef AppHwPatch_ConfigureTim2Ch2ForAdcTrigger(TIM_HandleTypeDef *htim);

HAL_StatusTypeDef AppHwPatch_StartAdcDoubleBufferedDma(
    ADC_HandleTypeDef *hadc,
    uint16_t *mem0,
    uint16_t *mem1,
    uint32_t length
);

#endif /* INC_APP_HW_PATCH_H_ */
