/*
 * SPDX-FileCopyrightText: 2024 Detlev Casanova <dc@detlev.ca>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <esp_log.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <string.h>
#include <esp_console.h>
#include <esp_task.h>

#include "audio_player.h"
#include "audio_recorder.h"
#include "rtp.h"
#include "udp.h"
#include "wifi.h"

enum state
{
    TALKING_STATE,
    LISTENING_STATE,
    IDLE_STATE
} state;

audio_player_t player;
audio_recorder_t recorder;

static const char *TAG = "main";

static void print_stats(void)
{
    ESP_LOGI(TAG, "Free memory: %lu bytes, Uptime: %" PRId64 " ms", esp_get_free_heap_size(), esp_timer_get_time() / 1000);
}

static int run_cmd(int argc, char *argv[])
{
    char *cmd = argv[0];

    if (strcmp(cmd, "listen") == 0)
    {
        if (state != IDLE_STATE)
        {
            ESP_LOGE(TAG, "Already streaming audio. Run stop before");
            return -1;
        }

        ESP_LOGI(TAG, "start listening");

        audio_recorder_init(&recorder);
        audio_recorder_start(&recorder);
        state = LISTENING_STATE;
    }
    else if (strcmp(cmd, "talk") == 0)
    {
        if (state != IDLE_STATE)
        {
            ESP_LOGE(TAG, "Already streaming audio. Run stop before");
            return -1;
        }

        ESP_LOGI(TAG, "start talking");

        audio_player_init(&player);
        audio_player_start(&player);
        state = TALKING_STATE;
    }
    else if (strcmp(cmd, "stop") == 0)
    {
        ESP_LOGI(TAG, "stop audio");

        if (state == TALKING_STATE)
        {
            audio_player_stop(&player);
            audio_player_deinit(&player);
        }

        if (state == LISTENING_STATE)
        {
            audio_recorder_stop(&recorder);
            audio_recorder_deinit(&recorder);
        }

        state = IDLE_STATE;
    }
    else if (strcmp(cmd, "stats") == 0)
    {
        print_stats();
    }
    else if (strcmp(cmd, "memtest") == 0)
    {
        const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
        unsigned int count = 0;

        while (1)
        {
            ESP_LOGI(TAG, "--------------------- LOOP %u", count++);
            ESP_LOGI(TAG, "--------------------- start recorder");
            audio_recorder_init(&recorder);
            audio_recorder_start(&recorder);
            vTaskDelay(xDelay);
            ESP_LOGI(TAG, "--------------------- stop  recorder");
            audio_recorder_stop(&recorder);
            audio_recorder_deinit(&recorder);

            ESP_LOGI(TAG, "--------------------- start player");
            audio_player_init(&player);
            audio_player_start(&player);
            vTaskDelay(xDelay);
            ESP_LOGI(TAG, "--------------------- stop  player");
            audio_player_stop(&player);
            audio_player_deinit(&player);

            print_stats();
        }
    }

    return 0;
}

static esp_console_cmd_t talk_cmd = {
    .command = "talk",
    .help = "Start talking",
    .func = run_cmd,
};
static esp_console_cmd_t listen_cmd = {
    .command = "listen",
    .help = "Start listening",
    .func = run_cmd,
};
static esp_console_cmd_t stop_cmd = {
    .command = "stop",
    .help = "Stop listening/talking",
    .func = run_cmd,
};
static esp_console_cmd_t stats_cmd = {
    .command = "stats",
    .help = "Show stats",
    .func = run_cmd,
};
static esp_console_cmd_t memtest_cmd = {
    .command = "memtest",
    .help = "Test Memory",
    .func = run_cmd,
};

static esp_err_t start_console(void)
{
    esp_err_t err = ESP_OK;
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = ">";
    repl_config.max_cmdline_length = 8;

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    err = esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot initialize console...");
        return err;
    }

    err = esp_console_cmd_register(&talk_cmd);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register console command...");
        goto deinit_console;
    }
    err = esp_console_cmd_register(&listen_cmd);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register console command...");
        goto deinit_console;
    }
    err = esp_console_cmd_register(&stop_cmd);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register console command...");
        goto deinit_console;
    }
    err = esp_console_cmd_register(&stats_cmd);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register console command...");
        goto deinit_console;
    }
    err = esp_console_cmd_register(&memtest_cmd);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register console command...");
        goto deinit_console;
    }
    err = esp_console_start_repl(repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot start console...");
        goto deinit_console;
    }

    return ESP_OK;

deinit_console:
    esp_console_deinit();
    return err;
}

void app_main(void)
{
    uint8_t show_stats = 0;

    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Failed to create task loop");
        return;
    }

    wifi_init();

    state = IDLE_STATE;

    if (start_console() != ESP_OK)
        show_stats = 1;

    while (true)
    {
        const TickType_t xDelay = 5000 / portTICK_PERIOD_MS;
        vTaskDelay(xDelay);
        if (show_stats)
            print_stats();
    }
}
