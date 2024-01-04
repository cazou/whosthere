/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <esp_log.h>
#include <errno.h>

#include "media_controller.h"

const char *TAG = "media_controller";

static void run_topic_callback(media_controller_t *controller, const char *topic, size_t topic_len, char *data, size_t data_len)
{
    for (int i = 0; i < controller->callback_table_size; i++)
    {
        struct topic_callback *t = &controller->callback_table[i];
        if (strncmp(t->topic, topic, topic_len) == 0)
        {
            t->callback(t->topic, data, data_len, t->user_data);
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    media_controller_t *controller = (media_controller_t *)handler_args;
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        controller->connected = true;
        // TODO: Subscribe only when the callback is added.
        esp_mqtt_client_subscribe(controller->client, "/whosthere/buzzin", 0);
        esp_mqtt_client_subscribe(controller->client, "/whosthere/talk", 0);
        esp_mqtt_client_subscribe(controller->client, "/whosthere/listen", 0);
        esp_mqtt_client_subscribe(controller->client, "/whosthere/silence", 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        controller->connected = false;
        // esp_mqtt_client_reconnect(controller->client);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        run_topic_callback(controller, event->topic, event->topic_len, event->data, event->data_len);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "An MQTT error occured");
        ESP_LOGE(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        break;
    }
}

int media_controller_add_callback(media_controller_t *controller, const char *topic, void (*callback)(const char *topic, char *data, size_t data_len, void *user_data), void *user_data)
{
    if (controller->callback_table_size == MAX_TOPIC_CALLBACKS)
    {
        ESP_LOGE(TAG, "Maximum of %d callbacks reached", MAX_TOPIC_CALLBACKS);
        return -ENOMEM;
    }

    struct topic_callback *t = &controller->callback_table[controller->callback_table_size];

    char *end = stpncpy(t->topic, topic, 64);
    if (end - t->topic >= 64)
    {
        ESP_LOGE(TAG, "Topic '%s' is too long", topic);
        return -EINVAL;
    }

    t->user_data = user_data;
    t->callback = callback;

    controller->callback_table_size += 1;

    return 0;
}

void media_controller_init(media_controller_t *controller)
{
    controller->connected = false;
    controller->callback_table_size = 0;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_ADDRESS,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.client_id = "whosthere", // ADD unique ID from MAC ADDR
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
    };

    controller->client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(controller->client, ESP_EVENT_ANY_ID, mqtt_event_handler, controller);
    esp_mqtt_client_start(controller->client);
}

void media_controller_ring(media_controller_t *controller)
{
    if (controller->connected)
        esp_mqtt_client_publish(controller->client, "/whosthere/ring", "", 0, 1, 0);
    else
        ESP_LOGE(TAG, "MQTT Not connected");
}

bool media_controller_is_connected(media_controller_t *controller)
{
    return controller->connected;
}