# Plastic Player Arduino Controller

This is a rework of the original Plastic Player js version.

I found the ESP wi-fi module together with Raspberry Pi 
(both sitting on wi-fi network) is a bit too much,
so I want to optimize it by moving hard work of network
requests and wi-fi handling to Raspberry Pi.
This is a new version of the player software.

It targets Arduino (not Espruino) and also has the Python component for Raspberry Pi.

## Hardware

- Raspberry Pi (with Pi Musicbox)
- Arduino (for controlling peripherals)
- Elechouse PN532 NFC-module
- SSD1306 OLED display (128x32)

### Wiring

Arduino Uno has I2C which is used to talk to NFC and OLED.

- PN532 SDA to A4, SCL to A5
- SSD1306 SDA to A4, SCL to A5

## Software

Arduino sketch is loaded is usual.

### Raspberry Pi Dependencies

1. Update /etc/apt/sources.list to point to stretch
2. sudo apt-get install vim raspi-config build-essential python-dev
3. pip install pyserial websocket-client wifi

Python `rp-receiver.py` file has to be loaded to your Raspberry Pi and run as `root`:

```
sudo nohup python rp-receiver.py &
```
