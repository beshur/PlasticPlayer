#!/bin/sh

# This is a script to compile and upload Arudino sketch right from the Raspberry Pi
# Only needed for development
# Dependencies:
# arduino-cli (https://github.com/arduino/arduino-cli)

flash() {
  echo "Uploading sketch..."
  arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano:cpu=atmega328old player1
}

cd /root/Arduino
echo "Compiling sketch..."
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old player1 && flash
