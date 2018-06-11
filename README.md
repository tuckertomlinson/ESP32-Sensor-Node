# ESP32-Sensor-Node

This code is associated with a project to create a wireless data-logging system for the laboratory environment.
The project is motivated by personal frustration with the persistent difficulty experienced adding new sensors
to the lab. (in this case the lab is a behavioral neuroscience lab collecting behavioral data at ~100hz, EMG 
data at ~2khz, and neurophysiologic data at ~30khz). Despite the advent of ever cheaper and more powerful 
processors and sensors, it always seems like there are not enough input lines to whatever ADC is being used,
and adding a new ADC adds complexity and failure modes as synchronization is not always reliable, or easy to 
wire. My goal is to create a framework where small, WIFI-capible microcontrollers can be powered up and 
automatically start streaming synchronized data to a central server. If you are looking for current solutions
I recommend taking a look at the "lab streaming layer." That code is much better established than mine, and 
currently a MUCH better prospect for forking. 

Overall Project goals:
-Sensors should be wireless, theoretically battery power capible
-Synchronization should be accomplished via NTP, and be accurate to 100us or better
   -High accuracy NTP servers on the local network will likely be required
-Sensors should be capible of collecting and sending 16 channels of data at 100hz
   -Stretch goal: sensors should be capible of collecting and sending 16 channels at 2khz
-Sensors should send data using the widely available MQTT protocol


This component of the project:
This code base is associated with the actual sensor development. I have identified the ESP32 as a good 
candidate for the sensor modules in this project, as it has a fairly fast dual core processor, integrated
WIFI, and excellent documentation/SDK, and a community that appears to be thriving after the runaway success
of the ESP8266. Alternative modules would be the ESP8266 (single core, slower, community SDK only), and TI's
CC3200 module (I avoided this because there was substantially less community activity for this chip when I 
started the project). In principle, any sensor that can communicate over MQTT will be a viable basis for 
this schema.

Status:
-Initial code written and compiling: NOT TESTED 
   -Smart connect to allow user to configure wifi via cell-phone app
   -Includes SNTP (copied liberally from espressif's example)
   -read analog data from ADC (copied liberally from espressif's example
   -Basic MQTT 
      -listen for control commands
      -write analog data
   
TODO:
-decide whether smart-connect or hardcoding into the firmware is the right way to do network connection
-update SNTP to full NTP spec
   -can use SNTP calls to get network time, but will need to implement local timestamping, buffers, and
    clock updating ala adjusttime
-add timestamps to analog data in the MQTT packets
-Design new PCB that integrates SPI ADC
   -Will need new code for the SPI communication
   -Will probably use FTDI for programming/communication with the microcontroller to avoid usb->serial
    components onboard
-change 10 sample while loop to be some continuous sampling process

