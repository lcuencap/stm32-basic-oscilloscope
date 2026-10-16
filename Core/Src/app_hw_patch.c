#include "app_hw_patch.h"

static void AppHwPatch_AdcDmaDoubleBufferCplt(DMA_HandleTypeDef *hdma);
static void AppHwPatch_AdcDmaDoubleBufferError(DMA_HandleTypeDef *hdma);

HAL_StatusTypeDef AppHwPatch_ApplyAdcTriggeredRegularConfig(
    ADC_HandleTypeDef *hadc,
    const ADC_ChannelConfTypeDef *channel_config
) {
    HAL_StatusTypeDef status;

    hadc->Init.ContinuousConvMode = DISABLE;
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISINGFALLING;
    hadc->Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    hadc->Init.DMAContinuousRequests = ENABLE;
    hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    status = HAL_ADC_Init(hadc);
    if (status != HAL_OK) {
        return status;
    }

    return HAL_ADC_ConfigChannel(hadc, (ADC_ChannelConfTypeDef *)channel_config);
}

HAL_StatusTypeDef AppHwPatch_ApplyDmaNormalMode(DMA_HandleTypeDef *hdma) {
    HAL_StatusTypeDef status;

    /* DMA_CIRCULAR requerit per doble buffer continu amb HAL_DMAEx_MultiBufferStart_IT. */
    hdma->Init.Mode = DMA_CIRCULAR;

    status = HAL_DMA_DeInit(hdma);
    if ((status != HAL_OK) && (status != HAL_ERROR)) {
        return status;
    }

    return HAL_DMA_Init(hdma);
}

HAL_StatusTypeDef AppHwPatch_ConfigureTim2Ch2ForAdcTrigger(TIM_HandleTypeDef *htim) {
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    HAL_StatusTypeDef status;

    status = HAL_TIM_OC_Init(htim);
    if (status != HAL_OK) {
        return status;
    }

    /* OC2REF es commuta a cada compare i surt per TRGO cap als ADC. */
    sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
    sConfigOC.Pulse = 0U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    status = HAL_TIM_OC_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_2);
    if (status != HAL_OK) {
        return status;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC2REF;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    return HAL_TIMEx_MasterConfigSynchronization(htim, &sMasterConfig);
}

