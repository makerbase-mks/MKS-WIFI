// Configuration for RepRapWiFi

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED



//#define SPI_DEBUG
#define SHOW_PASSWORDS

// Define the maximum length (bytes) of file upload data per SPI packet. Use a multiple of the SD card file or cluster size for efficiency.
// ************ This must be kept in step with the corresponding value in RepRapFirmwareWiFi *************
////const uint32_t maxSpiFileData = 2048;

// Define the SPI clock frequency
// The SAM occasionally transmits incorrect data at 40MHz, so we now use 26.7MHz.
//const uint32_t spiFrequency = 27000000;     // This will get rounded down to 80MHz/3
////const uint32_t spiFrequency = 16000000;     // This will get rounded down to 80MHz/5
//const uint32_t spiFrequency = 10000000;     // This will get rounded down to 80MHz/8
//const uint32_t spiFrequency = 8000000;     // This will get rounded down to 80MHz/10
//const uint32_t spiFrequency = 4000000;     // This will get rounded down to 80MHz/20

// Pin numbers
//const int SamSSPin = 15;          // GPIO15, output to SAM, SS pin for SPI transfer
const int EspReqTransferPin = 0;  // GPIO0, output, indicates to the SAM that we want to send something
const int McuTfrReadyPin = 4;     // GPIO4, input, indicates that MCU is ready to execute an SPI transaction

#endif



