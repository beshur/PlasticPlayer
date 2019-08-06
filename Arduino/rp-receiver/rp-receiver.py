#!/user/bin/env python
###
# The script to power diPlayer
#
# Alex Buznik <shu@buznik.net>
# 2019
###
import serial
import io
from wifi import Cell, Scheme

###
# Components
###
def sendToSerial(data):
  print(data)

class WiFi(object):
    ssid = ""
    password = ""
    errors = {
      "NOT_FOUND": "not found"
    }

    def __init__(self, ssid, password):
      print("WiFi\ns:" + ssid + " p:" + password)
      self.ssid = ssid
      self.password = password

    def connect(self):
      cell = Cell.where("wlan0", lambda cell: cell.ssid.lower() == self.ssid.lower())
      if cell.ssid is None:
        print "Wi-Fi not found: " + self.ssid
        sendToSerial("error&wifi&" + self.errors.NOT_FOUND)
        return
      passkey = self.password or None
      scheme = Scheme.for_cell('wlan0', 'home', cell, passkey)
      scheme.save()
      scheme.activate()

###
# Runtime
###
port = "/dev/ttyACM0"
s1 = serial.Serial(port, baudrate=9600, timeout=0.3)
sio = io.TextIOWrapper(io.BufferedRWPair(s1, s1))

s1.flush()

while True:
  if s1.inWaiting() > 0:
    line = sio.readline()
    print(line)
    words = line.split("&")

    if words[0] == "wifi":
      print("wifi command detected, connecting to " + words[1])
      theWifi = WiFi(words[1], words[2])
      theWifi.connect()
