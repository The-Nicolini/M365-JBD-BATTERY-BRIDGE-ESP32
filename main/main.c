#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#define JBD_UART_NUM UART_NUM_1
#define JBD_UART_TXD_PIN 47
#define JBD_UART_RXD_PIN 48
#define JBD_UART_BAUD 9600
#define JBD_UART_BUF_SIZE 256

static const char *TAG = "jbd_bms";

void app_main(void)
{
    const uart_config_t uart_config = {
        .baud_rate = JBD_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_LOGI(TAG, "Installing UART driver on UART%d", JBD_UART_NUM);
    ESP_ERROR_CHECK(uart_driver_install(JBD_UART_NUM, JBD_UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(JBD_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(JBD_UART_NUM, JBD_UART_TXD_PIN, JBD_UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "JBD BMS UART configured: TX=%d RX=%d baud=%d", JBD_UART_TXD_PIN, JBD_UART_RXD_PIN, JBD_UART_BAUD);

    uint8_t data[JBD_UART_BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(JBD_UART_NUM, data, sizeof(data), pdMS_TO_TICKS(200));
        if (len > 0) {
            printf("BMS:");
            for (int i = 0; i < len; i++) {
                printf(" %02X", data[i]);
            }
            printf("\r\n");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
