//
// MacOS X firmware downloader daemon for the EZUSB device,
// as found in MIDIMan/M-Audio MIDISPORT boxes.
//
// This is the MIDISPORT specific main function. The rest of the utility is applicable
// to all EZUSB boxes.
//

#include <iostream>
#include "EZLoader.h"
#include "HardwareConfiguration.h"
#include "IntelHexFile.h"

#define mAudioVendorID 0x0763

enum errorCodes {
    FIRMWARE_LOAD_SUCCESS = 0,
    MISSING_CONFIG_FILE,
    HEX_LOADER_FILE_READ_FAIL,
    FIRMWARE_FILE_READ_FAIL,
    NO_LOADED_MIDISPORT_FOUND
};

bool downloadFirmwareToDevice(EZUSBLoader *ezusb, struct DeviceFirmware device)
{
    std::cout << "Found " << device.modelName << " in cold booted state." << std::endl;
    if (device.firmwareFileName.length() != 0) {
        bool foundMIDSPORT = false;
        std::vector <INTEL_HEX_RECORD> firmwareToDownload;

        std::cout << "Reading MIDISPORT Firmware Intel hex file: " << device.firmwareFileName << std::endl;
        if (!IntelHexFile::ReadFirmwareFromHexFile(device.firmwareFileName, firmwareToDownload)) {
            std::cerr << "Unable to read MIDISPORT Firmware Intel hex file " << device.firmwareFileName << std::endl;
            return false;
        }
        std::cout << "Downloading firmware." << std::endl;
        if (ezusb->StartDevice(firmwareToDownload)) {
#if 0
            // Wait up to 20 seconds for the firmware to boot & re-enumerate the USB bus properly.
            for (unsigned int testCount = 0; testCount < 10 && !foundMIDSPORT; testCount++) {
                foundMIDSPORT = ezusb->FindVendorsProduct(mAudioVendorID, device.warmFirmwareProductID, false);
                std::cout << "Waiting before searching." << std::endl;
                sleep(2);
            }
#else
            foundMIDSPORT = true;
#endif
            if (foundMIDSPORT) {
                std::cout << "Booted " << device.modelName << std::endl;
            }
            else {
                std::cout << "Can't find re-enumerated MIDISPORT device, probable failure in downloading firmware." << std::endl;
                return false;
            }
        }
    }
    return true;
}

int main(int argc, const char * argv[])
{
    HardwareConfiguration *hardwareConfig;
    std::vector<INTEL_HEX_RECORD> hexLoader;

    // Load config file supplied on command line.
    if (argc < 2) {
        std::cerr << "Missing hardware configuration file. Usage: " << argv[0] << " configfile.xml" << std::endl;
        return MISSING_CONFIG_FILE;
    }
    try {
        hardwareConfig = new HardwareConfiguration(argv[1]);
    }
    catch (std::runtime_error e) {
        std::cerr << "Unable to read hardware configuration file: " << argv[1] << std::endl;
        return MISSING_CONFIG_FILE;
    }

    EZUSBLoader ezusb(mAudioVendorID, hardwareConfig->deviceList, true);

    // Retrieve the hex loader filename from the config file.
    std::string hexloaderFilePath = hardwareConfig->hexloaderFilePath();
    std::cout << "Reading Hex loader firmware Intel hex file: " << hexloaderFilePath << std::endl;
    if (!IntelHexFile::ReadFirmwareFromHexFile(hexloaderFilePath, hexLoader)) {
        std::cerr << "Unable to read Hex loader firmware Intel hex file " << hexloaderFilePath << std::endl;
        return HEX_LOADER_FILE_READ_FAIL;
    }
    ezusb.SetApplicationLoader(hexLoader);
    ezusb.SetFoundDeviceNotification(downloadFirmwareToDevice);

    // Scan for MIDISPORT in firmware unloaded state.
    // If cold booted, we need to download the firmware and restart the device to
    // enable the firmware product code to be found.
    std::cout << "Looking for uninitialised MIDISPORTs with vendor = 0x" << std::hex << mAudioVendorID << std::endl;
    ezusb.ScanDevices();  // Scan for the devices which are already registered to the USB registry.

    // Start the run loop so notifications will be received
    CFRunLoopRun();

    // Because the run loop will run forever until interrupted,
    // the program should never reach this point.
    return 0;
}
