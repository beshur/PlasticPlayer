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
###
import serial
import io
import os
import subprocess
import requests
import urllib
import websocket
import threading
import json
import pprint
import RPi.GPIO as GPIO
import time

SERIAL_PORT = "/dev/ttyUSB0"
# ASCII End of Transmission Block
SERIAL_TERMINATOR = str(chr(23))

###
# Components
###
def sendToSerial(data):
  print("deprecated: Use TalkToSerial class", data)

def onPowerBtnClick(talkToSerial):
  print("Power button was pushed!")
  talkToSerial.send(getSerialType("sys"), "Shutdown")
  time.sleep(3)
  os.system("poweroff")

class WiFi(object):
    ssid = ""
    password = ""

    def __init__(self, ssid, password):
      print("WiFi\ns:" + ssid + " p:" + password + " start")
      self.ssid = ssid
      self.password = password

    def connect(self):
      # Don't try to do this at home
      file_in = "/etc/mopidy/mopidy.conf"
      file_out = "mopidy_conf.tmp"

      os.spawnlp(os.P_WAIT, 'cp', 'cp', file_in, file_in + "_bak")

      with open(file_in, "rt") as fin:
        with open(file_out, "wt") as fout:
          for line in fin:
            if line.find('wifi_network') == 0:
              fout.write('wifi_network = ' + self.ssid)
            elif line.find('wifi_password') == 0:
              fout.write('wifi_network = ' + self.password)
            else:
              fout.write(line)

      os.rename(file_out, file_in)

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
  playing = False
  volume = 0

  def __init__(self, talkToSerial):
    print("PlayBack start")
    self.listenTo = ["playback_state_changed", "stream_title_changed"]
    self.talkToSerial = talkToSerial
    websocket.enableTrace(True)
    self.ws = websocket.WebSocketApp("ws://localhost:6680/mopidy/ws",
      on_message = self.on_message,
      on_error = self.on_error,
      on_close = self.on_close)
    self.ws.on_open = self.on_open

    self.wst = threading.Thread(target=self.ws.run_forever)
    self.wst.daemon = True
    self.wst.start()

  # mopidy events
  def playback_state_changed(self, data):
    state = data["new_state"]
    self.playing = True if state == "playing" else False
    if self.playing  != True:
      self.talkToSerial.send(getSerialType("text"), state)

  def stream_title_changed(self, data):
    title = data["title"]
    self.talkToSerial.send(getSerialType("title"), title)

  # websocket events
  def on_open(self, data):
    print("ws on_open")

  def on_message(self, data):
    # print("ws on_message", data)
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

  def on_close(self, data, error):
    print("ws on_close", data, error)

  def get_track_info(self, data):
    print("track info:")
    print(data)

  def _rpc(self, data):
    data["id"] = self.messageId
    data["jsonrpc"] = "2.0"
    self.messageId += 1
    return json.dumps(data)

  def _send_messages(self, messages):
    for i in messages:
      self.ws.send(self._rpc(i))

  def play(self, uri):
    print(uri)
    messages = [{"method":"core.tracklist.clear"},
      {"method":"core.tracklist.add","params":{"uris":[uri]}},
      {"method":"core.playback.play"}]

    self._send_messages(messages)

  def onPlayButton(self):
    if self.playing == True:
      self.pause()
    else:
      self.resume()

  def next(self):
    messages = [{"method":"core.playback.next"}]
    self._send_messages(messages)

  def resume(self):
    self.talkToSerial.send(getSerialType("text"), "Play")
    messages = [{"method":"core.playback.resume"}]
    self._send_messages(messages)

  def pause(self):
    self.talkToSerial.send(getSerialType("text"), "Pause")
    messages = [{"method":"core.playback.pause"}]
    self._send_messages(messages)

  def setVolume(self, volumeVal):
    print("setVolume", volumeVal)
    messages = [{"method":"core.mixer.set_volume", "params":{"volume": volumeVal}}]
    self._send_messages(messages)

  def clear(self):
    self.talkToSerial.send(getSerialType("text"), "Stop")
    messages = [{"method":"core.playback.stop"},
      {"method":"core.tracklist.clear"}]
    self._send_messages(messages)

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
    if command == "button" or command == "volume" or command == "wifi":
      return True

    if line == self.previousLine:
      return False

    if hasattr(self, command):
      self.previousLine = line
      return True
    else:
      return False

  def onCommand(self, line):
    cleanLine = line.split(SERIAL_TERMINATOR)[0]
    words = cleanLine.split("&")
    print("words ")
    print(words)
    if True == self.check(line, words[0]):
      getattr(self, words[0])(words[1:])

  # handshake
  def handshake(self, args):
    self.talkToSerial.send(getSerialType("handshake"), "true")

  # wifi setup card
  def wifi(self, args):
    print("wifi command detected, connecting to " + args[0])
    theWifi = WiFi(args[0], args[1])
    # theWifi.connect()

  def volume(self, args):
    volumeVal = args[0].replace('\n', '')
    self.playBack.setVolume(int(volumeVal))

  # play card to start track lookup
  def play(self, args):
    # 04:1A:D1:FA:86:52:81
    trackLookup = TrackLookup()
    nfcCardId = args[0][0:20].replace(' ', ':')
    track = trackLookup.find(nfcCardId)
    self.playBack.play(track)
  # button click
  def button(self, args):
    buttonName = args[0].replace('\n', '')
    print('button', buttonName)
    # toggle play/pause
    if buttonName == "play":
      self.playBack.onPlayButton()

    # next track
    if buttonName == "next":
      self.playBack.next()

    # clear and stop
    if buttonName == "clear":
      # clear previous line because the card was ejected
      self.previousLine = ""
      self.playBack.clear()

