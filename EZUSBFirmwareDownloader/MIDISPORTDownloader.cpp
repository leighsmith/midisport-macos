//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is the MIDISPORT specific main function. The rest of the utility is applicable
// to all EZUSB boxes.
//
// By Leigh Smith <leigh@leighsmith.com>
//
#include <iostream>
#include "EZLoader.h"
#include "HardwareConfiguration.h"

#define midimanVendorID 0x0763

enum errorCodes {
    FIRMWARE_LOAD_SUCCESS = 0,
    MISSING_CONFIG_FILE,
    HEX_LOADER_FILE_READ_FAIL,
    FIRMWARE_FILE_READ_FAIL,
    NO_LOADED_MIDISPORT_FOUND
};

int main(int argc, const char * argv[])
{
    HardwareConfiguration *hardwareConfig;
    EZUSBLoader ezusb;
    std::vector<INTEL_HEX_RECORD> hexLoader;

    // Load config file supplied on command line.
    if (argc < 2) {
        std::cout << "Missing hardware configuration file. Usage: " << argv[0] << " configfile.xml" << std::endl;
        return MISSING_CONFIG_FILE;
    }
    hardwareConfig = new HardwareConfiguration(argv[1]);
    
    // Retrieve the hex loader filename from the config file.
    std::string hexloaderFilePath = hardwareConfig->hexloaderFilePath();
    std::cout << "Reading MIDISPORT Firmware Intel hex file " << hexloaderFilePath << std::endl;
    if (!ezusb.ReadFirmwareFromHexFile(hexloaderFilePath, hexLoader)) {
        return HEX_LOADER_FILE_READ_FAIL;
    }
    ezusb.SetApplicationLoader(hexLoader);
    
    std::cout << "Looking for uninitialised MIDISPORTs." << std::endl;
#if 0
    // Determine if the MIDISPORT is in firmware downloaded or unloaded state.
    // If cold booted, we need to download the firmware and restart the device to
    // enable the firmware product code to be found.
    for (unsigned int productIndex = 0; productIndex < hardwareConfig->productCount(); productIndex++) {
        if (ezusb.FindVendorsProduct(midimanVendorID, productTable[productIndex].coldBootProductID, true)) {

            std::cout << "Found " << productTable[productIndex].modelName << " in cold booted state." << std::endl;
            if (productTable[productIndex].firmwareFileName.length() != 0) {
                bool foundMIDSPORT = false;
                std::vector <INTEL_HEX_RECORD> firmwareToDownload;

                std::cout << "Reading MIDISPORT Firmware Intel hex file " << productTable[productIndex].firmwareFileName << std::endl;
                if (!ezusb.ReadFirmwareFromHexFile(productTable[productIndex].firmwareFileName, firmwareToDownload)) {
                    return FIRMWARE_FILE_READ_FAIL;
                }
                std::cout << "Downloading firmware." << std::endl;
                if (ezusb.StartDevice(firmwareToDownload)) {
                    // Wait up to 20 seconds for the firmware to boot & re-enumerate the USB bus properly.
                    for (unsigned int testCount = 0; testCount < 10 && !foundMIDSPORT; testCount++) {
                        foundMIDSPORT = ezusb.FindVendorsProduct(midimanVendorID, productTable[productIndex].warmFirmwareProductID, false);
                        std::cout << "Waiting before searching." << std::endl;
                        sleep(2);
                    }
                    if (foundMIDSPORT) {
                        std::cout << "Booted MIDISPORT " << productTable[productIndex].modelName << std::endl;
                    }
                    else {
                        std::cout << "Can't find re-enumerated MIDISPORT device, probable failure in downloading firmware." << std::endl;
                        return NO_LOADED_MIDISPORT_FOUND;
                    }
                }
            }
        }
    }
#endif
    return FIRMWARE_LOAD_SUCCESS;
}

