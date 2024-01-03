/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <inttypes.h>
#include <string.h>
#include <sdkconfig.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/dac_continuous.h>
#include <errno.h>

#include "audio_player.h"
#include "udp.h"
#include "rtp.h"

static const char *TAG = "audio_player";
static int irq_counter = 0;

static bool IRAM_ATTR dac_on_convert_done_callback(dac_continuous_handle_t handle, const dac_event_data_t *event, void *user_data)
{
    QueueHandle_t que = (QueueHandle_t)user_data;
    BaseType_t need_awoke;
    irq_counter += 1;

    /* When the queue is full, drop the oldest item */
    if (xQueueIsQueueFullFromISR(que))
    {
        dac_event_data_t dummy;
        xQueueReceiveFromISR(que, &dummy, &need_awoke);
    }
    /* Send the event from callback */
    xQueueSendFromISR(que, event, &need_awoke);
    return need_awoke;
}

static void dac_write_data_asynchronously(dac_continuous_handle_t handle, QueueHandle_t que, uint8_t *data, size_t data_size)
{
    dac_event_data_t evt_data;
    size_t byte_written = 0;

    while (byte_written < data_size)
    {
        xQueueReceive(que, &evt_data, portMAX_DELAY);
        size_t loaded_bytes = 0;
        ESP_ERROR_CHECK(dac_continuous_write_asynchronously(handle, evt_data.buf, evt_data.buf_size,
                                                            data + byte_written, data_size - byte_written, &loaded_bytes));
        byte_written += loaded_bytes;
    }
}

static void audio_player_task(void *pvParameters)
{
    audio_player_t *player = pvParameters;
    uint8_t *buffer;
    size_t len;

    // FIXME: Maybe it should always be enabled
    ESP_ERROR_CHECK(dac_continuous_enable(player->dac_handle));
    ESP_ERROR_CHECK(dac_continuous_start_async_writing(player->dac_handle));

    while ((buffer = rtp_next_packet(&player->rtp, &len)) != NULL)
    {
        dac_write_data_asynchronously(player->dac_handle, player->que, buffer, len);
    }

    ESP_ERROR_CHECK(dac_continuous_stop_async_writing(player->dac_handle));
    ESP_ERROR_CHECK(dac_continuous_disable(player->dac_handle));

    ESP_LOGD(TAG, "Leaving...");

    uint8_t c = 1;
    xQueueSend(player->stop_queue, &c, 0);

    player->task_handle = NULL;
    vTaskDelete(NULL);
}

void audio_player_init(audio_player_t *player)
{
    player->task_handle = NULL;
    dac_continuous_config_t cont_cfg = {
        .chan_mask = DAC_CHANNEL_MASK_CH0,
        .desc_num = 4,
        .buf_size = 2048,
        .freq_hz = CONFIG_AUDIO_SAMPLE_RATE,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_APLL,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };
    /* Allocate continuous channels */
    ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfg, &player->dac_handle));

    /* Create a queue to transport the interrupt event data */
    player->que = xQueueCreate(10, sizeof(dac_event_data_t));
    assert(player->que);
    dac_event_callbacks_t cbs = {
        .on_convert_done = dac_on_convert_done_callback,
        .on_stop = NULL,
    };
    /* Must register the callback if using asynchronous writing */
    ESP_ERROR_CHECK(dac_continuous_register_event_callback(player->dac_handle, &cbs, player->que));

    rtp_init(&player->rtp, 5000, RTP_RECV);

    ESP_LOGD(TAG, "Audio player initialized at %d Hz", CONFIG_AUDIO_SAMPLE_RATE);
}

esp_err_t audio_player_start(audio_player_t *player)
{
    player->stop_queue = xQueueCreate(1, sizeof(uint8_t));
    rtp_start(&player->rtp);
    return xTaskCreate(audio_player_task, "audio_player", 4096, player, 5, &player->task_handle);
}

bool audio_player_playing(audio_player_t *player)
{
    return player->task_handle != NULL;
}

void audio_player_stop(audio_player_t *player)
{
    rtp_stop(&player->rtp);
    uint8_t c;
    xQueueReceive(player->stop_queue, &c, portMAX_DELAY);
    vQueueDelete(player->stop_queue);
}

void audio_player_deinit(audio_player_t *player)
{
    vQueueDelete(player->que);
    dac_continuous_del_channels(player->dac_handle);
    rtp_deinit(&player->rtp);
}
