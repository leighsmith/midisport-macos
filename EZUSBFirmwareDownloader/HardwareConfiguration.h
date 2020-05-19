//
//  HardwareConfiguration.h
//  EZUSBFirmwareDownloader
//
//  Defines a class holding the HardwareConfiguration table, which is read from a text configuration file,
//  associating device codes to Intel hex firmware files.
//
//  Created by Leigh Smith on 17/5/2020.
//

#ifndef HardwareConfiguration_h
#define HardwareConfiguration_h

#include <vector>
#include <string>
#include <CoreFoundation/CoreFoundation.h>   // For property list I/O.

struct DeviceFirmware {
    std::string modelName;
    unsigned int warmFirmwareProductID;               // Product ID indicating the firmware has been loaded and is working.
    unsigned int coldBootProductID;                   // Product ID indicating the firmware has not been loaded.
    // std::vector<INTEL_HEX_RECORD> firmware;
    std::string firmwareFileName;                     // Path to the Intel hex file of the firmware. NULL indicates no firmware needs to be downloaded.
};


class HardwareConfiguration {
public:
    HardwareConfiguration(const char *configFile);
    ~HardwareConfiguration();

    std::string hexloaderFilePath() { return hexloaderFilePathName; }
    unsigned int productCount() { return static_cast<unsigned int>(deviceList.size()); }
private:
    std::vector<struct DeviceFirmware> deviceList;
    std::string hexloaderFilePathName;

    bool readConfigFile(CFURLRef configFileURL);
};

#endif /* HardwareConfiguration_h */
