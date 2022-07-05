#include <osapi.h>
#include <mem.h>
#include <user_interface.h>
#include <gpio.h>
#include <espconn.h>
#include <sntp.h>
#include <json/jsonparse.h>

#include "mod_enums.h"
#include "mod_http.h"

// Update according to WiFi session ID
#define WIFI_SSID								"[WIFI-SESSION-ID]"
// Update according to WiFi session password
#define WIFI_PASSPHRASE							"[WIFI-PASSPHRASE]"
// Directions API Key
#define HTTP_QUERY_KEY							"[GOOGLE-DIRECTIONS-API-KEY]"

// Directions API tags
#define DIRECTIONS_API_BASE_URL					"https://maps.googleapis.com/maps/api/directions/json?"
#define DIRECTIONS_API_TAG_START				"origin"
#define DIRECTIONS_API_TAG_END					"destination"
#define DIRECTIONS_API_TAG_WAYPOINTS			"waypoints"
#define DIRECTIONS_API_TAG_VIA					"via"
#define DIRECTIONS_API_TAG_KEY					"key"
#define DIRECTIONS_API_TIME						"departure_time=now"

// JSON section name to extract
#define JSON_TAG_DURATION						"duration_in_traffic"
// JSON value tag name to extract
#define JSON_TAG_NESTED_VALUE					"value"
// JSON tag depth level to extract
#define JSON_DEPTH_NESTED_VALUE					1

#define UART_BAUD_RATE							115200
#define LABEL_BUFFER_SIZE						128
#define LED_COUNT								8

#define SYSTEM_PARTITION_RF_CAL_SZ				0x1000
#define SYSTEM_PARTITION_PHY_DATA_SZ			0x1000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ	0x3000
#define SYSTEM_SPI_SIZE							0x400000
#define SYSTEM_PARTITION_RF_CAL_ADDR			0x3FB000
#define SYSTEM_PARTITION_PHY_DATA_ADDR			0x3FC000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR	0x3FE000

struct gps_coords
{
	double lat;
	double lng;
};

// route start GPS position
static const struct gps_coords START_POSITION 	= { 51.564418, -0.062658 };
// route end GPS position
static const struct gps_coords END_POSITION 	= { 51.519986, -0.082895 };
// intermediate waypoint GPS positions
static const struct gps_coords WAYPOINTS[]		=
{
		{ 51.556724, -0.074518 },
		{ 51.531606, -0.077044 }
};
// defines worst time on the route (in seconds) - all warning LEDs will be ignited - means traffic jam
static const sint32 WORST_ROUTE_TIME = 1600;
// defines best time on the route (in seconds) - all warning LEDs will be off - means road is free
static const sint32 BEST_ROUTE_TIME = 960;

static const uint16 GPIO_PIN_LED		= 2;
static const uint16 GPIO_PIN_SER_DATA	= 4;
static const uint16 GPIO_PIN_SER_CLOCK	= 5;
static const uint16 GPIO_PIN_READ_LATCH	= 12;

static const uint16 DELAY_HEARTBEAT_FLASH		= 10 * 1000;
static const uint16 DELAY_SHIFT_REG				= 50;

// PERIOD UNITS 								x10ms
static const uint32 TIMER_PERIOD_LED			= 200;		// 2 sec
static const uint32 TIMER_PERIOD_BLANK_LED		= 100;		// 1 sec
static const uint32 TIMER_PERIOD_CONN			= 1000;		// 10 sec
static const uint32 TIMER_PERIOD_CONN_RETRY		= 12000;	// 2 mins
static const uint32 TIMER_PERIOD_CLOSE_SOCKET	= 10;		// 100 ms
static const uint32 TIMER_PERIOD_QUERY			= 60000;    // 10 min
static const uint32 TIMER_PERIOD_INITIAL_QUERY	= 6000;    	// 1 min
static const uint32 TIMER_IDX_RESET				= 200000000L;

static os_timer_t start_timer;
static uint32 tick_index = 0L;
static sint32 retry_tick_index = -1;
static sint32 duration_value = -1;

