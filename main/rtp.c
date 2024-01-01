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

const int BUF_COUNT = 5;
const int BUF_SIZE = 1400;

struct rtp_packet
{
    size_t len;
    uint8_t *data;
};

void rtp_init(rtp_t *rtp, uint16_t port)
{
    rtp->first_packet = 1;
    rtp->last_seq = 0;

    rtp->queue = xQueueCreate(5, sizeof(struct rtp_packet));

    audio_udp_init(&rtp->udp);
    audio_udp_bind(&rtp->udp, port);
}

static int push_packet(rtp_t *rtp, uint8_t *data, size_t length)
{
    struct rtp_header *hdr = (struct rtp_header *)data;
    int32_t seq_num;

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

    // use a queue
    struct rtp_packet p;
    p.data = data + RTP_HEADER_LEN;
    p.len = length - RTP_HEADER_LEN;
    xQueueSend(rtp->queue, &p, (TickType_t)portMAX_DELAY);

    return 0;
}
static void rtp_task(void *pvParameters)
{
    rtp_t *rtp = (rtp_t *)pvParameters;
    int len;

    ESP_LOGI(TAG, "Starting task");

    uint8_t buf_ring[BUF_COUNT][BUF_SIZE];
    uint8_t curr_buffer = 0;

    ESP_LOGI(TAG, "Starting loop");

    while ((len = udp_next(&rtp->udp, buf_ring[curr_buffer], BUF_SIZE)) > 0)
    {
        int ret = push_packet(rtp, buf_ring[curr_buffer], len);
        if (ret != 0)
            continue;

        curr_buffer = (curr_buffer + 1) % BUF_COUNT;
    }
    ESP_LOGI(TAG, "Leaving...");
    vQueueDelete(rtp->queue);
    vTaskDelete(NULL);
}

esp_err_t rtp_start(rtp_t *rtp)
{
    return xTaskCreate(rtp_task, "rtp", 4096 + (BUF_COUNT * BUF_SIZE) * sizeof(StackType_t), rtp, 5, &rtp->task_handle);
}

void rtp_stop(rtp_t *rtp)
{
    struct rtp_packet p = {0};
    xQueueSend(rtp->queue, &p, (TickType_t)portMAX_DELAY);
    udp_stop(&rtp->udp);
}

uint8_t *rtp_next_packet(rtp_t *rtp, size_t *length)
{
    struct rtp_packet p;
    BaseType_t ret = xQueueReceive(rtp->queue, &p, (TickType_t)portMAX_DELAY);
    if (ret == errQUEUE_EMPTY)
    {
        ESP_LOGE(TAG, "Packet queue receive timed out.");
        return NULL;
    }
    else if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Other error");
        return NULL;
    }

    *length = p.len;
    return p.data;
}