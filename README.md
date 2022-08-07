ESP8266 Traffic Jam Monitor
===========================

This project is designed to constantly display current traffic jams conditions on a specific route. Application is integrated with Google Directions API which allows
to get the current traffic congestion information using REST API. This solution can be executed on ESP8266 module with LED bar used for traffic congestion indication. Compatible PCB is also available under this link:
[ESP-12E \ ESP-12F LED Bar PCB](https://github.com/sigma-prj/esp-12-e-led-bar-pcb)

Traffic jams indication built using LED Bar - where the amount of ignited LEDs shows pro-rata amount of time spent in traffic under a specific route.
Within this application, the definition of the following parameter can be set:

```c++
// defines worst time on the route (in seconds) - all warning LEDs will be ignited - means traffic jam
static const sint32 WORST_ROUTE_TIME = 1600;
// defines best time on the route (in seconds) - all warning LEDs will be off - means road is free
static const sint32 BEST_ROUTE_TIME = 960;
```

In this specific case - if the traffic journey takes about 1600 seconds or more then all LEDs will be ignited (indicating that road traffic is over-congested).
And as the opposite - in case of 960 seconds or less spent in traffic then no LEDs will be ignited. All intermediate states will be interpolated linearly.

The journey route itself is defined using the following constant variables:

```c++
// route start GPS position
static const struct gps_coords START_POSITION  = { 51.564418, -0.062658 };
// route end GPS position
static const struct gps_coords END_POSITION  = { 51.519986, -0.082895 };
// intermediate waypoint GPS positions
static const struct gps_coords WAYPOINTS[]  =
{
  { 51.556724, -0.074518 },
  { 51.531606, -0.077044 }
};

```

**START_POSITION** and **END_POSITION** establishes GPS coordinates of the start and end location accordingly. Additional intermediate GPS coordinates can be set using **WAYPOINTS** array.
These 'waypoint' coordinates, which should represent intermediate points on a route, will help to get rid of alternative routes which Directions API can provide.
These alternative routes can create ambiguity in displaying traffic conditions at the LED bar. Like here - such alternative routes can be presented by Directions API and will create ambiguity in displaying using LED bar:

![Sample Route](https://github.com/sigma-prj/esp-highway-traffic-monitor/blob/main/docs/resources/sample_route.png)

### Configuring WiFi Connection and Access to Google Directions API

In order to connect to the WiFi router and to get access to Directions REST API the following parameters need to be set:

```c++
// Update according to WiFi session ID
#define WIFI_SSID             "[WIFI-SESSION-ID]"
// Update according to WiFi session password
#define WIFI_PASSPHRASE       "[WIFI-PASSPHRASE]"
// Directions API Key
#define HTTP_QUERY_KEY        "[GOOGLE-DIRECTIONS-API-KEY]"
```

*WIFI_SSID* and *WIFI_PASSPHRASE* values need to be set according to the Internet router WiFi session configuration. And *HTTP_QUERY_KEY* key needs to be received while registering with Google Directions API.
In case of requests to the Directions API REST Service will not be sent very frequently (e.g. one time per 10 - 20 minutes), then the free-access threshold is supposed to be met and this API usage should remain free.
More details can be checked on the official Google Maps Directions API pages: 
https://mapsplatform.google.com/pricing/
https://developers.google.com/maps/documentation/directions/usage-and-billing

Requirements and Dependencies
-----------------------------

This application is targeted to be built under ESP8266_NONOS_SDK platform (version 3.0.0 or 3.0.5).
Target ESP8266 NON OS SDK is available for download at [ESP NON OS SDK 3.0.5](https://github.com/espressif/ESP8266_NONOS_SDK/tree/release/v3.0.5).
Application repository project files can be placed to any subfolder under SDK root folder.

### Google Directions API and TLS 1.3 handshakes on ESP8266 SDK. MBEDTLS Library. 

Directions REST API which is used in this application is secured by the most recent TLS 1.3.
And the certificate size used for TLS handshake is a bit bigger than expected for standard ESP8266 SDK ssl libraries.
This way, a standard library like *libssl.a* might not work correctly when trying to establish the connection using *espconn_secure_connect*.
In a such case, the application might cause an out-of-memory error on ESP8266 and might trigger ESP chip reboot.

Due to these reasons - MBEDTLS library is used in this application. MBEDTL is an open-source library and allows to override the required memory
allocation for TLS handshakes. To allow TLS handshakes to be executed without errors, there is a need to increase MBEDTLS
allocated memory size from 8192 to 9800. This can be achieved by the following steps:

Under *[esp-sdk]/third_party/Makefile* there is a need to set build mode from *debug* to *release*:

```c++
TARGET = eagle
#FLAVOR = debug
FLAVOR = release
```

Under *[esp-sdk]/third_party/make_lib.sh* script there is a need to re-target output folder from *.output/eagle/debug* to *.output/eagle/release*

```sh
#cp .output/eagle/debug/lib/lib$1.a ../../lib/lib$1.a
cp .output/eagle/release/lib/lib$1.a ../../lib/lib$1.a
xtensa-lx106-elf-strip --strip-unneeded ../../lib/lib$1.a
cd ..
```

Within *[esp-sdk]/third_party/include/mbedtls/sys/espconn_mbedtls.h* the amount memory to be allocated needs to be increased from 8192 to 9800

```c++
//#define ESPCONN_SECURE_MAX_SIZE 8192
#define ESPCONN_SECURE_MAX_SIZE 9800
#define ESPCONN_SECURE_DEFAULT_HEAP 0x3800
#define ESPCONN_SECURE_DEFAULT_SIZE 0x0800
#define ESPCONN_HANDSHAKE_TIMEOUT 0x3C

```

And as the last step there is a need to outline build target for MBEDTLS as ESP8266.
Just need to uncomment at *[esp-sdk]/third_party/include/mbedtls/config_esp.h* :

```c++
/**
 * \def ESP8266_PLATFORM
 *
 * Enable the ESP8266 PLATFORM.
 *
 * Module:  library/ssl_tls.c
 * Caller:
 */
#define ESP8266_PLATFORM

```

Once pre-configuring is done - MBEDTLS library build can be triggered by the following instruction:

```sh
cd [esp-sdk]/third_party
./make_lib.sh mbedtls
```

Once compilation is done - newly built library can be found at : *[esp-sdk]/lib/libmbedtls.a*.
No need to copy this library anywhere, as it will be picked-up automatically by ESP application build Makefile.


Build and UART Logs Configuration
---------------------------------

In order to build this project, the standard ESP SDK procedure can be followed.
Build process can be triggered by execution of default target on a project's root Makefile
with specific set of arguments.

Typical Makefile target execution sample and arguments set can be found at ESP SDK example:
```
[esp-sdk]/examples/peripheral_test/gen_misc.sh
```

Particular shell command and argumets may depends on a specific ESP board configuration.
Below is the typical example to trigger build on a certain ESP configuration:

```sh
make COMPILE=gcc BOOT=none APP=0 SPI_SPEED=20 SPI_MODE=DIO SPI_SIZE_MAP=4 FLAVOR=release
```

This application also can be built with output debug UART logs enabled.
For these purposes, special symbol UART_DEBUG_LOGS can be defined in build configuration:

```sh
make COMPILE=gcc BOOT=none APP=0 SPI_SPEED=20 SPI_MODE=DIO SPI_SIZE_MAP=4 FLAVOR=release UNIVERSAL_TARGET_DEFINES=-DUART_DEBUG_LOGS
```


Flashing Compiled Binaries to ESP Chip
--------------------------------------

The following scripts can be found under project's root folder:

```sh
./erase_mem_non_ota.sh
./flash_mem_non_ota.sh
```

These scripts can be used to flash compiled binaries into ESP chip memory. Erase script also will allow to clean-up ESP memory before flashing actual application.
Once binaries are compiled, the shell script can be triggered directly either through corresponding Makefile targets:

```sh

# To erase ESP memory
make esp_erase
# To flash actual application into ESP memory
make esp_flash
# To remove binary eagle files from build output folder
make clean_image

```

Within shell script there is a need to update the following input parameters:

     - PORT : This argument needs to point to proper location of usb-to-serial device which makes connection to ESP module
     - PYTHON : Path to Python executable
     - ESP_TOOL : Path to Python esptool used to flash application
     - FW_BIN_DIR : Folder location where compiled binary eagle files are stored

For more information about how ESP NON-OS SDK can be setup and can be used for app compilation and flashing - please refer the following link:
http://www.sigmaprj.com/esp8266.html

License
-------


ESP8266 Traffic Jam Monitor

Project is distributed under GNU GENERAL PUBLIC LICENSE 3.0

Copyright (C) 2022 - www.sigmaprj.com

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

ESPRESSIF MIT License

Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case, it is free of charge, to any person obtaining a copy of this software and associated documentation files (the Software), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