// index - used for cursor position tracking at receive buffer
static size_t local_http_receive_idx = 0;
// used to resolve target hostname ip address by DNS
static ip_addr_t target_server_ip;
// composed URL to query
static char complete_url[HTTP_URL_BUFFER_SIZE];
// used to store url prefix type (HTTP or HTTPS)
static int url_prefix_type = HTTP_URL_HTTP;
// used to store http hostname
static char http_hostname[HTTP_HEADER_BUFFER_SIZE];
// used to store http path
static char http_path[HTTP_HEADER_BUFFER_SIZE];
// used to indicate whether HTTP data transfer has started
static bool is_transfer_started = false;
// used to indicate whether HTTP data transfer has failed
static bool query_error_flag = false;
// used to indicate whether valid HTTP response data is present
// (in case of data is missing - indicated as RED blinking LED)
static bool empty_response_flag = true;
// used to indicate whether HTTP data transfer has been completed
static bool is_transfer_completed = false;
// this buffer is used to persist HTTP content
static char* http_content = NULL;
// actual connection definition used to perform HTTP GET request
struct espconn* pespconn = NULL;

static const partition_item_t part_table[] =
{
	{ SYSTEM_PARTITION_RF_CAL,				SYSTEM_PARTITION_RF_CAL_ADDR,			SYSTEM_PARTITION_RF_CAL_SZ				},
	{ SYSTEM_PARTITION_PHY_DATA,			SYSTEM_PARTITION_PHY_DATA_ADDR,			SYSTEM_PARTITION_PHY_DATA_SZ			},
	{ SYSTEM_PARTITION_SYSTEM_PARAMETER,	SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR,	SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ	}
};

// ***************************** LED BAR - DISPLAY LEVEL  *****************************

static uint16 calculate_level(sint32 value)
{
	uint16 result;
	if (value > WORST_ROUTE_TIME)
	{
		result = LED_COUNT;
	}
	else if (value < BEST_ROUTE_TIME)
	{
		result = 0;
	}
	else
	{
		result = ((value - BEST_ROUTE_TIME) * LED_COUNT) / (WORST_ROUTE_TIME - BEST_ROUTE_TIME);
	}
	return result;
}

static void show_level(uint16 level)
{
	OS_UART_LOG("[INFO] Indicating Level: %d\n", level);
	uint16 i;
	for (i = 0; i < LED_COUNT; ++i)
	{
		GPIO_OUTPUT_SET(GPIO_PIN_SER_DATA, (i < level));
		os_delay_us(DELAY_SHIFT_REG);
		GPIO_OUTPUT_SET(GPIO_PIN_SER_CLOCK, 1);
		os_delay_us(DELAY_SHIFT_REG);
		GPIO_OUTPUT_SET(GPIO_PIN_SER_CLOCK, 0);
		os_delay_us(DELAY_SHIFT_REG);
	}
	GPIO_OUTPUT_SET(GPIO_PIN_READ_LATCH, 1);
	os_delay_us(DELAY_SHIFT_REG);
	GPIO_OUTPUT_SET(GPIO_PIN_READ_LATCH, 0);
	os_delay_us(DELAY_SHIFT_REG);
}

static void show_blank(bool phase)
{
	uint16 i;
	for (i = 0; i < LED_COUNT; ++i)
	{
		GPIO_OUTPUT_SET(GPIO_PIN_SER_DATA, (i == (LED_COUNT - 1)) && phase);
		os_delay_us(DELAY_SHIFT_REG);
		GPIO_OUTPUT_SET(GPIO_PIN_SER_CLOCK, 1);
		os_delay_us(DELAY_SHIFT_REG);
		GPIO_OUTPUT_SET(GPIO_PIN_SER_CLOCK, 0);
		os_delay_us(DELAY_SHIFT_REG);
	}
	GPIO_OUTPUT_SET(GPIO_PIN_READ_LATCH, 1);
	os_delay_us(DELAY_SHIFT_REG);
	GPIO_OUTPUT_SET(GPIO_PIN_READ_LATCH, 0);
	os_delay_us(DELAY_SHIFT_REG);
}

// ******************************** CONNECTION STATUS *********************************

static bool is_station_connecting(void)
{
	uint8 status = wifi_station_get_connect_status();
	return status == STATION_CONNECTING || status == STATION_GOT_IP;
}

static bool is_station_connected(void)
{
	return wifi_station_get_connect_status() == STATION_GOT_IP;
}

static bool is_secure(void)
{
	return url_prefix_type == HTTP_URL_HTTPS;
}

// ******************************** WIFI CONNECT COMMAND ********************************

