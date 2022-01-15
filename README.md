# Features
Based on ESP8266 WIFI Module, Makerbase develp more useful function facing to the 3d printing:

- Under local area network(LAN), using RepetierHost/pronterface,etc. 

    As long as the IP and port of the wifi module are connected to the host computer, controling the printer can be realized without using a USB cable.

- Under local area network(LAN), using Cura slicer.

  Makerbase has developed the Cura plug-in——MKS Plugin, which can be used to transfer files and control printers after being installed on Cura. Since then, after slicing on Cura, the gcode file can be directly transmitted to the printer wirelessly, and the transmission speed is normally 100kBytes/s, which is very convenient.
  
- Control and monitor the printer through the MKS Cloud

  Makerbase has specially developed cloud services for 3D printers and relative mobile apps, which not only provide model storage functions, but also support MKS WIFI link functions in the background. Users can upload model files to the server for free, or directly use the above model files.
Using the mobile APP (MKS CLOUD), users can transfer the cloud model to the SD card or USB on the printer, and remotely control and monitor the printer.


# Hardware #
Mks wifi has two types so far: mks robin wifi and mks tft wifi, actually they are the same eletronic connect with mcu of the host-board, just using different sockets. The wifi module is designed to connect to host-board with the following signals:
 - Uart Tx/Rx : for uart data transferring
 - Reset : for reseting the wifi by the host-board
 - GPIO4 : for the wifi module to read whether the host-board is ready to receive data on serial(low level valid)
 - GPIO0 : for switching the wifi module to boot mode(high level) or firmware flash mode(low level) by the host-board
 
For more details, you can refer to :[MKS WIFI hardware](https://github.com/makerbase-mks/MKS-WIFI/tree/master/hardware)

# Firmware #
## Release ##
In the past, mks wifi firmware was released together with mks-robin or mks-tft firmware. In order to facilitate downloading, the wifi firmware version was pulled out separately from V1.0.4.

## How to compile ##
1. Install the Arduino IDE at the 1.6.8 level or later. [Arduino website](https://www.arduino.cc/en/software).
2. Download the esp8266 core for arduion. As the core we use is based on [Duet3D's version](https://github.com/Duet3D/CoreESP8266), and have little modifation. So  using the [Esp8266 core official](https://github.com/esp8266/Arduino) may cause compile errors. Please directly download the [esp8266 core on MKS Github](https://github.com/makerbase-mks/Esp8266-Core-For-Arduino) to the arduino data path, eg, "C:\Users\xxx\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266".
3. Open Arduino IDE, open the MKS WIFI project
4. Config the Tools menu:
 - Tools > select "Generic ESP8266 Module"
 - Tools > Flash Mode > select "DOUT"
 - Tools > Flash Size > select "4M(3M SPIFFS)"
 other options can be default.
5. Click the Compile button and wait it finish
6. If you connect the wifi module to PC, you can use the upload function of Arduino to flash the firmware; if not, you can directly copy the ".bin" file to the sd card, insert to the MKS robin series or TFT series board, it can automatically update the wifi firmware.
  ** Hint: to export the "*.bin" file, you can click the Sketch > Export compiled Binary on Arduino, the ".bin" file will be exported to the project directory, remember rename the file to "MksWifi.bin" before copy it to sdcard.

## About the communication between the Wifi module and Host MCU ##
Please refer to the document:https://github.com/makerbase-mks/MKS-WIFI/blob/master/Transfer%20protocal%20%20between%20ESP%20and%20MCU.docx

# Compatibility with esp3d ?
Esp3d is a well known wifi project in 3d printing , too. If you are using MKS Robin series or MKS TFT series, mks wifi firmware can make the transfer speed up to 100KBytes/s. If you are using other 2560 series board, Esp3d firmware is more suitable. Esp3d firmware also can runs on MKS WIFI hardware, but, the interface needs to be transferred to the AUX-1.

## NEWS ##

** V1.0.4 **

1. Optimize the socket communication, realize wireless control and printing using RepetierHost/pronterface etc.
2. Fix serval bugs.

## How to update ##

- Extract the zip package and you will get a file of "mkswifi.bin"
- Copy "mkswifi.bin" to the sdcard
- Insert sdcard to the relative board(Robin series and mks tft series)
- Reboot the board, firmware will updated automatically

## Note
- Thank you for using MKS products. If you have any questions during use, please contact us in time and we will work with you to solve it.
- For more product dynamic information and tutorial materials, you can always follow MKS's Facebook/Twitter/Discord/Reddit/Youtube and Github. Thank you!
- MKS Github: https://github.com/makerbase-mks  
- MKS Facebook: https://www.facebook.com/Makerbase.mks/  
- MKS Twitter: https://twitter.com/home?lang=en  
- MKS Discord: https://discord.gg/4uar57NEyU
- MKS Reddit: https://www.reddit.com/user/MAKERBASE-TEAM/ 
![mks_link](https://user-images.githubusercontent.com/12979070/149612539-d630dc46-a1b8-4696-a534-2ab1ad050462.png)