HAL_StatusTypeDef AppHwPatch_StartAdcDoubleBufferedDma(
    ADC_HandleTypeDef *hadc,
    uint16_t *mem0,
    uint16_t *mem1,
    uint32_t length
) {
    __IO uint32_t counter = 0U;
    ADC_Common_TypeDef *tmpADC_Common;
    HAL_StatusTypeDef status;
    DMA_HandleTypeDef *hdma;

    hdma = hadc->DMA_Handle;

    if ((hdma == NULL) || (mem0 == NULL) || (mem1 == NULL) || (length == 0U)) {
        return HAL_ERROR;
    }

    __HAL_LOCK(hadc);

    if ((hadc->Instance->CR2 & ADC_CR2_ADON) != ADC_CR2_ADON) {
        __HAL_ADC_ENABLE(hadc);

        counter = (ADC_STAB_DELAY_US * (SystemCoreClock / 1000000U));
        while (counter != 0U) {
            counter--;
        }
    }

    if ((hadc->Instance->CR2 & ADC_CR2_DMA) == ADC_CR2_DMA) {
        CLEAR_BIT(hadc->Instance->CR2, ADC_CR2_DMA);
    }

    if (HAL_IS_BIT_CLR(hadc->Instance->CR2, ADC_CR2_ADON)) {
        __HAL_UNLOCK(hadc);
        return HAL_ERROR;
    }

    ADC_STATE_CLR_SET(
        hadc->State,
        HAL_ADC_STATE_READY | HAL_ADC_STATE_REG_EOC | HAL_ADC_STATE_REG_OVR,
        HAL_ADC_STATE_REG_BUSY
    );

    if (READ_BIT(hadc->Instance->CR1, ADC_CR1_JAUTO) != RESET) {
        ADC_STATE_CLR_SET(hadc->State, HAL_ADC_STATE_INJ_EOC, HAL_ADC_STATE_INJ_BUSY);
    }

    if (HAL_IS_BIT_SET(hadc->State, HAL_ADC_STATE_INJ_BUSY)) {
        CLEAR_BIT(hadc->ErrorCode, (HAL_ADC_ERROR_OVR | HAL_ADC_ERROR_DMA));
    } else {
        ADC_CLEAR_ERRORCODE(hadc);
    }

    __HAL_UNLOCK(hadc);

    tmpADC_Common = ADC_COMMON_REGISTER(hadc);

    hdma->XferCpltCallback = AppHwPatch_AdcDmaDoubleBufferCplt;
    hdma->XferM1CpltCallback = AppHwPatch_AdcDmaDoubleBufferCplt;
    hdma->XferHalfCpltCallback = NULL;
    hdma->XferM1HalfCpltCallback = NULL;
    hdma->XferErrorCallback = AppHwPatch_AdcDmaDoubleBufferError;

    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_EOC | ADC_FLAG_OVR);
    __HAL_ADC_ENABLE_IT(hadc, ADC_IT_OVR);
    SET_BIT(hadc->Instance->CR2, ADC_CR2_DMA);

    /* Reiniciem CT/DBM per saber que el primer bloc omple M0. */
    CLEAR_BIT(hdma->Instance->CR, DMA_SxCR_DBM | DMA_SxCR_CT);

    status = HAL_DMAEx_MultiBufferStart_IT(
        hdma,
        (uint32_t)&hadc->Instance->DR,
        (uint32_t)mem0,
        (uint32_t)mem1,
        length
    );

    if (status != HAL_OK) {
        CLEAR_BIT(hadc->Instance->CR2, ADC_CR2_DMA);
        __HAL_ADC_DISABLE_IT(hadc, ADC_IT_OVR);
        ADC_STATE_CLR_SET(
            hadc->State,
            HAL_ADC_STATE_REG_BUSY | HAL_ADC_STATE_INJ_BUSY,
            HAL_ADC_STATE_READY
        );
        return status;
    }

    if (HAL_IS_BIT_CLR(tmpADC_Common->CCR, ADC_CCR_MULTI)) {
#if defined(ADC2) && defined(ADC3)
        if ((hadc->Instance == ADC1) ||
            ((hadc->Instance == ADC2) && ((ADC->CCR & ADC_CCR_MULTI_Msk) < ADC_CCR_MULTI_0)) ||
            ((hadc->Instance == ADC3) && ((ADC->CCR & ADC_CCR_MULTI_Msk) < ADC_CCR_MULTI_4))) {
#endif
            if ((hadc->Instance->CR2 & ADC_CR2_EXTEN) == RESET) {
                hadc->Instance->CR2 |= (uint32_t)ADC_CR2_SWSTART;
            }
#if defined(ADC2) && defined(ADC3)
        }
#endif
    }

    return HAL_OK;
}

static void AppHwPatch_AdcDmaDoubleBufferCplt(DMA_HandleTypeDef *hdma) {
    ADC_HandleTypeDef *hadc;

    hadc = (ADC_HandleTypeDef *)hdma->Parent;

    if (HAL_IS_BIT_CLR(hadc->State, HAL_ADC_STATE_ERROR_INTERNAL | HAL_ADC_STATE_ERROR_DMA)) {
        SET_BIT(hadc->State, HAL_ADC_STATE_REG_EOC);
        HAL_ADC_ConvCpltCallback(hadc);
        return;
    }

    AppHwPatch_AdcDmaDoubleBufferError(hdma);
}

static void AppHwPatch_AdcDmaDoubleBufferError(DMA_HandleTypeDef *hdma) {
    ADC_HandleTypeDef *hadc;

    hadc = (ADC_HandleTypeDef *)hdma->Parent;
    hadc->State = HAL_ADC_STATE_ERROR_DMA;
    hadc->ErrorCode |= HAL_ADC_ERROR_DMA;
    HAL_ADC_ErrorCallback(hadc);
}