void connection_configure(void)
{
	char ssid[] = WIFI_SSID;
	char password[] = WIFI_PASSPHRASE;
	struct station_config sta_conf = { 0 };

	os_memcpy(sta_conf.ssid, ssid, sizeof(ssid));
	os_memcpy(sta_conf.password, password, sizeof(password));
	wifi_station_set_config(&sta_conf);
	wifi_station_set_auto_connect(true);
	wifi_station_set_reconnect_policy(true);
}

void connect(void)
{
	if (!is_station_connecting())
	{
		OS_UART_LOG("\n[INFO] Connecting to predefined SSID ...\n");
		connection_configure();
		if (wifi_station_connect())
		{
			OS_UART_LOG("[INFO] Command \"connect\" has been submitted\n");
		}
		else
		{
			OS_UART_LOG("[ERROR] Unable to submit \"connect\" command\n");
			query_error_flag = true;
		}
	}
	else
	{
		OS_UART_LOG("\n[INFO] Already triggered connect command\n");
	}
}

// ******************************** METHODS TO PERFORM HTTP REQUEST ********************************

// Forward-declarations

void close_espconn_resources(struct espconn* pconn);
void process_content(void);

// Callback methods

static void ICACHE_FLASH_ATTR on_dns_ip_resoved_callback(const char* hostnaname, ip_addr_t* ip, void* arg);
static void ICACHE_FLASH_ATTR on_tcp_connected_callback(void* arg);
static void ICACHE_FLASH_ATTR on_tcp_receive_data_callback(void* arg, char* user_data, unsigned short len);
static void ICACHE_FLASH_ATTR on_tcp_close_callback(void* arg);
static void ICACHE_FLASH_ATTR on_tcp_failed_callback(void* arg, sint8 error_type);

// ON IP ADDRESS RESOLVED BY HOSTNAME callback method

static void ICACHE_FLASH_ATTR on_dns_ip_resoved_callback(const char* hostnaname, ip_addr_t* ip, void* arg)
{
	struct espconn* pconn = (struct espconn*)arg;
	if (ip)
	{
		OS_UART_LOG("[INFO] IP address by hostname `%s` is resolved: %d.%d.%d.%d\n",
				hostnaname,
				*((uint8*)&ip->addr),
				*((uint8*)&ip->addr+1),
				*((uint8*)&ip->addr+2),
				*((uint8*)&ip->addr+3));
		// TCP port configured to 80 (or 433) to make standard HTTP (or HTTPS) request
		if (is_secure())
		{
			pconn->proto.tcp->remote_port = 443;
		}
		else
		{
			pconn->proto.tcp->remote_port = 80;
		}
		// TCP IP address configured to value resolved by DNS
		os_memcpy(pconn->proto.tcp->remote_ip, &ip->addr, 4);
		espconn_regist_connectcb(pconn, on_tcp_connected_callback);
		espconn_regist_reconcb(pconn, on_tcp_failed_callback);
#ifdef UART_DEBUG_LOGS
		char res_status[LABEL_BUFFER_SIZE];
#endif
		// Establishes TCP connection
		if (is_secure())
		{
			sint8 res = espconn_secure_connect(pconn);
#ifdef UART_DEBUG_LOGS
			lookup_espconn_error(res_status, res);
			os_printf("[INFO] Establishing secure TCP connection... %s\n", res_status);
#endif
		}
		else
		{
			sint8 res = espconn_connect(pconn);
#ifdef UART_DEBUG_LOGS
			lookup_espconn_error(res_status, res);
			os_printf("[INFO] Establishing TCP connection... %s\n", res_status);
#endif
		}
	}
	else
	{
		OS_UART_LOG("[ERROR] Unable get IP address by hostname `%s`\n", hostnaname);
		close_espconn_resources(pconn);
		query_error_flag = true;
	}
}

// ON-SUCCESSFUL TCP CONNECT callback method (triggered upon TCP connection is established, but download has not stared yet)

static void ICACHE_FLASH_ATTR on_tcp_connected_callback(void* arg)
{
	OS_UART_LOG("[INFO] TCP connection is established\n");
	struct espconn* pconn = (struct espconn*)arg;
	espconn_regist_disconcb(pconn, on_tcp_close_callback);
	espconn_regist_recvcb(pconn, on_tcp_receive_data_callback);

	char* tx_buf = (char*)os_malloc(HTTP_TX_BUFFER_SIZE);
	os_sprintf(tx_buf, "GET %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n\r\n", http_path, http_hostname);
	OS_UART_LOG("[DEBUG] HTTP TX buffer:\n%s\n", tx_buf);
	if (is_secure())
	{
		espconn_secure_send(pconn, tx_buf, os_strlen(tx_buf));
	}
	else
	{
		espconn_send(pconn, tx_buf, os_strlen(tx_buf));
	}
        os_free(tx_buf);
}

