#!/user/bin/env python
###
# The script to power diPlayer
#
# Alex Buznik <shu@buznik.net>
# 2019
#
# Dependencies:
# - pyserial
# - websocket-client
# - wifi
###
import serial
import io
import os
import requests
import urllib
import websocket
import threading
import json
import pprint
from wifi import Cell, Scheme

SERIAL_PORT = "/dev/ttyUSB0"

###
# Components
###
def sendToSerial(data):
  print("deprecated: Use TalkToSerial class", data)

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

    result = ""
    resp = {}
    getUri = 'https://api.airtable.com/v0/' + self.baseId + '/records?filterByFormula=%7BnfcId%7D%3D\'' + urllib.quote_plus(id) + '\''
    print(getUri)
    try:
      response = requests.get(getUri, headers=headers, allow_redirects=False)
      # if init_res.status_code == 200:
      resp = response.json()

    except Exception as ce:
      print(ce)

    print(resp)

    try:
      result = resp["records"][0]["fields"]["uri"]
    except Exception as ce:
      print(ce)

    return result

class PlayBack(object):
  ws = {}
  wst = {}
  talkToSerial = {}
  messageId = 0
  listenTo = []

  def __init__(self, talkToSerial):
    print("PlayBack start")
    self.listenTo = ["playback_state_changed", "stream_title_changed", "volume_changed"]
    self.talkToSerial = talkToSerial
    websocket.enableTrace(True)
    self.ws = websocket.WebSocketApp("ws://localhost:6680/mopidy/ws",
      on_message = self.on_message,
      on_error = self.on_error,
      on_close = self.on_close)
    # self.ws.on_open = self.on_open

    self.wst = threading.Thread(target=self.ws.run_forever)
    self.wst.daemon = True
    self.wst.start()

  # mopidy events
  def playback_state_changed(self, data):
    state = data["new_state"]
    self.talkToSerial.send(getSerialType("text"), state)

  def stream_title_changed(self, data):
    title = data["title"]
    self.talkToSerial.send(getSerialType("text"), title)

  def volume_changed(self, data):
    volume = data["volume"]
    self.talkToSerial.send(getSerialType("text"), "Volume: " + str(volume))

  # websocket events
  def on_open(self, data):
    print("ws on_open")

  def on_message(self, data):
    print("ws on_message", data)
    parsed = {}
    event = {}
    parsed = json.loads(data)

    if parsed.has_key("event"):
      event = parsed["event"]
      if event in self.listenTo:
        method = getattr(self, event)
        method(parsed)

  def on_error(self, data):
    print("ws on_error")

  def on_close(self, data):
    print("ws on_close")

  def get_track_info(self, data):
    print("track info:")
    print(data)

  def _rpc(self, data):
    data["id"] = self.messageId
    data["jsonrpc"] = "2.0"
    self.messageId += 1
    return json.dumps(data)

  def play(self, uri):
    print(uri)
    messages = [{"method":"core.tracklist.clear"},
      {"method":"core.tracklist.add","params":{"uris":[uri]}},
      {"method":"core.playback.play"}]
    for i in messages:
      self.ws.send(self._rpc(i))

  def close(self):
    self.ws.close()
    self.wst.stop()


class Commands(object):
  previousLine = ""
  playBack = {}
  talkToSerial = {}

  def __init__(self, playBack=None, talkToSerial=None):
    print("Commands start")
    self.playBack = playBack
    self.talkToSerial = talkToSerial

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
    nfcCardId = args[0].replace('\n', '').replace(' ', ':')
    track = trackLookup.find(nfcCardId)
    self.playBack.play(track)

  # button click
  def button(self, args):
    buttonName = args[0].replace('\n', '')
    if buttonName == "power":
      self.talkToSerial.send(getSerialType("text"), "Shutting down...")
      os.system("poweroff")

class TalkToSerial(object):
  s = {}
  delimiter = "&"
  # ASCII End of Transmission Block
  terminator = str(chr(23))

  def __init__(self, s):
    print("TalkToSerial start")
    self.s = s

  def _verifyInput(self, stype):
    result = True
    try:
      getSerialType(stype)
    except Exception(ce):
      print("_verifyInput failed", ce)
      result = False
    return result

  def send(self, stype, data):
    print("TalkToSerial send", stype, data)
    if self._verifyInput(stype) != True:
      print("TalkToSerial stype invalid")

    command = self.delimiter + stype + self.delimiter + data + self.terminator
    encodedCommand = command.encode()
    print("TalkToSerial send command", encodedCommand)
    self.s.write(command.encode())

def getSerialType(name):
  types = {
    "text": "text"
  }
  return types[name];

###
# Runtime
###
print("RC Receiver 1")
s1 = serial.Serial(SERIAL_PORT, baudrate=115200, timeout=0.3)
sio = io.TextIOWrapper(io.BufferedRWPair(s1, s1))
TalkToSerialInstance = TalkToSerial(s1)
PlayBackInstance = PlayBack(talkToSerial = TalkToSerialInstance)
CommandsInstance = Commands(playBack = PlayBackInstance, talkToSerial = TalkToSerialInstance)

s1.flush()

while True:
  if s1.inWaiting() > 0:
    line = sio.readline()
    print(line)

    CommandsInstance.onCommand(line)
