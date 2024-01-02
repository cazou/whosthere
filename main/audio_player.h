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

#include <driver/dac_continuous.h>

#include "rtp.h"

typedef struct audio_player
{
    dac_continuous_handle_t dac_handle;
    QueueHandle_t que;
    TaskHandle_t task_handle;
    rtp_t rtp;
} audio_player_t;

void audio_player_init(audio_player_t *player);
esp_err_t audio_player_start(audio_player_t *player);
bool audio_player_playing(audio_player_t *player);
void audio_player_stop(audio_player_t *player);
void audio_player_deinit(audio_player_t *player);