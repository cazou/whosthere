/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "rtc_wdt.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "udp.h"

static const char *TAG = "UDP";

void audio_udp_init(udp_t *udp)
{
    ESP_ERROR_CHECK(esp_netif_init());
    udp->sock = 0;
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

int audio_udp_bind(udp_t *udp, uint16_t port)
{
    int addr_family = AF_INET;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return sock;
    }
    ESP_LOGI(TAG, "Socket created");

    // Set timeout
    // struct timeval timeout;
    // timeout.tv_sec = 10;
    // timeout.tv_usec = 0;
    // setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    int err = bind(sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return err;
    }

    ESP_LOGI(TAG, "Socket bound, port %d", port);

    udp->sock = sock;

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
