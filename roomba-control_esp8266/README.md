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

<img src="https://github.com/BozoDev/roomba_wifi/blob/master/roomba-control_esp8266/img/roomba_hack_0.jpg?raw=true" width=500>

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

# Setup
## hardware

Most is on the main page, so here the


  * ESP specifics:
    * Voltage levels - it takes both 3.3V Vcc and serial RX
      * I took a "DC-DC step down" (was a bit of an overkill version - pic to follow) and hooked it up
        to the 14V Batt of the "PS2" (mini-DIN?) connector. Works great, but if I were to do it again,
        I'd also put in a small switch, so that I could power off the ESP without having to remove the
        Roomba battery... Make sure it allows a sufficient input range - I'm not the mentioned 14V are
        still 14V whilst charging - documentation calls it 'unregulated' - so it may go up to close to
        20V. I used some hotglue to attach it in the front left half of the Roomba. The main
        part mentionions a +5V connection on the curcuit-board, but mine looked quite a bit different
        (780 model) and I had read somewhere that supposedly you couldn't draw too much power from the
        internal 5V without risking the  device going into some sort of protective mode. Don't forget that
        running an AP or using WiFi with the ESP may require some spike power... The down-side is, that
        (at least the one I used) the DC-DC was close to the size of a match-box, as opposed to a simple
        fixed 5V to 3.3V DC-DC step down the size of a thumbnail.
      * RX-ESP to TX Roomba (Pin4) - I used a simple voltage divider with classical simple resistors.
        I used 1.7KOhm and 3.3KOhm. Soldered both to the RX pin of the ESP, the wire from Roomba's TX-pin
        to the other end of the 1.7KOhm and the free end of the 3.3K to ground on the ESP. All on the
        "chip" side of the ESP - not the long PINs of the connector. Turns out that a classical resistor
        has the perfect length to run from GND to RX.
    * Other connections
      * RESET - I used a 0805 sized SMD 3.3KOhm resistor soldered between Reset and Vcc on the "chip" side
        fits just as perfectly as the classical resistor before from GND to RX.
      * CH-PD - There seem to be conflicting recommendations on how to hook up the CH-PD pin - with or
        without 3K resistor. Since I wanted to be able to reset the ESP, and ran wires from GND and Reset
        to the outside of the Roomba, I didn't think using the same SMD resistor lead was cool (it gets
        shorted upon reset) - I simply ran a wire directly from Vcc to the CH-PD pin.
      * TX-ESP directly to RX Roomba (Pin3)
      * GPIO-0 to Roomba Pin5 (DD or sometimes BRC - either "Device Detect" or "Baud Rate Change"). It
        turned out that this was sufficient to wake the Roomba even in Power-Off mode - but check this
        also applies to your model. Originally I wanted this to go to GPIO-2, but somehow mixed the pins
        whilst soldering :( - I originally also wanted to add a push-button, so that in emergencies I
        could have connected to the mini-DIN and been able to flash the ESP. But I had ran out of suitable
        switches, and didn't want to wait 3 weeks for a new stash to arrive. Should you want to include
        such a switch, remember you need to pull down GPIO-0 at boot for it to accept serial flash
        programming.
      * Vcc ESP to OUT+ of DC-DC step down
      * GND ESP to OUT- of DC-DC step down
      * Roomba mini-DIN Pin 1 OR 2 to In+ of DC-DC step down
      * Roomba mini-DIN Pin 6 OR 7 to In- of DC-DC step down

  * General tips:
    * Breadboard jumper cables can be inserted into the mini-DIN, and to a bread-board hosting the ESP, so
      you can test it all works before dismantling the Roomba and make sure all will work. The Roomba is
      not really hard to take apart, but in order to get to the circuit-board you'll have to take it apart
      entirely. Seeing all those bits spread out around my working area I fear my wife got a shock and
      couldn't imagine me ever getting all those bits back together into working order.
    * Use a multi-meter to measure/set the output of the DC-DC BEFORE connecting it to the ESP ;)
    * Also use e.g. USB 5V and measure the voltage at the place you'd connect to ESP-RX BEFORE you hook it
      up - just to make sure it's the desired 3.3V and all resistors are correctly connected
    * I used 2 bread-board female-female jumper cables on the ESP-01 connector side and ran them out below
      the Roomba DC charging socket at the side. One connected to GND and on RESET.

      




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


