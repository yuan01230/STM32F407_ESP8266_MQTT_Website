#include "audio_test.h"

#include <stdio.h>

#include "../wm8978/wm8978.h"

#define AUDIO_TEST_REPEAT_COUNT     8U
#define AUDIO_TEST_SINE_POINTS      32U
#define AUDIO_TEST_FRAME_COUNT      (AUDIO_TEST_SINE_POINTS * AUDIO_TEST_REPEAT_COUNT)
#define AUDIO_TEST_WORD_COUNT       (AUDIO_TEST_FRAME_COUNT * 2U)
#define AUDIO_TEST_SQUARE_AMPLITUDE 28000

/* 音频硬件 bring-up 测试模块。
 * 目的不是播放音乐，而是用一段固定 PCM 方波验证：
 * WM8978 软件 IIC 控制、I2S2 时钟/数据、DMA 传输和耳机输出是否全部打通。
 */
static int16_t audio_test_buffer[AUDIO_TEST_WORD_COUNT];
static uint8_t audio_test_buffer_ready = 0U;

/**
 * @brief 打印 I2S2 和 TX DMA 的运行状态。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 无。
 * @note 通过比较两次 DMA NDTR 计数判断 DMA 是否正在持续搬运音频数据。
 */
static void AudioTest_PrintI2SDiagnostics(I2S_HandleTypeDef *hi2s)
{
    uint32_t ndtr_0 = 0U;
    uint32_t ndtr_1 = 0U;
    uint32_t dma_state = 0U;
    uint32_t dma_error = 0U;

    /* 连续读取两次 NDTR，如果计数发生变化，说明 DMA 正在搬运 I2S 数据。 */
    if ((hi2s != NULL) && (hi2s->hdmatx != NULL))
    {
        ndtr_0 = __HAL_DMA_GET_COUNTER(hi2s->hdmatx);
        HAL_Delay(50U);
        ndtr_1 = __HAL_DMA_GET_COUNTER(hi2s->hdmatx);
        dma_state = (uint32_t)HAL_DMA_GetState(hi2s->hdmatx);
        dma_error = HAL_DMA_GetError(hi2s->hdmatx);
    }

    printf("[Audio] I2S state=%u err=0x%08lX DMA state=%lu err=0x%08lX NDTR=%lu->%lu\r\n",
           (unsigned int)HAL_I2S_GetState(hi2s),
           (unsigned long)HAL_I2S_GetError(hi2s),
           (unsigned long)dma_state,
           (unsigned long)dma_error,
           (unsigned long)ndtr_0,
           (unsigned long)ndtr_1);

    if ((hi2s != NULL) && (hi2s->Instance != NULL))
    {
        printf("[Audio] I2S cfg=0x%04lX psc=0x%04lX\r\n",
               (unsigned long)hi2s->Instance->I2SCFGR,
               (unsigned long)hi2s->Instance->I2SPR);
    }
}

/**
 * @brief 生成左右声道相同的方波测试 PCM 缓冲。
 * @param 无。
 * @return 无。
 * @note 该缓冲只用于硬件链路验证，幅度较大，不作为正式音乐播放数据。
 */
static void AudioTest_FillToneBuffer(void)
{
    uint32_t frame;

    /* 左右声道写入相同样本，耳机两边都应听到稳定测试音。 */
    for (frame = 0U; frame < AUDIO_TEST_FRAME_COUNT; frame++)
    {
        int16_t sample = ((frame / 8U) & 1U) ? AUDIO_TEST_SQUARE_AMPLITUDE : -AUDIO_TEST_SQUARE_AMPLITUDE;
        audio_test_buffer[(frame * 2U) + 0U] = sample;
        audio_test_buffer[(frame * 2U) + 1U] = sample;
    }

    audio_test_buffer_ready = 1U;
    printf("[Audio] Tone buffer square amplitude=%d words=%u\r\n",
           AUDIO_TEST_SQUARE_AMPLITUDE,
           (unsigned int)AUDIO_TEST_WORD_COUNT);
}

/**
 * @brief 启动 WM8978 + I2S2 DMA 方波测试音。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 0 表示启动成功；1 表示 I2S 句柄为空；2 表示 WM8978 初始化失败；3 表示 I2S DMA 启动失败。
 * @note 该函数用于验证音频硬件链路，正式 WAV/MP3 播放不依赖持续 tone。
 */
uint8_t AudioTest_StartTone(I2S_HandleTypeDef *hi2s)
{
    uint8_t ret;

    if (hi2s == NULL)
    {
        return 1U;
    }

    if (audio_test_buffer_ready == 0U)
    {
        AudioTest_FillToneBuffer();
    }

    /* 播放测试音前先初始化 WM8978，确保 DAC、耳机输出和 I2S 接口处于已知状态。 */
    printf("[Audio] Initializing WM8978...\r\n");
    ret = WM8978_Init();
    if (ret != 0U)
    {
        printf("[Audio] WM8978 init failed, code=%u\r\n", ret);
        return 2U;
    }
    printf("[Audio] WM8978 active addr=0x%02X\r\n", WM8978_GetActiveAddress());

    /* 测试音使用 DMA 循环输出，适合观察 I2S/DMA 是否稳定工作。 */
    printf("[Audio] Starting I2S2 DMA tone test...\r\n");
    if (HAL_I2S_Transmit_DMA(hi2s,
                             (uint16_t *)audio_test_buffer,
                             AUDIO_TEST_WORD_COUNT) != HAL_OK)
    {
        printf("[Audio] I2S2 DMA start failed\r\n");
        return 3U;
    }

    AudioTest_PrintI2SDiagnostics(hi2s);
    printf("[Audio] Tone test running\r\n");
    return 0U;
}

/**
 * @brief 停止方波测试音 DMA 输出。
 * @param hi2s I2S 句柄，允许传入 NULL；NULL 时函数不执行任何操作。
 * @return 无。
 */
void AudioTest_StopTone(I2S_HandleTypeDef *hi2s)
{
    if (hi2s != NULL)
    {
        (void)HAL_I2S_DMAStop(hi2s);
    }
}
