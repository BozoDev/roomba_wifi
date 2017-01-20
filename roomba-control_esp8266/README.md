# Description
I had a Roomba 780 model, that had just lost its warranty - so, time to turn it into something
more useful than the original.

Since I already had a python Wemo "emulator" running, I simply added the Roomba, and can now
say "Alexa, turn on Roomba" to start cleaning cycle and "Alexa, turn off Roomba" to send it
back to its dock.
But I can also issue commands like "goniki" to it from FHEM via MQTT to have it spot-clean a
specific room.

The WiFi setup credentials were removed from the source code (well, at least "my" credentials)
I thought it better the Roomba started an AP and lets you enter your credentials.

With the Arduino-IDE I used the OTA method, to upload changed versions of the firmware.


# Prerequisities
## hardware
 * ESP8266-01 Module (preferably with 1M Flash)
 * A Roomba (I had 780 model - others may need tweaking to the commands)


## software
 * Arduino IDE (during development)
 * MQTT-Broker
 * Optional:
  * Python-Wemo emulator
  * Some sort of home automation system that will control the Roomba either
      via MQTT or Web-API


# Experienced issues
## Hardware
  * Getting the flash/spiffs size correct is crutial - otherwise, all sorts of nasty stuff happen
     (I used a 1M chip and set 192K SPIFFS)
  * ESP-01 512K version is a tricky to get to work - probably due to sketch size (+256K) - make sure
      all works BEFORE permanently mounting it into the Roomba and closeing the case. By ALL I mean ALL.
      Upload new version via OTA, change WiFi-credentials back and forth in the Webinterface "settings" 
      and so on...
  * ESP-01 needs an extra wire soldering to its pins in order to be able to wake up after sleep

## Software
  * At a later stage I set up another ESP-01 for development purposes, since I didn't want to risk having
      to dis-assemble the Roomba due to some bug.
      * Turned out, that 2 ESPs with the same MQTT-Broker-ID are not such a good idea, so if using more than
        1 ESP concurrently, make sure that both the HOST name AND the MQTT-ID are different...


# ToDo
## Software
  * MQTT settings are stored, but not read - it will only use the compiled-in values
  * Create some means of storing a command-file on SPIFFS to be executed
      (to easily create/update different paths to rooms)


