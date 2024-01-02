/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <lwip/sockets.h>

typedef struct udp
{
    int sock;
    struct sockaddr_in dest_addr;
} udp_t;

int audio_udp_init(udp_t *udp, uint16_t port);
void udp_stop(udp_t *udp);
int audio_udp_bind(udp_t *udp);
int udp_next(udp_t *udp, uint8_t *data, size_t max_size);
int udp_send_bytes(udp_t *udp, const uint8_t *data, size_t size);