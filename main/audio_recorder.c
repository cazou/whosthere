/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio_recorder.h"

#include <string.h>
#include <stdio.h>
#include <sdkconfig.h>
#include <stdint.h>
#include <esp_log.h>

#define ADC_BIT_WIDTH 12 // (8 might is not supported) FIXME: Could read 12 bits per sample and encode on 16 bit audio. TBD

#define ADC_READ_LEN 1388 * SOC_ADC_DIGI_RESULT_BYTES // Read a complete RTP packet at once

static adc_channel_t channel = ADC_CHANNEL_6; // VDET_1 / GPIO34

static TaskHandle_t s_task_handle;
static const char *TAG = "audio_recorder";

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;

    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

void audio_recorder_init(audio_recorder_t *recorder)
{
    recorder->task_handle = NULL;
    recorder->adc_handle = NULL;

    rtp_init(&recorder->rtp, 5000, RTP_SEND);

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = ADC_READ_LEN,
        .conv_frame_size = ADC_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &recorder->adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = CONFIG_AUDIO_SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    adc_digi_pattern_config_t adc_pattern = {0};
    dig_cfg.pattern_num = 1;

    adc_pattern.atten = ADC_ATTEN_DB_11;
    adc_pattern.channel = channel & 0x7;
    adc_pattern.unit = ADC_UNIT_1;
    adc_pattern.bit_width = ADC_BIT_WIDTH;

    ESP_LOGI(TAG, "adc_pattern.atten is :%" PRIx8, adc_pattern.atten);
    ESP_LOGI(TAG, "adc_pattern.channel is :%" PRIx8, adc_pattern.channel);
    ESP_LOGI(TAG, "adc_pattern.unit is :%" PRIx8, adc_pattern.unit);

    dig_cfg.adc_pattern = &adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(recorder->adc_handle, &dig_cfg));
}

void audio_recorder_task(void *data)
{
    esp_err_t ret;
    uint32_t ret_num = 0;

    uint8_t result[ADC_READ_LEN] = {0};
    uint8_t raw_data[ADC_READ_LEN / 2] = {0};

    audio_recorder_t *recorder = data;

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(recorder->adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(recorder->adc_handle));

    while (1)
    {
        ret = adc_continuous_read(recorder->adc_handle, result, ADC_READ_LEN, &ret_num, 20);

        if (recorder->stopping)
        {
            break;
        }

        // ESP_LOGI(TAG, "ret: %d, ret_num: %" PRIu32 " bytes", ret, ret_num);
        if (ret == ESP_OK)
        {
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
            {
                adc_digi_output_data_t *p = (void *)&result[i];
                uint32_t chan_num = p->type1.channel;
                uint32_t data = p->type1.data;
                /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
                if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1))
                {
                    // Currently only working with 8 bits samples, so only keeping 8 of the 12 data bits (MSB)
                    raw_data[i / SOC_ADC_DIGI_RESULT_BYTES] = (data >> 4);
                }
            }

            rtp_push_data(&recorder->rtp, raw_data, ret_num / SOC_ADC_DIGI_RESULT_BYTES);
        }
        else if (ret == ESP_ERR_TIMEOUT)
        {
            // FIXME: What should be done here ?
            continue;
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(recorder->adc_handle));

    uint8_t c = 1;
    xQueueSend(recorder->stop_queue, &c, 0);

    recorder->task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_recorder_start(audio_recorder_t *recorder)
{
    recorder->stopping = 0;
    recorder->stop_queue = xQueueCreate(1, sizeof(uint8_t));
    rtp_start(&recorder->rtp);
    return xTaskCreate(audio_recorder_task, "audio_recorder", 4096 + (ADC_READ_LEN * sizeof(StackType_t)), recorder, 5, &recorder->task_handle);
}

bool audio_recorder_recording(audio_recorder_t *recorder)
{
    return recorder->task_handle != NULL;
}
void audio_recorder_stop(audio_recorder_t *recorder)
{
    uint8_t c = 1;
    recorder->stopping = 1;
    xQueueReceive(recorder->stop_queue, &c, portMAX_DELAY);
    rtp_stop(&recorder->rtp);

    vQueueDelete(recorder->stop_queue);
}

void audio_recorder_deinit(audio_recorder_t *recorder)
{
    ESP_ERROR_CHECK(adc_continuous_deinit(recorder->adc_handle));
    rtp_deinit(&recorder->rtp);
}
