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
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_mqtt.h"

#define MQTT_HOST "argo"
#define MQTT_USER "ESP32-logger"
#define MQTT_PASS "testpass"
#define MQTT_PORT "1833"
#define MQTT_COMMAND_CHANNEL "ESP32-Node-Control"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "sn";

static void obtain_time(void);
static void initialize_sntp(void);

void smartconfig_example_task(void * parm);

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
	ESP_LOGI(TAG, "eFuse Two Point: Supported\n");
        //printf("eFuse Two Point: Supported\n");
    } else {
        ESP_LOGI(TAG, "eFuse Two Point: NOT Supported\n");
        //printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Vref: Supported\n");
        //printf("eFuse Vref: Supported\n");
    } else {
        ESP_LOGI(TAG, "eFuse Vref: NOT Supported\n");
        //printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "Characterized using Two Point Value\n");
        //printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "Characterized using eFuse Vref\n");
        //printf("Characterized using eFuse Vref\n");
    } else {
        ESP_LOGI(TAG, "Characterized using Default Vref\n");
        //printf("Characterized using Default Vref\n");
    }
}


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
	/* copied directly from smartconfig example in esp-idf*/
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
        default:
            break;
    }
}
static void esp_mqtt_status_callback( esp_mqtt_status_t status)
{
	/* callback for mqtt status events */
	switch (status) {
    		case ESP_MQTT_STATUS_CONNECTED:
      			// subscribe
      			esp_mqtt_subscribe(MQTT_COMMAND_CHANNEL, 2);
      			break;
    		case ESP_MQTT_STATUS_DISCONNECTED:
      			// reconnect
      			esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
			break;
        	default:
        	    	break;
	}
		
}
/* 
  establish an enum for sensor node command codes:
    0 stop sending
    1 start sending
    2 rate set
    3 packet length set
*/
typedef enum esp_sensor_node_command_t {ESP_SN_CMD_STOP,ESP_SN_CMD_START,ESP_SN_CMD_RATE,ESP_SN_CMD_PKT_LEN,ESP_SN_CMD_DISCONNECT} esp_sensor_node_command_t;
/* create variables to control sensor node behavior. */
/* TODO: figure out how to lock access to rate & pkt_len parameters so the control writing doesnt conflict with the update reading */
static uint16_t sensor_node_rate; /* frequency for polling ADC */
static uint16_t sensor_node_pkt_len; /* number of sample per mqtt packet during send*/

static void esp_mqtt_message_callback(const char *topic, uint8_t *payload, size_t len)
{
	esp_sensor_node_command_t control_code;
	/* To Do: update with node spcific sub-channels. For now all nodes respond the same way*/
	if (strcmp(topic, "command")) {
		control_code=payload[0];
		switch (control_code){
			case ESP_SN_CMD_STOP
				/* stop sending data*/
				/* NOTE: stop function built into esp_mqtt.c disconnects network after disconnecting from broker */
				ESP_LOGI(TAG, "Got command to stop sending data");
				break;
			case ESP_SN_CMD_START
				/* start sending data*/
				ESP_LOGI(TAG, "Got command to start sending data");
				break;
			case ESP_SN_CMD_RATE
				/* set rate at which the ADC will be polled */
				if (len!=sizeof(sensor_node_rate)/sizeof(payload[0])) {
					/* note the following assumes that len only contains the number of elements in payload. If len includes the topic, or the len element itself, we need to adjust this */
					ESP_LOGI(TAG, "Got bad sample rate packet with %d bytes",sizeof(payload[0])*len);			
				}else{
					memcpy(&sensor_node_rate,*payload(1),sizeof(sensor_node_rate)) /* take data from UINT8_t in payload, and convert to UINT16, or whatever sensor_node_rate is */
					ESP_LOGI(TAG, "Got command to set sample rate to %d hz",sensor_node_rate);
				}
				break;
			case ESP_SN_CMD_PKT_LEN
				/* set how many samples are in each packet. Sets the effective update rate */
				if (len!=sizeof(sensor_node_rate)/sizeof(*payload)) {
					/* note the following assumes that len only contains the number of elements in payload. If len includes the topic, or the len element itself, we need to adjust this */
					ESP_LOGI(TAG, "Got bad pkt_len packet with %d bytes",sizeof(payload[0])*len);			
				}else{
					memcpy(&sensor_node_pkt_len,payload(1),sizeof(sensor_node_pkt_len)) /* take data from UINT8_t in payload, and convert to UINT16, or whatever sensor_node_rate is */
					ESP_LOGI(TAG, "Got command to set packet length to %d samples",sensor_node_pkt_len);	
				}
				break;	
			case ESP_SN_CMD_DISCONNECT
				esp_mqtt_stop();		
			default:
				ESP_LOGI(TAG, "Got bad control code on command channel. Received code: %d",control_code);
				break;
		}
	}
}
void smartconfig_example_task(void * parm)
{
	/* copied from esp-idf smartconfig example */
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
	/* largely copied from esp-idf sntp example code */
    ESP_ERROR_CHECK( nvs_flash_init() );
//    initialise_wifi();
//    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,false, true, portMAX_DELAY);
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

//    ESP_ERROR_CHECK( esp_wifi_stop() );
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
	/* TODO: implement NTP or iterative SNTP calls and spawn process here instead of a single SNTP call to set time*/
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
	/* TODO: figure out timezone on the fly */ 
	setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    //begin MQTT process
    	esp_mqtt_init(esp_mqtt_status_callback, esp_mqtt_message_callback, size_t buffer_size, int command_timeout);	
    //Establish MQTT last will and testimate
    //esp_mqtt_lwt(const char *topic, const char *payload, int qos, bool retained);
    	esp_mqtt_start(const char *host, const char *port, const char *client_id, const char *username, const char *password);

    //set up ADC
	check_efuse();
    	if (unit == ADC_UNIT_1) {
        	adc1_config_width(ADC_WIDTH_BIT_12);
        	adc1_config_channel_atten(channel, atten);
    	} else {
        	adc2_config_channel_atten((adc2_channel_t)channel, atten);
	}
	uint32_t adc_reading = 0;

    //begin pushing data
	/*TODO: change while loop to timer watchdog/callback. Ditch multisampling?*/
	int i=0;    
	while (i<10){  
		/* simple 10 sample stream */
		adc_reading = 0;
		//grab data from ADC with multisampling
        	for (int i = 0; i < NO_OF_SAMPLES; i++) {
            		if (unit == ADC_UNIT_1) {
                		adc_reading += adc1_get_raw((adc1_channel_t)channel);
            		} else {
                		int raw;
                		adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                		adc_reading += raw;
            		}
        	}
        	adc_reading /= NO_OF_SAMPLES;
        	//Convert adc_reading to voltage in mV
        	uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
		ESP_LOGI(TAG, "Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
		//publish ADC data
    		esp_mqtt_publish(const char *topic, uint8_t *payload, size_t len, int qos, bool retained);
		i++;
		vTaskDelay(pdMS_TO_TICKS(1000));
    	}
    //unsubscribe
    //bool esp_mqtt_unsubscribe(const char *topic);
    //stop MQTT process
    //esp_mqtt_stop();
}

