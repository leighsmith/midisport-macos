//
//  Defines a class holding the HardwareConfiguration table, which is read from a text configuration file,
//  associating device USB id codes to Intel hex firmware files.
//

#ifndef HardwareConfiguration_h
#define HardwareConfiguration_h

#include <map>
#include <string>
#include <CoreFoundation/CoreFoundation.h>   // For property list I/O.

struct DeviceFirmware {
    std::string modelName;
    unsigned int warmFirmwareProductID;     // Product ID indicating the firmware has been loaded and is working.
    unsigned int coldBootProductID;         // Product ID indicating the firmware has not been loaded.
    std::string firmwareFileName;           // Path to the Intel hex file of the firmware. NULL indicates no firmware needs to be downloaded.
};

typedef std::map<unsigned int, struct DeviceFirmware> DeviceList;

class HardwareConfiguration {
public:
    HardwareConfiguration(const char *configFile);
    ~HardwareConfiguration();

    std::string hexloaderFilePath() { return hexloaderFilePathName; }
    unsigned int productCount() { return static_cast<unsigned int>(deviceList.size()); }
    struct DeviceFirmware deviceFirmwareForBootId(unsigned int);

    DeviceList deviceList; // temporarily public
private:
    std::string hexloaderFilePathName;

    bool readConfigFile(CFURLRef configFileURL);
    bool deviceListFromDictionary(CFDictionaryRef deviceConfig, struct DeviceFirmware &deviceFirmware);
};

#endif /* HardwareConfiguration_h */
