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

#if __BYTE_ORDER == __LITTLE_ENDIAN
struct rtp_header
{
    uint8_t cc : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t pt : 7;
    uint8_t mark : 1;
    uint16_t sequence_number;
    uint32_t ts;
    uint32_t ssrc;
};
#else
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
#endif

const int BUF_COUNT = 5;
const int BUF_SIZE = 1400;

struct rtp_packet
{
    size_t len;
    uint8_t *data;
};

void rtp_init(rtp_t *rtp, uint16_t port, enum rtp_direction direction)
{
    rtp->first_packet = 1;
    rtp->last_seq = 0;
    rtp->sent_bytes = 0;
    rtp->direction = direction;

    audio_udp_init(&rtp->udp, port);

    if (direction == RTP_RECV)
    {
        rtp->queue = xQueueCreate(5, sizeof(struct rtp_packet)); // TODO: Port to using a bytebuf ringbuffer
        audio_udp_bind(&rtp->udp);
    }
    else
    {
        rtp->ring_buffer = xRingbufferCreate(BUF_COUNT * BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    }
}

void rtp_deinit(rtp_t *rtp)
{
    if (rtp->direction == RTP_RECV)
    {
        vQueueDelete(rtp->queue);
    }
    else
    {
        vRingbufferDelete(rtp->ring_buffer);
    }

    // udp_deinit(&rtp->udp); // Check again later
}

static int push_packet(rtp_t *rtp, uint8_t *data, size_t length)
{
    struct rtp_header *hdr = (struct rtp_header *)data;
    int32_t seq_num;

    if (hdr->version != 2)
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
    {
        rtp->first_packet = 0;
    }
    else if ((int32_t)seq_num - rtp->last_seq < 1)
    {
        ESP_LOGW(TAG, "Packets are not in order");
    }
    else if ((int32_t)seq_num - rtp->last_seq > 1)
    {
        ESP_LOGW(TAG, "Dropped %ld rtp packets", (seq_num - rtp->last_seq) - 1);
    }

    rtp->last_seq = seq_num;

    ESP_LOGD(TAG, "RTP Packet: v: %u p: %s e: %s seq: %ld", hdr->version, hdr->padding ? "true" : "false", hdr->extension ? "true" : "false", seq_num);

    // use a ringbuffer ?
    struct rtp_packet p;
    p.data = data + RTP_HEADER_LEN;
    p.len = length - RTP_HEADER_LEN;
    xQueueSend(rtp->queue, &p, portMAX_DELAY);

    return 0;
}

#define MAX_PACKET_LEN 1400
#define MIN(a, b) (a) < (b) ? (a) : (b)

// TODO: May need restrict
static void pack_rtp(rtp_t *rtp, uint8_t *bytes, size_t len, uint8_t *rtp_packet, size_t *consumed, size_t *packet_size)
{
    *packet_size = MIN(MAX_PACKET_LEN, len + RTP_HEADER_LEN);
    *consumed = *packet_size - RTP_HEADER_LEN;

    struct rtp_header *p = (struct rtp_header *)rtp_packet;
    memset(p, 0, sizeof(*p));

    p->version = 2;
    p->sequence_number = htons(++(rtp->last_seq));
    p->ts = htonl((rtp->sent_bytes * 1000) / CONFIG_AUDIO_SAMPLE_RATE);
    rtp->sent_bytes += *consumed;
    p->pt = 96;
    p->ssrc = htonl(0x42245987);

    memcpy(rtp_packet + RTP_HEADER_LEN, bytes, *consumed);
}

static void rtp_recv_task(void *pvParameters)
{
    rtp_t *rtp = (rtp_t *)pvParameters;
    int len;

    ESP_LOGD(TAG, "Starting recv task");

    uint8_t buf_ring[BUF_COUNT][BUF_SIZE];
    // memset(buf_ring, 0, BUF_COUNT * BUF_SIZE);
    uint8_t curr_buffer = 0;

    while ((len = udp_next(&rtp->udp, buf_ring[curr_buffer], BUF_SIZE)) > 0)
    {
        if (rtp->stop_requested)
            break;

        if (len < 0 && errno == -EAGAIN)
        {
            // Timed out
            continue;
        }
        int ret = push_packet(rtp, buf_ring[curr_buffer], len);
        if (ret != 0)
            continue;

        curr_buffer = (curr_buffer + 1) % BUF_COUNT;
    }

    ESP_LOGD(TAG, "Leaving...");

    rtp->task_handle = NULL;
    vTaskDelete(NULL);
}

static void rtp_send_task(void *pvParameters)
{
    rtp_t *rtp = (rtp_t *)pvParameters;
    size_t len;
    void *item = NULL;
    uint8_t rtp_data[MAX_PACKET_LEN];

    ESP_LOGD(TAG, "Starting send task");

    while ((item = xRingbufferReceive(rtp->ring_buffer, &len, portMAX_DELAY)) != NULL)
    {
        uint8_t *buf = item;

        while (len > 0)
        {
            size_t rtp_len;
            size_t bytes_consumed;
            pack_rtp(rtp, buf, len, rtp_data, &bytes_consumed, &rtp_len);
            udp_send_bytes(&rtp->udp, rtp_data, rtp_len);
            buf += bytes_consumed;
            len -= bytes_consumed;
        }

        vRingbufferReturnItem(rtp->ring_buffer, item);
    }

    ESP_LOGD(TAG, "Leaving...");
}

void rtp_push_data(rtp_t *rtp, const uint8_t *data, size_t length)
{
    xRingbufferSend(rtp->ring_buffer, data, length, (TickType_t)portMAX_DELAY);
}

esp_err_t rtp_start(rtp_t *rtp)
{
    rtp->stop_requested = false;

    if (rtp->direction == RTP_RECV)
        return xTaskCreate(rtp_recv_task, "rtp_recv", 4096 + (BUF_COUNT * BUF_SIZE) * sizeof(StackType_t), rtp, 5, &rtp->task_handle);
    else
        return xTaskCreate(rtp_send_task, "rtp_send", 4096 + (BUF_COUNT * BUF_SIZE) * sizeof(StackType_t), rtp, 5, &rtp->task_handle);
}

void rtp_stop(rtp_t *rtp)
{
    if (rtp->direction == RTP_RECV)
    {
        struct rtp_packet p = {0};
        xQueueSend(rtp->queue, &p, (TickType_t)portMAX_DELAY);
        rtp->stop_requested = true;
    }
    else
    {
        vTaskDelete(rtp->task_handle);
        rtp->task_handle = NULL;
    }

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