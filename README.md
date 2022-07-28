ESP8266 Traffic Jam Monitor
===========================

Requirements and Dependencies
-----------------------------

This application is targeted to be built under ESP8266_NONOS_SDK platform (version 3.0.0 or 3.0.5).
Target ESP8266 NON OS SDK is available for download at [ESP NON OS SDK 3.0.5](https://github.com/espressif/ESP8266_NONOS_SDK/tree/release/v3.0.5).
Application repository project files can be placed to any subfolder under SDK root folder.

### Google Directions API and TLS 1.3 handshakes on ESP8266 SDK. MBEDTLS Library. 

Directions REST API which is used in this application is secured by the most recent TLS 1.3.
And the certificate size used for TLS handshake is a bit bigger than expected for standard ESP8266 SDK ssl libraries.
This way, standard library like *libssl.a* might not work correctly when trying to establish connection using *espconn_secure_connect*.
In a such case application might cause out-of-memory error on ESP8266 and might trigger ESP chip reboot.

Due to these reasons - MBEDTLS library is used at this application. MBEDTL is open-source library and allows to override the required memory
allocation for TLS handshakes. In order to allow TLS handshakes to be executed without errors there is a need to increase MBEDTLS
allocated memory size from 8192 to 9800. This can be achieved by the following steps:

Under *[esp-sdk]/third_party/Makefile* there is a need to set build mode from *debug* to *release*:

```
TARGET = eagle
#FLAVOR = debug
FLAVOR = release
```

Under *[esp-sdk]/third_party/make_lib.sh* script there is a need to re-target output folder from *.output/eagle/debug* to *.output/eagle/release*

```
#cp .output/eagle/debug/lib/lib$1.a ../../lib/lib$1.a
cp .output/eagle/release/lib/lib$1.a ../../lib/lib$1.a
xtensa-lx106-elf-strip --strip-unneeded ../../lib/lib$1.a
cd ..
```

Within *[esp-sdk]/third_party/include/mbedtls/sys/espconn_mbedtls.h* the amount memory to be allocated needs to be increased from 8192 to 9800

```
//#define ESPCONN_SECURE_MAX_SIZE 8192
#define ESPCONN_SECURE_MAX_SIZE 9800
#define ESPCONN_SECURE_DEFAULT_HEAP 0x3800
#define ESPCONN_SECURE_DEFAULT_SIZE 0x0800
#define ESPCONN_HANDSHAKE_TIMEOUT 0x3C

```

And as the last step there is a need to outline build target for MBEDTLS as ESP8266.
Just need to uncomment at *[esp-sdk]/third_party/include/mbedtls/config_esp.h* :

```
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

```
cd [esp-sdk]/third_party
./make_lib.sh mbedtls
```

Once compilation is done - newly built library can be found at : *[esp-sdk]/lib/libmbedtls.a*.
No need to copy this library anywhere, as it will be picked-up automatically by ESP application build Makefile.


Build and UART Logs Configuration
-----------------------------

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
