/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <inttypes.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_adc/adc_continuous.h>

#include "rtp.h"

typedef struct audio_recorder
{
    adc_continuous_handle_t adc_handle;
    QueueHandle_t stop_queue;
    bool stopping;
    TaskHandle_t task_handle;
    rtp_t rtp;
} audio_recorder_t;

void audio_recorder_init(audio_recorder_t *recorder);
esp_err_t audio_recorder_start(audio_recorder_t *recorder);
bool audio_recorder_recording(audio_recorder_t *recorder);
void audio_recorder_stop(audio_recorder_t *recorder);
void audio_recorder_deinit(audio_recorder_t *recorder);