// ON-SUCCESSFUL TCP DISCONNECT callback method (triggered upon successful HTTP response download completed and socket connection is closed)

static void ICACHE_FLASH_ATTR on_tcp_close_callback(void* arg)
{
	OS_UART_LOG("[INFO] TCP connection closed\n");
	struct espconn* pconn = (struct espconn*)arg;
	close_espconn_resources(pconn);
	process_content();
}

// ON-FAILED TCP CONNECT callback method (triggered in case of TCP connection cannot be established, used for re-try logic)

static void ICACHE_FLASH_ATTR on_tcp_failed_callback(void* arg, sint8 error_type)
{
#ifdef UART_DEBUG_LOGS
	char error_info[LABEL_BUFFER_SIZE];
	lookup_espconn_error(error_info, error_type);
	os_printf("[ERROR] Failed to establish TCP connection: %s\n", error_info);
#endif
	struct espconn* pconn = (struct espconn*)arg;
	close_espconn_resources(pconn);
	query_error_flag = true;
	retry_tick_index = tick_index;
}

// TCP DATA RECEIVE callback method

static void ICACHE_FLASH_ATTR on_tcp_receive_data_callback(void* arg, char* user_data, unsigned short len)
{
	if (!is_transfer_completed)
	{
		OS_UART_LOG("[DEBUG] On TCP data receive callback handler. Bytes received: %d.\n", len);
		char* local_content = (char*)os_malloc(local_http_receive_idx + len + 1);
		if (local_http_receive_idx > 0)
		{
			os_memcpy(local_content, http_content, local_http_receive_idx);
			os_free(http_content);
		}
		os_memcpy(&local_content[local_http_receive_idx], user_data, len);
		http_content = local_content;
		local_http_receive_idx += len;
		http_content[local_http_receive_idx] = 0;
		if (is_end_of_content(http_content))
		{
			local_http_receive_idx = 0;
			OS_UART_LOG("[INFO] Full HTTP content has been received\n");
			is_transfer_completed = true;
		}
	}
}

// Releases ESP connection resources
void close_espconn_resources(struct espconn* pconn)
{
	if (pconn)
	{
		if (pconn->proto.tcp)
		{
			os_free(pconn->proto.tcp);
			pconn->proto.tcp = NULL;
		}
		OS_UART_LOG("[INFO] TCP connection resources released\n");
		os_free(pconn);
		pespconn = NULL;
	}
	is_transfer_started = false;
}

// os_sprintf does not supports '%.6f' formatting rule, composing it manually
void print_coords(char* output, double lat, double lng)
{
	char* target = output;
	uint32 whole;
	if (lat < 0.0)
	{
		lat = -lat;
		whole = (uint32)lat;
		target += os_sprintf(target, "-%ld.%06ld", whole, ((uint32)(lat * 1000000)) - (whole * 1000000));
	}
	else
	{
		whole = (uint32)lat;
		target += os_sprintf(target, "%ld.%06ld", whole, ((uint32)(lat * 1000000)) - (whole * 1000000));
	}
	if (lng < 0.0)
	{
		lng = -lng;
		whole = (uint32)lng;
		target += os_sprintf(target, "%%2C-%ld.%06ld", whole, ((uint32)(lng * 1000000)) - (whole * 1000000));
	}
	else
	{
		whole = (uint32)lng;
		target += os_sprintf(target, "%%2C%ld.%06ld", whole, ((uint32)(lng * 1000000)) - (whole * 1000000));
	}
}