class TalkToSerial(object):
  s = {}
  delimiter = "&"

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

    command = self.delimiter + stype + self.delimiter + data + SERIAL_TERMINATOR
    encodedCommand = command.encode()
    print("TalkToSerial send command", encodedCommand)
    self.s.write(command.encode())

def getSerialType(name):
  types = {
    "text": "text",
    "title": "title",
    "sys": "sys",
    "handshake": "handshake"
  }
  return types[name];

class CpuTemp(object):
  talkToSerial = {}
  temp = 0.0
  timer = {}
  cmd = "vcgencmd measure_temp | egrep -o '[0-9]*\.[0-9]*' | tr -d '\n'"

  def __init__(self, talkToSerial):
    self.talkToSerial = talkToSerial

  def start(self):
    timer = threading.Timer(20.0, self.measure)
    timer.start()

  def stop(self):
    timer.cancel()

  def measure(self):
    self.temp = float(subprocess.check_output(self.cmd, shell=True))
    print("CPU Temperature: " + str(self.temp))

    if self.temp > 85:
      print("CPU Temperature Overheat Shutdown")
      onPowerBtnClick(self.talkToSerial)
    elif self.temp > 80:
      print("CPU Temperature near OverHeat")
      self.talkToSerial.send(getSerialType("sys"), "Overheating")

    return self.temp

###
# Runtime
###
print("RC Receiver 1")
s1 = serial.Serial(SERIAL_PORT, baudrate=115200, timeout=0.3)
sio = io.TextIOWrapper(io.BufferedRWPair(s1, s1))
TalkToSerialInstance = TalkToSerial(s1)
PlayBackInstance = PlayBack(talkToSerial = TalkToSerialInstance)
CommandsInstance = Commands(playBack = PlayBackInstance, talkToSerial = TalkToSerialInstance)
CpuTempInstance = CpuTemp(talkToSerial = TalkToSerialInstance)

GPIO.setmode(GPIO.BOARD)
GPIO.setup(10, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)

CpuTempInstance.start()
s1.flush()

while True:
  if s1.inWaiting() > 0:
    line = sio.readline()
    print(line)

    CommandsInstance.onCommand(line)
    CpuTempInstance.measure()

  if GPIO.input(10) == GPIO.HIGH:
    onPowerBtnClick(TalkToSerialInstance)
