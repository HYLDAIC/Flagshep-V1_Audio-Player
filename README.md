# Flagshep V1 Audio Player
A semi-portable Audio Player device with provided PCB files, 3D files and code
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
- **Battery :** 1000 to 5000mAh LiPo with integrated charging and battery management
- **Connectors :** USB Type-C and TRS 3.5mm Audio Jack
- **Dimensions :** 98.7 x 76.8 x 34.7 mm (length-width-height)

# Design 

## Hardware & Included Sensors
- Microcontroller : ESP32 S3 Wroom 1
- DAC Chip : PCM5102A
- Battery charging & Protection : TP4056 & DW01A + FS8205A
- Voltage Regulator (LDO) : AP2112K-3.3
- Battery Percentage Chip :  MAX17048G+T10
- Battery Connector : JST XH-B2B-A
<img width="612" height="579" alt="image" src="https://github.com/user-attachments/assets/79bd4c52-8111-4478-a9ed-d182519d546c" />

*Logic Depiction of the Hardware*

## Firmware 
-Coming soon to a superstore near you!

# Enclosure & Build Instructions 
-Coming soon to a town near you!

# Future Improvements
The design has notable failure points that weren't or can't be addressed in version 1.1 and would need a more substantial redesign, here are some :
- The circuit drains significant amounts of battery on sleeping parts when the microcontroller is sleeping; physical software activated kill switch needed
- Due to bad design around the battery; the enclosure is huge
- The esp32 s3 is realistically overkill for the scope of this project AND wireless isn't the focus and could've been avoided
- Components are way too close together making assembly difficult 
- Outside ports are hard to access (too far from outside ridge)
- The lack of a knobs proves more difficult than expected
- The screen is really small (this was intended but a more substantial screen could've been afforded under this battery capacity)
- Visible screws 
- The battery percentage chip has interference issues with nearby components and should be placed closer to the battery's connector
- Thermal relief for ground pads NEED to be added

# Acknowledgements
## Help & Design inspirations 
- r/PrintedCircuits for help with circuit design (https://www.reddit.com/r/PrintedCircuitBoard/)
- PocketMage by Ashtf at Talisman Designs (specs & features section and general design inspiration) (https://www.crowdsupply.com/talisman-design/pocketmage)
## Firmware
- Adafruit (GFX & SSD1306)
- ESP32-audioI2S by Schreibfaul1
