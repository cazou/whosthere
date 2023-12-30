/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

typedef struct udp
{
    int sock;
} udp_t;

void audio_udp_init(udp_t *udp);
void udp_stop(udp_t *udp);
int audio_udp_bind(udp_t *udp, uint16_t port);
int udp_next(udp_t *udp, uint8_t *data, size_t max_size);