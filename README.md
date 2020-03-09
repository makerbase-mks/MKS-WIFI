Mks Robin Wifi and Mks TFT Wifi use different socket, but the same firmware.

# Features
Based on ESP8266 WIFI Module, Makerbase develp more useful function facing to the 3d printing:

- Under local area network(LAN), using RepetierHost/pronterface,etc. 

    As long as the IP and port of the wifi module are connected to the host computer, controling the printer can be realized without using a USB cable.

- Under local area network(LAN), using Cura slicer.

  Makerbase has developed the Cura plug-in——MKS Plugin, which can be used to transfer files and control printers after being installed on Cura. Since then, after slicing on Cura, the gcode file can be directly transmitted to the printer wirelessly, and the transmission speed is normally 100kBytes/s, which is very convenient.
  
- Control and monitor the printer through the MKS Cloud

  Makerbase has specially developed cloud services for 3D printers and relative mobile apps, which not only provide model storage functions, but also support MKS WIFI link functions in the background. Users can upload model files to the server for free, or directly use the above model files.
Using the mobile APP (MKS CLOUD), users can transfer the cloud model to the SD card or USB on the printer, and remotely control and monitor the printer.

# Compatibility with esp3d ?
Esp3d is a well known wifi project in 3d printing , too. If you are using MKS Robin series or MKS TFT series, mks wifi firmware can make the transfer speed up to 100KBytes/s. If you are using other 2560 series board, Esp3d firmware is more suitable. Esp3d firmware also can runs on MKS WIFI hardware, but, the interface needs to be transferred to the AUX-1.
