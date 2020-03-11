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

#define midimanVendorID 0x0763

extern INTEL_HEX_RECORD firmware1x1[];
extern INTEL_HEX_RECORD firmware2x2[];
extern INTEL_HEX_RECORD firmware4x4[];

enum WarmFirmwareProductIDs {
    MIDISPORT1x1 = 0x1011,
    MIDISPORT2x2 = 0x1002,
    MIDISPORT4x4 = 0x1021,
    MIDISPORT8x8 = 0x1031
};

struct HardwareConfigurationDescription {
    enum WarmFirmwareProductIDs warmFirmwareProductID;   // product ID indicating the firmware has been loaded and is working.
    int coldBootProductID;                          // product ID indicating the firmware has not been loaded.
    int numberOfPorts;
    int readBufSize;
    int writeBufSize;
    const char *modelName;
    INTEL_HEX_RECORD *firmware;
} productTable[] = {
    { MIDISPORT1x1, 0x1010, 1, 32, 32, "1x1", firmware1x1 },
    { MIDISPORT2x2, 0x1001, 2, 32, 32, "2x2", firmware2x2 },
    { MIDISPORT4x4, 0x1020, 4, 64, 64, "4x4", firmware4x4 },
    // Strictly speaking, the endPoint 2 can sustain 40 bytes output on the 8x8.
    // There are 9 ports including the SMPTE control.
    { MIDISPORT8x8, 0x1030, 9, 64, 32, "8x8", NULL }
};

#define PRODUCT_TOTAL (sizeof(productTable) / sizeof(struct HardwareConfigurationDescription))

int main(int argc, const char * argv[])
{
    EZUSBLoader ezusb;
    
    std::cout << "Downloading MIDISPORT Firmware" << std::endl;
    
    // Determine if the MIDISPORT is in firmware downloaded or unloaded state.
    // If cold booted, we need to download the firmware and restart the device to
    // enable the firmware product code to be found.
    for(unsigned int productIndex = 0; productIndex < PRODUCT_TOTAL; productIndex++) {
        if (ezusb.FindVendorsProduct(midimanVendorID, productTable[productIndex].coldBootProductID, true)) {
            std::cout << "MIDISPORT " << productTable[productIndex].modelName << " in cold booted state, downloading firmware." << std::endl;
#if 0
            ezusb.setFirmware(productTable[productIndex].firmware);
            ezusb.StartDevice();
            // check that we re-enumerated the USB bus properly.
            for(unsigned int testCount = 0; // Number of 2 second interval tests we'll do for the firmware.
                testCount < 10 && !ezusb.FindVendorsProduct(midimanVendorID, productTable[productIndex].warmFirmwareProductID, false);
                testCount++) {
                std::cout << "loop searching" << std::endl;
                sleep(2);
            }
            if(testCount == 10) {
                std::cout << "Can't find re-enumerated MIDISPORT device, probable failure in downloading firmware." << std::endl;
            }
#endif
            return 0;
        }
    }
    return 1;
}

