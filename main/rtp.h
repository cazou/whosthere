/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <inttypes.h>
#include <stddef.h>

typedef struct rtp
{
    size_t next_len;
    uint8_t *next_data;
    int32_t last_seq;
    uint8_t first_packet;
} rtp_t;

void rtp_init(rtp_t *rtp);
int rtp_push_packet(rtp_t *rtp, uint8_t *data, size_t length);
uint8_t *rtp_next_data(rtp_t *rtp, size_t *length);
