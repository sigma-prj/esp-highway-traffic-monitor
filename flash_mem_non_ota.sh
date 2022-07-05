PORT=/dev/serial/esp8266
PYTHON=/usr/bin/python2.7
ESP_TOOL=~/.local/bin/esptool.py
FW_BIN_DIR=~/esp8266-dev-kits/esp-native-sdk/bin

if [ ! -c $PORT ]; then
  echo "ERROR: No USB tty device found"
  exit 1
else
  echo "INFO: Using USB tty device: $PORT"
fi

echo
echo "INFO: Before this step - please make sure to move jumper to \"FLASH\" pin and reset ESP"
echo -n "INFO: Flashing Firware ..."
$PYTHON $ESP_TOOL --chip esp8266 --port $PORT --baud 115200 write_flash --flash_freq 20m --flash_mode dio --flash_size detect --verify 0x000000 "$FW_BIN_DIR/eagle.flash.bin" 0x010000 "$FW_BIN_DIR/eagle.irom0text.bin" 0x3FB000 "$FW_BIN_DIR/blank.bin" 0x3FC000 "$FW_BIN_DIR/esp_init_data_default_v08.bin" 0x3FE000 "$FW_BIN_DIR/blank.bin"
if [ $? -eq "0" ]; then
  echo "INFO: Flashing is completed"
  echo "INFO: To run application please move jumper to \"EXEC\" pin and reset ESP module ..."
else
  echo "ERROR: Unable to complete \"Flash Memory\" step"
  exit 1
fi
