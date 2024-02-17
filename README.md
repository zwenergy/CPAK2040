# CPAK2040
An RP2040-based controller pak for the N64.

## Overview
The main idea of CPAK2040 is to a have a simple, cheap and easily DIY-able controller pak alternative for the N64 which is non-volatile (meaning not relying on a battery to keep the controller pak data like the original based on SRAM). 
For this, a Raspberry Pi Pico board is used, as it is widely available, cheap and easy to solder even for beginners.

A regular Raspberry Pi Pico board has a 2 MB Flash, whereas a single N64 controller pak has a capacity of 32 kB.
The software running on the Pico to emulate the controller pak only takes up a fraction of the available Flash memory, hence quite many "virtual controller paks" can be stored in the Pico's Flash.
By default, the CPAK2040 holds 10 "virtual controller paks", as a more efficient navigating method needs to implemented to better use a larger number of controller paks.

## BOM
Well, the BOM is short.
You need the **PCB** and a **Raspberry Pi Pico board**.

## PCB
The Gerber files to create the PCB can be found in the "gerber" folder.
You need to order the PCB with **1.2 mm thickness** and **ENIG or hard gold**.
Using regular HASL is not recommended, as the PCB contacts quickly wear down, while solder may also rub onto the controller's connection pins.

## Setting up the Pi Pico
Hold down the BOOTSEL button, connect the Pi Pico via USB to your computer and drag'n'drop the .UF2 from the latest release onto the Pi Pico device.

## Using the CPAK2040
If you're using the CPAK2040 without the 3D printed case, make sure that you orientate the PCB correctly.
It's noted on the PCB, which side should face the front of the controller ("controller cable side") and which to the back of the controller ("Z-trigger side").

Initially, all virtual controller paks will be "corrupted", as they are not formatted as a controller pak data.
Start a game using a controller pak and hold START during the startup, this will bring up the controller pak manager.
Here, you can typically "repair" aka format the current virtual controller pak.

### Storing behaviour
The current controller pak's content is hold in the RP2040's SRAM.
When a write the controller pak happens, the updated contents are stored into the Flash.
In order to avoid unneccassary writes to the Flash, the Flash content is not directly updated, but a small timeout of some seconds is introduced, such that multiple writes can be combined into a single Flash rewrite.
This is signaled using the Pico's LED.
When then LED is on, this means that there is fresh data in the controller pak which is not written back to the Flash yet.
**Do not unplug the controller pak, while the LED is on, or otherwise you risk losing the latest controller pak data or data corruption.**

This delayed storing behaviour is done to reduce the number of Flash rewrites.
The Flash has a limited number of writes of ca. 100,000 before it wears out.
**This also means, that each "virtual controller pak" on the CPAK2040 has a lifetime of around 100,000 writes.**
Realistically, this should be enough forever for most people I guess.
If you save each day 10 times to the controller pak, you can do that for 27 years straight before you're getting close the 100,000 writes.

### Changing Virtual Controller Pak
To change the current virtual controller pak (VCP), hold the BOOTSEL button down.
The CPAK2040 will jump to the next VCP, indicating the current index by flashing the LED.
After the last VCP, the index will jump back to the first.
The current VCP index is stored into the Flash after 10s not changing it, so also after unplugging the CPAK2040 it will remember the last VCP used.

Note that many games will not reload the controller pak content, unless they sense that the controller pak is physically removed and plugged back in again.

The simple "hold button to increase VCP index" is not ideal, especially with a larger number of VCPs.
I am planning on adding a better navigating scheme.

### Backup Controller Pak Data
When plugging in the CPAK2040 via USB to a computer, the BOOTSEL button can be hold for 10 seconds (**AFTER** plugging in via USB).
This enables USB mode, which registers the CPAK2040 as a mass storage device and all the controller paks can be copied off the device (this is implemented using TinyUSB).
There is currently no way to manually exit the USB mode again, so the CPAK2040 needs to be unplugged before used again normally.

**Writing to the CPAK2040 to restore data is currently not yet implemented, but the next feature I want to include.**

## Shell
A first version of a 3D printable shell can be found in the STL folder.

## Disclaimer
**Use the files and/or schematics to build your own board at your own risk**.
This board works fine for me, but it's a simple hobby project, so there is no liability for errors in the schematics and/or board files.
**Use at your own risk**.
