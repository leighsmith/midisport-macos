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
            // Either raise an exception or some other signalling?
        
        }
        CFRelease(configFileURL);
    }
}

HardwareConfiguration::~HardwareConfiguration()
{
    
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
                printf("%ld\n", (long) propertyListFormat);
                // Handle the error
                CFStringRef errorDescription = CFErrorCopyDescription(errorCode);
                CFShow(errorDescription);
                CFRelease(errorCode);
                return false;
            }

            if (CFGetTypeID(configurationPropertyList) != CFDictionaryGetTypeID()) {
                CFRelease(configurationPropertyList);
                return false;
            }
            CFShow(configurationPropertyList);

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

            // Loop over the property list
            // Create deviceList as std::vector<struct DeviceFirmware> ;

            CFRelease(configurationPropertyList);
        }
    }
    
    CFRelease(stream);
    return true;
}
