/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rtp.h"

#include <errno.h>
#include <string.h>
#include <esp_log.h>
#include <stdio.h>
#include <arpa/inet.h>

static const char *TAG = "rtp";

#define RTP_HEADER_LEN 12

struct rtp_header
{
    uint8_t version : 2;
    uint8_t padding : 1;
    uint8_t extension : 1;
    uint8_t cc : 4;
    uint8_t mark : 1;
    uint8_t pt : 7;
    uint16_t sequence_number;
    uint32_t ts;
    uint32_t ssrc;
};

void rtp_init(rtp_t *rtp)
{
    rtp->next_len = 0;
    rtp->first_packet = 1;
    rtp->last_seq = 0;
}

int rtp_push_packet(rtp_t *rtp, uint8_t *data, size_t length)
{
    struct rtp_header *hdr = (struct rtp_header *)data;
    int32_t seq_num;

    if (rtp->next_len)
        return -EBUSY;

    if (hdr->version != 0)
    {
        ESP_LOGE(TAG, "Unsupported RTP version: %u ", hdr->version);
        return -EINVAL;
    }

    if (hdr->extension)
    {
        ESP_LOGE(TAG, "RTP extensions are not supported");
        return -EINVAL;
    }

    if (hdr->padding)
    {
        ESP_LOGW(TAG, "Padding is not supported, expect artifacts in output");
    }

    seq_num = (int32_t)ntohs(hdr->sequence_number);

    if (rtp->first_packet)
        rtp->first_packet = 0;
    else if ((int32_t)seq_num - rtp->last_seq < 1)
        ESP_LOGW(TAG, "Packets are not in order");
    else if ((int32_t)seq_num - rtp->last_seq > 1)
        ESP_LOGW(TAG, "Dropped %ld rtp packets", (seq_num - rtp->last_seq) - 1);

    rtp->last_seq = seq_num;

    ESP_LOGD(TAG, "RTP Packet: v: %u p: %s e: %s seq: %ld", hdr->version, hdr->padding ? "true" : "false", hdr->extension ? "true" : "false", seq_num);

    rtp->next_data = data + RTP_HEADER_LEN;
    rtp->next_len = length - RTP_HEADER_LEN;

    return 0;
}

uint8_t *rtp_next_data(rtp_t *rtp, size_t *length)
{
    if (rtp->next_len == 0)
        return NULL;

    *length = rtp->next_len;

    rtp->next_len = 0;

    return rtp->next_data;
}