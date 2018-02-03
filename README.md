# OctoPrint-Octoremote

## Work in Progress! Use with care ##

Octoremote is a system to control serveral functions of your printer with a simple press of a button. Control the axis movement, manual extrusion and homing without the need of navigating through the printer menu or opening the Octopi GUI in your browser. 

OctoRemote consists of two components, the Octoprint plugin and an arduino based hardware controller. The controller and the plugin communicate through a serial connection using a simple byte-based protocol. As OctoRemote is based on Octoprint it is totally printer independent and works easily with any Octorprint compatible printer. At the moment following printer actions are implemented:

* Homing X&Y and Z
* Moving of all axis
* Extrude/Retract material
* Stop Print
* Pause Print
* Start/Resume Print
* Select Extruder
* Select movement distance (4 options)
* User programmable keys Supporting custom GCODE and OctoPrint Scripting

## Setup

Install via the bundled [Plugin Manager](https://github.com/foosel/OctoPrint/wiki/Plugin:-Plugin-Manager)
or manually using this URL:

    https://github.com/pkElectronics/OctoPrint-Octoremote/archive/master.zip

The plugin repository also contains the source code of the Arduino program which is necessary for the hardware controller. Installation if described in detail in the Hardware section.

## Configuration

Use the OctoRemote Plugin page from the Octoprint settings to adjust the parameters to your needs. 

The correct setting for the Comport field is something you have to figure out for yourself, using either for example the Windows device manager or the /dev/tty/\* files under linux. Usually an Arduino UNO will appear as something like /dev/ttyACM0. 

The default settings were developed and tested using an Anet A8 printer.

## Usage

It´s pretty easy, wait for Octoprint to start and start hitting the buttons, movement distance starts at Stage 1 and goes up until Stage 4 with each keypress. From Stage 4 it jumps back to 1. Same holds true for the extruder selection except that it wraps around when the number of configured extruders is reached. Keep in mind that extruding and retracting material only works if the hotend has the right temperature.

## Hardware

There are two possible hardware configurations available, a simple breadboard version using an Arduino UNO, a 4x4 matrix keypad and 8 leds. 

![Alt text](/doku/Fritzing.PNG)

Of course you could build the circuit in a different way, you just have to change the pin assignments in the software.

If you stick to the recommended schematic you will end up with a key assignment like this:

![Alt text](/doku/Keypad.PNG)

Please Note: User Key 1 is only available when the Tool/Extruder count is 1. Otherwise the key is used to switch between extruders.

In the future there will also be a custom made PCB which includes all necessary parts and uses cherry mx switches for the keys. If you are interested in getting a custom OctoRemote Board
feel free to contact me.
The device is still be compatible with the Arduino IDE for easy upgrading and hacking. Development of the Controller is in pre-production state and will be heading for Kickstarter soon.

![Alt text](/doku/octoremote.jpg)

Regardless of the type of the used hardware, programming is done using the Arduino IDE and the sourcecodes from the arduino/OctoRemote/ folder.


