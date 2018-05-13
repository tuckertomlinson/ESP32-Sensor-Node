/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include <esp_mqtt.h>

#define MQTT_HOST "argo"
#define MQTT_USER "ESP32-logger"
#define MQTT_PASS "testpass"
#define MQTT_PORT "1833"
#define MQTT_COMMAND_CHANNEL "ESP32-Node-Control"


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "sc";

static void obtain_time(void);
static void initialize_sntp(void);

void smartconfig_example_task(void * parm);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	// start mqtt
	esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      // stop mqtt
      esp_mqtt_stop();

      // reconnect wifi
	esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
    	case ESP_MQTT_STATUS_CONNECTED:
      		// subscribe
      		esp_mqtt_subscribe(COMMAND_CHANNEL, 2);
      		break;
    	case ESP_MQTT_STATUS_DISCONNECTED:
      		// reconnect
      		esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
		break;
        default:
            break;
    }
}
static void message_callback(const char *topic, uint8_t *payload, size_t len) {
	//callback for MQTT messages
	//TODO, check command and update frequency, or start/stop streaming
  	ESP_LOGI("test", "incoming: %s => %s (%d)", topic, payload, (int)len);
}
void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
//    initialise_wifi();
//    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init(); 
}

void app_main()
{
    	ESP_ERROR_CHECK( nvs_flash_init() );
    	initialise_wifi();
    	time_t now;
    	struct tm timeinfo;
    	time(&now);
    	localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    	if (timeinfo.tm_year < (2016 - 1900)) {
        	ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        	obtain_time();
        	// update 'now' variable with current time
        	time(&now);
    	}

    char strftime_buf[64];
    // Set timezone to Eastern Standard Time and print local time
	setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to China Standard Time
	setenv("TZ", "CST-8", 1);
	tzset();
    	localtime_r(&now, &timeinfo);
    	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    	ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    //begin MQTT process
    	esp_mqtt_init(esp_mqtt_status_callback_t scb, esp_mqtt_message_callback_t mcb, size_t buffer_size, int command_timeout);	
    //Establish MQTT last will and testimate
    //esp_mqtt_lwt(const char *topic, const char *payload, int qos, bool retained);
    	esp_mqtt_start(const char *host, const char *port, const char *client_id, const char *username, const char *password);
    //wait for connection?

    //subscribe to control channel
    	bool esp_mqtt_subscribe(const char *topic, int qos);
    //begin pushing data
	//TODO: change while loop to timer watchdog/callback
	int i=0;    
	while (i<10){
		//grab data from ADC
		//publish ADC data
    		esp_mqtt_publish(const char *topic, uint8_t *payload, size_t len, int qos, bool retained);
		i++;
    	}
    //unsubscribe
    //bool esp_mqtt_unsubscribe(const char *topic);
    //stop MQTT process
    //esp_mqtt_stop();
}