// Direction API request composition
void compose_http_request_url(char* url)
{
	char str_coords[100];
	char* target = url;
	// url basis
	target += os_sprintf(target, DIRECTIONS_API_BASE_URL);
	// route start position
	print_coords(str_coords, START_POSITION.lat, START_POSITION.lng);
	target += os_sprintf(target, "%s=%s", DIRECTIONS_API_TAG_START, str_coords);
	// route intermediate waypoints
	size_t waypoints_number = sizeof(WAYPOINTS) / sizeof(struct gps_coords);
	if (waypoints_number)
	{
		target += os_sprintf(target, "&%s=", DIRECTIONS_API_TAG_WAYPOINTS);
	}
	int i;
	for (i = 0; i < waypoints_number; ++i)
	{
		if (i)
		{
			target += os_sprintf(target, "%%7C");
		}
		print_coords(str_coords, WAYPOINTS[i].lat, WAYPOINTS[i].lng);
		target += os_sprintf(target, "via%%3A%s", str_coords);
	}
	// route end position
	print_coords(str_coords, END_POSITION.lat, END_POSITION.lng);
	target += os_sprintf(target, "&%s=%s", DIRECTIONS_API_TAG_END, str_coords);
	// route query time
	target += os_sprintf(target, "&%s", DIRECTIONS_API_TIME);
	// query key
	target += os_sprintf(target, "&%s=%s", DIRECTIONS_API_TAG_KEY, HTTP_QUERY_KEY);
}

// Clears HTTP downloaded content memory;
void release_http_content(void)
{
	if (http_content)
	{
		os_free(http_content);
		http_content = NULL;
	}
}

// Actual HTTP request execution
void http_request(const char* url)
{
	// Memory allocation for pespconn
	pespconn = (struct espconn*)os_zalloc(sizeof(struct espconn));
	// ESP connection setup for TCP
	pespconn->type = ESPCONN_TCP;
	pespconn->state = ESPCONN_NONE;
	// Configuring ESP TCP settings
	pespconn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	// Performing basic URL parsing to extract hostname and HTTP path
	url_prefix_type = parse_url(url, http_hostname, http_path);
	OS_UART_LOG("[INFO] Trying to resolve IP address by hostname `%s` ...\n", http_hostname);
	// Clean HTTP Content loaded on previous submission
	release_http_content();
	// Resolve IP address by hostname
	espconn_gethostbyname(pespconn, http_hostname, &target_server_ip, on_dns_ip_resoved_callback);
}

// HTTP JSON Content Parsing
void process_content(void)
{
	if (http_content)
	{
		char* json_body = (char*)os_zalloc(os_strlen(http_content));
		parse_http_body(http_content, json_body);
		release_http_content();
		bool result_found = false;
		char value_buffer[LABEL_BUFFER_SIZE];
		os_bzero(value_buffer, LABEL_BUFFER_SIZE);

		// "duration_in_traffic" section extraction - to do not parse full heavy HTTP JSON response
		char* duration_section = os_strstr(json_body, JSON_TAG_DURATION);
		if (duration_section)
		{
			duration_section = os_strstr(duration_section, "{");
		}
		char* duration_section_end = 0;
		if (duration_section)
		{
			duration_section_end = os_strstr(duration_section, "}");
		}
		if (duration_section && duration_section_end)
		{
			duration_section_end[1] = 0;
			OS_UART_LOG("[INFO] Parsing JSON section:\n%s\n\n", duration_section);
			// JSON parsing
			struct jsonparse_state parser;
			jsonparse_setup(&parser, duration_section, os_strlen(duration_section));
			int node_type;
			while ((node_type = jsonparse_next(&parser)) != 0 && !result_found)
			{
				if (node_type == JSON_TYPE_PAIR_NAME && jsonparse_strcmp_value(&parser, JSON_TAG_NESTED_VALUE) == 0
												&& jsonparse_get_len(&parser) == os_strlen(JSON_TAG_NESTED_VALUE)
												&& parser.depth == JSON_DEPTH_NESTED_VALUE)
				{
					jsonparse_next(&parser);
					node_type = jsonparse_next(&parser);
					if (node_type == JSON_TYPE_NUMBER)
					{
						jsonparse_copy_value(&parser, value_buffer, sizeof(value_buffer));
						result_found = true;
					}
				}
			}
		}
		if (result_found)
		{
			duration_value = strtol(value_buffer, NULL, 10);
			OS_UART_LOG("[INFO] Parsed time duration value successfully: %d\n", duration_value);
		}
		else
		{
			duration_value = -1;
			OS_UART_LOG("[ERROR] Unable to find time duration in JSON response\n");
		}
		os_free(json_body);
		query_error_flag = !result_found;
	}
	else
	{
		OS_UART_LOG("[ERROR] HTTP content is empty\n");
		query_error_flag = true;
		duration_value = -1;
	}

	if (duration_value > 0)
	{
		uint16 trafic_level = calculate_level(duration_value);
		empty_response_flag = false;
		show_level(trafic_level);
	}
	else
	{
		empty_response_flag = true;
	}
}

