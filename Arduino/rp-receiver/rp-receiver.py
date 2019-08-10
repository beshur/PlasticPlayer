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
import requests
import urllib
from wifi import Cell, Scheme

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
  apiKey = ""
  baseId = ""
  def __init__(self):
    self.apiKey = os.environ["AIRTABLE_API_KEY"]
    self.baseId = os.environ["AIRTABLE_BASE_ID"]
    print("TrackLookup start")

  def find(self, id):
    print("TrackLookup find: " + id)
    headers = {
        'authorization': "Bearer " + self.apiKey,
        'content-type': "application/json",
    }

    result = {}
    getUri = 'https://api.airtable.com/v0/' + self.baseId + '/records?filterByFormula=%7BnfcId%7D%3D\'' + urllib.quote_plus(id) + '\''
    print(getUri)
    try:
      response = requests.get(getUri, headers=headers, allow_redirects=False)
      # if init_res.status_code == 200:
      result = response.json()

    except Exception as ce:
      print(ce)

    print(result)
    print(result["records"][0]["fields"]["uri"])

    return result


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
    trackLookup.find(args[0].replace('\n', ''))


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
