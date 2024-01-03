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
#include <freertos/ringbuf.h>

#include "udp.h"

enum rtp_direction
{
    RTP_SEND,
    RTP_RECV
};

typedef struct rtp
{
    QueueHandle_t queue;
    RingbufHandle_t ring_buffer;
    TaskHandle_t task_handle;
    enum rtp_direction direction;
    int32_t last_seq;
    uint8_t first_packet;
    uint64_t sent_bytes;
    bool stop_requested;
    udp_t udp;
} rtp_t;

void rtp_init(rtp_t *rtp, u_int16_t port, enum rtp_direction);
esp_err_t rtp_start(rtp_t *rtp);
void rtp_stop(rtp_t *rtp);
void rtp_deinit(rtp_t *rtp);
uint8_t *rtp_next_packet(rtp_t *rtp, size_t *length);
void rtp_push_data(rtp_t *rtp, const uint8_t *data, size_t length);