// ############################# APPLICATION MAIN LOOP METHOD (TRIGGERED EACH 10 MS) #############################

void main_timer_handler(void* arg)
{
	++tick_index;
	if (tick_index % TIMER_PERIOD_CONN == 0)
	{
		if (!is_station_connected())
		{
			connect();
		}
	}

	if (tick_index % TIMER_PERIOD_LED == 0)
	{
		if (is_station_connected())
		{
			if (!query_error_flag)
			{
				// Build-in LED Heartbeat flashing - when WiFi connection established
				GPIO_OUTPUT_SET(GPIO_PIN_LED, 0);
				os_delay_us(DELAY_HEARTBEAT_FLASH);
				GPIO_OUTPUT_SET(GPIO_PIN_LED, 1);
			}
			else
			{
				// Build-in LED constantly ON - when WiFi connection established and HTTP query error is present
				// (please enable UART_DEBUG_LOGS to investigate if you have such issue)
				GPIO_OUTPUT_SET(GPIO_PIN_LED, 0);
			}
		}
	}

	if (tick_index % TIMER_PERIOD_BLANK_LED == 0)
	{
		if (empty_response_flag)
		{
			show_blank((tick_index / TIMER_PERIOD_BLANK_LED) % 2);
		}
	}

	if ( ( tick_index % TIMER_PERIOD_QUERY == 0 ) ||
		 ( retry_tick_index > 0 && ((tick_index - retry_tick_index) > TIMER_PERIOD_CONN_RETRY) ) ||
		 ( empty_response_flag && (tick_index % TIMER_PERIOD_INITIAL_QUERY == 0) ) )
	{
		if (retry_tick_index > 0)
		{
			retry_tick_index = -1;
			OS_UART_LOG("[INFO] Re-trying to connect after failure ...\n");
		}
		if (is_station_connected() && !is_transfer_started)
		{
			is_transfer_started = true;
			compose_http_request_url(complete_url);
			OS_UART_LOG("[INFO] Submitting HTTP GET Request: %s\n", complete_url);
			http_request(complete_url);
		}
		else
		{
			OS_UART_LOG("[WARNING] Unable to submit HTTP query: is_station_connected:%d, is_already_started:%d\n",
					is_station_connected(),
					is_transfer_started);
			if (!is_station_connected())
			{
				empty_response_flag = true;
			}
		}
	}

	// Close TCP socket connection upon data transfer is completed
	if (tick_index % TIMER_PERIOD_CLOSE_SOCKET == 0)
	{
		if (is_transfer_completed)
		{
			is_transfer_completed = false;
			if (is_secure())
			{
				espconn_secure_disconnect(pespconn);
			}
			else
			{
				espconn_disconnect(pespconn);
			}
		}
	}

	if (tick_index >= TIMER_IDX_RESET)
	{
		tick_index = 0;
		retry_tick_index = -1;
	}
}

// ##################################### APPLICATION MAIN INIT METHODS #####################################

// Used to extend memory by extra 17 KB of iRAM
uint32 user_iram_memory_is_enabled(void)
{
	return 1;
}

void ICACHE_FLASH_ATTR user_pre_init(void)
{
	system_partition_table_regist(part_table, 3, SPI_FLASH_SIZE_MAP);
}

void on_user_init_completed(void)
{
	espconn_secure_set_size(0x01, TLS_HANDSHAKE_BUFFER_SIZE);
	// SNTP connection initialization (used for TLS shared key generation)
	sntp_setservername(0, SNTP_URL);
	sntp_init();
	os_timer_setfn(&start_timer, (os_timer_func_t*)main_timer_handler, NULL);
	os_timer_arm(&start_timer, 10, 1);
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(UART_BAUD_RATE, UART_BAUD_RATE);

	gpio_init();
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	GPIO_OUTPUT_SET(GPIO_PIN_LED, 1);
	GPIO_OUTPUT_SET(GPIO_PIN_SER_DATA, 0);
	GPIO_OUTPUT_SET(GPIO_PIN_SER_CLOCK, 0);
	GPIO_OUTPUT_SET(GPIO_PIN_READ_LATCH, 0);

	wifi_set_opmode(STATION_MODE);
	system_init_done_cb(on_user_init_completed);
}
