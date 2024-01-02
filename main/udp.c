/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include <sys/param.h>
#include <esp_netif.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#include "udp.h"

static const char *TAG = "UDP";

#define DEST "10.42.0.1" // TODO: Make this configurable. This will more likely come from the listen command on the TCP control connection later

int audio_udp_init(udp_t *udp, uint16_t port)
{
    ESP_ERROR_CHECK(esp_netif_init());

    udp->dest_addr.sin_addr.s_addr = inet_addr(DEST);
    udp->dest_addr.sin_family = AF_INET;
    udp->dest_addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return sock;
    }

    udp->sock = sock;

    ESP_LOGI(TAG, "Socket created");

    return 0;
}

void udp_stop(udp_t *udp)
{
    if (udp->sock > 0)
    {
        ESP_LOGI(TAG, "Shutting down socket");
        shutdown(udp->sock, 0);
        close(udp->sock);
    }
}

int audio_udp_bind(udp_t *udp)
{
    udp->dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int err = bind(udp->sock, (struct sockaddr *)&udp->dest_addr, sizeof(udp->dest_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return err;
    }

    ESP_LOGI(TAG, "Socket bound, port %d", udp->dest_addr.sin_port);

    return 0;
}

int udp_next(udp_t *udp, uint8_t *data, size_t max_size)
{
    // char addr_str[128];
    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);

    int len = recvfrom(udp->sock, data, max_size, 0, (struct sockaddr *)&source_addr, &socklen);

    if (len < 0)
    {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
    }
    else
    {
        // Get the sender's ip address as string
        /*if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        else if (source_addr.ss_family == PF_INET6)
        {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }*/

        // ESP_LOGI(TAG, "Received %d bytes from %s", len, addr_str);
    }

    return len;
}

int udp_send_bytes(udp_t *udp, const uint8_t *data, size_t size)
{
    int ret = sendto(udp->sock, data, size, 0, (struct sockaddr *)&udp->dest_addr, sizeof(udp->dest_addr));
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Cannot send %u bytes to %s: %d (ret = %d)", size, DEST, errno, ret);
    }

    return ret;
}