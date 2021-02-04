//
//  HardwareConfiguration.cpp
//

#include "HardwareConfiguration.h"
#define MAX_PATH_LEN 256

HardwareConfiguration::HardwareConfiguration(const char *configFilePath)
{
    CFStringRef filePath = CFStringCreateWithCString(kCFAllocatorDefault, configFilePath, kCFStringEncodingUTF8);
    CFURLRef configFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filePath, kCFURLPOSIXPathStyle, false);
    if (configFileURL) {
        if (!this->readConfigFile(configFileURL)) {
            // Throw an exception if the file can't be found.
            throw std::runtime_error("Unable to read MIDISPORT device config file.");
        }
        CFRelease(configFileURL);
    }
}

HardwareConfiguration::~HardwareConfiguration()
{
    // TODO free up the deviceList.
}

// Retrieve the DeviceFirmware structure for the given cold boot device id.
struct DeviceFirmware HardwareConfiguration::deviceFirmwareForBootId(unsigned int coldBootDeviceId)
{
    // Search for coldBootDeviceId
    // Use at() to catch the cold boot device id not being found and throw as an std::out_of_range exception.
    return deviceList.at(coldBootDeviceId);
}

// Retrieve the DeviceFirmware structure for the given warm boot device id.
struct DeviceFirmware HardwareConfiguration::deviceFirmwareForWarmBootId(unsigned int warmBootDeviceId)
{
    // Search for warmBootDeviceId, which is not the key in the map.
    for(DeviceList::iterator deviceIterator = deviceList.begin(); deviceIterator != deviceList.end(); deviceIterator++) {
        if(deviceIterator->second.warmFirmwareProductID == warmBootDeviceId) {
            return deviceIterator->second;
        }
    }
    // Throw an exception when the warm boot id is not found?
    throw std::out_of_range("Could not find warm boot device id");
}

// Converts the parameters of a single MIDISPORT model, in a property list CFDictionary, into the C++ DeviceFirmware structure.
// This does the data validation that all the parameters are there, returning true or false.
bool HardwareConfiguration::deviceListFromDictionary(CFDictionaryRef deviceConfig, struct DeviceFirmware &deviceFirmware)
{
    // Name of device model
    CFTypeRef deviceNameString = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("DeviceName"));
    if (!deviceNameString)
        return false;
    if (CFGetTypeID(deviceNameString) == CFStringGetTypeID()) {
        char deviceName[MAX_PATH_LEN];

        if (CFStringGetCString((CFStringRef) deviceNameString, deviceName, MAX_PATH_LEN, kCFStringEncodingUTF8)) {
            deviceFirmware.modelName = std::string(deviceName);
        }
        else {
            return false;
        }
    }
    // Firmware pathname
    CFTypeRef firmwareFileName = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("FilePath"));
    if (!firmwareFileName)
        return false;
    if (CFGetTypeID(firmwareFileName) == CFStringGetTypeID()) {
        char fileName[MAX_PATH_LEN];

        if (CFStringGetCString((CFStringRef) firmwareFileName, fileName, MAX_PATH_LEN, kCFStringEncodingUTF8)) {
            deviceFirmware.firmwareFileName = std::string(fileName);
        }
        else {
            return false;
        }
    }
    // The cold boot product id.
    CFTypeRef coldBootProductId = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("ColdBootProductID"));
    if (CFGetTypeID(coldBootProductId) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef) coldBootProductId, kCFNumberIntType, &deviceFirmware.coldBootProductID)) {
            return false;
        }
    }
    // The warm firmware product id.
    CFTypeRef warmFirmwareProductId = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("WarmFirmwareProductID"));
    if (CFGetTypeID(warmFirmwareProductId) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef) warmFirmwareProductId, kCFNumberIntType, &deviceFirmware.warmFirmwareProductID)) {
            return false;
        }
    }
    // Whether the MIDI ports should be named numerically or alphabetically.
    CFTypeRef numericPortNaming;
    if (CFDictionaryGetValueIfPresent((CFDictionaryRef) deviceConfig, CFSTR("NumericPortNaming"), &numericPortNaming)) {
        if (CFGetTypeID(numericPortNaming) == CFBooleanGetTypeID()) {
            deviceFirmware.numericPortNaming = CFBooleanGetValue((CFBooleanRef) numericPortNaming);
        }
        else {
            deviceFirmware.numericPortNaming = false;
        }
    }
    // The number of MIDI ports. Legacy parameter, nowadays we specify input and output ports individually.
    CFTypeRef numberOfPorts;
    if (CFDictionaryGetValueIfPresent((CFDictionaryRef) deviceConfig, CFSTR("NumberOfPorts"), &numberOfPorts)) {
        if (CFGetTypeID(numberOfPorts) == CFNumberGetTypeID()) {
            if (!CFNumberGetValue((CFNumberRef) numberOfPorts, kCFNumberIntType, &deviceFirmware.numberOfInputPorts)) {
                return false;
            }
            // Make output == input ports.
            deviceFirmware.numberOfOutputPorts = deviceFirmware.numberOfInputPorts;
        }
    }
    // The number of MIDI input ports.
    CFTypeRef numberOfInputPorts;
    if (CFDictionaryGetValueIfPresent((CFDictionaryRef) deviceConfig, CFSTR("NumberOfInputPorts"), &numberOfInputPorts)) {
        if (CFGetTypeID(numberOfInputPorts) == CFNumberGetTypeID()) {
            if (!CFNumberGetValue((CFNumberRef) numberOfInputPorts, kCFNumberIntType, &deviceFirmware.numberOfInputPorts)) {
                return false;
            }
        }
    }
    // The number of MIDI output ports.
    CFTypeRef numberOfOutputPorts;
    if (CFDictionaryGetValueIfPresent((CFDictionaryRef) deviceConfig, CFSTR("NumberOfOutputPorts"), &numberOfOutputPorts)) {
        if (CFGetTypeID(numberOfOutputPorts) == CFNumberGetTypeID()) {
            if (!CFNumberGetValue((CFNumberRef) numberOfOutputPorts, kCFNumberIntType, &deviceFirmware.numberOfOutputPorts)) {
                return false;
            }
        }
    }
    // The port that receives SMPTE code, so it can be labelled as such.
    CFTypeRef SMPTEport;
    if (CFDictionaryGetValueIfPresent((CFDictionaryRef) deviceConfig, CFSTR("SMPTEport"), &SMPTEport)) {
        if (CFGetTypeID(SMPTEport) == CFNumberGetTypeID()) {
            if (!CFNumberGetValue((CFNumberRef) SMPTEport, kCFNumberIntType, &deviceFirmware.SMPTEport)) {
                return false;
            }
        }
    }
    // The read buffer size.
    CFTypeRef readBufSize = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("ReadBufferSize"));
    if (CFGetTypeID(readBufSize) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef) readBufSize, kCFNumberIntType, &deviceFirmware.readBufSize)) {
            return false;
        }
    }
    // The write buffer size.
    CFTypeRef writeBufSize = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("WriteBufferSize"));
    if (CFGetTypeID(writeBufSize) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef) writeBufSize, kCFNumberIntType, &deviceFirmware.writeBufSize)) {
            return false;
        }
    }
    return true;
}

