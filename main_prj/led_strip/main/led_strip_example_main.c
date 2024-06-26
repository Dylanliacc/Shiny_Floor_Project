#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "string.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM 9

#define EXAMPLE_LED_NUMBERS 8
#define EXAMPLE_CHASE_SPEED_MS 10

// #define WIFI_SSID      "IAG_2.4G"

#define Client_ID "2"

#define WIFI_SSID "shiny_floor"
#define WIFI_PASS "12345678"
#define MAX_RETRY 5

#define SERVER_IP "192.168.214.210"
#define SERVER_PORT 3333

static const char *TAG = "example";
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];
static bool led_effect_on = false;
static uint8_t rgb_color[3] = {255, 0, 0}; // Default color for RGB effects

typedef enum
{
    LED_EFFECT_NONE,
    LED_EFFECT_RAINBOW_CHASE,
    LED_EFFECT_BREATHING,
    LED_EFFECT_CONSTANT
} led_effect_t;

static led_effect_t current_effect = LED_EFFECT_NONE;

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void tcp_client_task(void *pvParameters);
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
void wifi_init_sta(void);
void led_effect_task(void *pvParameters);

void app_main(void)
{
    // Wi-Fi 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // 创建 LED 效果任务
    xTaskCreate(led_effect_task, "led_effect_task", 4096, NULL, 5, NULL);
}

void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1)
    {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(SERVER_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", SERVER_IP, SERVER_PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");
        char *message = "Connection successful";
        char *id = Client_ID; // Example Client_ID
        char result[100];     // Make sure this is large enough to hold the final string
        //send(sock, message, strlen(message), 0);
        strcpy(result, id); // Concatenate the Client_ID
        send(sock, result, strlen(result), 0);

        while (1)
        {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0)
            {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            else if (len == 0)
            {
                ESP_LOGI(TAG, "Connection closed");
                break;
            }
            else
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

                // 根据接收到的消息控制 LED 效果
                if (strcmp(rx_buffer, "rainbow") == 0)
                {
                    current_effect = LED_EFFECT_RAINBOW_CHASE;
                    led_effect_on = true;
                }
                else if (strcmp(rx_buffer, "breath") == 0)
                {
                    current_effect = LED_EFFECT_BREATHING;
                    led_effect_on = true;
                }
                else if (strcmp(rx_buffer, "constant") == 0)
                {
                    current_effect = LED_EFFECT_CONSTANT;
                    led_effect_on = true;
                }
                else if (strcmp(rx_buffer, "off") == 0)
                {
                    led_effect_on = false;
                }
                else if (sscanf(rx_buffer, "rgb %hhu %hhu %hhu", &rgb_color[0], &rgb_color[1], &rgb_color[2]) == 3)
                {
                    // Update RGB color for breathing and constant effects
                }
            }
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAX_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGI(TAG, "connect to the AP fail");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}

void led_effect_task(void *pvParameters)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;
    uint8_t brightness = 0;
    bool increasing = true;

    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    ESP_LOGI(TAG, "Start LED effects");
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    while (1)
    {
        if (led_effect_on)
        {
            switch (current_effect)
            {
            case LED_EFFECT_RAINBOW_CHASE:
                for (int i = 0; i < 3; i++)
                {
                    for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3)
                    {
                        hue = j * 360 / EXAMPLE_LED_NUMBERS + start_rgb;
                        led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                        led_strip_pixels[j * 3 + 0] = green;
                        led_strip_pixels[j * 3 + 1] = blue;
                        led_strip_pixels[j * 3 + 2] = red;
                    }
                    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
                    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
                    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
                }
                start_rgb += 60;
                break;

            case LED_EFFECT_BREATHING:
                for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++)
                {
                    led_strip_pixels[i * 3 + 0] = (rgb_color[0] * brightness) / 255;
                    led_strip_pixels[i * 3 + 1] = (rgb_color[1] * brightness) / 255;
                    led_strip_pixels[i * 3 + 2] = (rgb_color[2] * brightness) / 255;
                }
                ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                vTaskDelay(pdMS_TO_TICKS(20));
                brightness = increasing ? brightness + 1 : brightness - 1;
                if (brightness == 0 || brightness == 255)
                {
                    increasing = !increasing;
                }
                break;

            case LED_EFFECT_CONSTANT:
                for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++)
                {
                    led_strip_pixels[i * 3 + 0] = rgb_color[0];
                    led_strip_pixels[i * 3 + 1] = rgb_color[1];
                    led_strip_pixels[i * 3 + 2] = rgb_color[2];
                }
                ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                vTaskDelay(pdMS_TO_TICKS(100)); // Prevent CPU overutilization
                break;

            case LED_EFFECT_NONE:
            default:
                // Turn off the LEDs when no effect is selected
                memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
                ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                vTaskDelay(pdMS_TO_TICKS(100)); // Prevent CPU overutilization
                break;
            }
        }
        else
        {
            // Turn off the LEDs when led_effect_on is false
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(100)); // Add a delay to prevent CPU overutilization
        }
    }
}
