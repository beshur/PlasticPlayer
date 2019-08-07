#!/user/bin/env python
###
# The script to power diPlayer
#
# Alex Buznik <shu@buznik.net>
# 2019
###
import serial
import io
import os
from wifi import Cell, Scheme
from airtable import Airtable

SERIAL_PORT = "/dev/ttyUSB0"

###
# Components
###
def sendToSerial(data):
  print(data)

class WiFi(object):
    ssid = ""
    password = ""
    schemeName = "wifiCard"
    wlan = "wlan0"
    errors = {
      "NOT_FOUND": "not found"
    }

    def __init__(self, ssid, password):
      print("WiFi\ns:" + ssid + " p:" + password + " start")
      self.ssid = ssid
      self.password = password

    def connect(self):
      cell = Cell.where(self.wlan, lambda findCell: findCell.ssid.lower() == self.ssid.lower())[0]
      if cell.ssid is None:
        print "Wi-Fi not found: " + self.ssid
        sendToSerial("error&wifi&" + self.errors.NOT_FOUND)
        return

      existingScheme = Scheme.find(self.wlan, self.schemeName)
      if existingScheme:
        existingScheme.delete()
      passkey = self.password or None
      scheme = Scheme.for_cell(self.wlan, self.schemeName, cell, passkey)
      scheme.save()
      scheme.activate()
###
# Airtable provider
###
class TrackLookup(object):
  at = ""
  def __init__(self):
    print("TrackLookup start")
    self.at = Airtable(os.environ["AIRTABLE_BASE_ID"], "")

  def find(self, id):
    print("TrackLookup find: " + id)
    allRecords = self.at.get_all("viw3BQcg0Pdt9NCWK")
    # records = allRecords.search('id', id)
    print(allRecords)


class Commands(object):
  previousLine = ""

  def __init__(self):
    print("Commands start")

  def check(self, line, command):
    if line == self.previousLine:
      return False
    return hasattr(self, command)

  def onCommand(self, line):
    words = line.split("&")
    print("words ")
    print(words)
    if True == self.check(line, words[0]):
      getattr(self, words[0])(words[1:])

  # wifi setup card
  def wifi(self, args):
    print("wifi command detected, connecting to " + args[0])
    theWifi = WiFi(args[0], args[1])
    theWifi.connect()

  # play card to start track lookup
  def play(self, args):
    trackLookup = TrackLookup()
    trackLookup.find(args[0])


###
# Runtime
###
print("RC Receiver 1")
s1 = serial.Serial(SERIAL_PORT, baudrate=9600, timeout=0.3)
sio = io.TextIOWrapper(io.BufferedRWPair(s1, s1))
CommandsInstance = Commands()

s1.flush()

while True:
  if s1.inWaiting() > 0:
    line = sio.readline()
    print(line)

    CommandsInstance.onCommand(line)
