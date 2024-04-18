#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/sockets.h"

#define ESP_WIFI_SSID "Esp_Test"
#define ESP_WIFI_PASS "12345678"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define CONNECT_MAX_TRY 5

static const char *TAG = "example";
EventGroupHandle_t wifi_event_handle;
int server_socket;
char recv_buff[1500] = {0};

void wifi_sta_init();

void wifi_callback(void *arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data)
{
    static uint8_t connect_try_count;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (connect_try_count < CONNECT_MAX_TRY)
        {
            esp_wifi_connect();
            connect_try_count++;
        }
        else
        {
            xEventGroupSetBits(wifi_event_handle, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event_ip_msg = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "已成功连接到AP\n获取到的的IP地址为:" IPSTR "", IP2STR(&event_ip_msg->ip_info.ip));
        ESP_LOGI(TAG, "网关的IP地址为:" IPSTR "", IP2STR(&event_ip_msg->ip_info.gw));
        xEventGroupSetBits(wifi_event_handle, WIFI_CONNECTED_BIT);
    }
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_sta_init();
    EventBits_t event_bits = xEventGroupWaitBits(wifi_event_handle, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    if (event_bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "wifi连接失败\r\n");
        return;
    }
    if (event_bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "wifi连接成功\r\n");
    }
    // 为服务器创建套接字
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        ESP_LOGI(TAG, "创建套接字时出错\r\n");
        return;
    }
    struct sockaddr_in server_msg = {0};
    server_msg.sin_family = AF_INET;
    server_msg.sin_port = htons(30000);
    server_msg.sin_addr.s_addr = inet_addr("192.168.137.1");
    /*连接服务器，并绑定服务器套接字*/
    int error_id = connect(server_socket, (const struct sockaddr *)&server_msg, sizeof(server_msg));
    if (error_id == -1)
    {
        ESP_LOGI(TAG, "连接服务器时出错了");
        return;
    }
    int recv_count = recv(server_socket, recv_buff, 1499, 0);
    recv_buff[recv_count] = 0;
    ESP_LOGI(TAG, "收到服务器的消息:%s", recv_buff);
    int i = 0;
    while (1)
    {
        i++;
        vTaskDelay(pdMS_TO_TICKS(2000));
        send(server_socket, "你好", strlen("你好"), 0);
        recv_count = recv(server_socket, recv_buff, 1499, 0);
        recv_buff[recv_count] = 0;
        ESP_LOGI(TAG, "收到服务器的消息:%s", recv_buff);
        if (i > 10)
        {
            shutdown(server_socket, SHUT_RD);
            close(server_socket);
            break;
        }
    }
}

void wifi_sta_init()
{
    wifi_event_handle = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_callback, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_callback, NULL, NULL);

    // 初始化网卡底层设置
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    // 初始化wifi底层设置
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /*10.设置sta的属性*/
    wifi_config_t wifi_config =
        {
            .sta =
                {
                    .ssid = ESP_WIFI_SSID,
                    .password = ESP_WIFI_PASS,
                },
        };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}