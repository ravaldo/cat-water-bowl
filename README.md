# Auto-filling pet water bowl

As a (former) cat owner for many years, this project arose from of my increasing dislike with the time it took to clean and maintain the pet [drinking fountain](https://i.imgur.com/fLEMcdn.jpg) I had. I wanted to return to a standard water bowl again and have it automatically filled with water:
* quick to clean
* no need to purchase filters
* smaller volumes of water meant fewer worries of water stagnating
* push notifications sent to my phone
  
![3d printed](/images/20161102_161907.jpg)



## CAD / 3d Printing
The Fusion 360 file is included. I printed the spout in ABS in three parts so I could solvent weld them together and then vapour smooth them to be water proof. The light ring was printed in a transparent PLA. While the base could be printed (assuming your print bed is large enough) I opted to make it out of plywood and a 1:1 paper template is included.

## Hardware
|quantity|parts|
|:-:|-|
|1|Arduino Nano|
|2|NRF24L01|
|1|N-Channel Mosfet (SUP75N06-08)|
|1|Flyback Diode (1N4004)|
|3|NPN Transistors (BC548)|
|1|5V Voltage Regulator (LD117AV50)|
|1|3.3V Voltage Regulator (LD117AV33)|
|1|12V RGB LED strip segment|
|6|capacitors|
|6|resistors|
|2|RJ45 breakout boards or keystone jacks|
|1|DC Barrel Jack|
|1|12V Power Supply|
|1|12V Solenoid Valve (!NORMALLY CLOSED!)|
|1|15mm equal isolating tee|
|1|15mm x 1/4" compression straight adaptor|
|1|10mm barbed fitting to 1/4" BSP|
|1m|3/8" ID, 1/2" OD Hose|
|1|small tension spring|
|2|M5 nuts
|2|M5 pan head screws


It's worth mentioning that I made this project around 2012 which is why the NRF24L01 module is used for communication (the first ESP8266 was released a few years later). If I were redeploying this again today I would opt for a single ESP32 board and set it up for [MQTT](https://mqtt.org/).

Like most UK bathrooms I didn't have a convenient socket nearby, so, while the fountain resided next to my bathroom sink (plumbed into the cold water line), the circuit and power supply were in an adjacent room; the fountain was connected by an ethernet cable (hence the RJ45 adapters) and the solenoid valve was connected using 2 core speaker wire.

## Code
The arduino sketch is set up as a [finite-state machine](https://en.wikipedia.org/wiki/Finite-state_machine) with two primary inputs; one to detect if the bowl is present and another to detect if the bowl contains water. Two secondary inputs are available for setup and testing; one allows to you set the "water filling time" at the fountain and the other is used to test connectivity between the NRF24 modules.

Short demo video [HERE](https://youtu.be/1sBqBGg-HPs).