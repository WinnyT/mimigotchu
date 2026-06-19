# Mimigotchu: a non-distractible reminder
This pet will help me reduce my screen time by playing hide-and-seek with me, so I can stay away from my phone! To be specific, they will play sound when I was doing a task for too long and run away so have to stand up and find them. Potential turn into a stress reliever.

<img width="383" height="544" alt="image" src="https://github.com/user-attachments/assets/c058fa5b-ee16-4294-ba5d-80691e04b5b1" />


# How to build
1. 3D print the pet case and wheels
2. Solder all the component of the PCB on to the PCB
   1. XIAO ESP32-C6 (SMD, center of board)
   2. L293D motor driver (U3)
   3. OLED header pins (U2)
   4. Buttons SW1, SW2, SW3
   5. Buzzer (BUZZER1)
   6. Battery connector (BT1)
   7. Motor connector (M1)
4. Screw the PCB on to the upper case using the M3 bolt 
5. Put on the rubber band to the wheels
6. Place the motor on the **Right Bottom** corner, it should fit the motor holder case
7. Put the stick in between the hole. **THEN** put on the wheels. Do the same for both front and back
8. Snap 2 case todether and connect the pet to your computer with a type-c cable
9. Download the firmware file and upload it to XIAO ESP32

# How to use
There are 3 buttons from LEFT to RIGHT. I'll call them Left - Middle - Right

Left Button: 
Short press - +1 minute

Middle Button: 
Short press - +1 second

Right Button:
Short press - Start countdown
Long press - Switch to pet screen

Left & Middle Button - Reset Timer

Oled screen:
Timer mode: showing countdown of the timer MM:SS
Pet mode: showing hapiness and age

**MIMI will run away when the timer is UP**

# Why I make this
I found myself addicted to phone, and I have tried many ways to reduce my screen time like set a timer or use a timer lock down. But it never work. The problem I found with setting a timer is I leave it by my side, so it was too easy for me to turn it off and not getting away from my phone. I hope this robot can run away, make me stand up and catch them. _Sounds silly but what if it works ?.?_

# CAD 

<img width="541" height="550" alt="image" src="https://github.com/user-attachments/assets/671dd2f5-11c2-4e9b-b1cd-bda810b7a749" />


[Link](https://cad.onshape.com/documents/4d0aa6bf6d748b99eef8425c/w/4eb2ead038335fc9ea83a4bf/e/15b4b795a0ade6c865b0f643?renderMode=0&uiState=6a34a4132959f86fc017db1b) to Onshape design. 

# PCB

[Download gebber file](https://github.com/WinnyT/mimigotchu/blob/main/assets/pcb/gebber/gebber.zip)
### 3D view of the PCB

Front

<img width="321" height="618" alt="image" src="https://github.com/user-attachments/assets/6c3f02f4-7a8a-4ef4-aac3-24152046cf83" />

Back

<img width="289" height="514" alt="image" src="https://github.com/user-attachments/assets/687b615e-9ec3-4c2c-a051-80cd51eabd05" />

### 3D model after assembly the PCB
Front

<img width="365" height="264" alt="image" src="https://github.com/user-attachments/assets/9c586dce-db25-4eaa-a9d9-edabb440b5c8" />

Back

<img width="575" height="345" alt="image" src="https://github.com/user-attachments/assets/7e37e917-98e2-4815-a4ff-64facc8fed87" />

### Schematic

<img width="1097" height="777" alt="image" src="https://github.com/user-attachments/assets/70342290-0ddb-4812-a186-a4f8aa7b29e8" />


# Firmware

You can flash Mimigotchi without installing PlatformIO.

[Download latest firmware](https://github.com/WinnyT/mimigotchu/releases/tag/v1.0.0)

### Flash instructions (macOS/Windows/Linux)
1. Install [esptool](https://docs.espressif.com/projects/esptool/en/latest/) via terminal:

pip install esptool

2. Connect your Mimigotchi via USB-C
3. Download `bootloader.bin`, `partitions.bin`, and `firmware.bin` from the release
4. Run (from the folder containing all three files):
esptool.py --port /dev/tty.usbmodem* write_flash 

0x0     bootloader.bin 

0x8000  partitions.bin 

0x10000 firmware.bin
   On Windows, replace `/dev/tty.usbmodem*` with `COM3`
   
### Build from source
If you want to modify the code, open the project in VS Code with PlatformIO installed and press `Ctrl+Alt+B` to build, then `Ctrl+Alt+U` to upload.

# BOM
Check out `mimigotchu_bom.csv` or [this](https://docs.google.com/spreadsheets/d/1_V7xxnciXNeib1qt1jn1ameioscYv5FXHhLbUMrxZ48/edit?usp=sharing) link.

# Easter Egg
If you finished a study session ( 50+ minutes) your pet will be happier !!
