/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "udp.h"

typedef struct rtp
{
    QueueHandle_t queue;
    TaskHandle_t task_handle;
    int32_t last_seq;
    uint8_t first_packet;
    udp_t udp;
} rtp_t;

void rtp_init(rtp_t *rtp, u_int16_t port);
esp_err_t rtp_start(rtp_t *rtp);
void rtp_stop(rtp_t *rtp);
uint8_t *rtp_next_packet(rtp_t *rtp, size_t *length);
