/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <mqtt_client.h>

#define MAX_TOPIC_CALLBACKS 8

struct topic_callback
{
    char topic[64];
    void (*callback)(const char *topic, char *data, size_t data_len, void *user_data);
    void *user_data;
};

typedef struct media_controller
{
    esp_mqtt_client_handle_t client;
    bool connected;
    struct topic_callback callback_table[MAX_TOPIC_CALLBACKS];
    size_t callback_table_size;
} media_controller_t;

void media_controller_init(media_controller_t *controller);
void media_controller_ring(media_controller_t *controller);
bool media_controller_is_connected(media_controller_t *controller);
int media_controller_add_callback(media_controller_t *controller, const char *topic, void (*callback)(const char *topic, char *data, size_t data_len, void *user_data), void *user_data);