// Read the XML property list file containing the declarations for the MIDISPORT devices.
bool HardwareConfiguration::readConfigFile(CFURLRef configFileURL)
{
    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, configFileURL);
    
    if (stream == NULL)
        return false;
    if (!CFReadStreamOpen(stream)) {
        CFRelease(stream);
        return false;
    }
    else {
        // Reconstitute the dictionary using the XML data
        CFErrorRef errorCode;
        CFPropertyListFormat propertyListFormat;
        CFPropertyListRef configurationPropertyList = CFPropertyListCreateWithStream(kCFAllocatorDefault,
                                                                                     stream, 0,
                                                                                     kCFPropertyListImmutable,
                                                                                     &propertyListFormat,
                                                                                     &errorCode);
        if (configurationPropertyList == NULL) {
            // Handle the error
            fprintf(stderr, "Unable to load configuration property list format %ld\n", (long) propertyListFormat);
            CFStringRef errorDescription = CFErrorCopyDescription(errorCode);
            CFShow(errorDescription);
            CFRelease(errorCode);
            return false;
        }

        if (CFGetTypeID(configurationPropertyList) != CFDictionaryGetTypeID()) {
            CFRelease(configurationPropertyList);
            return false;
        }
        // CFShow(configurationPropertyList); // Debugging.

        // Break out the parameters and populate the internal state.
        // Retrieve and save the hex loader file path.
        CFTypeRef hexloaderFilePathString = CFDictionaryGetValue((CFDictionaryRef) configurationPropertyList, CFSTR("HexLoader"));
        if (!hexloaderFilePathString)
            return false;

        if (CFGetTypeID(hexloaderFilePathString) != CFStringGetTypeID()) {
            CFRelease(hexloaderFilePathString);
            return false;
        }
        char fileName[MAX_PATH_LEN];
        if (CFStringGetCString((CFStringRef) hexloaderFilePathString, fileName, MAX_PATH_LEN, kCFStringEncodingUTF8)) {
            hexloaderFilePathName = std::string(fileName);
        }

        // Verify there is a device list:
        CFTypeRef deviceArray = CFDictionaryGetValue((CFDictionaryRef) configurationPropertyList, CFSTR("Devices"));
        if (CFGetTypeID(deviceArray) != CFArrayGetTypeID()) {
            return false;
        }
        
        // Loop over the device list:
        for (CFIndex deviceIndex = 0; deviceIndex < CFArrayGetCount((CFArrayRef) deviceArray); deviceIndex++) {
            CFDictionaryRef deviceConfig = (CFDictionaryRef) CFArrayGetValueAtIndex((CFArrayRef) deviceArray, deviceIndex);

            if (CFGetTypeID(deviceConfig) == CFDictionaryGetTypeID()) {
                struct DeviceFirmware deviceFirmware;

                if (!this->deviceListFromDictionary(deviceConfig, deviceFirmware))
                    return false;
                deviceList[deviceFirmware.coldBootProductID] = deviceFirmware;
            }
        }
        CFRelease(configurationPropertyList);
    }
    CFRelease(stream);
    return true;
}
