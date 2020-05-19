//
//  HardwareConfiguration.cpp
//  EZUSBFirmwareDownloader
//
//  Created by Leigh Smith on 17/5/2020.
//

#include "HardwareConfiguration.h"

HardwareConfiguration::HardwareConfiguration(const char *configFilePath)
{
    CFStringRef filePath = CFStringCreateWithCString(kCFAllocatorDefault, configFilePath, kCFStringEncodingUTF8);
    CFURLRef configFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filePath, kCFURLPOSIXPathStyle, false);
    if (configFileURL) {
        if (!this->readConfigFile(configFileURL)) {
            // TODO Either raise an exception or some other signalling?
        }
        CFRelease(configFileURL);
    }
}

HardwareConfiguration::~HardwareConfiguration()
{
    // TODO free up the deviceList.
}

bool HardwareConfiguration::readConfigFile(CFURLRef configFileURL)
{
    // Read the XML file
    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, configFileURL);
    
    if (stream != NULL) {
        if (CFReadStreamOpen(stream)) {
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
                printf("%ld\n", (long) propertyListFormat);
                CFStringRef errorDescription = CFErrorCopyDescription(errorCode);
                CFShow(errorDescription);
                CFRelease(errorCode);
                return false;
            }

            if (CFGetTypeID(configurationPropertyList) != CFDictionaryGetTypeID()) {
                CFRelease(configurationPropertyList);
                return false;
            }
            //CFShow(configurationPropertyList);

            // Break out the parameters and populate the internal state.
            // Retrieve and save the hex loader file path.
            CFTypeRef hexloaderFilePathString = CFDictionaryGetValue((CFDictionaryRef) configurationPropertyList, CFSTR("HexLoader"));
            if (hexloaderFilePathString) CFRetain(hexloaderFilePathString);
            if (!hexloaderFilePathString) return false;
            
            if (CFGetTypeID(hexloaderFilePathString) != CFStringGetTypeID()) {
                CFRelease(hexloaderFilePathString);
                return false;
            }
            char fileName[256];
            if (CFStringGetCString((CFStringRef) hexloaderFilePathString, fileName, 256, kCFStringEncodingUTF8)) {
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
                    CFShow(deviceConfig);

                    // Create deviceList as std::vector<struct DeviceFirmware> ;
                    struct DeviceFirmware deviceFirmware;
                    char fileName[256];
//                    if (CFStringGetCString((CFStringRef) firmwareFileName, fileName, 256, kCFStringEncodingUTF8)) {
//                        deviceFirmware.firmwareFileName = std::string(fileName);
//                    }
//                    deviceList.append(deviceFirmware);
                }
            }
            CFRelease(configurationPropertyList);
        }
    }
    
    CFRelease(stream);
    return true;
}
