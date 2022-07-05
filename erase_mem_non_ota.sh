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
echo "INFO: Erasing Flash Memory ..."
$PYTHON $ESP_TOOL --port $PORT --baud 115200 erase_flash
if [ $? -eq "0" ]; then
  echo "INFO: Erasing is completed"
else
  echo "ERROR: Unable to complete \"Erase Flash Memory\" step"
  exit 1
fi
