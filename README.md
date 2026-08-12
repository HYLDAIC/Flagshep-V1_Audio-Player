# Flagshep V1 Audio Player
<img width="4896" height="3672" alt="P1040043" src="https://github.com/user-attachments/assets/1a108c3d-ff52-4c17-b454-7bcd1c073cc4" />
*During Playback*

## Overview
The **Flagshep** is a semi-portable open-source Audio Player designed to provide a reliable, high-quality offline listening experience. It's purpose is to be an alternative to advertisement-ridden subscription based streaming services. The pilot project includes the PCB design files, enclosure CAD models and firmware source code required to build upon, modify or simply take inspiration of the device.

# Features & Specifications 
## Features
- High Quality Audio Diffusion 
- USB-C Charging and Flashing 
## Specifications 
- **Processor :** ESP32 S3 (2 Core 240MHz, 2.4 GHz Wi-Fi 150 Mbps, Bluetooth 5 (LE) )
- **Audio Quality :** PCM5102A ; 16-32bit at 8kHz to 384kHz
- **Flash :** 8MB quad SPI Flash
- **RAM :** 8MB quad SPI SRAM
- **Display :** 0.96in 128x64px OLED TFT display
- **Input :** 6 Button Matrix
- **Storage :** MicroSD card slot
- **Battery :** 5000mAh LiPo with integrated charging and battery management
- **Connectors :** USB Type-C and TRS 3.5mm Audio Jack
- **Dimensions :** 98.7 x 76.8 x 34.7 mm (length-width-height)

# Design 

## Hardware & Included Sensors
- **Microcontroller :** ESP32 S3 Wroom 1
- **DAC Chip :** PCM5102A
- **Battery charging & Protection :** TP4056 & DW01A + FS8205A
- **Voltage Regulator (LDO) :** AP2112K-3.3
- **Battery Percentage Chip :**  MAX17048G+T10
- **Battery Connector :** JST XH-B2B-A
<img width="612" height="579" alt="image" src="https://github.com/user-attachments/assets/79bd4c52-8111-4478-a9ed-d182519d546c" />

*Logic Depiction of the Hardware*

## Firmware 
-Coming soon to a superstore near you!

# Enclosure & Build Instructions 
- M4 screwdriver
- 3D printed parts (Main, Top of your choosing, and Bottom cover)
- The PCB
- A Lithium Polymer battery (LiPo) with the connector JST-
- 12 6-8mm screws M4 screws
- 12 6x6mm (M4) threaded heated inserts
- Formatted SD Card (FAT32,  exFAT ...)
- Computer (Linux and or Windows 10-11)
- USB-C Data cable
- Soldering iron

  1. The Main 3D printed part should have prominent holes, 8 on the front side and 4 in the back
  2. Prepare heated element to 250 degrees Celsius (+/- 30)
  3. Place and slowly push in the threaded heated insert with the soldering iron into the hole until the mounting surface is flat (repeat 12 times)
  4. If the battery connector of the PCB isn't on the bottom face (face with no components), solder it on the bottom making sure to keep the same polarities
  5. Take the PCB and insert it inside the side with 8 screws, it should fit with the bottom four; screw it on (it should match with the outside holes on the side of the Main piece)
  6. Insert and screw in the Top cover with 4 screws
  7. Insert a LiPo battery in the bottom compartment and plug it in
-Coming soon to a town near you!

# Successes & Future Improvements (Hardware focused)
## Successes
- The audio is smooth and doesn't have noticeable glitching or noise within playback
- A battery life of about 32-40 hours on a 5000mAh battery (~35h of playback in an room varying from 23 to 35 degrees Celsius)
- A smooth user experience; fairly intuitive button mapping, responsive menus and UI (tested by passing my friends the player and receiving feedback)
- An easily accessible and removable battery/screen (4 Screws each)
- Fairly resistant; could be dropped several times with no issues to the PCB (1m drop height)
- The PCB worked first try :)

## Future Improvements
The design has notable failure points that can't be addressed in version 1.1 and would need a more substantial redesign, here are some :
- The circuit drains significant amounts of battery on sleeping parts when the microcontroller is sleeping; physical software activated kill switch needed
- Due to bad design around the battery; the enclosure is huge (height)
- The ESP32 S3 is realistically overkill for the scope of this project AND wireless isn't the focus and could've been avoided
- Components are way too close together making assembly difficult 
- The lack of a knobs proves more difficult than expected
- The screen is really small (this was intended but a more substantial screen could've been afforded under this battery capacity)
- Visible and protruding screws
- Somewhat Heavy (battery)
- The battery percentage chip has interference issues with nearby components and should be placed closer to the battery's connector
- Adding test pads for important signals

Issues that weren't but can be easily addressed in version 1.2 :
- Outside ports are hard to access (too far from outside ridge)
- Thermal relief for ground pads NEED to be added

Personal issues with the design : 
- Many parts of the circuit could have been separated on different boards to permit easy fixing and maintenance (e.g. board 1; power, board 2; logic, etc)

## Plans for the project
Apart from version 1.2, I plan to remake the audio player in a couple of years after I have learned more about circuit design and embedded development.
For now, all the following improvements made to this project will made in the form of sporadic software improvements as I catch problems or wants with the device in my own usage.

# Acknowledgements
## Help & Design inspirations 
- My non-technical friends for support & UX feedback
- r/PrintedCircuits for help with circuit design (https://www.reddit.com/r/PrintedCircuitBoard/)
- PocketMage by Ashtf at Talisman Designs (specs & features section and general design inspiration) (https://www.crowdsupply.com/talisman-design/pocketmage)
## Firmware
- Adafruit (GFX & SSD1306)
- ESP32-audioI2S by Schreibfaul1 (https://github.com/schreibfaul1/ESP32-audioI2S)
